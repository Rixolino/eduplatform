#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sqlite3.h"

#pragma comment(lib, "ws2_32.lib")

#define PORT 3000
#define MAX_CONNECTIONS 64
#define BUFFER_SIZE 8192
#define DB_PATH "courses.db"

sqlite3 *db = NULL;

// Utility per lettura file
int read_file(const char *path, char *buffer, int max_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    int size = fread(buffer, 1, max_size - 1, f);
    buffer[size] = 0;
    fclose(f);
    return size;
}

// Get MIME type
const char *get_mime_type(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css")) return "text/css";
    if (strstr(path, ".js")) return "application/javascript";
    if (strstr(path, ".json")) return "application/json";
    return "text/plain";
}

// Serve static file
void serve_static(SOCKET client, const char *path) {
    char file_path[512];
    char buffer[65536];
    
    if (strcmp(path, "/") == 0) {
        strcpy(file_path, "index.html");
    } else {
        strcpy(file_path, path + 1);
    }
    
    int size = read_file(file_path, buffer, sizeof(buffer));
    if (size < 0) {
        const char *resp = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 9\r\n\r\nNot Found";
        send(client, resp, strlen(resp), 0);
        return;
    }
    
    char header[512];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nAccess-Control-Allow-Origin: *\r\n\r\n",
            get_mime_type(file_path), size);
    send(client, header, strlen(header), 0);
    send(client, buffer, size, 0);
}

// Risposta JSON semplice
void send_json_response(SOCKET client, int status, const char *json) {
    char header[512];
    sprintf(header, "HTTP/1.1 %d OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nAccess-Control-Allow-Origin: *\r\n\r\n",
            status, (int)strlen(json));
    send(client, header, strlen(header), 0);
    send(client, json, strlen(json), 0);
}

// GET /api/courses
void api_get_courses(SOCKET client) {
    char json[65536] = "[";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, "SELECT id, title, description, category FROM courses LIMIT 100", -1, &stmt, NULL) == SQLITE_OK) {
        int first = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) strcat(json, ",");
            first = 0;
            
            sprintf(json + strlen(json), "{\"id\":%d,\"title\":\"%s\",\"description\":\"%s\",\"category\":\"%s\"}",
                    sqlite3_column_int(stmt, 0),
                    (const char*)sqlite3_column_text(stmt, 1),
                    (const char*)sqlite3_column_text(stmt, 2),
                    (const char*)sqlite3_column_text(stmt, 3));
        }
        sqlite3_finalize(stmt);
    }
    strcat(json, "]");
    send_json_response(client, 200, json);
}

// POST /api/users/register (semplificato)
void api_register(SOCKET client, const char *body) {
    send_json_response(client, 201, "{\"status\":\"success\",\"message\":\"User registered\"}");
}

// GET /api/users/profile (semplificato)
void api_get_profile(SOCKET client) {
    send_json_response(client, 200, "{\"id\":1,\"username\":\"demo_user\",\"email\":\"demo@example.com\",\"full_name\":\"Demo User\",\"role\":\"student\"}");
}

// POST /api/users/login (semplificato)
void api_login(SOCKET client, const char *body) {
    send_json_response(client, 200, "{\"status\":\"success\",\"token\":\"demo-token-12345\"}");
}

// Process HTTP request
void handle_request(SOCKET client, const char *request) {
    char method[32] = {0};
    char path[512] = {0};
    
    sscanf(request, "%s %s", method, path);
    
    // OPTIONS CORS
    if (strcmp(method, "OPTIONS") == 0) {
        const char *resp = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type,Authorization\r\nContent-Length: 0\r\n\r\n";
        send(client, resp, strlen(resp), 0);
        return;
    }
    
    // API routes
    if (strstr(path, "/api/") == path) {
        if (strcmp(path, "/api/courses") == 0 && strcmp(method, "GET") == 0) {
            api_get_courses(client);
        }
        else if (strcmp(path, "/api/users/register") == 0 && strcmp(method, "POST") == 0) {
            api_register(client, "");
        }
        else if (strcmp(path, "/api/users/login") == 0 && strcmp(method, "POST") == 0) {
            api_login(client, "");
        }
        else if (strcmp(path, "/api/users/profile") == 0 && strcmp(method, "GET") == 0) {
            api_get_profile(client);
        }
        else {
            send_json_response(client, 404, "{\"error\":\"Not found\"}");
        }
    }
    else {
        // Serve static files
        serve_static(client, path);
    }
}

// Initialize database
void init_database() {
    char schema[4096];
    if (read_file("schema.sql", schema, sizeof(schema)) > 0) {
        char *err = NULL;
        sqlite3_exec(db, schema, NULL, NULL, &err);
        if (err) {
            fprintf(stderr, "Schema error: %s\n", err);
            sqlite3_free(err);
        }
    }
}

// Main server loop
int main() {
    WSADATA wsa;
    SOCKET listen_socket, client_socket;
    struct sockaddr_in server, client;
    int client_len;
    char buffer[BUFFER_SIZE];
    
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
    
    // Open database
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database\n");
        return 1;
    }
    
    // Initialize schema
    init_database();
    
    // Create listening socket
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed\n");
        return 1;
    }
    
    // Allow socket reuse
    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    // Bind
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    
    if (bind(listen_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed: %d\n", WSAGetLastError());
        closesocket(listen_socket);
        return 1;
    }
    
    // Listen
    listen(listen_socket, SOMAXCONN);
    printf("Server listening on http://localhost:%d\n", PORT);
    printf("Press Ctrl+C to stop\n");
    
    // Accept connections
    while (1) {
        client_len = sizeof(client);
        client_socket = accept(listen_socket, (struct sockaddr*)&client, &client_len);
        
        if (client_socket == INVALID_SOCKET) {
            fprintf(stderr, "accept() failed\n");
            continue;
        }
        
        // Receive request
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
