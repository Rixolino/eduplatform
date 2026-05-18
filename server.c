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

#define PORT 3000
#define MAX_PATH 1024
#define MAX_RESPONSE 16384
#define MAX_POST_SIZE 8192

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
    if (*pos != '"') return NULL;
    
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
static int request_handler(void *cls,
                          struct MHD_Connection *connection,
                          const char *url,
                          const char *method,
                          const char *version,
                          const char *upload_data,
                          size_t *upload_data_size,
                          void **con_cls) {
    
    log_message("%s %s", method, url);
    
    // OPTIONS per CORS
    if (strcmp(method, "OPTIONS") == 0) {
        const char *response = "{}";
        send_json_response(connection, response, MHD_HTTP_OK);
        return MHD_YES;
    }
    
    // Root API endpoint
    if (strcmp(url, "/") == 0) {
        const char *response = "{\"status\": \"ok\", \"message\": \"Piattaforma Corsi Online API v1.0\"}";
        send_json_response(connection, response, MHD_HTTP_OK);
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
                const char *query = "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')";
                
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
                    
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        int user_id = sqlite3_column_int(stmt, 0);
                        
                        // Get user profile
                        AuthUser user;
                        memset(&user, 0, sizeof(AuthUser));
                        
                        if (db_get_user_by_id(db, user_id, &user)) {
                            log_activity(user.user_id, "get_profile", "user", user.user_id, NULL);
                            
                            // Build response
                            JSONBuilder *jb = json_create();
                            json_start_object(jb);
                            json_add_string(jb, "status", "success");
                            json_add_int(jb, "user_id", user.user_id);
                            json_add_string(jb, "username", user.username);
                            json_add_string(jb, "email", user.email);
                            json_add_string(jb, "role", user.role);
                            json_end_object(jb);
                            response = json_get_string(jb);
                            json_free(jb);
                            status = MHD_HTTP_OK;
                        } else {
                            response = json_error_response("User not found");
                            status = MHD_HTTP_NOT_FOUND;
                        }
                    } else {
                        response = json_error_response("Invalid or expired token");
                        status = MHD_HTTP_UNAUTHORIZED;
                    }
                    
                    sqlite3_finalize(stmt);
                } else {
                    response = json_error_response("Database error");
                }
            } else {
                response = json_error_response("Invalid authorization header format");
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

            // Extract fields
            char *title = json_get_field(pdata->data, "title");
            char *description = json_get_field(pdata->data, "description");
            char *teacher_id_s = json_get_field(pdata->data, "teacher_id");
            char *category = json_get_field(pdata->data, "category");
            char *difficulty = json_get_field(pdata->data, "difficulty_level");
            char *duration_s = json_get_field(pdata->data, "duration_hours");
            char *num_lessons_s = json_get_field(pdata->data, "num_lessons");

            if (!title || !description || !teacher_id_s) {
                char *err = json_error_response("Missing required course fields");
                send_json_response(connection, err, MHD_HTTP_BAD_REQUEST);
                free(err);
            } else {
                int teacher_id = atoi(teacher_id_s);
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
                        JSONBuilder *jb = json_create();
                        json_start_object(jb);
                        json_add_string(jb, "status", "success");
                        json_add_int(jb, "course_id", new_id);
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

            // GET specific course
            if (strcmp(method, "GET") == 0) {
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
    log_message("Starting Piattaforma Corsi Online Backend...");
    
    // Aprire il database
    if (!open_database("courses.db")) {
        return EXIT_FAILURE;
    }

    // Initialize DB schema from schema.sql if needed
    if (!initialize_database_from_schema("schema.sql")) {
        log_message("WARNING: Database schema may not be initialized. Check schema.sql and permissions.");
    }
    
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
