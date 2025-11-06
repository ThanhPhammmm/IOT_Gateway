#include "database.h"

// data manager will copy nodes that haven't been processed_by_data
// Strategy: lock, traverse list, for each unprocessed_by_data mark processed_by_data=1 and decrement refcount (do NOT free here if refcount==0? we can free here)
sensor_packet_t *data_copy_buffer = NULL; // temp dynamic buffer (we allocate per batch)
size_t data_copy_count = 0;

int db_init_and_open(sqlite3 **out_db){
    sqlite3 *db;
    int rc = sqlite3_open(DB_FILE, &db);
    if(rc != SQLITE_OK){
        if(db) sqlite3_close(db);
        return rc;
    }
    const char *sql = "CREATE TABLE IF NOT EXISTS sensor_data("
                      "id INTEGER, type INTEGER, value REAL, ts DATETIME DEFAULT CURRENT_TIMESTAMP);";
    char *errmsg = NULL;
    rc = sqlite3_exec(db, sql, 0, 0, &errmsg);
    if(rc != SQLITE_OK){
        fprintf(stderr, "sqlite create table error: %s\n", errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return rc;
    }
    *out_db = db;
    return SQLITE_OK;
}

int db_insert_measure(sqlite3 *db, sensor_packet_t *pkt){
    const char *sql = "INSERT INTO sensor_data(id, type, value, ts) VALUES (?1, ?2, ?3, datetime('now'))";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        if(stmt) sqlite3_finalize(stmt);
        return rc;
    }
    sqlite3_bind_int(stmt, 1, pkt->id);
    sqlite3_bind_int(stmt, 2, pkt->type);
    sqlite3_bind_double(stmt, 3, pkt->value);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if(rc != SQLITE_DONE) return rc;
    return SQLITE_OK;
}
