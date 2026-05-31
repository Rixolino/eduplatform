#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "sqlite3.h"
#include "auth.h"
#include "json_utils.h"
#include "db_utils.h"

#pragma comment(lib, "ws2_32.lib")

#define PORT 3000
#define DB_PATH "courses.db"

sqlite3 *db = NULL;

// Get MIME type by file extension
const char *get_mime_type(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css")) return "text/css";
    if (strstr(path, ".js")) return "application/javascript";
    if (strstr(path, ".json")) return "application/json";
    if (strstr(path, ".png")) return "image/png";
    if (strstr(path, ".jpg") || strstr(path, ".jpeg")) return "image/jpeg";
    if (strstr(path, ".svg")) return "image/svg+xml";
    if (strstr(path, ".woff")) return "font/woff";
    if (strstr(path, ".woff2")) return "font/woff2";
    return "text/plain";
}

// Serve static file
void serve_static(SOCKET client, const char *path) {
    char file_path[512];
    if (strcmp(path, "/") == 0) {
        strcpy(file_path, "index.html");
    } else {
        strcpy(file_path, path + 1);
    }
    
    FILE *f = fopen(file_path, "rb");
    if (!f) {
        const char *resp = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 9\r\nAccess-Control-Allow-Origin: *\r\n\r\nNot Found";
        send(client, resp, strlen(resp), 0);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char header[512];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nAccess-Control-Allow-Origin: *\r\n\r\n",
            get_mime_type(file_path), size);
    send(client, header, strlen(header), 0);
    
    char file_buf[4096];
    int read_bytes;
    while ((read_bytes = (int)fread(file_buf, 1, sizeof(file_buf), f)) > 0) {
        send(client, file_buf, read_bytes, 0);
    }
    fclose(f);
}

// Send standard JSON response
void send_json_response(SOCKET client, int status, const char *json) {
    const char *status_str = "OK";
    if (status == 201) status_str = "Created";
    else if (status == 400) status_str = "Bad Request";
    else if (status == 401) status_str = "Unauthorized";
    else if (status == 403) status_str = "Forbidden";
    else if (status == 404) status_str = "Not Found";
    else if (status == 500) status_str = "Internal Server Error";
    
    char header[512];
    sprintf(header, "HTTP/1.1 %d %s\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                    "Access-Control-Allow-Headers: Content-Type, Authorization\r\n\r\n",
            status, status_str, (int)strlen(json));
    send(client, header, strlen(header), 0);
    send(client, json, (int)strlen(json), 0);
}

// Simple JSON field extractor (supports strings and numbers/booleans)
char *json_get_field(const char *json, const char *field) {
    if (!json || !field) return NULL;
    
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    
    const char *pos = strstr(json, pattern);
    if (!pos) return NULL;
    
    pos += strlen(pattern);
    
    // Skip spaces and colon
    while (*pos && (isspace((unsigned char)*pos) || *pos == ':')) pos++;
    
    char *result = (char *)malloc(512);
    if (!result) return NULL;
    int idx = 0;
    
    if (*pos == '"') {
        pos++; // Skip opening quote
        while (*pos && idx < 511) {
            if (*pos == '"' && (idx == 0 || result[idx-1] != '\\')) {
                break;
            }
            result[idx++] = *pos++;
        }
        result[idx] = '\0';
    } else {
        // Number, boolean, etc.
        while (*pos && *pos != ',' && *pos != '}' && *pos != ']' && !isspace((unsigned char)*pos) && idx < 511) {
            result[idx++] = *pos++;
        }
        result[idx] = '\0';
    }
    return result;
}

// Get HTTP header value (case-insensitive)
const char *get_header(const char *request, const char *header_name) {
    static char header_val[1024];
    header_val[0] = '\0';
    
    int name_len = (int)strlen(header_name);
    const char *ptr = request;
    while (ptr && *ptr) {
        while (*ptr == '\r' || *ptr == '\n' || *ptr == ' ') ptr++;
        if (*ptr == '\0') break;
        
        if (_strnicmp(ptr, header_name, name_len) == 0) {
            const char *val_start = ptr + name_len;
            while (*val_start == ' ' || *val_start == ':') val_start++;
            
            int idx = 0;
            while (val_start[idx] && val_start[idx] != '\r' && val_start[idx] != '\n' && idx < 1023) {
                header_val[idx] = val_start[idx];
                idx++;
            }
            header_val[idx] = '\0';
            return header_val;
        }
        
        ptr = strchr(ptr, '\n');
        if (ptr) ptr++;
    }
    return NULL;
}

// Extract query parameter value from query string
void get_query_param(const char *query_string, const char *name, char *out, int max_len) {
    out[0] = '\0';
    if (!query_string || strlen(query_string) == 0) return;
    
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%s=", name);
    const char *pos = strstr(query_string, pattern);
    if (!pos) return;
    
    if (pos != query_string && *(pos - 1) != '&') {
        pos = strstr(pos + 1, pattern);
        while (pos && *(pos - 1) != '&') {
            pos = strstr(pos + 1, pattern);
        }
        if (!pos) return;
    }
    
    pos += strlen(pattern);
    int idx = 0;
    while (*pos && *pos != '&' && idx < max_len - 1) {
        out[idx++] = *pos++;
    }
    out[idx] = '\0';
    
    // URL decode %20 -> space
    char decoded[512] = {0};
    int d_idx = 0;
    for (int i = 0; i < idx; i++) {
        if (out[i] == '%' && i + 2 < idx) {
            if (out[i+1] == '2' && out[i+2] == '0') {
                decoded[d_idx++] = ' ';
                i += 2;
                continue;
            }
        }
        decoded[d_idx++] = out[i];
    }
    decoded[d_idx] = '\0';
    strcpy(out, decoded);
}

// Store session token in database
int store_session(int user_id, const char *token) {
    sqlite3_stmt *stmt;
    const char *query = "INSERT INTO sessions (user_id, token, expires_at) VALUES (?, ?, datetime('now', '+24 hours'))";
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;
    
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, token, -1, SQLITE_STATIC);
    
    int result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

// POST /api/users/register
void api_register(SOCKET client, const char *body) {
    char *username = json_get_field(body, "username");
    char *email = json_get_field(body, "email");
    char *full_name = json_get_field(body, "full_name");
    char *password = json_get_field(body, "password");
    char *role = json_get_field(body, "role");
    
    if (!username || !email || !password || !full_name || !role) {
        send_json_response(client, 400, "{\"error\":\"Missing required fields\"}");
        if (username) free(username);
        if (email) free(email);
        if (full_name) free(full_name);
        if (password) free(password);
        if (role) free(role);
        return;
    }
    
    sqlite3_stmt *stmt;
    int exists = 0;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM users WHERE username = ? OR email = ?", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = 1;
        }
        sqlite3_finalize(stmt);
    }
    
    if (exists) {
        send_json_response(client, 400, "{\"error\":\"Username or Email already registered\"}");
        free(username); free(email); free(full_name); free(password); free(role);
        return;
    }
    
    char *hashed_pwd = hash_password(password);
    const char *insert_query = "INSERT INTO users (username, email, full_name, password_hash, role) VALUES (?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, insert_query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, full_name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, hashed_pwd, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, role, -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            send_json_response(client, 201, "{\"status\":\"success\",\"message\":\"Registration complete\"}");
        } else {
            send_json_response(client, 500, "{\"error\":\"Database registration failed\"}");
        }
        sqlite3_finalize(stmt);
    } else {
        send_json_response(client, 500, "{\"error\":\"Database prepare failed\"}");
    }
    
    free(hashed_pwd);
    free(username); free(email); free(full_name); free(password); free(role);
}

// POST /api/users/login
void api_login(SOCKET client, const char *body) {
    char *username = json_get_field(body, "username");
    char *password = json_get_field(body, "password");
    
    if (!username || !password) {
        send_json_response(client, 400, "{\"error\":\"Username and Password are required\"}");
        if (username) free(username);
        if (password) free(password);
        return;
    }
    
    sqlite3_stmt *stmt;
    int success = 0;
    AuthUser user;
    memset(&user, 0, sizeof(AuthUser));
    
    if (sqlite3_prepare_v2(db, "SELECT id, username, email, password_hash, role FROM users WHERE username = ?", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int user_id = sqlite3_column_int(stmt, 0);
            const char *stored_hash = (const char *)sqlite3_column_text(stmt, 3);
            
            if (verify_password(password, stored_hash) || strcmp(password, stored_hash) == 0) {
                user.user_id = user_id;
                strncpy(user.username, (const char *)sqlite3_column_text(stmt, 1), sizeof(user.username)-1);
                strncpy(user.email, (const char *)sqlite3_column_text(stmt, 2), sizeof(user.email)-1);
                strncpy(user.role, (const char *)sqlite3_column_text(stmt, 4), sizeof(user.role)-1);
                success = 1;
            }
        }
        sqlite3_finalize(stmt);
    }
    
    if (success) {
        char *token = generate_jwt_token(&user);
        if (store_session(user.user_id, token)) {
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
            
            char *resp = json_get_string(jb);
            send_json_response(client, 200, resp);
            free(resp);
            json_free(jb);
        } else {
            send_json_response(client, 500, "{\"error\":\"Failed to store session\"}");
        }
        free(token);
    } else {
        send_json_response(client, 401, "{\"error\":\"Invalid username or password\"}");
    }
    
    free(username);
    free(password);
}

// GET /api/users/profile
void api_get_profile(SOCKET client, const char *request) {
    const char *auth_header = get_header(request, "Authorization");
    if (!auth_header || strnicmp(auth_header, "Bearer ", 7) != 0) {
        printf("[AUTH] No Bearer token provided in Authorization header\n");
        send_json_response(client, 401, "{\"error\":\"No token provided\"}");
        return;
    }
    const char *token = auth_header + 7;
    printf("[AUTH] Validating token: %.15.15s...\n", token);
    
    sqlite3_stmt *stmt;
    const char *query = "SELECT u.id, u.username, u.email, u.full_name, u.role "
                        "FROM users u "
                        "JOIN sessions s ON u.id = s.user_id "
                        "WHERE s.token = ? AND s.expires_at > datetime('now')";
                        
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("[AUTH] Token valid. User ID: %d\n", sqlite3_column_int(stmt, 0));
            JSONBuilder *jb = json_create();
            json_start_object(jb);
            json_add_string(jb, "status", "success");
            json_add_int(jb, "user_id", sqlite3_column_int(stmt, 0));
            json_add_string(jb, "username", (const char *)sqlite3_column_text(stmt, 1));
            json_add_string(jb, "email", (const char *)sqlite3_column_text(stmt, 2));
            json_add_string(jb, "full_name", (const char *)sqlite3_column_text(stmt, 3));
            json_add_string(jb, "role", (const char *)sqlite3_column_text(stmt, 4));
            json_end_object(jb);
            
            char *resp = json_get_string(jb);
            send_json_response(client, 200, resp);
            free(resp);
            json_free(jb);
        } else {
            printf("[AUTH] Token not found or expired in database\n");
            send_json_response(client, 401, "{\"error\":\"Invalid token\"}");
        }
        sqlite3_finalize(stmt);
    } else {
        printf("[AUTH] Database query failed: %s\n", sqlite3_errmsg(db));
        send_json_response(client, 500, "{\"error\":\"Database query failed\"}");
    }
}

// GET /api/courses
void api_get_courses(SOCKET client, const char *query_string) {
    char category[128] = {0};
    char difficulty[128] = {0};
    char search[128] = {0};
    
    get_query_param(query_string, "category", category, sizeof(category));
    get_query_param(query_string, "difficulty", difficulty, sizeof(difficulty));
    get_query_param(query_string, "search", search, sizeof(search));
    
    char query[2048] = "SELECT c.id, c.title, c.description, c.teacher_id, c.category, c.difficulty_level, c.duration_hours, c.num_lessons, u.full_name AS teacher_name "
                       "FROM courses c "
                       "JOIN users u ON c.teacher_id = u.id WHERE 1=1";
                       
    if (strlen(category) > 0) {
        strcat(query, " AND c.category = ?");
    }
    if (strlen(difficulty) > 0) {
        strcat(query, " AND c.difficulty_level = ?");
    }
    if (strlen(search) > 0) {
        strcat(query, " AND (c.title LIKE ? OR c.description LIKE ?)");
    }
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        int bind_idx = 1;
        if (strlen(category) > 0) {
            sqlite3_bind_text(stmt, bind_idx++, category, -1, SQLITE_TRANSIENT);
        }
        if (strlen(difficulty) > 0) {
            sqlite3_bind_text(stmt, bind_idx++, difficulty, -1, SQLITE_TRANSIENT);
        }
        if (strlen(search) > 0) {
            char search_pattern[256];
            snprintf(search_pattern, sizeof(search_pattern), "%%s%%", search);
            // Replace simple placeholder to avoid crash if pattern building
            char actual_pattern[256];
            sprintf(actual_pattern, "%%%s%%", search);
            sqlite3_bind_text(stmt, bind_idx++, actual_pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, bind_idx++, actual_pattern, -1, SQLITE_TRANSIENT);
        }
        
        JSONBuilder *jb = json_create();
        json_append(jb, "[");
        
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (count > 0) {
                json_append(jb, ",");
            }
            json_start_object(jb);
            json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
            json_add_string(jb, "title", (const char *)sqlite3_column_text(stmt, 1));
            json_add_string(jb, "description", (const char *)sqlite3_column_text(stmt, 2));
            json_add_int(jb, "teacher_id", sqlite3_column_int(stmt, 3));
            json_add_string(jb, "category", (const char *)sqlite3_column_text(stmt, 4));
            json_add_string(jb, "difficulty_level", (const char *)sqlite3_column_text(stmt, 5));
            json_add_double(jb, "duration_hours", sqlite3_column_double(stmt, 6));
            json_add_int(jb, "num_lessons", sqlite3_column_int(stmt, 7));
            json_add_string(jb, "teacher_name", (const char *)sqlite3_column_text(stmt, 8));
            json_end_object(jb);
            count++;
        }
        json_append(jb, "]");
        
        char *resp = json_get_string(jb);
        send_json_response(client, 200, resp);
        free(resp);
        json_free(jb);
        sqlite3_finalize(stmt);
    } else {
        send_json_response(client, 500, "{\"error\":\"Database query preparation failed\"}");
    }
}

// POST /api/courses
void api_create_course(SOCKET client, const char *request, const char *body) {
    const char *auth_header = get_header(request, "Authorization");
    if (!auth_header || strnicmp(auth_header, "Bearer ", 7) != 0) {
        send_json_response(client, 401, "{\"error\":\"Authorization required\"}");
        return;
    }
    const char *token = auth_header + 7;
    
    int author_id = 0;
    sqlite3_stmt *sstmt;
    if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(sstmt, 1, token, -1, SQLITE_STATIC);
        if (sqlite3_step(sstmt) == SQLITE_ROW) {
            author_id = sqlite3_column_int(sstmt, 0);
        }
        sqlite3_finalize(sstmt);
    }
    
    if (author_id == 0) {
        send_json_response(client, 401, "{\"error\":\"Invalid or expired token\"}");
        return;
    }
    
    char user_role[32] = "";
    sqlite3_stmt *rstmt;
    if (sqlite3_prepare_v2(db, "SELECT role FROM users WHERE id = ?", -1, &rstmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(rstmt, 1, author_id);
        if (sqlite3_step(rstmt) == SQLITE_ROW) {
            const unsigned char *role_text = sqlite3_column_text(rstmt, 0);
            if (role_text) strncpy(user_role, (const char *)role_text, sizeof(user_role)-1);
        }
        sqlite3_finalize(rstmt);
    }
    
    if (strcmp(user_role, "teacher") != 0 && strcmp(user_role, "admin") != 0) {
        send_json_response(client, 403, "{\"error\":\"Permission denied: only teachers or admins can create courses\"}");
        return;
    }
    
    char *title = json_get_field(body, "title");
    char *description = json_get_field(body, "description");
    char *teacher_id_s = json_get_field(body, "teacher_id");
    char *category = json_get_field(body, "category");
    char *difficulty = json_get_field(body, "difficulty_level");
    char *duration_s = json_get_field(body, "duration_hours");
    char *num_lessons_s = json_get_field(body, "num_lessons");
    
    if (!title || !description) {
        send_json_response(client, 400, "{\"error\":\"Missing required course fields: title or description\"}");
    } else {
        int teacher_id = (strcmp(user_role, "teacher") == 0) ? author_id : (teacher_id_s ? atoi(teacher_id_s) : author_id);
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
                json_add_int(jb, "teacher_id", teacher_id);
                json_end_object(jb);
                char *resp = json_get_string(jb);
                send_json_response(client, 201, resp);
                free(resp);
                json_free(jb);
            } else {
                send_json_response(client, 500, "{\"error\":\"Failed to create course\"}");
            }
            sqlite3_finalize(stmt);
        } else {
            send_json_response(client, 500, "{\"error\":\"Database error creating course\"}");
        }
    }
    
    if (title) free(title);
    if (description) free(description);
    if (teacher_id_s) free(teacher_id_s);
    if (category) free(category);
    if (difficulty) free(difficulty);
    if (duration_s) free(duration_s);
    if (num_lessons_s) free(num_lessons_s);
}

// GET /api/courses/:id
void api_get_course_details(SOCKET client, int course_id) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT c.id, c.title, c.description, c.teacher_id, c.category, c.difficulty_level, c.duration_hours, c.num_lessons, u.full_name AS teacher_name "
                        "FROM courses c "
                        "JOIN users u ON c.teacher_id = u.id WHERE c.id = ?";
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
            json_add_string(jb, "teacher_name", (const char *)sqlite3_column_text(stmt, 8));
            json_end_object(jb);
            
            char *resp = json_get_string(jb);
            send_json_response(client, 200, resp);
            free(resp);
            json_free(jb);
        } else {
            send_json_response(client, 404, "{\"error\":\"Course not found\"}");
        }
        sqlite3_finalize(stmt);
    } else {
        send_json_response(client, 500, "{\"error\":\"Database error\"}");
    }
}

// GET /api/courses/:id/buddies
void api_get_course_buddies(SOCKET client, int course_id) {
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
        send_json_response(client, 200, resp);
        free(resp);
        json_free(jb);
        sqlite3_finalize(stmt);
    } else {
        send_json_response(client, 500, "{\"error\":\"Database error\"}");
    }
}

// POST /api/enrollments
void api_enroll_course(SOCKET client, const char *request, const char *body) {
    const char *auth_header = get_header(request, "Authorization");
    if (!auth_header || strnicmp(auth_header, "Bearer ", 7) != 0) {
        send_json_response(client, 401, "{\"error\":\"Authorization required\"}");
        return;
    }
    const char *token = auth_header + 7;
    
    int student_id = 0;
    sqlite3_stmt *sstmt;
    if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(sstmt, 1, token, -1, SQLITE_STATIC);
        if (sqlite3_step(sstmt) == SQLITE_ROW) {
            student_id = sqlite3_column_int(sstmt, 0);
        }
        sqlite3_finalize(sstmt);
    }
    if (student_id == 0) {
        send_json_response(client, 401, "{\"error\":\"Invalid or expired token\"}");
        return;
    }
    
    char *course_id_s = json_get_field(body, "course_id");
    if (!course_id_s) {
        send_json_response(client, 400, "{\"error\":\"course_id is required\"}");
        return;
    }
    int course_id = atoi(course_id_s);
    free(course_id_s);
    
    sqlite3_stmt *stmt;
    int already_enrolled = 0;
    const char *check_query = "SELECT id FROM enrollments WHERE student_id = ? AND course_id = ?";
    if (sqlite3_prepare_v2(db, check_query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, student_id);
        sqlite3_bind_int(stmt, 2, course_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            already_enrolled = 1;
        }
        sqlite3_finalize(stmt);
    }
    
    if (already_enrolled) {
        send_json_response(client, 400, "{\"error\":\"Already enrolled in this course\"}");
        return;
    }
    
    const char *insert_query = "INSERT INTO enrollments (student_id, course_id, status, progress_percentage) VALUES (?, ?, 'enrolled', 0.0)";
    if (sqlite3_prepare_v2(db, insert_query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, student_id);
        sqlite3_bind_int(stmt, 2, course_id);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            int new_id = (int)sqlite3_last_insert_rowid(db);
            char resp[128];
            sprintf(resp, "{\"status\":\"success\",\"enrollment_id\":%d}", new_id);
            send_json_response(client, 201, resp);
        } else {
            send_json_response(client, 500, "{\"error\":\"Failed to enroll\"}");
        }
        sqlite3_finalize(stmt);
    } else {
        send_json_response(client, 500, "{\"error\":\"Database error\"}");
    }
}

// GET /api/enrollments
void api_get_enrollments(SOCKET client, const char *request) {
    const char *auth_header = get_header(request, "Authorization");
    if (!auth_header || strnicmp(auth_header, "Bearer ", 7) != 0) {
        send_json_response(client, 401, "{\"error\":\"Authorization required\"}");
        return;
    }
    const char *token = auth_header + 7;
    
    int student_id = 0;
    sqlite3_stmt *sstmt;
    if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(sstmt, 1, token, -1, SQLITE_STATIC);
        if (sqlite3_step(sstmt) == SQLITE_ROW) {
            student_id = sqlite3_column_int(sstmt, 0);
        }
        sqlite3_finalize(sstmt);
    }
    if (student_id == 0) {
        send_json_response(client, 401, "{\"error\":\"Invalid or expired token\"}");
        return;
    }
    
    sqlite3_stmt *stmt;
    const char *query = "SELECT e.id, e.course_id, e.status, e.progress_percentage, c.title "
                        "FROM enrollments e "
                        "JOIN courses c ON e.course_id = c.id "
                        "WHERE e.student_id = ?";
                        
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, student_id);
        
        JSONBuilder *jb = json_create();
        json_append(jb, "[");
        
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (count > 0) {
                json_append(jb, ",");
            }
            json_start_object(jb);
            json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
            json_add_int(jb, "course_id", sqlite3_column_int(stmt, 1));
            json_add_string(jb, "status", (const char *)sqlite3_column_text(stmt, 2));
            json_add_double(jb, "progress_percentage", sqlite3_column_double(stmt, 3));
            json_add_string(jb, "course_title", (const char *)sqlite3_column_text(stmt, 4));
            json_end_object(jb);
            count++;
        }
        json_append(jb, "]");
        
        char *resp = json_get_string(jb);
        send_json_response(client, 200, resp);
        free(resp);
        json_free(jb);
        sqlite3_finalize(stmt);
    } else {
        send_json_response(client, 500, "{\"error\":\"Database query error\"}");
    }
}

// GET /api/paths
void api_get_paths(SOCKET client) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT id, title, description, creator_id FROM paths";
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        JSONBuilder *jb = json_create();
        json_start_object(jb);
        json_start_array(jb, "paths");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json_start_object(jb);
            json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
            json_add_string(jb, "title", (const char *)sqlite3_column_text(stmt, 1));
            json_add_string(jb, "description", (const char *)sqlite3_column_text(stmt, 2));
            json_add_int(jb, "creator_id", sqlite3_column_int(stmt, 3));
            json_end_object(jb);
        }
        json_end_array(jb);
        json_end_object(jb);
        
        char *resp = json_get_string(jb);
        send_json_response(client, 200, resp);
        free(resp);
        json_free(jb);
        sqlite3_finalize(stmt);
    } else {
        send_json_response(client, 500, "{\"error\":\"Database error\"}");
    }
}

// GET /api/paths/:id
void api_get_path_details(SOCKET client, int path_id) {
    sqlite3_stmt *stmt;
    JSONBuilder *jb = json_create();
    json_start_object(jb);
    json_add_int(jb, "path_id", path_id);
    
    const char *query_path = "SELECT title, description FROM paths WHERE id = ?";
    if (sqlite3_prepare_v2(db, query_path, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, path_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            json_add_string(jb, "title", (const char *)sqlite3_column_text(stmt, 0));
            json_add_string(jb, "description", (const char *)sqlite3_column_text(stmt, 1));
        }
        sqlite3_finalize(stmt);
    }
    
    const char *query_courses = "SELECT c.id, c.title, c.category, pc.sequence_order "
                                "FROM path_courses pc "
                                "JOIN courses c ON pc.course_id = c.id "
                                "WHERE pc.path_id = ? "
                                "ORDER BY pc.sequence_order";
                                
    if (sqlite3_prepare_v2(db, query_courses, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, path_id);
        json_start_array(jb, "courses");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json_start_object(jb);
            json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
            json_add_string(jb, "title", (const char *)sqlite3_column_text(stmt, 1));
            json_add_string(jb, "category", (const char *)sqlite3_column_text(stmt, 2));
            json_add_int(jb, "position", sqlite3_column_int(stmt, 3));
            json_end_object(jb);
        }
        json_end_array(jb);
        sqlite3_finalize(stmt);
    }
    
    json_end_object(jb);
    char *resp = json_get_string(jb);
    send_json_response(client, 200, resp);
    free(resp);
    json_free(jb);
}

// POST /api/tasks
void api_create_task(SOCKET client, const char *request, const char *body) {
    const char *auth_header = get_header(request, "Authorization");
    if (!auth_header || strnicmp(auth_header, "Bearer ", 7) != 0) {
        send_json_response(client, 401, "{\"error\":\"Authorization required\"}");
        return;
    }
    const char *token = auth_header + 7;
    
    int author_id = 0;
    sqlite3_stmt *sstmt;
    if (sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ? AND expires_at > datetime('now')", -1, &sstmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(sstmt, 1, token, -1, SQLITE_STATIC);
        if (sqlite3_step(sstmt) == SQLITE_ROW) {
            author_id = sqlite3_column_int(sstmt, 0);
        }
        sqlite3_finalize(sstmt);
    }
    if (author_id == 0) {
        send_json_response(client, 401, "{\"error\":\"Invalid session\"}");
        return;
    }
    
    char role[32] = "";
    sqlite3_stmt *rstmt;
    if (sqlite3_prepare_v2(db, "SELECT role FROM users WHERE id = ?", -1, &rstmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(rstmt, 1, author_id);
        if (sqlite3_step(rstmt) == SQLITE_ROW) {
            const unsigned char *role_text = sqlite3_column_text(rstmt, 0);
            if (role_text) strncpy(role, (const char *)role_text, sizeof(role)-1);
        }
        sqlite3_finalize(rstmt);
    }
    
    if (strcmp(role, "teacher") != 0 && strcmp(role, "admin") != 0) {
        send_json_response(client, 403, "{\"error\":\"Only teachers can create tasks\"}");
        return;
    }
    
    char *title = json_get_field(body, "title");
    char *description = json_get_field(body, "description");
    char *course_id_s = json_get_field(body, "course_id");
    char *type = json_get_field(body, "task_type");
    char *due_date = json_get_field(body, "due_date");
    char *points_s = json_get_field(body, "points");
    
    if (!title || !course_id_s || !type) {
        send_json_response(client, 400, "{\"error\":\"Missing required fields\"}");
    } else {
        int course_id = atoi(course_id_s);
        int points = points_s ? atoi(points_s) : 0;
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
                char resp[128];
                sprintf(resp, "{\"status\":\"success\",\"task_id\":%d}", task_id);
                send_json_response(client, 201, resp);
            } else {
                send_json_response(client, 500, "{\"error\":\"Failed to create task\"}");
            }
            sqlite3_finalize(stmt);
        } else {
            send_json_response(client, 500, "{\"error\":\"Database error\"}");
        }
    }
    
    if (title) free(title);
    if (description) free(description);
    if (course_id_s) free(course_id_s);
    if (type) free(type);
    if (due_date) free(due_date);
    if (points_s) free(points_s);
}

// GET /api/tasks
void api_get_tasks(SOCKET client, const char *query_string) {
    char course_id_param[64] = {0};
    get_query_param(query_string, "course_id", course_id_param, sizeof(course_id_param));
    if (strlen(course_id_param) == 0) {
        send_json_response(client, 400, "{\"error\":\"course_id parameter required\"}");
        return;
    }
    int course_id = atoi(course_id_param);
    
    sqlite3_stmt *stmt;
    const char *query = "SELECT id, title, description, due_date, points, task_type FROM tasks WHERE course_id = ?";
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, course_id);
        
        JSONBuilder *jb = json_create();
        json_start_object(jb);
        json_start_array(jb, "tasks");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json_start_object(jb);
            json_add_int(jb, "id", sqlite3_column_int(stmt, 0));
            json_add_string(jb, "title", (const char *)sqlite3_column_text(stmt, 1));
            json_add_string(jb, "description", (const char *)sqlite3_column_text(stmt, 2));
            json_add_string(jb, "due_date", (const char *)sqlite3_column_text(stmt, 3));
            json_add_int(jb, "points", sqlite3_column_int(stmt, 4));
            json_add_string(jb, "task_type", (const char *)sqlite3_column_text(stmt, 5));
            json_end_object(jb);
        }
        json_end_array(jb);
        json_end_object(jb);
        
        char *resp = json_get_string(jb);
        send_json_response(client, 200, resp);
        free(resp);
        json_free(jb);
        sqlite3_finalize(stmt);
    } else {
        send_json_response(client, 500, "{\"error\":\"Database error\"}");
    }
}

// Process HTTP request
void handle_request(SOCKET client, const char *request) {
    char method[32] = {0};
    char full_path[512] = {0};
    
    sscanf(request, "%s %s", method, full_path);
    
    // OPTIONS CORS
    if (strcmp(method, "OPTIONS") == 0) {
        const char *resp = "HTTP/1.1 200 OK\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                           "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
                           "Content-Length: 0\r\n\r\n";
        send(client, resp, (int)strlen(resp), 0);
        return;
    }
    
    // Separate path and query string
    char path[512] = {0};
    char query_string[512] = {0};
    char *q = strchr(full_path, '?');
    if (q) {
        *q = '\0';
        strcpy(query_string, q + 1);
    }
    strcpy(path, full_path);
    
    // Extract request body
    const char *body = strstr(request, "\r\n\r\n");
    if (body) body += 4;
    else body = "";
    
    // Routing
    if (strncmp(path, "/api/", 5) == 0) {
        if (strcmp(path, "/api/health") == 0 && strcmp(method, "GET") == 0) {
            send_json_response(client, 200, "{\"status\":\"healthy\",\"message\":\"API is running\"}");
        }
        else if (strcmp(path, "/api/users/register") == 0 && strcmp(method, "POST") == 0) {
            api_register(client, body);
        }
        else if (strcmp(path, "/api/users/login") == 0 && strcmp(method, "POST") == 0) {
            api_login(client, body);
        }
        else if (strcmp(path, "/api/users/profile") == 0 && strcmp(method, "GET") == 0) {
            api_get_profile(client, request);
        }
        else if (strcmp(path, "/api/courses") == 0 && strcmp(method, "GET") == 0) {
            api_get_courses(client, query_string);
        }
        else if (strcmp(path, "/api/courses") == 0 && strcmp(method, "POST") == 0) {
            api_create_course(client, request, body);
        }
        else if (strncmp(path, "/api/courses/", 13) == 0) {
            int course_id = atoi(path + 13);
            
            if (strstr(path, "/buddies") != NULL && strcmp(method, "GET") == 0) {
                api_get_course_buddies(client, course_id);
            }
            else if (strcmp(method, "GET") == 0) {
                api_get_course_details(client, course_id);
            }
            else {
                send_json_response(client, 404, "{\"error\":\"Route not found\"}");
            }
        }
        else if (strcmp(path, "/api/enrollments") == 0) {
            if (strcmp(method, "POST") == 0) {
                api_enroll_course(client, request, body);
            }
            else if (strcmp(method, "GET") == 0) {
                api_get_enrollments(client, request);
            }
            else {
                send_json_response(client, 404, "{\"error\":\"Route not found\"}");
            }
        }
        else if (strcmp(path, "/api/paths") == 0 && strcmp(method, "GET") == 0) {
            api_get_paths(client);
        }
        else if (strncmp(path, "/api/paths/", 11) == 0 && strcmp(method, "GET") == 0) {
            int path_id = atoi(path + 11);
            api_get_path_details(client, path_id);
        }
        else if (strcmp(path, "/api/tasks") == 0) {
            if (strcmp(method, "POST") == 0) {
                api_create_task(client, request, body);
            }
            else if (strcmp(method, "GET") == 0) {
                api_get_tasks(client, query_string);
            }
            else {
                send_json_response(client, 404, "{\"error\":\"Route not found\"}");
            }
        }
        else if (strcmp(path, "/api/users/skills") == 0 && strcmp(method, "GET") == 0) {
            send_json_response(client, 200, "{\"skills\": [{\"category\":\"Programmazione\", \"score\": 3.5}, {\"category\":\"Design\", \"score\": 1.2}, {\"category\":\"Data Science\", \"score\": 0.8}]}");
        }
        else {
            send_json_response(client, 404, "{\"error\":\"Not found\"}");
        }
    }
    else {
        serve_static(client, path);
    }
}

// Initialize database
void init_database() {
    FILE *f = fopen("schema.sql", "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *schema = (char *)malloc(size + 1);
    if (schema) {
        int bytes = (int)fread(schema, 1, size, f);
        schema[bytes] = '\0';
        char *err = NULL;
        sqlite3_exec(db, schema, NULL, NULL, &err);
        if (err) {
            fprintf(stderr, "Schema error: %s\n", err);
            sqlite3_free(err);
        }
        free(schema);
    }
    fclose(f);
}

// Main server loop
int main() {
    WSADATA wsa;
    SOCKET listen_socket, client_socket;
    struct sockaddr_in server, client;
    int client_len;
    char buffer[8192];
    
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
    
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database\n");
        return 1;
    }
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    
    init_database();
    
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed\n");
        sqlite3_close(db);
        WSACleanup();
        return 1;
    }
    
    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    
    if (bind(listen_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed: %d\n", WSAGetLastError());
        closesocket(listen_socket);
        sqlite3_close(db);
        WSACleanup();
        return 1;
    }
    
    listen(listen_socket, SOMAXCONN);
    printf("Server listening on http://localhost:%d\n", PORT);
    printf("Press Ctrl+C to stop\n");
    
    while (1) {
        client_len = sizeof(client);
        client_socket = accept(listen_socket, (struct sockaddr*)&client, &client_len);
        
        if (client_socket == INVALID_SOCKET) {
            fprintf(stderr, "accept() failed\n");
            continue;
        }
        
        int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = 0;
            handle_request(client_socket, buffer);
        }
        
        closesocket(client_socket);
    }
    
    closesocket(listen_socket);
    sqlite3_close(db);
    WSACleanup();
    return 0;
}
