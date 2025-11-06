#include "database.h"
#include "logger.h"

int db_init_and_open(sqlite3 **out_db) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open(DB_FILE, &db);
    if (rc != SQLITE_OK) {
        log_event("[SQL] Failed to open DB: %s", sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return rc;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS sensor_data ("
        "id INTEGER, type INTEGER, value REAL, "
        "ts DATETIME DEFAULT CURRENT_TIMESTAMP);";
    char *errmsg = NULL;
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        log_event("[SQL] Create table error: %s", errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return rc;
    }

    // bật WAL mode để tăng tốc ghi và tránh lock toàn DB
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);

    *out_db = db;
    return SQLITE_OK;
}

/**
 * Tối ưu hóa insert bằng prepared statement dùng lại.
 * stmt được truyền từ bên ngoài (đã chuẩn bị sẵn).
 */
int db_insert_measure(sqlite3_stmt *stmt, const sensor_packet_t *pkt) {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    sqlite3_bind_int(stmt, 1, pkt->id);
    sqlite3_bind_int(stmt, 2, pkt->type);
    sqlite3_bind_double(stmt, 3, pkt->value);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        return rc;
    }
    return SQLITE_OK;
}
