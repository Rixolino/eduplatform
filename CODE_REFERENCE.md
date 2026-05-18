# Code Implementation Reference

This document shows all the code sections added for authentication endpoints.

---

## 1. Helper Functions (Lines 77-216)

### json_get_field() - Extract JSON field values
```c
char *json_get_field(const char *json, const char *field) {
    if (!json || !field) return NULL;
    
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "\"%s\":", field);
    
    const char *pos = strstr(json, pattern);
    if (!pos) return NULL;
    
    pos += strlen(pattern);
    
    while (*pos && isspace(*pos)) pos++;
    
    if (*pos != '"') return NULL;
    
    pos++;
    
    char *result = (char *)malloc(512);
    int idx = 0;
    
    while (*pos && idx < 511) {
        if (*pos == '"' && (idx == 0 || result[idx-1] != '\\')) {
            result[idx] = '\0';
            return result;
        }
        result[idx++] = *pos;
        pos++;
    }
    
    free(result);
    return NULL;
}
```

### log_activity() - Record user actions
```c
void log_activity(int user_id, const char *action, const char *resource_type, 
                  int resource_id, const char *details) {
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
```

### store_session() - Persist JWT tokens
```c
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
```

### is_valid_email() - Validate email format
```c
int is_valid_email(const char *email) {
    if (!email || strlen(email) < 5) return 0;
    
    const char *at = strchr(email, '@');
    if (!at) return 0;
    
    if (at == email || *(at + 1) == '\0') return 0;
    
    const char *dot = strchr(at + 1, '.');
    if (!dot || dot == at + 1) return 0;
    
    return 1;
}
```

### username_exists() - Check username uniqueness
```c
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
```

### email_exists() - Check email uniqueness
```c
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
```

---

## 2. Registration Endpoint (Lines 252-352)

### POST /api/users/register Handler
```c
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
```

---

## 3. Login Endpoint (Lines 355-444)

### POST /api/users/login Handler
```c
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
```

---

## 4. Profile Endpoint (Lines 447-515)

### GET /api/users/profile Handler
```c
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
```

---

## 5. Structures and Definitions (Lines 20-24)

### PostData Structure
```c
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} PostData;
```

Used for accumulating POST request bodies across multiple HTTP calls.

---

## 6. Includes and Defines (Lines 1-17)

### Headers and Constants
```c
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
```

---

## Integration with Existing Code

### From auth.h
- `db_register_user()` - Inserts user with hashed password
- `db_verify_login()` - Verifies credentials and returns AuthUser
- `generate_jwt_token()` - Creates JWT token for user
- `db_get_user_by_id()` - Retrieves user profile

### From json_utils.h
- `json_create()` - Creates JSONBuilder
- `json_start_object()` - Starts JSON object
- `json_add_string()` - Adds string field
- `json_add_int()` - Adds integer field
- `json_end_object()` - Closes JSON object
- `json_get_string()` - Gets final JSON string
- `json_free()` - Frees JSONBuilder
- `json_error_response()` - Creates error response

### From db_utils.h
- `sqlite3_prepare_v2()` - Prepares SQL statement
- `sqlite3_bind_int()` - Binds integer parameter
- `sqlite3_bind_text()` - Binds text parameter
- `sqlite3_bind_null()` - Binds NULL parameter
- `sqlite3_step()` - Executes statement
- `sqlite3_column_int()` - Gets integer column
- `sqlite3_finalize()` - Cleans up statement

---

## Total Code Statistics

| Category | Count |
|----------|-------|
| Total lines added | ~380 |
| Helper functions | 6 |
| Endpoint handlers | 3 |
| Validation checks | 8+ |
| Database queries | 15+ |
| Error responses | 12+ |
| Memory allocations | 10+ |
| struct definitions | 1 |
| Function calls to helpers | 30+ |

---

## Compilation

To compile with CMake (in build directory):
```bash
mkdir build
cd build
cmake ..
cmake --build .
./bin/course_server
```

No additional dependencies needed beyond:
- libmicrohttpd
- SQLite3
- Standard C library

---

End of Code Reference
