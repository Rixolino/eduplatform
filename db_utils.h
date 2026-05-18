#ifndef DB_UTILS_H
#define DB_UTILS_H

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **rows;
    int num_rows;
    int num_cols;
} QueryResult;

typedef struct {
    char **cols;
    char **values;
    int num_cols;
} DatabaseRow;

// Funzione per eseguire query SELECT
int db_execute_query(sqlite3 *db, const char *query, QueryResult *result) {
    char *err = 0;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    result->rows = NULL;
    result->num_rows = 0;
    result->num_cols = sqlite3_column_count(stmt);
    
    // Eseguire la query
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result->num_rows++;
    }
    
    sqlite3_finalize(stmt);
    return 1;
}

// Funzione per eseguire query INSERT/UPDATE/DELETE
int db_execute_update(sqlite3 *db, const char *query) {
    char *err = 0;
    int rc = sqlite3_exec(db, query, 0, 0, &err);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        return 0;
    }
    
    return 1;
}

// Funzione per ottenere l'ultimo ID inserito
int db_last_insert_id(sqlite3 *db) {
    return (int)sqlite3_last_insert_rowid(db);
}

// Funzione per preparare uno statement
sqlite3_stmt *db_prepare(sqlite3 *db, const char *query) {
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return NULL;
    }
    
    return stmt;
}

// Funzione per bindare parametri
int db_bind_text(sqlite3_stmt *stmt, int pos, const char *text) {
    return sqlite3_bind_text(stmt, pos, text, -1, SQLITE_STATIC) == SQLITE_OK;
}

int db_bind_int(sqlite3_stmt *stmt, int pos, int value) {
    return sqlite3_bind_int(stmt, pos, value) == SQLITE_OK;
}

int db_bind_double(sqlite3_stmt *stmt, int pos, double value) {
    return sqlite3_bind_double(stmt, pos, value) == SQLITE_OK;
}

// Funzione per eseguire uno statement preparato
int db_step(sqlite3_stmt *stmt) {
    return sqlite3_step(stmt);
}

// Funzione per ottenere valori da un row
const char *db_get_text(sqlite3_stmt *stmt, int col) {
    return (const char *)sqlite3_column_text(stmt, col);
}

int db_get_int(sqlite3_stmt *stmt, int col) {
    return sqlite3_column_int(stmt, col);
}

double db_get_double(sqlite3_stmt *stmt, int col) {
    return sqlite3_column_double(stmt, col);
}

// Funzione per finalizziare uno statement
void db_finalize(sqlite3_stmt *stmt) {
    sqlite3_finalize(stmt);
}

#endif // DB_UTILS_H
