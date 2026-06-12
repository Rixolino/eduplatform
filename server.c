#ifdef _WIN32
/* Impedisce a windows.h di includere il vecchio winsock.h */
#define WIN32_LEAN_AND_MEAN
/* Includi esplicitamente winsock2 prima di windows.h */
#include <winsock2.h>
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <microhttpd.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include "db_utils.h"
#include "auth.h"
#include "json_utils.h"

#define PORT 5000

/* Rimuove il warning di ridefinizione di MAX_PATH */
#ifdef MAX_PATH
#undef MAX_PATH
#endif
#define MAX_PATH 1024

#define MAX_RESPONSE 16384
#define MAX_POST_SIZE 8192
void log_message(const char *format, ...);

sqlite3 *db;

// Determine MIME type by file extension
const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".woff") == 0) return "font/woff";
    if (strcmp(ext, ".woff2") == 0) return "font/woff2";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    return "application/octet-stream";
}

// Serve static file from disk
int serve_static_file(struct MHD_Connection *connection, const char *url) {
    char path[MAX_PATH];
    const char *rel = url;

    if (strcmp(url, "/") == 0 || strcmp(url, "") == 0) {
        rel = "/index.html";
    }

    // Remove leading '/'
    if (rel[0] == '/') rel++;

    snprintf(path, sizeof(path), "%s", rel);

    FILE *f = fopen(path, "rb");
    if (!f) return MHD_NO;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(sz);
    if (!buf) {
        fclose(f);
        return MHD_NO;
    }

    if (fread(buf, 1, sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return MHD_NO;
    }
    fclose(f);

    struct MHD_Response *response = MHD_create_response_from_buffer(sz, (void *)buf, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(response, "Content-Type", get_mime_type(path));
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return MHD_YES;
}

// Initialize database schema from file if core tables missing
int initialize_database_from_schema(const char *schema_path) {
    // Check if 'users' table exists
    sqlite3_stmt *stmt;
    const char *query = "SELECT name FROM sqlite_master WHERE type='table' AND name='users'";
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        int exists = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        if (exists) return 1; // already initialized
    }

    // Read schema file
    FILE *f = fopen(schema_path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, sz, f) != (size_t)sz) { free(buf); fclose(f); return 0; }
    buf[sz] = '\0';
    fclose(f);

    char *err = NULL;
    rc = sqlite3_exec(db, buf, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        log_message("ERROR: Failed to initialize DB schema: %s", err ? err : "unknown");
        if (err) sqlite3_free(err);
        free(buf);
        return 0;
    }
    free(buf);
    log_message("Database schema initialized from %s", schema_path);
    return 1;
}

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} PostData;

// Funzione per loggare messaggi
void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[26];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    printf("[%s] ", timestamp);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

// Funzione per aprire il database
int open_database(const char *db_path) {
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        log_message("ERROR: Cannot open database: %s", sqlite3_errmsg(db));
        return 0;
    }
    log_message("Database opened successfully: %s", db_path);
    return 1;
}

// Funzione per chiudere il database
void close_database() {
    if (db) {
        sqlite3_close(db);
        log_message("Database closed");
    }
}

// Funzione per preparare una risposta JSON
void send_json_response(struct MHD_Connection *connection, const char *json, int status) {
    struct MHD_Response *response;
    size_t json_len = strlen(json);
    char *json_copy = (char *)malloc(json_len + 1);
    strcpy(json_copy, json);

    response = MHD_create_response_from_buffer(json_len,
                                              (void *)json_copy,
                                              MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(response, "Content-Type", "application/json");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
}

// Simple JSON field extractor (no regex)
char *json_get_field(const char *json, const char *field) {
    if (!json || !field) return NULL;

    // Build search pattern: "fieldname":
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "\"%s\":", field);

    // Find the pattern
    const char *pos = strstr(json, pattern);
    if (!pos) return NULL;

    // Move past the pattern to find the value
    pos += strlen(pattern);

    // Skip whitespace
    while (*pos && isspace(*pos)) pos++;

    // Check if value is a string (starts with ")
    if (*pos == '"') {
        pos++; // Skip opening quote

        // Find closing quote (handle escaping)
        char *result = (char *)malloc(512);
        int idx = 0;

        while (*pos && idx < 511) {
            if (*pos == '"' && (idx == 0 || result[idx-1] != '\\')) {
                // Found closing quote
                result[idx] = '\0';
                return result;
            }
            result[idx++] = *pos;
            pos++;
        }
        free(result);
        return NULL;
    } 
    // Handle numbers or booleans
    else if (isdigit(*pos) || *pos == '-' || *pos == 't' || *pos == 'f' || *pos == 'n') {
        char *result = (char *)malloc(512);
        int idx = 0;
        
        while (*pos && idx < 511 && *pos != ',' && *pos != '}' && !isspace(*pos)) {
            result[idx++] = *pos;
            pos++;
        }
        result[idx] = '\0';
        return result;
    }

    return NULL;
}

void run_task_visibility_migration() {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS task_visibility ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "task_id INTEGER NOT NULL,"
        "student_id INTEGER NOT NULL,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "UNIQUE(task_id, student_id),"
        "FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE,"
        "FOREIGN KEY (student_id) REFERENCES users(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_task_visibility_task_id ON task_visibility(task_id);"
        "CREATE INDEX IF NOT EXISTS idx_task_visibility_student_id ON task_visibility(student_id);";
    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        log_message("ERROR: task visibility migration failed: %s", err ? err : "unknown");
        if (err) sqlite3_free(err);
    }
}

int get_auth_user_id(struct MHD_Connection *connection) {
    const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
    if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) return 0;

    int user_id = 0;
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, auth_header + 7, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) user_id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return user_id;
}

int user_has_role(int user_id, const char *role_a, const char *role_b) {
    char role[32] = "";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, "SELECT role FROM users WHERE id = ?", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *role_text = (const char *)sqlite3_column_text(stmt, 0);
            if (role_text) strncpy(role, role_text, sizeof(role) - 1);
        }
        sqlite3_finalize(stmt);
    }
    return (role_a && strcmp(role, role_a) == 0) || (role_b && strcmp(role, role_b) == 0);
}

int teacher_owns_task(int user_id, int task_id) {
    int allowed = 0;
    sqlite3_stmt *stmt;
    const char *q = "SELECT 1 FROM tasks WHERE id = ? AND teacher_id = ?";
    if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, task_id);
        sqlite3_bind_int(stmt, 2, user_id);
        allowed = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }
    return allowed || user_has_role(user_id, "admin", NULL);
}

int teacher_owns_course(int user_id, int course_id) {
    int allowed = 0;
    sqlite3_stmt *stmt;
    const char *q = "SELECT 1 FROM courses WHERE id = ? AND teacher_id = ?";
    if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, course_id);
        sqlite3_bind_int(stmt, 2, user_id);
        allowed = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }
    return allowed || user_has_role(user_id, "admin", NULL);
}

int task_visible_to_student(int task_id, int student_id) {
    int visible = 0;
    sqlite3_stmt *stmt;
    const char *q =
        "SELECT CASE "
        "WHEN NOT EXISTS (SELECT 1 FROM task_visibility WHERE task_id = ?) THEN 1 "
        "WHEN EXISTS (SELECT 1 FROM task_visibility WHERE task_id = ? AND student_id = ?) THEN 1 "
        "ELSE 0 END";
    if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, task_id);
        sqlite3_bind_int(stmt, 2, task_id);
        sqlite3_bind_int(stmt, 3, student_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) visible = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return visible;
}

void save_task_visibility_from_csv(int task_id, const char *csv) {
    sqlite3_stmt *del_stmt;
    if (sqlite3_prepare_v2(db, "DELETE FROM task_visibility WHERE task_id = ?", -1, &del_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(del_stmt, 1, task_id);
        sqlite3_step(del_stmt);
        sqlite3_finalize(del_stmt);
    }

    if (!csv || strlen(csv) == 0) return;

    char *copy = strdup(csv);
    char *token = strtok(copy, ",");
    sqlite3_stmt *stmt;
    const char *q = "INSERT OR IGNORE INTO task_visibility (task_id, student_id) VALUES (?, ?)";
    if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
        while (token) {
            int student_id = atoi(token);
            if (student_id > 0) {
                sqlite3_reset(stmt);
                sqlite3_clear_bindings(stmt);
                sqlite3_bind_int(stmt, 1, task_id);
                sqlite3_bind_int(stmt, 2, student_id);
                sqlite3_step(stmt);
            }
            token = strtok(NULL, ",");
        }
        sqlite3_finalize(stmt);
    }
    free(copy);
}

// Log activity to database
void log_activity(int user_id, const char *action, const char *resource_type, int resource_id, const char *details) {
    sqlite3_stmt *stmt;
    const char *query = "INSERT INTO activity_log (user_id, action, resource_type, resource_id, details, ip_address) VALUES (?, ?, ?, ?, ?, ?)";

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_message("ERROR: Failed to prepare activity_log insert: %s", sqlite3_errmsg(db));
        return;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, action, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, resource_type, -1, SQLITE_STATIC);
    if (resource_id > 0) {
        sqlite3_bind_int(stmt, 4, resource_id);
    } else {
        sqlite3_bind_null(stmt, 4);
    }
    sqlite3_bind_text(stmt, 5, details ? details : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, "127.0.0.1", -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        log_message("ERROR: Failed to log activity: %s", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}

// Store token in sessions table
int store_session(int user_id, const char *token) {
    sqlite3_stmt *stmt;
    const char *query = "INSERT INTO sessions (user_id, token, expires_at) VALUES (?, ?, datetime('now', '+24 hours'))";

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, token, -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

// Validate email format (simple check)
int is_valid_email(const char *email) {
    if (!email || strlen(email) < 5) return 0;

    // Check for @ and .
    const char *at = strchr(email, '@');
    if (!at) return 0;

    // Check for @ position
    if (at == email || *(at + 1) == '\0') return 0;

    // Check for dot after @
    const char *dot = strchr(at + 1, '.');
    if (!dot || dot == at + 1) return 0;

    // Check for valid characters (simple check)
    return 1;
}

// Check if username exists
int username_exists(const char *username) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT 1 FROM users WHERE username = ? LIMIT 1";

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    int exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

// Check if email exists
int email_exists(const char *email) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT 1 FROM users WHERE email = ? LIMIT 1";

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);

    int exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

// Handler per le richieste HTTP
static enum MHD_Result request_handler(void *cls,
                          struct MHD_Connection *connection,
                          const char *url,
                          const char *method,
                          const char *version,
                          const char *upload_data,
                          size_t *upload_data_size,
                          void **con_cls) {
    (void)cls;
    (void)version;

    log_message("%s %s", method, url);

    // OPTIONS per CORS
    if (strcmp(method, "OPTIONS") == 0) {
        const char *response = "{}";
        send_json_response(connection, response, MHD_HTTP_OK);
        return MHD_YES;
    }

    // Serve static files for non-API GET requests
    if (strcmp(method, "GET") == 0 && strncmp(url, "/api/", 5) != 0) {
        if (serve_static_file(connection, url) == MHD_YES) {
            return MHD_YES;
        }
    }

    // PUT /api/submissions/{id}/grade - Docente assegna voto e feedback
    if (strncmp(url, "/api/submissions/", 17) == 0 && strstr(url, "/grade") != NULL && strcmp(method, "PUT") == 0) {
        int sub_id = atoi(url + 17);
        
        // Gestione accumulo dati POST/PUT
        PostData *pdata = (PostData *)*con_cls;
        if (pdata == NULL) {
            pdata = (PostData *)malloc(sizeof(PostData));
            pdata->data = (char *)malloc(MAX_POST_SIZE);
            pdata->size = 0;
            pdata->capacity = MAX_POST_SIZE;
            *con_cls = (void *)pdata;
            return MHD_YES;
        }
        if (*upload_data_size > 0) {
            size_t to_copy = (*upload_data_size < (pdata->capacity - pdata->size)) 
                ? *upload_data_size 
                : (pdata->capacity - pdata->size - 1);
            memcpy(pdata->data + pdata->size, upload_data, to_copy);
            pdata->size += to_copy;
            pdata->data[pdata->size] = '\0';
            *upload_data_size = 0;
            return MHD_YES;
        }

        // Ora che abbiamo i dati, procediamo con l'autenticazione
        const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
        if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
            char *err = json_error_response("Authorization required");
            send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
            free(err); 
            free(pdata->data); free(pdata); *con_cls = NULL;
            return MHD_YES;
        }
        int teacher_id = 0;
        sqlite3_stmt *sstmt;
        if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(sstmt, 1, auth_header + 7, -1, SQLITE_STATIC);
            if (sqlite3_step(sstmt) == SQLITE_ROW) teacher_id = sqlite3_column_int(sstmt, 0);
            sqlite3_finalize(sstmt);
        }
        if (teacher_id == 0) {
            char *err = json_error_response("Invalid session");
            send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
            free(err); 
            free(pdata->data); free(pdata); *con_cls = NULL;
            return MHD_YES;
        }

        const char *grade_s = json_get_field(pdata->data, "grade");
        const char *feedback = json_get_field(pdata->data, "feedback");
        if (!grade_s) {
            char *err = json_error_response("Grade is required");
            send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
            free(err); 
            free(pdata->data); free(pdata); *con_cls = NULL;
            return MHD_YES;
        }
        double grade = atof(grade_s);

        sqlite3_stmt *vstmt;
        int is_teacher = 0;
        int max_points = 0;
        if (sqlite3_prepare_v2(db, "SELECT tasks.teacher_id, tasks.points FROM task_submissions ts JOIN tasks ON ts.task_id = tasks.id WHERE ts.id = ?", -1, &vstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(vstmt, 1, sub_id);
            if (sqlite3_step(vstmt) == SQLITE_ROW) {
                if (sqlite3_column_int(vstmt, 0) == teacher_id) {
                    is_teacher = 1;
                    max_points = sqlite3_column_int(vstmt, 1);
                }
            }
            sqlite3_finalize(vstmt);
        }
        if (!is_teacher) {
            char *err = json_error_response("You are not the teacher for this task");
            send_json_response(connection, err, MHD_HTTP_FORBIDDEN);
            free(err); 
            free(pdata->data); free(pdata); *con_cls = NULL;
            return MHD_YES;
        }

        if (grade < 0 || (max_points > 0 && grade > max_points)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Il voto deve essere compreso tra 0 e %d", max_points);
            char *err = json_error_response(msg);
            send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
            free(err); 
            free(pdata->data); free(pdata); *con_cls = NULL;
            return MHD_YES;
        }

        sqlite3_stmt *stmt;
        const char *q = "UPDATE task_submissions SET grade = ?, teacher_feedback = ?, status = 'graded' WHERE id = ?";
        if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_double(stmt, 1, grade);
            sqlite3_bind_text(stmt, 2, feedback ? feedback : "", -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, sub_id);
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                char *ok = json_success_response("Grade assigned successfully");
                send_json_response(connection, ok, MHD_HTTP_OK);
                free(ok);
            } else {
                char *err = json_error_response("Failed to update grade");
                send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                free(err);
            }
            sqlite3_finalize(stmt);
        } else {
            char *err = json_error_response("Database error");
            send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
            free(err);
        }
        free(pdata->data); free(pdata); *con_cls = NULL;
        return MHD_YES;
    }

    // Health check
    if (strcmp(url, "/api/health") == 0) {
        const char *response = "{\"status\": \"healthy\", \"message\": \"API is running\"}";
        send_json_response(connection, response, MHD_HTTP_OK);
        return MHD_YES;
    }

    // Registration endpoint
    if (strcmp(url, "/api/users/register") == 0 && strcmp(method, "POST") == 0) {
        PostData *pdata = (PostData *)*con_cls;

        // First call - initialize post data structure
        if (pdata == NULL) {
            pdata = (PostData *)malloc(sizeof(PostData));
            pdata->data = (char *)malloc(MAX_POST_SIZE);
            pdata->size = 0;
            pdata->capacity = MAX_POST_SIZE;
            *con_cls = (void *)pdata;
            return MHD_YES;
        }

        // Accumulate POST data
        if (*upload_data_size > 0) {
            size_t to_copy = (*upload_data_size < (pdata->capacity - pdata->size))
                ? *upload_data_size
                : (pdata->capacity - pdata->size - 1);

            memcpy(pdata->data + pdata->size, upload_data, to_copy);
            pdata->size += to_copy;
            pdata->data[pdata->size] = '\0';
            *upload_data_size = 0;
            return MHD_YES;
        }

        // Process POST data
        char *username = json_get_field(pdata->data, "username");
        char *email = json_get_field(pdata->data, "email");
        char *password = json_get_field(pdata->data, "password");
        char *full_name = json_get_field(pdata->data, "full_name");
        char *role = json_get_field(pdata->data, "role");

        char *response = NULL;
        int status = MHD_HTTP_BAD_REQUEST;

        // Validate inputs
        if (!username || strlen(username) < 3) {
            response = json_error_response("Username must be at least 3 characters");
        } else if (!email || !is_valid_email(email)) {
            response = json_error_response("Invalid email format");
        } else if (!password || strlen(password) < 6) {
            response = json_error_response("Password must be at least 6 characters");
        } else if (!full_name || strlen(full_name) < 2) {
            response = json_error_response("Full name is required");
        } else if (!role) {
            response = json_error_response("Role is required");
        } else if (username_exists(username)) {
            response = json_error_response("Username already exists");
            status = MHD_HTTP_CONFLICT;
        } else if (email_exists(email)) {
            response = json_error_response("Email already exists");
            status = MHD_HTTP_CONFLICT;
        } else if (db_register_user(db, username, email, password, full_name, role)) {
            // Get the newly created user ID
            sqlite3_stmt *stmt;
            const char *query = "SELECT id FROM users WHERE username = ?";
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    int user_id = sqlite3_column_int(stmt, 0);
                    log_activity(user_id, "register", "user", user_id, NULL);

                    // Build success response
                    JSONBuilder *jb = json_create();
                    json_start_object(jb);
                    json_add_string(jb, "status", "success");
                    json_add_string(jb, "message", "User registered successfully");
                    json_add_int(jb, "user_id", user_id);
                    json_add_string(jb, "username", username);
                    json_add_string(jb, "email", email);
                    json_add_string(jb, "role", role);
                    json_end_object(jb);
                    response = json_get_string(jb);
                    json_free(jb);
                    status = MHD_HTTP_CREATED;
                }
                sqlite3_finalize(stmt);
            }
        } else {
            response = json_error_response("Failed to register user");
        }

        if (!response) {
            response = json_error_response("Registration failed");
        }

        send_json_response(connection, response, status);

        // Cleanup
        if (username) free(username);
        if (email) free(email);
        if (password) free(password);
        if (full_name) free(full_name);
        if (role) free(role);
        free(response);
        free(pdata->data);
        free(pdata);
        *con_cls = NULL;

        return MHD_YES;
    }

    // Check user/email existence endpoint
    if (strcmp(url, "/api/users/check") == 0 && strcmp(method, "GET") == 0) {
        const char *username = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "username");
        const char *email = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "email");
        
        int exists = 0;
        char message[100] = {0};
        
        if (username && username_exists(username)) {
            exists = 1;
            strcpy(message, "Username already exists");
        } else if (email && email_exists(email)) {
            exists = 1;
            strcpy(message, "Email already exists");
        } else {
            strcpy(message, "Available");
        }

        JSONBuilder *jb = json_create();
        json_start_object(jb);
        json_add_int(jb, "exists", exists);
        json_add_string(jb, "message", message);
        json_end_object(jb);
        
        char *resp = json_get_string(jb);
        send_json_response(connection, resp, MHD_HTTP_OK);
        free(resp);
        json_free(jb);
        return MHD_YES;
    }

    // Login endpoint
    if (strcmp(url, "/api/users/login") == 0 && strcmp(method, "POST") == 0) {
        PostData *pdata = (PostData *)*con_cls;

        // First call - initialize post data structure
        if (pdata == NULL) {
            pdata = (PostData *)malloc(sizeof(PostData));
            pdata->data = (char *)malloc(MAX_POST_SIZE);
            pdata->size = 0;
            pdata->capacity = MAX_POST_SIZE;
            *con_cls = (void *)pdata;
            return MHD_YES;
        }

        // Accumulate POST data
        if (*upload_data_size > 0) {
            size_t to_copy = (*upload_data_size < (pdata->capacity - pdata->size)) ? *upload_data_size : (pdata->capacity - pdata->size - 1);
            memcpy(pdata->data + pdata->size, upload_data, to_copy);
            pdata->size += to_copy;
            pdata->data[pdata->size] = '\0';
            *upload_data_size = 0;
            return MHD_YES;
        }

        // Process POST data
        char *username = json_get_field(pdata->data, "username");
        char *password = json_get_field(pdata->data, "password");

        char *response = NULL;
        int status = MHD_HTTP_UNAUTHORIZED;

        // Validate inputs
        if (!username || !password) {
            response = json_error_response("Username and password are required");
            status = MHD_HTTP_BAD_REQUEST;
        } else {
            // Verify credentials
            AuthUser user;
            memset(&user, 0, sizeof(AuthUser));

            if (db_verify_login(db, username, password, &user)) {
                // Generate JWT token
                char *token = generate_jwt_token(&user);

                // Store session
                if (store_session(user.user_id, token)) {
                    log_activity(user.user_id, "login", "user", user.user_id, NULL);

                    // Build success response
                    JSONBuilder *jb = json_create();
                    json_start_object(jb);
                    json_add_string(jb, "status", "success");
                    json_add_string(jb, "message", "Login successful");
                    json_add_string(jb, "token", token);
                    json_add_int(jb, "user_id", user.user_id);
                    json_add_string(jb, "username", user.username);
                    json_add_string(jb, "email", user.email);
                    json_add_string(jb, "role", user.role);
                    json_end_object(jb);
                    response = json_get_string(jb);
                    json_free(jb);
                    status = MHD_HTTP_OK;
                } else {
                    response = json_error_response("Failed to create session");
                }

                free(token);
            } else {
                response = json_error_response("Invalid username or password");
            }
        }

        if (!response) {
            response = json_error_response("Login failed");
        }

        send_json_response(connection, response, status);

        // Cleanup
        if (username) free(username);
        if (password) free(password);
        free(response);
        free(pdata->data);
        free(pdata);
        *con_cls = NULL;

        return MHD_YES;
    }

    // Profile endpoint - GET /api/users/profile
    if (strcmp(url, "/api/users/profile") == 0 && strcmp(method, "GET") == 0) {
        const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");

        char *response = NULL;
        int status = MHD_HTTP_UNAUTHORIZED;

        if (!auth_header) {
            response = json_error_response("Authorization header missing");
        } else {
            // Extract Bearer token (Bearer <token>)
            if (strncmp(auth_header, "Bearer ", 7) == 0) {
                const char *token = auth_header + 7;

                // Find user by token in sessions table
                sqlite3_stmt *stmt;
                const char *query = "SELECT u.id, u.username, u.email, u.role "
                                    "FROM users u "
                                    "JOIN sessions s ON u.id = s.user_id "
                                    "WHERE s.token = ? AND s.expires_at > datetime('now')";

                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);

                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        // Build response directly from the JOIN result
                        JSONBuilder *jb = json_create();
                        json_start_object(jb);
                        json_add_string(jb, "status", "success");
                        json_add_int(jb, "user_id", sqlite3_column_int(stmt, 0));
                        json_add_string(jb, "username", (const char *)sqlite3_column_text(stmt, 1));
                        json_add_string(jb, "email", (const char *)sqlite3_column_text(stmt, 2));
                        json_add_string(jb, "role", (const char *)sqlite3_column_text(stmt, 3));
                        json_end_object(jb);
                        response = json_get_string(jb);
                        json_free(jb);
                        status = MHD_HTTP_OK;
                    } else {
                        response = json_error_response("Invalid or expired token");
                        status = MHD_HTTP_UNAUTHORIZED;
                    }

                    sqlite3_finalize(stmt);
                } else {
                    response = json_error_response("Database error");
                    status = MHD_HTTP_INTERNAL_SERVER_ERROR;
                }
            } else {
                response = json_error_response("Invalid authorization header format");
                status = MHD_HTTP_BAD_REQUEST;
            }
        }

        if (!response) {
            response = json_error_response("Profile retrieval failed");
        }

        send_json_response(connection, response, status);
        free(response);
        return MHD_YES;
    }

    // Courses endpoints
    if (strncmp(url, "/api/courses", 12) == 0) {
        // /api/courses
        if (strcmp(url, "/api/courses") == 0 && strcmp(method, "GET") == 0) {
            // List courses (simple implementation)
            sqlite3_stmt *stmt;
            const char *query = "SELECT id, title, description, teacher_id, category, difficulty_level, duration_hours, num_lessons FROM courses";
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                JSONBuilder *jb = json_create();
                json_start_object(jb);
                json_start_array(jb, "courses");

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    int id = sqlite3_column_int(stmt, 0);
                    const char *title = (const char *)sqlite3_column_text(stmt, 1);
                    const char *description = (const char *)sqlite3_column_text(stmt, 2);
                    int teacher_id = sqlite3_column_int(stmt, 3);
                    const char *category = (const char *)sqlite3_column_text(stmt, 4);
                    const char *difficulty = (const char *)sqlite3_column_text(stmt, 5);
                    double duration = sqlite3_column_double(stmt, 6);
                    int num_lessons = sqlite3_column_int(stmt, 7);

                    json_start_object(jb);
                    json_add_int(jb, "id", id);
                    json_add_string(jb, "title", title ? title : "");
                    json_add_string(jb, "description", description ? description : "");
                    json_add_int(jb, "teacher_id", teacher_id);
                    json_add_string(jb, "category", category ? category : "");
                    json_add_string(jb, "difficulty_level", difficulty ? difficulty : "");
                    json_add_double(jb, "duration_hours", duration);
                    json_add_int(jb, "num_lessons", num_lessons);
                    json_end_object(jb);
                }

                json_end_array(jb);
                json_end_object(jb);
                char *resp = json_get_string(jb);
                send_json_response(connection, resp, MHD_HTTP_OK);
                free(resp);
                json_free(jb);
                sqlite3_finalize(stmt);
                return MHD_YES;
            } else {
                char *err = json_error_response("Database error listing courses");
                send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                free(err);
                return MHD_YES;
            }
        }

        // /api/courses (create)
        if (strcmp(url, "/api/courses") == 0 && strcmp(method, "POST") == 0) {
            PostData *pdata = (PostData *)*con_cls;
            if (pdata == NULL) {
                pdata = (PostData *)malloc(sizeof(PostData));
                pdata->data = (char *)malloc(MAX_POST_SIZE);
                pdata->size = 0;
                pdata->capacity = MAX_POST_SIZE;
                *con_cls = (void *)pdata;
                return MHD_YES;
            }

            if (*upload_data_size > 0) {
                size_t to_copy = (*upload_data_size < (pdata->capacity - pdata->size)) ? *upload_data_size : (pdata->capacity - pdata->size - 1);
                memcpy(pdata->data + pdata->size, upload_data, to_copy);
                pdata->size += to_copy;
                pdata->data[pdata->size] = '\0';
                *upload_data_size = 0;
                return MHD_YES;
            }

            // Authorization: require Bearer token and teacher/admin role
            const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
            if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
                char *err = json_error_response("Authorization header missing or invalid");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err);
                free(pdata->data);
                free(pdata);
                *con_cls = NULL;
                return MHD_YES;
            }

            const char *token = auth_header + 7;
            sqlite3_stmt *sstmt;
            const char *squery = "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')";
            int author_id = 0;
            if (sqlite3_prepare_v2(db, squery, -1, &sstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(sstmt, 1, token, -1, SQLITE_STATIC);
                if (sqlite3_step(sstmt) == SQLITE_ROW) {
                    author_id = sqlite3_column_int(sstmt, 0);
                }
                sqlite3_finalize(sstmt);
            }

            if (author_id == 0) {
                char *err = json_error_response("Invalid or expired token");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err);
                free(pdata->data);
                free(pdata);
                *con_cls = NULL;
                return MHD_YES;
            }

            // Get user role
            char user_role[32] = "";
            sqlite3_stmt *rstmt;
            const char *rquery = "SELECT role FROM users WHERE id = ?";
            if (sqlite3_prepare_v2(db, rquery, -1, &rstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(rstmt, 1, author_id);
                if (sqlite3_step(rstmt) == SQLITE_ROW) {
                    const unsigned char *role_text = sqlite3_column_text(rstmt, 0);
                    if (role_text) strncpy(user_role, (const char *)role_text, sizeof(user_role)-1);
                }
                sqlite3_finalize(rstmt);
            }

            if (strcmp(user_role, "teacher") != 0 && strcmp(user_role, "admin") != 0) {
                char *err = json_error_response("Permission denied: only teachers or admins can create courses");
                send_json_response(connection, err, MHD_HTTP_FORBIDDEN);
                free(err);
                free(pdata->data);
                free(pdata);
                *con_cls = NULL;
                return MHD_YES;
            }

            // Extract fields
            char *title = json_get_field(pdata->data, "title");
            char *description = json_get_field(pdata->data, "description");
            char *teacher_id_s = json_get_field(pdata->data, "teacher_id");
            char *category = json_get_field(pdata->data, "category");
            char *difficulty = json_get_field(pdata->data, "difficulty_level");
            char *duration_s = json_get_field(pdata->data, "duration_hours");
            char *num_lessons_s = json_get_field(pdata->data, "num_lessons");

            if (!title || !description) {
                char *err = json_error_response("Missing required course fields: title or description");
                send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
                free(err);
            } else {
                int teacher_id = 0;
                if (strcmp(user_role, "teacher") == 0) {
                    // enforce teacher as the authenticated user
                    teacher_id = author_id;
                } else {
                    // admin may specify teacher_id, otherwise use admin's id
                    teacher_id = teacher_id_s ? atoi(teacher_id_s) : author_id;
                }

                double duration = duration_s ? atof(duration_s) : 0.0;
                int num_lessons = num_lessons_s ? atoi(num_lessons_s) : 0;

                sqlite3_stmt *stmt;
                const char *query = "INSERT INTO courses (title, description, teacher_id, category, difficulty_level, duration_hours, num_lessons) VALUES (?, ?, ?, ?, ?, ?, ?)";
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 2, description, -1, SQLITE_STATIC);
                    sqlite3_bind_int(stmt, 3, teacher_id);
                    sqlite3_bind_text(stmt, 4, category ? category : "", -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 5, difficulty ? difficulty : "", -1, SQLITE_STATIC);
                    sqlite3_bind_double(stmt, 6, duration);
                    sqlite3_bind_int(stmt, 7, num_lessons);

                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        int new_id = (int)sqlite3_last_insert_rowid(db);
                        log_activity(author_id, "create_course", "course", new_id, title);
                        JSONBuilder *jb = json_create();
                        json_start_object(jb);
                        json_add_string(jb, "status", "success");
                        json_add_int(jb, "course_id", new_id);
                        json_add_int(jb, "teacher_id", teacher_id);
                        json_end_object(jb);
                        char *resp = json_get_string(jb);
                        send_json_response(connection, resp, MHD_HTTP_CREATED);
                        free(resp);
                        json_free(jb);
                    } else {
                        char *err = json_error_response("Failed to create course");
                        send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                        free(err);
                    }

                    sqlite3_finalize(stmt);
                } else {
                    char *err = json_error_response("Database error creating course");
                    send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                    free(err);
                }
            }

            if (title) free(title);
            if (description) free(description);
            if (teacher_id_s) free(teacher_id_s);
            if (category) free(category);
            if (difficulty) free(difficulty);
            if (duration_s) free(duration_s);
            if (num_lessons_s) free(num_lessons_s);

            free(pdata->data);
            free(pdata);
            *con_cls = NULL;
            return MHD_YES;
        }

        // /api/courses/{id}
        if (strncmp(url, "/api/courses/", 13) == 0) {
            int course_id = atoi(url + 13);

            // GET course buddies
            if (strcmp(method, "GET") == 0 && strstr(url, "/buddies") != NULL) {
                sqlite3_stmt *stmt;
                const char *query = "SELECT u.id, u.username, u.full_name, e.progress_percentage "
                                    "FROM enrollments e "
                                    "JOIN users u ON e.student_id = u.id "
                                    "WHERE e.course_id = ?";
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, course_id);
                    JSONBuilder *jb = json_create();
                    json_start_object(jb);
                    json_start_array(jb, "buddies");
                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                        json_start_object(jb);
                        json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                        json_add_string(jb, "username", (const char *)sqlite3_column_text(stmt, 1));
                        json_add_string(jb, "full_name", (const char *)sqlite3_column_text(stmt, 2));
                        json_add_double(jb, "progress_percentage", sqlite3_column_double(stmt, 3));
                        json_end_object(jb);
                    }
                    json_end_array(jb);
                    json_end_object(jb);
                    char *resp = json_get_string(jb);
                    send_json_response(connection, resp, MHD_HTTP_OK);
                    free(resp);
                    json_free(jb);
                    sqlite3_finalize(stmt);
                    return MHD_YES;
                }
            }

            // GET enrolled students for a course (teacher only)
            if (strcmp(method, "GET") == 0 && strstr(url, "/students") != NULL) {
                int user_id = get_auth_user_id(connection);
                if (user_id == 0) {
                    char *err = json_error_response("Authorization required");
                    send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                    free(err);
                    return MHD_YES;
                }
                if (!teacher_owns_course(user_id, course_id)) {
                    char *err = json_error_response("You are not the teacher for this course");
                    send_json_response(connection, err, MHD_HTTP_FORBIDDEN);
                    free(err);
                    return MHD_YES;
                }

                sqlite3_stmt *stmt;
                const char *query = "SELECT u.id, u.username, u.full_name, u.email, e.status "
                                    "FROM enrollments e "
                                    "JOIN users u ON e.student_id = u.id "
                                    "WHERE e.course_id = ? AND e.status = 'enrolled' "
                                    "ORDER BY u.full_name, u.username";
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, course_id);
                    JSONBuilder *jb = json_create();
                    json_start_object(jb);
                    json_start_array(jb, "students");
                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                        json_start_object(jb);
                        json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                        json_add_string(jb, "username", (const char *)sqlite3_column_text(stmt, 1));
                        json_add_string(jb, "full_name", (const char *)sqlite3_column_text(stmt, 2));
                        json_add_string(jb, "email", (const char *)sqlite3_column_text(stmt, 3));
                        json_add_string(jb, "status", (const char *)sqlite3_column_text(stmt, 4));
                        json_end_object(jb);
                    }
                    json_end_array(jb);
                    json_end_object(jb);
                    char *resp = json_get_string(jb);
                    send_json_response(connection, resp, MHD_HTTP_OK);
                    free(resp);
                    json_free(jb);
                    sqlite3_finalize(stmt);
                    return MHD_YES;
                }
            }

            // GET specific course
            if (strcmp(method, "GET") == 0 && strstr(url, "/buddies") == NULL && strstr(url, "/students") == NULL) {
                sqlite3_stmt *stmt;
                const char *query = "SELECT id, title, description, teacher_id, category, difficulty_level, duration_hours, num_lessons FROM courses WHERE id = ?";
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, course_id);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        JSONBuilder *jb = json_create();
                        json_start_object(jb);
                        json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                        json_add_string(jb, "title", (const char *)sqlite3_column_text(stmt, 1));
                        json_add_string(jb, "description", (const char *)sqlite3_column_text(stmt, 2));
                        json_add_int(jb, "teacher_id", sqlite3_column_int(stmt, 3));
                        json_add_string(jb, "category", (const char *)sqlite3_column_text(stmt, 4));
                        json_add_string(jb, "difficulty_level", (const char *)sqlite3_column_text(stmt, 5));
                        json_add_double(jb, "duration_hours", sqlite3_column_double(stmt, 6));
                        json_add_int(jb, "num_lessons", sqlite3_column_int(stmt, 7));
                        json_end_object(jb);
                        char *resp = json_get_string(jb);
                        send_json_response(connection, resp, MHD_HTTP_OK);
                        free(resp);
                        json_free(jb);
                    } else {
                        char *err = json_error_response("Course not found");
                        send_json_response(connection, err, MHD_HTTP_NOT_FOUND);
                        free(err);
                    }
                    sqlite3_finalize(stmt);
                    return MHD_YES;
                }
            }

            // PUT update course
            if (strcmp(method, "PUT") == 0) {
                PostData *pdata = (PostData *)*con_cls;
                if (pdata == NULL) {
                    pdata = (PostData *)malloc(sizeof(PostData));
                    pdata->data = (char *)malloc(MAX_POST_SIZE);
                    pdata->size = 0;
                    pdata->capacity = MAX_POST_SIZE;
                    *con_cls = (void *)pdata;
                    return MHD_YES;
                }

                if (*upload_data_size > 0) {
                    size_t to_copy = (*upload_data_size < (pdata->capacity - pdata->size)) ? *upload_data_size : (pdata->capacity - pdata->size - 1);
                    memcpy(pdata->data + pdata->size, upload_data, to_copy);
                    pdata->size += to_copy;
                    pdata->data[pdata->size] = '\0';
                    *upload_data_size = 0;
                    return MHD_YES;
                }

                char *title = json_get_field(pdata->data, "title");
                char *description = json_get_field(pdata->data, "description");
                char *category = json_get_field(pdata->data, "category");
                char *difficulty = json_get_field(pdata->data, "difficulty_level");
                char *duration_s = json_get_field(pdata->data, "duration_hours");
                char *num_lessons_s = json_get_field(pdata->data, "num_lessons");

                sqlite3_stmt *stmt;
                const char *query = "UPDATE courses SET title = COALESCE(?, title), description = COALESCE(?, description), category = COALESCE(?, category), difficulty_level = COALESCE(?, difficulty_level), duration_hours = COALESCE(?, duration_hours), num_lessons = COALESCE(?, num_lessons), updated_at = CURRENT_TIMESTAMP WHERE id = ?";
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, title ? title : NULL, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 2, description ? description : NULL, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 3, category ? category : NULL, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 4, difficulty ? difficulty : NULL, -1, SQLITE_STATIC);
                    if (duration_s) sqlite3_bind_double(stmt, 5, atof(duration_s)); else sqlite3_bind_null(stmt, 5);
                    if (num_lessons_s) sqlite3_bind_int(stmt, 6, atoi(num_lessons_s)); else sqlite3_bind_null(stmt, 6);
                    sqlite3_bind_int(stmt, 7, course_id);

                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        char *ok = json_success_response("Course updated");
                        send_json_response(connection, ok, MHD_HTTP_OK);
                        free(ok);
                    } else {
                        char *err = json_error_response("Failed to update course");
                        send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                        free(err);
                    }
                    sqlite3_finalize(stmt);
                }

                if (title) free(title);
                if (description) free(description);
                if (category) free(category);
                if (difficulty) free(difficulty);
                if (duration_s) free(duration_s);
                if (num_lessons_s) free(num_lessons_s);

                free(pdata->data);
                free(pdata);
                *con_cls = NULL;
                return MHD_YES;
            }

            // DELETE course
            if (strcmp(method, "DELETE") == 0) {
                sqlite3_stmt *stmt;
                const char *query = "DELETE FROM courses WHERE id = ?";
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, course_id);
                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        char *ok = json_success_response("Course deleted");
                        send_json_response(connection, ok, MHD_HTTP_OK);
                        free(ok);
                    } else {
                        char *err = json_error_response("Failed to delete course");
                        send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                        free(err);
                    }
                    sqlite3_finalize(stmt);
                }
                return MHD_YES;
            }
        }
    }

    // Enrollments endpoints
    if (strncmp(url, "/api/enrollments", 16) == 0) {
        const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
        if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
            char *err = json_error_response("Authorization required");
            send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
            free(err);
            return MHD_YES;
        }

        const char *token = auth_header + 7;
        int user_id = 0;
        char user_role[20] = {0};
        sqlite3_stmt *sstmt;

        // Verify token
        if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(sstmt, 1, token, -1, SQLITE_STATIC);
            if (sqlite3_step(sstmt) == SQLITE_ROW) {
                user_id = sqlite3_column_int(sstmt, 0);
            }
            sqlite3_finalize(sstmt);
        }

        if (user_id == 0) {
            char *err = json_error_response("Invalid or expired token");
            send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
            free(err);
            return MHD_YES;
        }

        // Get user role
        if (sqlite3_prepare_v2(db, "SELECT role FROM users WHERE id = ?", -1, &sstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(sstmt, 1, user_id);
            if (sqlite3_step(sstmt) == SQLITE_ROW) {
                strcpy(user_role, (const char *)sqlite3_column_text(sstmt, 0));
            }
            sqlite3_finalize(sstmt);
        }

        // GET /api/enrollments (List user enrollments)
        if (strcmp(url, "/api/enrollments") == 0 && strcmp(method, "GET") == 0) {
            sqlite3_stmt *stmt;
            const char *query = "SELECT e.id, e.course_id, e.status, e.progress_percentage, c.title as course_title "
                                "FROM enrollments e "
                                "JOIN courses c ON e.course_id = c.id "
                                "WHERE e.student_id = ?";
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, user_id);

                JSONBuilder *jb = json_create();
                json_start_object(jb);
                json_start_array(jb, "enrollments");

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    json_start_object(jb);
                    json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                    json_add_int(jb, "course_id", sqlite3_column_int(stmt, 1));
                    json_add_string(jb, "status", (const char *)sqlite3_column_text(stmt, 2));
                    json_add_double(jb, "progress_percentage", sqlite3_column_double(stmt, 3));
                    json_add_string(jb, "course_title", (const char *)sqlite3_column_text(stmt, 4));
                    json_end_object(jb);
                    json_append(jb, ",");
                }

                json_end_array(jb);
                json_end_object(jb);

                char *resp = json_get_string(jb);
                send_json_response(connection, resp, MHD_HTTP_OK);
                free(resp);
                json_free(jb);
                sqlite3_finalize(stmt);
                return MHD_YES;
            } else {
                char *err = json_error_response("Database error fetching enrollments");
                send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                free(err);
                return MHD_YES;
            }
        }

        // POST /api/enrollments (Enroll in a course)
        if (strcmp(url, "/api/enrollments") == 0 && strcmp(method, "POST") == 0) {
            PostData *pdata = (PostData *)*con_cls;
            
            if (pdata == NULL) {
                pdata = (PostData *)malloc(sizeof(PostData));
                pdata->data = (char *)malloc(MAX_POST_SIZE);
                pdata->size = 0;
                pdata->capacity = MAX_POST_SIZE;
                *con_cls = (void *)pdata;
                return MHD_YES;
            }

            if (*upload_data_size != 0) {
                if (pdata->size + *upload_data_size < pdata->capacity) {
                    memcpy(pdata->data + pdata->size, upload_data, *upload_data_size);
                    pdata->size += *upload_data_size;
                    *upload_data_size = 0;
                    return MHD_YES;
                }
                return MHD_NO;
            }

            pdata->data[pdata->size] = '\0';
            
            // Extract course_id
            char *course_id_s = json_get_field(pdata->data, "course_id");
            if (!course_id_s) {
                char *err = json_error_response("Missing required field: course_id");
                send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
                free(err);
                free(pdata->data);
                free(pdata);
                *con_cls = NULL;
                return MHD_YES;
            }

            int course_id = atoi(course_id_s);
            free(course_id_s);

            // Check if already enrolled
            int already_enrolled = 0;
            sqlite3_stmt *chk_stmt;
            if (sqlite3_prepare_v2(db, "SELECT id FROM enrollments WHERE student_id = ? AND course_id = ?", -1, &chk_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(chk_stmt, 1, user_id);
                sqlite3_bind_int(chk_stmt, 2, course_id);
                if (sqlite3_step(chk_stmt) == SQLITE_ROW) {
                    already_enrolled = 1;
                }
                sqlite3_finalize(chk_stmt);
            }

            if (already_enrolled) {
                char *err = json_error_response("Already enrolled in this course");
                send_json_response(connection, err, MHD_HTTP_CONFLICT);
                free(pdata->data);
                free(pdata);
                *con_cls = NULL;
                return MHD_YES;
            }

            // Perform enrollment
            sqlite3_stmt *stmt;
            const char *query = "INSERT INTO enrollments (student_id, course_id, status, progress_percentage) VALUES (?, ?, 'enrolled', 0.0)";
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, user_id);
                sqlite3_bind_int(stmt, 2, course_id);

                if (sqlite3_step(stmt) == SQLITE_DONE) {
                    char *ok = json_success_response("Successfully enrolled");
                    send_json_response(connection, ok, MHD_HTTP_CREATED);
                    free(ok);
                } else {
                    char *err = json_error_response("Failed to enroll");
                    send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                    free(err);
                }
                sqlite3_finalize(stmt);
            }

            free(pdata->data);
            free(pdata);
            *con_cls = NULL;
            return MHD_YES;
        }
    }

    // Paths endpoints
    if (strncmp(url, "/api/paths", 10) == 0) {
        // /api/paths
        if (strcmp(url, "/api/paths") == 0 && strcmp(method, "GET") == 0) {
            sqlite3_stmt *stmt;
            const char *query = "SELECT id, title, description, teacher_id FROM paths";
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                JSONBuilder *jb = json_create();
                json_start_object(jb);
                json_start_array(jb, "paths");
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    json_start_object(jb);
                    json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                    json_add_string(jb, "title", (const char *)sqlite3_column_text(stmt, 1));
                    json_add_string(jb, "description", (const char *)sqlite3_column_text(stmt, 2));
                    json_add_int(jb, "teacher_id", sqlite3_column_int(stmt, 3));
                    json_end_object(jb);
                }
                json_end_array(jb);
                json_end_object(jb);
                char *resp = json_get_string(jb);
                send_json_response(connection, resp, MHD_HTTP_OK);
                free(resp);
                json_free(jb);
                sqlite3_finalize(stmt);
                return MHD_YES;
            } else {
                char *err = json_error_response("Database error listing paths");
                send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                free(err);
                return MHD_YES;
            }
        }

        if (strncmp(url, "/api/paths/", 11) == 0 && strcmp(method, "GET") == 0) {
            int path_id = atoi(url + 11);
            sqlite3_stmt *stmt;
            const char *query = "SELECT id, title, description, teacher_id FROM paths WHERE id = ?";
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, path_id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    JSONBuilder *jb = json_create();
                    json_start_object(jb);
                    json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                    json_add_string(jb, "title", (const char *)sqlite3_column_text(stmt, 1));
                    json_add_string(jb, "description", (const char *)sqlite3_column_text(stmt, 2));
                    json_add_int(jb, "teacher_id", sqlite3_column_int(stmt, 3));
                    json_end_object(jb);
                    char *resp = json_get_string(jb);
                    send_json_response(connection, resp, MHD_HTTP_OK);
                    free(resp);
                    json_free(jb);
                } else {
                    char *err = json_error_response("Path not found");
                    send_json_response(connection, err, MHD_HTTP_NOT_FOUND);
                    free(err);
                }
                sqlite3_finalize(stmt);
                return MHD_YES;
            }
        }
    }

    // Tasks and Quizzes endpoints
    if (strncmp(url, "/api/tasks", 9) == 0) {
        // POST /api/tasks - Create a task or quiz
        if (strcmp(url, "/api/tasks") == 0 && strcmp(method, "POST") == 0) {
            PostData *pdata = (PostData *)*con_cls;
            if (pdata == NULL) {
                pdata = (PostData *)malloc(sizeof(PostData));
                pdata->data = (char *)malloc(MAX_POST_SIZE);
                pdata->size = 0;
                pdata->capacity = MAX_POST_SIZE;
                *con_cls = (void *)pdata;
                return MHD_YES;
            }
            if (*upload_data_size > 0) {
                size_t to_copy = (*upload_data_size < (pdata->capacity - pdata->size)) ? *upload_data_size : (pdata->capacity - pdata->size - 1);
                memcpy(pdata->data + pdata->size, upload_data, to_copy);
                pdata->size += to_copy;
                pdata->data[pdata->size] = '\0';
                *upload_data_size = 0;
                return MHD_YES;
            }

            const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
            if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
                char *err = json_error_response("Authorization required");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err);
                free(pdata->data); free(pdata); *con_cls = NULL;
                return MHD_YES;
            }

            int author_id = 0;
            sqlite3_stmt *sstmt;
            if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(sstmt, 1, auth_header + 7, -1, SQLITE_STATIC);
                if (sqlite3_step(sstmt) == SQLITE_ROW) author_id = sqlite3_column_int(sstmt, 0);
                sqlite3_finalize(sstmt);
            }

            if (author_id == 0) {
                char *err = json_error_response("Invalid session");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err);
                free(pdata->data); free(pdata); *con_cls = NULL;
                return MHD_YES;
            }

            char role[32] = "";
            sqlite3_stmt *rstmt;
            if (sqlite3_prepare_v2(db, "SELECT role FROM users WHERE id = ?", -1, &rstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(rstmt, 1, author_id);
                if (sqlite3_step(rstmt) == SQLITE_ROW) strncpy(role, (const char *)sqlite3_column_text(rstmt, 0), 31);
                sqlite3_finalize(rstmt);
            }

            if (strcmp(role, "teacher") != 0 && strcmp(role, "admin") != 0) {
                char *err = json_error_response("Only teachers can create tasks");
                send_json_response(connection, err, MHD_HTTP_FORBIDDEN);
                free(err);
                free(pdata->data); free(pdata); *con_cls = NULL;
                return MHD_YES;
            }

            char *title = json_get_field(pdata->data, "title");
            char *description = json_get_field(pdata->data, "description");
            char *course_id_s = json_get_field(pdata->data, "course_id");
            char *type = json_get_field(pdata->data, "task_type");
            char *due_date = json_get_field(pdata->data, "due_date");
            char *points_s = json_get_field(pdata->data, "points");
            char *visible_to_s = json_get_field(pdata->data, "selected_student_ids");

            if (!title || !course_id_s || !type) {
                char *err = json_error_response("Missing required fields");
                send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
                free(err);
            } else {
                int course_id = atoi(course_id_s);
                int points = points_s ? atoi(points_s) : 0;
                if (!teacher_owns_course(author_id, course_id)) {
                    char *err = json_error_response("You are not the teacher for this course");
                    send_json_response(connection, err, MHD_HTTP_FORBIDDEN);
                    free(err);
                    if (title) free(title);
                    if (description) free(description);
                    if (course_id_s) free(course_id_s);
                    if (type) free(type);
                    if (due_date) free(due_date);
                    if (points_s) free(points_s);
                    if (visible_to_s) free(visible_to_s);
                    free(pdata->data); free(pdata); *con_cls = NULL;
                    return MHD_YES;
                }
                sqlite3_stmt *stmt;
                const char *query = "INSERT INTO tasks (course_id, teacher_id, title, description, due_date, points, task_type) VALUES (?, ?, ?, ?, ?, ?, ?)";
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, course_id);
                    sqlite3_bind_int(stmt, 2, author_id);
                    sqlite3_bind_text(stmt, 3, title, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 4, description ? description : "", -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 5, due_date ? due_date : "", -1, SQLITE_STATIC);
                    sqlite3_bind_int(stmt, 6, points);
                    sqlite3_bind_text(stmt, 7, type, -1, SQLITE_STATIC);

                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        int task_id = (int)sqlite3_last_insert_rowid(db);
                        save_task_visibility_from_csv(task_id, visible_to_s);
                        
                        // Notify students
                        char course_title[128] = "Corso";
                        sqlite3_stmt *ctitle_stmt;
                        if (sqlite3_prepare_v2(db, "SELECT title FROM courses WHERE id = ?", -1, &ctitle_stmt, NULL) == SQLITE_OK) {
                            sqlite3_bind_int(ctitle_stmt, 1, course_id);
                            if (sqlite3_step(ctitle_stmt) == SQLITE_ROW) {
                                strncpy(course_title, (const char *)sqlite3_column_text(ctitle_stmt, 0), sizeof(course_title) - 1);
                            }
                            sqlite3_finalize(ctitle_stmt);
                        }

                        sqlite3_stmt *notif_stmt;
                        const char *notif_query = (visible_to_s && strlen(visible_to_s) > 0)
                            ? "INSERT INTO notifications (user_id, type, message, reference_id) SELECT student_id, 'task_assigned', ?, ? FROM task_visibility WHERE task_id = ?"
                            : "INSERT INTO notifications (user_id, type, message, reference_id) SELECT student_id, 'task_assigned', ?, ? FROM enrollments WHERE course_id = ?";
                        if (sqlite3_prepare_v2(db, notif_query, -1, &notif_stmt, NULL) == SQLITE_OK) {
                            char msg_buf[256];
                            snprintf(msg_buf, sizeof(msg_buf), "Nuovo task '%s' assegnato nel corso '%s'", title, course_title);
                            sqlite3_bind_text(notif_stmt, 1, msg_buf, -1, SQLITE_STATIC);
                            sqlite3_bind_int(notif_stmt, 2, task_id);
                            sqlite3_bind_int(notif_stmt, 3, (visible_to_s && strlen(visible_to_s) > 0) ? task_id : course_id);
                            sqlite3_step(notif_stmt);
                            sqlite3_finalize(notif_stmt);
                        }

                        JSONBuilder *jb = json_create();
                        json_start_object(jb);
                        json_add_string(jb, "status", "success");
                        json_add_int(jb, "task_id", task_id);
                        json_end_object(jb);
                        char *resp = json_get_string(jb);
                        send_json_response(connection, resp, MHD_HTTP_CREATED);
                        free(resp);
                        json_free(jb);
                    } else {
                        char *err = json_error_response("Failed to create task");
                        send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                        free(err);
                    }
                    sqlite3_finalize(stmt);
                }
            }
            if (title) free(title);
            if (description) free(description);
            if (course_id_s) free(course_id_s);
            if (type) free(type);
            if (due_date) free(due_date);
            if (points_s) free(points_s);
            if (visible_to_s) free(visible_to_s);
            free(pdata->data); free(pdata); *con_cls = NULL;
            return MHD_YES;
        }

        // PUT /api/tasks/{id} - Docente modifica task
        if (strncmp(url, "/api/tasks/", 11) == 0 && strstr(url + 11, "/") == NULL && strcmp(method, "PUT") == 0) {
            int task_id = atoi(url + 11);
            PostData *pdata = (PostData *)*con_cls;
            if (pdata == NULL) {
                pdata = (PostData *)malloc(sizeof(PostData));
                pdata->data = (char *)malloc(MAX_POST_SIZE);
                pdata->size = 0;
                pdata->capacity = MAX_POST_SIZE;
                *con_cls = (void *)pdata;
                return MHD_YES;
            }
            if (*upload_data_size > 0) {
                size_t to_copy = (*upload_data_size < (pdata->capacity - pdata->size)) ? *upload_data_size : (pdata->capacity - pdata->size - 1);
                memcpy(pdata->data + pdata->size, upload_data, to_copy);
                pdata->size += to_copy;
                pdata->data[pdata->size] = '\0';
                *upload_data_size = 0;
                return MHD_YES;
            }

            int user_id = get_auth_user_id(connection);
            if (user_id == 0) {
                char *err = json_error_response("Authorization required");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err);
                free(pdata->data); free(pdata); *con_cls = NULL;
                return MHD_YES;
            }
            if (!teacher_owns_task(user_id, task_id)) {
                char *err = json_error_response("You are not the teacher for this task");
                send_json_response(connection, err, MHD_HTTP_FORBIDDEN);
                free(err);
                free(pdata->data); free(pdata); *con_cls = NULL;
                return MHD_YES;
            }

            char *title = json_get_field(pdata->data, "title");
            char *description = json_get_field(pdata->data, "description");
            char *type = json_get_field(pdata->data, "task_type");
            char *due_date = json_get_field(pdata->data, "due_date");
            char *points_s = json_get_field(pdata->data, "points");
            char *visible_to_s = json_get_field(pdata->data, "selected_student_ids");

            if (!title || !type) {
                char *err = json_error_response("Missing required fields");
                send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
                free(err);
            } else {
                int points = points_s ? atoi(points_s) : 0;
                sqlite3_stmt *stmt;
                const char *q = "UPDATE tasks SET title = ?, description = ?, due_date = ?, points = ?, task_type = ? WHERE id = ?";
                if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 2, description ? description : "", -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 3, due_date ? due_date : "", -1, SQLITE_STATIC);
                    sqlite3_bind_int(stmt, 4, points);
                    sqlite3_bind_text(stmt, 5, type, -1, SQLITE_STATIC);
                    sqlite3_bind_int(stmt, 6, task_id);
                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        save_task_visibility_from_csv(task_id, visible_to_s);
                        char *ok = json_success_response("Task updated successfully");
                        send_json_response(connection, ok, MHD_HTTP_OK);
                        free(ok);
                    } else {
                        char *err = json_error_response("Failed to update task");
                        send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                        free(err);
                    }
                    sqlite3_finalize(stmt);
                }
            }

            if (title) free(title);
            if (description) free(description);
            if (type) free(type);
            if (due_date) free(due_date);
            if (points_s) free(points_s);
            if (visible_to_s) free(visible_to_s);
            free(pdata->data); free(pdata); *con_cls = NULL;
            return MHD_YES;
        }

        // DELETE /api/tasks/{id} - Docente cancella task
        if (strncmp(url, "/api/tasks/", 11) == 0 && strstr(url + 11, "/") == NULL && strcmp(method, "DELETE") == 0) {
            int task_id = atoi(url + 11);
            int user_id = get_auth_user_id(connection);
            if (user_id == 0) {
                char *err = json_error_response("Authorization required");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err);
                return MHD_YES;
            }
            if (!teacher_owns_task(user_id, task_id)) {
                char *err = json_error_response("You are not the teacher for this task");
                send_json_response(connection, err, MHD_HTTP_FORBIDDEN);
                free(err);
                return MHD_YES;
            }

            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db, "DELETE FROM tasks WHERE id = ?", -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, task_id);
                if (sqlite3_step(stmt) == SQLITE_DONE) {
                    char *ok = json_success_response("Task deleted successfully");
                    send_json_response(connection, ok, MHD_HTTP_OK);
                    free(ok);
                } else {
                    char *err = json_error_response("Failed to delete task");
                    send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                    free(err);
                }
                sqlite3_finalize(stmt);
            }
            return MHD_YES;
        }

        // GET /api/tasks/{id}/submissions - Docente vede tutte le consegne per un task
        if (strncmp(url, "/api/tasks/", 11) == 0 && strstr(url, "/submissions") != NULL && strcmp(method, "GET") == 0) {
            int task_id = atoi(url + 11);
            const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
            if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
                char *err = json_error_response("Authorization required");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err); return MHD_YES;
            }
            sqlite3_stmt *stmt;
            const char *q = "SELECT ts.id, ts.student_id, ts.content, ts.submission_date, ts.status, "
                            "u.full_name, u.username "
                            "FROM task_submissions ts "
                            "JOIN users u ON ts.student_id = u.id "
                            "WHERE ts.task_id = ? ORDER BY ts.submission_date DESC";
            if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, task_id);
                JSONBuilder *jb = json_create();
                json_start_object(jb);
                json_start_array(jb, "submissions");
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    json_start_object(jb);
                    json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                    json_add_int(jb, "student_id", sqlite3_column_int(stmt, 1));
                    const char *content = (const char *)sqlite3_column_text(stmt, 2);
                    const char *sub_date = (const char *)sqlite3_column_text(stmt, 3);
                    const char *status = (const char *)sqlite3_column_text(stmt, 4);
                    const char *full_name = (const char *)sqlite3_column_text(stmt, 5);
                    const char *username = (const char *)sqlite3_column_text(stmt, 6);
                    json_add_string(jb, "content", content ? content : "");
                    json_add_string(jb, "submission_date", sub_date ? sub_date : "");
                    json_add_string(jb, "status", status ? status : "submitted");
                    json_add_string(jb, "student_name", full_name ? full_name : "");
                    json_add_string(jb, "username", username ? username : "");
                    json_end_object(jb);
                }
                json_end_array(jb);
                json_end_object(jb);
                char *resp = json_get_string(jb);
                send_json_response(connection, resp, MHD_HTTP_OK);
                free(resp); json_free(jb);
                sqlite3_finalize(stmt);
            }
            return MHD_YES;
        }

        // GET /api/tasks/{id}/submission - controlla se lo studente ha già consegnato
        if (strncmp(url, "/api/tasks/", 11) == 0 && strstr(url, "/submission") != NULL && strcmp(method, "GET") == 0) {
            int task_id = atoi(url + 11);
            const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
            if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
                char *err = json_error_response("Authorization required");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err); return MHD_YES;
            }
            int student_id = 0;
            sqlite3_stmt *sstmt;
            if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(sstmt, 1, auth_header + 7, -1, SQLITE_STATIC);
                if (sqlite3_step(sstmt) == SQLITE_ROW) student_id = sqlite3_column_int(sstmt, 0);
                sqlite3_finalize(sstmt);
            }
            if (student_id == 0) {
                char *err = json_error_response("Invalid session");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err); return MHD_YES;
            }
            sqlite3_stmt *stmt;
            const char *q = "SELECT ts.id, ts.content, ts.submission_date, ts.status, ts.grade, ts.teacher_feedback, t.points "
                            "FROM task_submissions ts "
                            "JOIN tasks t ON ts.task_id = t.id "
                            "WHERE ts.task_id = ? AND ts.student_id = ? ORDER BY ts.submission_date DESC LIMIT 1";
            if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, task_id);
                sqlite3_bind_int(stmt, 2, student_id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    JSONBuilder *jb = json_create();
                    json_start_object(jb);
                    json_add_string(jb, "submitted", "true");
                    json_add_int(jb, "submission_id", sqlite3_column_int(stmt, 0));
                    const char *c = (const char *)sqlite3_column_text(stmt, 1);
                    const char *d = (const char *)sqlite3_column_text(stmt, 2);
                    const char *s = (const char *)sqlite3_column_text(stmt, 3);
                    json_add_double(jb, "grade", sqlite3_column_double(stmt, 4));
                    const char *f = (const char *)sqlite3_column_text(stmt, 5);
                    json_add_string(jb, "teacher_feedback", f ? f : "");
                    json_add_int(jb, "max_points", sqlite3_column_int(stmt, 6));
                    json_add_string(jb, "content", c ? c : "");
                    json_add_string(jb, "submission_date", d ? d : "");
                    json_add_string(jb, "status", s ? s : "submitted");
                    json_end_object(jb);
                    char *resp = json_get_string(jb);
                    send_json_response(connection, resp, MHD_HTTP_OK);
                    free(resp); json_free(jb);
                } else {
                    JSONBuilder *jb = json_create();
                    json_start_object(jb);
                    json_add_string(jb, "submitted", "false");
                    json_end_object(jb);
                    char *resp = json_get_string(jb);
                    send_json_response(connection, resp, MHD_HTTP_OK);
                    free(resp); json_free(jb);
                }
                sqlite3_finalize(stmt);
            }
            return MHD_YES;
        }
    
        // PUT /api/submissions/{id}/grade - Docente assegna voto e feedback
        if (strncmp(url, "/api/submissions/", 17) == 0 && strstr(url, "/grade") != NULL && strcmp(method, "PUT") == 0) {
            int sub_id = atoi(url + 17);
            const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
            if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
                char *err = json_error_response("Authorization required");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err); return MHD_YES;
            }
            int teacher_id = 0;
            sqlite3_stmt *sstmt;
            if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(sstmt, 1, auth_header + 7, -1, SQLITE_STATIC);
                if (sqlite3_step(sstmt) == SQLITE_ROW) teacher_id = sqlite3_column_int(sstmt, 0);
                sqlite3_finalize(sstmt);
            }
            if (teacher_id == 0) {
                char *err = json_error_response("Invalid session");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err); return MHD_YES;
            }
            // ... rest of the function

        PostData *pdata = (PostData *)*con_cls;
        if (pdata == NULL || pdata->data == NULL) {
            char *err = json_error_response("Missing grade data");
            send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
            free(err); return MHD_YES;
        }

        const char *grade_s = json_get_field(pdata->data, "grade");
        const char *feedback = json_get_field(pdata->data, "feedback");
        if (!grade_s) {
            char *err = json_error_response("Grade is required");
            send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
            free(err); return MHD_YES;
        }
        double grade = atof(grade_s);

        sqlite3_stmt *vstmt;
        int is_teacher = 0;
        int max_points = 0;
        if (sqlite3_prepare_v2(db, "SELECT tasks.teacher_id, tasks.points FROM task_submissions ts JOIN tasks ON ts.task_id = tasks.id WHERE ts.id = ?", -1, &vstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(vstmt, 1, sub_id);
            if (sqlite3_step(vstmt) == SQLITE_ROW) {
                if (sqlite3_column_int(vstmt, 0) == teacher_id) {
                    is_teacher = 1;
                    max_points = sqlite3_column_int(vstmt, 1);
                }
            }
            sqlite3_finalize(vstmt);
        }
        if (!is_teacher) {
            char *err = json_error_response("You are not the teacher for this task");
            send_json_response(connection, err, MHD_HTTP_FORBIDDEN);
            free(err); return MHD_YES;
        }

        if (grade < 0 || (max_points > 0 && grade > max_points)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Il voto deve essere compreso tra 0 e %d", max_points);
            char *err = json_error_response(msg);
            send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
            free(err); return MHD_YES;
        }

        sqlite3_stmt *stmt;
        const char *q = "UPDATE task_submissions SET grade = ?, teacher_feedback = ?, status = 'graded' WHERE id = ?";
        if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_double(stmt, 1, grade);
            sqlite3_bind_text(stmt, 2, feedback ? feedback : "", -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, sub_id);
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                char *ok = json_success_response("Grade assigned successfully");
                send_json_response(connection, ok, MHD_HTTP_OK);
                free(ok);
            } else {
                char *err = json_error_response("Failed to update grade");
                send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                free(err);
            }
            sqlite3_finalize(stmt);
        } else {
            char *err = json_error_response("Database error");
            send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
            free(err);
        }
        return MHD_YES;
    }
        if (strncmp(url, "/api/tasks/", 11) == 0 && strstr(url, "/submission") != NULL && strcmp(method, "DELETE") == 0) {
            int task_id = atoi(url + 11);
            const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
            if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
                char *err = json_error_response("Authorization required");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err); return MHD_YES;
            }
            int student_id = 0;
            sqlite3_stmt *sstmt;
            if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(sstmt, 1, auth_header + 7, -1, SQLITE_STATIC);
                if (sqlite3_step(sstmt) == SQLITE_ROW) student_id = sqlite3_column_int(sstmt, 0);
                sqlite3_finalize(sstmt);
            }
            if (student_id == 0) {
                char *err = json_error_response("Invalid session");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err); return MHD_YES;
            }
            // Verifica che la data di scadenza non sia passata
            sqlite3_stmt *dstmt;
            int past_due = 0;
            if (sqlite3_prepare_v2(db, "SELECT due_date FROM tasks WHERE id = ?", -1, &dstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(dstmt, 1, task_id);
                if (sqlite3_step(dstmt) == SQLITE_ROW) {
                    const char *due = (const char *)sqlite3_column_text(dstmt, 0);
                    if (due && strlen(due) > 0) {
                        sqlite3_stmt *nowstmt;
                        if (sqlite3_prepare_v2(db, "SELECT ? < datetime('now')", -1, &nowstmt, NULL) == SQLITE_OK) {
                            sqlite3_bind_text(nowstmt, 1, due, -1, SQLITE_STATIC);
                            if (sqlite3_step(nowstmt) == SQLITE_ROW) past_due = sqlite3_column_int(nowstmt, 0);
                            sqlite3_finalize(nowstmt);
                        }
                    }
                }
                sqlite3_finalize(dstmt);
            }
            if (past_due) {
                char *err = json_error_response("Impossibile ritirare: la data di scadenza e' passata");
                send_json_response(connection, err, MHD_HTTP_FORBIDDEN);
                free(err); return MHD_YES;
            }
            // Elimina la consegna
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db, "DELETE FROM task_submissions WHERE task_id = ? AND student_id = ?", -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, task_id);
                sqlite3_bind_int(stmt, 2, student_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                // Aggiorna il progresso del corso
                sqlite3_stmt *pstmt;
                const char *pq = "UPDATE enrollments SET progress_percentage = ("
                    "SELECT CAST(COUNT(DISTINCT ts.task_id) AS REAL) * 100.0 / NULLIF((SELECT COUNT(*) FROM tasks WHERE course_id = e2.course_id), 0) "
                    "FROM task_submissions ts JOIN tasks t2 ON ts.task_id = t2.id JOIN enrollments e2 ON e2.student_id = ts.student_id AND e2.course_id = t2.course_id "
                    "WHERE ts.student_id = ? AND t2.course_id = (SELECT course_id FROM tasks WHERE id = ?)"
                    ") WHERE student_id = ? AND course_id = (SELECT course_id FROM tasks WHERE id = ?)";
                if (sqlite3_prepare_v2(db, pq, -1, &pstmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(pstmt, 1, student_id);
                    sqlite3_bind_int(pstmt, 2, task_id);
                    sqlite3_bind_int(pstmt, 3, student_id);
                    sqlite3_bind_int(pstmt, 4, task_id);
                    sqlite3_step(pstmt);
                    sqlite3_finalize(pstmt);
                }
                char *ok = json_success_response("Consegna ritirata con successo");
                send_json_response(connection, ok, MHD_HTTP_OK);
                free(ok);
            } else {
                char *err = json_error_response("Errore nel ritiro della consegna");
                send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                free(err);
            }
            return MHD_YES;
        }

        // POST /api/tasks/{id}/submit
        if (strncmp(url, "/api/tasks/", 11) == 0 && strstr(url, "/submit") != NULL && strcmp(method, "POST") == 0) {
            int task_id = atoi(url + 11);
            PostData *pdata = (PostData *)*con_cls;
            if (pdata == NULL) {
                pdata = (PostData *)malloc(sizeof(PostData));
                pdata->data = (char *)malloc(MAX_POST_SIZE);
                pdata->size = 0;
                pdata->capacity = MAX_POST_SIZE;
                *con_cls = (void *)pdata;
                return MHD_YES;
            }
            if (*upload_data_size > 0) {
                size_t to_copy = (*upload_data_size < (pdata->capacity - pdata->size)) ? *upload_data_size : (pdata->capacity - pdata->size - 1);
                memcpy(pdata->data + pdata->size, upload_data, to_copy);
                pdata->size += to_copy;
                pdata->data[pdata->size] = '\0';
                *upload_data_size = 0;
                return MHD_YES;
            }

            const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
            if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
                char *err = json_error_response("Authorization required");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err);
                free(pdata->data); free(pdata); *con_cls = NULL;
                return MHD_YES;
            }

            int student_id = 0;
            char student_name[100] = {0};
            sqlite3_stmt *sstmt;
            if (sqlite3_prepare_v2(db, "SELECT s.user_id, u.full_name FROM sessions s JOIN users u ON s.user_id = u.id WHERE s.token = ? AND s.expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(sstmt, 1, auth_header + 7, -1, SQLITE_STATIC);
                if (sqlite3_step(sstmt) == SQLITE_ROW) {
                    student_id = sqlite3_column_int(sstmt, 0);
                    strncpy(student_name, (const char *)sqlite3_column_text(sstmt, 1), 99);
                }
                sqlite3_finalize(sstmt);
            }

            if (student_id == 0) {
                char *err = json_error_response("Invalid session");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err);
                free(pdata->data); free(pdata); *con_cls = NULL;
                return MHD_YES;
            }

            char *content = json_get_field(pdata->data, "content");
            if (!content) content = strdup(""); // Handle empty submission

            sqlite3_stmt *stmt;
            const char *query = "INSERT INTO task_submissions (task_id, student_id, content) VALUES (?, ?, ?)";
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, task_id);
                sqlite3_bind_int(stmt, 2, student_id);
                sqlite3_bind_text(stmt, 3, content, -1, SQLITE_STATIC);

                if (sqlite3_step(stmt) == SQLITE_DONE) {
                    int sub_id = (int)sqlite3_last_insert_rowid(db);
                    
                    // Notify teacher
                    sqlite3_stmt *tstmt;
                    if (sqlite3_prepare_v2(db, "SELECT t.teacher_id, t.title, c.title FROM tasks t JOIN courses c ON t.course_id = c.id WHERE t.id = ?", -1, &tstmt, NULL) == SQLITE_OK) {
                        sqlite3_bind_int(tstmt, 1, task_id);
                        if (sqlite3_step(tstmt) == SQLITE_ROW) {
                            int teacher_id = sqlite3_column_int(tstmt, 0);
                            const char *task_title = (const char *)sqlite3_column_text(tstmt, 1);
                            const char *course_title = (const char *)sqlite3_column_text(tstmt, 2);
                            
                            sqlite3_stmt *notif_stmt;
                            if (sqlite3_prepare_v2(db, "INSERT INTO notifications (user_id, type, message, reference_id) VALUES (?, 'submission_received', ?, ?)", -1, &notif_stmt, NULL) == SQLITE_OK) {
                                char msg_buf[256];
                                snprintf(msg_buf, sizeof(msg_buf), "%s ha consegnato il task '%s' nel corso '%s'", student_name, task_title, course_title);
                                sqlite3_bind_int(notif_stmt, 1, teacher_id);
                                sqlite3_bind_text(notif_stmt, 2, msg_buf, -1, SQLITE_STATIC);
                                sqlite3_bind_int(notif_stmt, 3, sub_id);
                                sqlite3_step(notif_stmt);
                                sqlite3_finalize(notif_stmt);
                            }
                        }
                        sqlite3_finalize(tstmt);
                    }

                    // Aggiorna progresso del corso
                    sqlite3_stmt *pstmt;
                    const char *pq = "UPDATE enrollments SET progress_percentage = ("
                        "SELECT CAST(COUNT(DISTINCT ts.task_id) AS REAL) * 100.0 / NULLIF((SELECT COUNT(*) FROM tasks WHERE course_id = e2.course_id), 0) "
                        "FROM task_submissions ts JOIN tasks t2 ON ts.task_id = t2.id JOIN enrollments e2 ON e2.student_id = ts.student_id AND e2.course_id = t2.course_id "
                        "WHERE ts.student_id = ? AND t2.course_id = (SELECT course_id FROM tasks WHERE id = ?)"
                        ") WHERE student_id = ? AND course_id = (SELECT course_id FROM tasks WHERE id = ?)";
                    if (sqlite3_prepare_v2(db, pq, -1, &pstmt, NULL) == SQLITE_OK) {
                        sqlite3_bind_int(pstmt, 1, student_id);
                        sqlite3_bind_int(pstmt, 2, task_id);
                        sqlite3_bind_int(pstmt, 3, student_id);
                        sqlite3_bind_int(pstmt, 4, task_id);
                        sqlite3_step(pstmt);
                        sqlite3_finalize(pstmt);
                    }

                    char *ok = json_success_response("Submission successful");
                    send_json_response(connection, ok, MHD_HTTP_CREATED);
                    free(ok);
                } else {
                    char *err = json_error_response("Failed to submit task");
                    send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                    free(err);
                }
                sqlite3_finalize(stmt);
            }
            if (content) free(content);
            free(pdata->data); free(pdata); *con_cls = NULL;
            return MHD_YES;
        }

        // GET /api/tasks
        if (strcmp(url, "/api/tasks") == 0 && strcmp(method, "GET") == 0) {
            const char *task_id_param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "id");
            const char *course_id_param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "course_id");

            if (task_id_param) {
                // Get single task details
                int task_id = atoi(task_id_param);
                sqlite3_stmt *stmt;
                const char *query = "SELECT id, title, description, due_date, points, task_type, course_id FROM tasks WHERE id = ?";
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, task_id);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        int fetched_task_id = sqlite3_column_int(stmt, 0);
                        JSONBuilder *jb = json_create();
                        json_start_object(jb);
                        json_add_int(jb, "id", fetched_task_id);
                        const char *title = (const char *)sqlite3_column_text(stmt, 1);
                        const char *desc = (const char *)sqlite3_column_text(stmt, 2);
                        const char *due = (const char *)sqlite3_column_text(stmt, 3);
                        const char *ttype = (const char *)sqlite3_column_text(stmt, 5);
                        int task_course_id = sqlite3_column_int(stmt, 6);
                        json_add_string(jb, "title", title ? title : "");
                        json_add_string(jb, "description", desc ? desc : "");
                        json_add_string(jb, "due_date", due ? due : "");
                        json_add_int(jb, "points", sqlite3_column_int(stmt, 4));
                        json_add_string(jb, "task_type", ttype ? ttype : "");
                        json_add_int(jb, "course_id", task_course_id);
                        /* Build visible_to: CSV of student IDs from task_visibility */
                        char visible_csv[1024] = "";
                        sqlite3_stmt *vis_stmt;
                        if (sqlite3_prepare_v2(db, "SELECT student_id FROM task_visibility WHERE task_id = ? ORDER BY student_id", -1, &vis_stmt, NULL) == SQLITE_OK) {
                            sqlite3_bind_int(vis_stmt, 1, fetched_task_id);
                            int vfirst = 1;
                            while (sqlite3_step(vis_stmt) == SQLITE_ROW) {
                                char sid_buf[16];
                                snprintf(sid_buf, sizeof(sid_buf), "%d", sqlite3_column_int(vis_stmt, 0));
                                if (!vfirst) strncat(visible_csv, ",", sizeof(visible_csv) - strlen(visible_csv) - 1);
                                strncat(visible_csv, sid_buf, sizeof(visible_csv) - strlen(visible_csv) - 1);
                                vfirst = 0;
                            }
                            sqlite3_finalize(vis_stmt);
                        }
                        json_add_string(jb, "visible_to", visible_csv);
                        json_end_object(jb);
                        char *resp = json_get_string(jb);
                        send_json_response(connection, resp, MHD_HTTP_OK);
                        free(resp);
                        json_free(jb);
                        sqlite3_finalize(stmt);
                        return MHD_YES;
                    }
                    sqlite3_finalize(stmt);
                    char *err = json_error_response("Task not found");
                    send_json_response(connection, err, MHD_HTTP_NOT_FOUND);
                    free(err);
                    return MHD_YES;
                }
            } else if (course_id_param) {
                // Get tasks for a course, filtered by visibility for students
                int course_id = atoi(course_id_param);
                int requester_id = get_auth_user_id(connection);
                int is_student = requester_id > 0 && user_has_role(requester_id, "student", NULL);

                sqlite3_stmt *stmt;
                /* For students: only tasks visible to them (no restriction OR explicitly listed).
                   For teachers/admins: all tasks. */
                const char *query_student =
                    "SELECT id, title, description, due_date, points, task_type FROM tasks "
                    "WHERE course_id = ? "
                    "AND (NOT EXISTS (SELECT 1 FROM task_visibility WHERE task_id = tasks.id) "
                    "     OR EXISTS  (SELECT 1 FROM task_visibility WHERE task_id = tasks.id AND student_id = ?))";
                const char *query_teacher =
                    "SELECT id, title, description, due_date, points, task_type FROM tasks WHERE course_id = ?";

                const char *query = is_student ? query_student : query_teacher;
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, course_id);
                    if (is_student) sqlite3_bind_int(stmt, 2, requester_id);
                    JSONBuilder *jb = json_create();
                    json_start_object(jb);
                    json_start_array(jb, "tasks");
                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                        json_start_object(jb);
                        json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                        const char *title = (const char *)sqlite3_column_text(stmt, 1);
                        const char *desc = (const char *)sqlite3_column_text(stmt, 2);
                        const char *due = (const char *)sqlite3_column_text(stmt, 3);
                        const char *ttype = (const char *)sqlite3_column_text(stmt, 5);
                        json_add_string(jb, "title", title ? title : "");
                        json_add_string(jb, "description", desc ? desc : "");
                        json_add_string(jb, "due_date", due ? due : "");
                        json_add_int(jb, "points", sqlite3_column_int(stmt, 4));
                        json_add_string(jb, "task_type", ttype ? ttype : "");
                        json_end_object(jb);
                    }
                    json_end_array(jb);
                    json_end_object(jb);
                    char *resp = json_get_string(jb);
                    send_json_response(connection, resp, MHD_HTTP_OK);
                    free(resp);
                    json_free(jb);
                    sqlite3_finalize(stmt);
                    return MHD_YES;
                }
            } else {
                char *err = json_error_response("Either id or course_id parameter required");
                send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
                free(err);
                return MHD_YES;
            }
        }
    }

        // GET /api/submissions/{id} - Docente vede risposta singola consegna
        if (strncmp(url, "/api/submissions/", 17) == 0 && strcmp(method, "GET") == 0) {
            int sub_id = atoi(url + 17);
            const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
            if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
                char *err = json_error_response("Authorization required");
                send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
                free(err); return MHD_YES;
            }
            sqlite3_stmt *stmt;
            const char *q = "SELECT ts.id, ts.task_id, ts.student_id, ts.content, ts.submission_date, ts.status, "
                            "u.full_name, u.username, t.title as task_title, t.points, ts.grade, ts.teacher_feedback "
                            "FROM task_submissions ts "
                            "JOIN users u ON ts.student_id = u.id "
                            "JOIN tasks t ON ts.task_id = t.id "
                            "WHERE ts.id = ?";
            if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, sub_id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    JSONBuilder *jb = json_create();
                    json_start_object(jb);
                    json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                    json_add_int(jb, "task_id", sqlite3_column_int(stmt, 1));
                    json_add_int(jb, "student_id", sqlite3_column_int(stmt, 2));
                    const char *content = (const char *)sqlite3_column_text(stmt, 3);
                    const char *sub_date = (const char *)sqlite3_column_text(stmt, 4);
                    const char *status = (const char *)sqlite3_column_text(stmt, 5);
                    const char *full_name = (const char *)sqlite3_column_text(stmt, 6);
                    const char *username = (const char *)sqlite3_column_text(stmt, 7);
                    const char *task_title = (const char *)sqlite3_column_text(stmt, 8);
                    json_add_string(jb, "content", content ? content : "");
                    json_add_string(jb, "submission_date", sub_date ? sub_date : "");
                    json_add_string(jb, "status", status ? status : "submitted");
                    json_add_string(jb, "student_name", full_name ? full_name : "");
                    json_add_string(jb, "username", username ? username : "");
                    json_add_string(jb, "task_title", task_title ? task_title : "");
                    json_add_int(jb, "points", sqlite3_column_int(stmt, 9));
                    json_add_double(jb, "grade", sqlite3_column_double(stmt, 10));
                    const char *feedback = (const char *)sqlite3_column_text(stmt, 11);
                    json_add_string(jb, "teacher_feedback", feedback ? feedback : "");
                    json_end_object(jb);
                    char *resp = json_get_string(jb);
                    send_json_response(connection, resp, MHD_HTTP_OK);
                    free(resp); json_free(jb);
                } else {
                    char *err = json_error_response("Submission not found");
                    send_json_response(connection, err, MHD_HTTP_NOT_FOUND);
                    free(err);
                }
                sqlite3_finalize(stmt);
            }
            return MHD_YES;
        }

    // Notifications endpoints
    if (strncmp(url, "/api/notifications", 18) == 0) {
        const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
        if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
            char *err = json_error_response("Authorization required");
            send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
            free(err);
            return MHD_YES;
        }

        int user_id = 0;
        sqlite3_stmt *sstmt;
        if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(sstmt, 1, auth_header + 7, -1, SQLITE_STATIC);
            if (sqlite3_step(sstmt) == SQLITE_ROW) {
                user_id = sqlite3_column_int(sstmt, 0);
            }
            sqlite3_finalize(sstmt);
        }

        if (user_id == 0) {
            char *err = json_error_response("Invalid session");
            send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
            free(err);
            return MHD_YES;
        }

        // GET /api/notifications
        if (strcmp(method, "GET") == 0) {
            sqlite3_stmt *stmt;
            const char *query = "SELECT id, type, message, reference_id, is_read, created_at FROM notifications WHERE user_id = ? ORDER BY created_at DESC LIMIT 50";
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, user_id);
                JSONBuilder *jb = json_create();
                json_start_object(jb);
                json_start_array(jb, "notifications");
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    json_start_object(jb);
                    json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                    json_add_string(jb, "type", (const char *)sqlite3_column_text(stmt, 1));
                    json_add_string(jb, "message", (const char *)sqlite3_column_text(stmt, 2));
                    json_add_int(jb, "reference_id", sqlite3_column_int(stmt, 3));
                    json_add_int(jb, "is_read", sqlite3_column_int(stmt, 4));
                    json_add_string(jb, "created_at", (const char *)sqlite3_column_text(stmt, 5));
                    json_end_object(jb);
                }
                json_end_array(jb);
                
                // Count unread
                sqlite3_stmt *cstmt;
                if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM notifications WHERE user_id = ? AND is_read = 0", -1, &cstmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(cstmt, 1, user_id);
                    if (sqlite3_step(cstmt) == SQLITE_ROW) {
                        json_add_int(jb, "unread_count", sqlite3_column_int(cstmt, 0));
                    }
                    sqlite3_finalize(cstmt);
                }

                json_end_object(jb);
                char *resp = json_get_string(jb);
                send_json_response(connection, resp, MHD_HTTP_OK);
                free(resp);
                json_free(jb);
                sqlite3_finalize(stmt);
                return MHD_YES;
            }
        }

        // PUT /api/notifications/{id}/read
        if (strcmp(method, "PUT") == 0 && strstr(url, "/read") != NULL) {
            int notif_id = atoi(url + 18); // /api/notifications/ID/read
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db, "UPDATE notifications SET is_read = 1 WHERE id = ? AND user_id = ?", -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, notif_id);
                sqlite3_bind_int(stmt, 2, user_id);
                if (sqlite3_step(stmt) == SQLITE_DONE) {
                    char *ok = json_success_response("Notification marked as read");
                    send_json_response(connection, ok, MHD_HTTP_OK);
                    free(ok);
                } else {
                    char *err = json_error_response("Failed to update notification");
                    send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
                    free(err);
                }
                sqlite3_finalize(stmt);
                return MHD_YES;
            }
        }
    }

    // Teacher students endpoint
    if (strcmp(url, "/api/teacher/students") == 0 && strcmp(method, "GET") == 0) {
        const char *auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
        if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
            char *err = json_error_response("Authorization required");
            send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
            free(err);
            return MHD_YES;
        }

        const char *token = auth_header + 7;
        int teacher_id = 0;
        sqlite3_stmt *sstmt;
        
        // Retrieve teacher ID from token
        if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(sstmt, 1, token, -1, SQLITE_STATIC);
            if (sqlite3_step(sstmt) == SQLITE_ROW) {
                teacher_id = sqlite3_column_int(sstmt, 0);
            }
            sqlite3_finalize(sstmt);
        }

        if (teacher_id == 0) {
            char *err = json_error_response("Invalid or expired token");
            send_json_response(connection, err, MHD_HTTP_UNAUTHORIZED);
            free(err);
            return MHD_YES;
        }

        const char *query =
            "SELECT u.id, u.username, u.full_name, u.email, "
            "       c.id AS course_id, c.title AS course_title, e.progress_percentage, e.status "
            "FROM enrollments e "
            "JOIN users u ON e.student_id = u.id "
            "JOIN courses c ON e.course_id = c.id "
            "WHERE c.teacher_id = ? "
            "ORDER BY u.id";

        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, teacher_id);

            JSONBuilder *jb = json_create();
            json_start_object(jb);
            json_start_array(jb, "students");

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                json_start_object(jb);
                json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
                json_add_string(jb, "username", (const char *)sqlite3_column_text(stmt, 1));
                json_add_string(jb, "full_name", (const char *)sqlite3_column_text(stmt, 2));
                json_add_string(jb, "email", (const char *)sqlite3_column_text(stmt, 3));
                json_add_int(jb, "course_id", sqlite3_column_int(stmt, 4));
                json_add_string(jb, "course_title", (const char *)sqlite3_column_text(stmt, 5));
                json_add_double(jb, "progress_percentage", sqlite3_column_double(stmt, 6));
                json_add_string(jb, "status", (const char *)sqlite3_column_text(stmt, 7));
                json_end_object(jb);
                json_append(jb, ",");
            }

            json_end_array(jb);
            json_end_object(jb);

            char *resp = json_get_string(jb);
            send_json_response(connection, resp, MHD_HTTP_OK);
            free(resp);
            json_free(jb);
            sqlite3_finalize(stmt);
            return MHD_YES;
        } else {
            char *err = json_error_response("Database error");
            send_json_response(connection, err, MHD_HTTP_INTERNAL_SERVER_ERROR);
            free(err);
            return MHD_YES;
        }
    }

    // If it's a GET request and not an API route, try to serve static file (frontend)
    if (strcmp(method, "GET") == 0) {
        // Avoid serving files for API namespace
        if (strncmp(url, "/api/", 5) != 0) {
            if (serve_static_file(connection, url) == MHD_YES) {
                return MHD_YES;
            }
        }
    }

    // 404 Not Found
    char *not_found = json_error_response("Endpoint not found");
    send_json_response(connection, not_found, MHD_HTTP_NOT_FOUND);
    free(not_found);
    return MHD_YES;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    log_message("Starting Piattaforma Corsi Online Backend...");

    // Aprire il database
    if (!open_database("courses.db")) {
        return EXIT_FAILURE;
    }

    // Initialize DB schema from schema.sql if needed
    if (!initialize_database_from_schema("schema.sql")) {
        log_message("WARNING: Database schema may not be initialized. Check schema.sql and permissions.");
    }
    run_task_visibility_migration();

    // Creare il server HTTP
    struct MHD_Daemon *daemon;
    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY,
                             PORT,
                             NULL, NULL,
                             &request_handler, NULL,
                             MHD_OPTION_END);

    if (daemon == NULL) {
        log_message("ERROR: Cannot start HTTP server on port %d", PORT);
        close_database();
        return EXIT_FAILURE;
    }

    log_message("HTTP Server started on http://localhost:%d", PORT);
    log_message("API Documentation: http://localhost:%d/api/docs", PORT);

    // Mantenere il server in esecuzione
    log_message("Press Ctrl+C to stop...");
    while (1) {
        sleep(1);
    }

    // Cleanup
    MHD_stop_daemon(daemon);
    close_database();

    return EXIT_SUCCESS;
}
