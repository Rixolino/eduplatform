#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *buffer;
    int capacity;
    int length;
} JSONBuilder;

// Create a new JSON builder
JSONBuilder *json_create() {
    JSONBuilder *jb = (JSONBuilder *)malloc(sizeof(JSONBuilder));
    jb->capacity = 4096;
    jb->length = 0;
    jb->buffer = (char *)malloc(jb->capacity);
    jb->buffer[0] = '\0';
    return jb;
}

// Append to JSON buffer
void json_append(JSONBuilder *jb, const char *str) {
    int len = strlen(str);
    
    while (jb->length + len + 1 > jb->capacity) {
        jb->capacity *= 2;
        jb->buffer = (char *)realloc(jb->buffer, jb->capacity);
    }
    
    strcat(jb->buffer, str);
    jb->length += len;
}

// Escape JSON string
char *json_escape_string(const char *str) {
    if (!str) return strdup("");
    
    int len = strlen(str);
    char *escaped = (char *)malloc(len * 2 + 1);
    int pos = 0;
    
    for (int i = 0; i < len; i++) {
        switch (str[i]) {
            case '"':
                escaped[pos++] = '\\';
                escaped[pos++] = '"';
                break;
            case '\\':
                escaped[pos++] = '\\';
                escaped[pos++] = '\\';
                break;
            case '\n':
                escaped[pos++] = '\\';
                escaped[pos++] = 'n';
                break;
            case '\r':
                escaped[pos++] = '\\';
                escaped[pos++] = 'r';
                break;
            case '\t':
                escaped[pos++] = '\\';
                escaped[pos++] = 't';
                break;
            default:
                escaped[pos++] = str[i];
        }
    }
    
    escaped[pos] = '\0';
    return escaped;
}

// Start JSON object
void json_start_object(JSONBuilder *jb) {
    json_append(jb, "{");
}

// End JSON object
void json_end_object(JSONBuilder *jb) {
    // Remove trailing comma if present
    if (jb->buffer[jb->length - 1] == ',') {
        jb->buffer[jb->length - 1] = '\0';
        jb->length--;
    }
    json_append(jb, "}");
}

// Add key-value pairs
void json_add_string(JSONBuilder *jb, const char *key, const char *value) {
    char *escaped = json_escape_string(value);
    char buf[2048];
    snprintf(buf, sizeof(buf), "\"%s\":\"%s\",", key, escaped);
    json_append(jb, buf);
    free(escaped);
}

void json_add_int(JSONBuilder *jb, const char *key, int value) {
    char buf[512];
    snprintf(buf, sizeof(buf), "\"%s\":%d,", key, value);
    json_append(jb, buf);
}

void json_add_double(JSONBuilder *jb, const char *key, double value) {
    char buf[512];
    snprintf(buf, sizeof(buf), "\"%s\":%f,", key, value);
    json_append(jb, buf);
}

void json_add_bool(JSONBuilder *jb, const char *key, int value) {
    char buf[512];
    snprintf(buf, sizeof(buf), "\"%s\":%s,", key, value ? "true" : "false");
    json_append(jb, buf);
}

void json_add_null(JSONBuilder *jb, const char *key) {
    char buf[256];
    snprintf(buf, sizeof(buf), "\"%s\":null,", key);
    json_append(jb, buf);
}

// Start array
void json_start_array(JSONBuilder *jb, const char *key) {
    char buf[512];
    snprintf(buf, sizeof(buf), "\"%s\":[", key);
    json_append(jb, buf);
}

// End array
void json_end_array(JSONBuilder *jb) {
    if (jb->buffer[jb->length - 1] == ',') {
        jb->buffer[jb->length - 1] = '\0';
        jb->length--;
    }
    json_append(jb, "],");
}

// Get final JSON string
char *json_get_string(JSONBuilder *jb) {
    char *result = (char *)malloc(jb->length + 1);
    strcpy(result, jb->buffer);
    return result;
}

// Free JSON builder
void json_free(JSONBuilder *jb) {
    if (jb) {
        if (jb->buffer) free(jb->buffer);
        free(jb);
    }
}

// Create error response
char *json_error_response(const char *error) {
    JSONBuilder *jb = json_create();
    json_start_object(jb);
    json_add_string(jb, "error", error);
    json_end_object(jb);
    char *result = json_get_string(jb);
    json_free(jb);
    return result;
}

// Create success response
char *json_success_response(const char *message) {
    JSONBuilder *jb = json_create();
    json_start_object(jb);
    json_add_string(jb, "status", "success");
    json_add_string(jb, "message", message);
    json_end_object(jb);
    char *result = json_get_string(jb);
    json_free(jb);
    return result;
}

#endif // JSON_UTILS_H
