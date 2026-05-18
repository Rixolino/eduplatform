#ifndef AUTH_H
#define AUTH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

#define JWT_SECRET "your-secret-key-change-in-production"
#define TOKEN_EXPIRY 86400  // 24 hours

typedef struct {
    int user_id;
    char username[256];
    char email[256];
    char role[50];
} AuthUser;

// Simple base64 encoding (for JWT)
char *base64_encode(const unsigned char *input, int len) {
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    char *output = (char *)malloc(((len + 2) / 3) * 4 + 1);
    int out_len = 0;
    int i = 0;
    
    while (i < len) {
        unsigned char b1 = input[i++];
        unsigned char b2 = (i < len) ? input[i++] : 0;
        unsigned char b3 = (i < len) ? input[i++] : 0;
        
        int n = (b1 << 16) | (b2 << 8) | b3;
        
        output[out_len++] = base64_chars[(n >> 18) & 63];
        output[out_len++] = base64_chars[(n >> 12) & 63];
        output[out_len++] = (i - 1 < len) ? base64_chars[(n >> 6) & 63] : '=';
        output[out_len++] = (i < len) ? base64_chars[n & 63] : '=';
    }
    
    output[out_len] = '\0';
    return output;
}

// Simple hashing function (should use bcrypt in production)
char *hash_password(const char *password) {
    // This is a placeholder - in production use bcrypt or similar
    char *hash = (char *)malloc(65);
    unsigned int hash_value = 5381;
    int c;
    
    while ((c = *password++)) {
        hash_value = ((hash_value << 5) + hash_value) + c;
    }
    
    sprintf(hash, "%08x%08x", hash_value, hash_value ^ 0xAAAAAAAA);
    return hash;
}

// Verify password
int verify_password(const char *password, const char *hash) {
    char *computed_hash = hash_password(password);
    int result = strcmp(computed_hash, hash) == 0;
    free(computed_hash);
    return result;
}

// Generate JWT token
char *generate_jwt_token(AuthUser *user) {
    time_t now = time(NULL);
    time_t expires = now + TOKEN_EXPIRY;
    
    // Create payload: {"user_id": X, "username": "Y", "role": "Z", "exp": T}
    char payload[512];
    snprintf(payload, sizeof(payload),
             "{\"user_id\":%d,\"username\":\"%s\",\"role\":\"%s\",\"exp\":%ld}",
             user->user_id, user->username, user->role, expires);
    
    // In a real implementation, this should be properly signed
    // For now, just encode the payload in base64
    char *token = base64_encode((unsigned char *)payload, strlen(payload));
    
    return token;
}

// Verify JWT token
int verify_jwt_token(const char *token, AuthUser *user) {
    // This is a simplified verification
    // In production, properly verify the signature and expiry
    
    if (!token || strlen(token) == 0) {
        return 0;
    }
    
    // For now, just return 1 if token is not empty
    // Proper implementation would decode and verify
    return 1;
}

// Get user by ID from database
int db_get_user_by_id(sqlite3 *db, int user_id, AuthUser *user) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT id, username, email, role FROM users WHERE id = ?";
    
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user->user_id = sqlite3_column_int(stmt, 0);
        strncpy(user->username, (const char *)sqlite3_column_text(stmt, 1), 255);
        strncpy(user->email, (const char *)sqlite3_column_text(stmt, 2), 255);
        strncpy(user->role, (const char *)sqlite3_column_text(stmt, 3), 49);
        sqlite3_finalize(stmt);
        return 1;
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

// Get user by username
int db_get_user_by_username(sqlite3 *db, const char *username, AuthUser *user) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT id, username, email, role FROM users WHERE username = ?";
    
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user->user_id = sqlite3_column_int(stmt, 0);
        strncpy(user->username, (const char *)sqlite3_column_text(stmt, 1), 255);
        strncpy(user->email, (const char *)sqlite3_column_text(stmt, 2), 255);
        strncpy(user->role, (const char *)sqlite3_column_text(stmt, 3), 49);
        sqlite3_finalize(stmt);
        return 1;
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

// Register user
int db_register_user(sqlite3 *db, const char *username, const char *email,
                     const char *password, const char *full_name, const char *role) {
    sqlite3_stmt *stmt;
    const char *query = "INSERT INTO users (username, email, password_hash, full_name, role) VALUES (?, ?, ?, ?, ?)";
    
    char *password_hash = hash_password(password);
    
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        free(password_hash);
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, password_hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, full_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, role, -1, SQLITE_STATIC);
    
    int result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    free(password_hash);
    
    return result;
}

// Verify login credentials
int db_verify_login(sqlite3 *db, const char *username, const char *password, AuthUser *user) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT id, username, email, password_hash, role FROM users WHERE username = ?";
    
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *stored_hash = (const char *)sqlite3_column_text(stmt, 3);
        
        if (verify_password(password, stored_hash)) {
            user->user_id = sqlite3_column_int(stmt, 0);
            strncpy(user->username, (const char *)sqlite3_column_text(stmt, 1), 255);
            strncpy(user->email, (const char *)sqlite3_column_text(stmt, 2), 255);
            strncpy(user->role, (const char *)sqlite3_column_text(stmt, 4), 49);
            sqlite3_finalize(stmt);
            return 1;
        }
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

#endif // AUTH_H
