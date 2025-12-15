#include "database.h"
#include "logger.h"

sensor_packet_t *data_copy_buffer = NULL; 
size_t data_copy_count = 0;

int db_init_and_open(sqlite3 **out_db){
    if(!out_db) return SQLITE_ERROR;
    
    sqlite3 *db = NULL;
    int rc = sqlite3_open(DB_FILE, &db);
    if(rc != SQLITE_OK){
        if(db) sqlite3_close(db);
        log_event("[SQL] Failed to open database: %s", sqlite3_errstr(rc));
        return rc;
    }
    
    // Create table if not exists
    const char *sql = 
        "CREATE TABLE IF NOT EXISTS sensor_data("
        "id INTEGER, "
        "type INTEGER, "
        "value REAL, "
        "ts DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";
    
    char *errmsg = NULL;
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if(rc != SQLITE_OK){
        log_event("[SQL] Create table error: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return rc;
    }
    
    // Optimize for continuous writes
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA cache_size=-64000;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA temp_store=MEMORY;", NULL, NULL, NULL);
    
    *out_db = db;
    return SQLITE_OK;
}

int db_insert_measure(sqlite3 *db, sensor_packet_t *pkt){
    if(!db || !pkt) return SQLITE_ERROR;
    
    static const char *sql = 
        "INSERT INTO sensor_data(id, type, value, ts) "
        "VALUES (?1, ?2, ?3, datetime(?4, 'unixepoch'));";
    
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        return rc;
    }
    
    sqlite3_bind_int(stmt, 1, pkt->id);
    sqlite3_bind_int(stmt, 2, pkt->type);
    sqlite3_bind_double(stmt, 3, pkt->value);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64)pkt->ts);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : rc;
}

int db_insert_measures_batch(sqlite3 *db, sensor_packet_t *packets, size_t count){
    if(!db || !packets || count == 0) return SQLITE_ERROR;
    
    // Begin transaction
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, &errmsg);
    if(rc != SQLITE_OK){
        log_event("[SQL] Failed to begin transaction: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return rc;
    }
    
    // Prepare statement once
    static const char *sql = 
        "INSERT INTO sensor_data(id, type, value, ts) "
        "VALUES (?1, ?2, ?3, datetime(?4, 'unixepoch'));";
    
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return rc;
    }
    
    // Insert all packets
    size_t success_count = 0;
    for(size_t i = 0; i < count; i++){
        sqlite3_bind_int(stmt, 1, packets[i].id);
        sqlite3_bind_int(stmt, 2, packets[i].type);
        sqlite3_bind_double(stmt, 3, packets[i].value);
        sqlite3_bind_int64(stmt, 4, (sqlite3_int64)packets[i].ts);

        rc = sqlite3_step(stmt);
        if(rc == SQLITE_DONE){
            success_count++;
        }
        
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    
    sqlite3_finalize(stmt);
    
    // Commit transaction
    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, &errmsg);
    if(rc != SQLITE_OK){
        log_event("[SQL] Failed to commit: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return rc;
    }
    
    return SQLITE_OK;
}