#include "storage_manager.h"
#include "sbuffer.h"
#include "logger.h"
#include "database.h"

#define STORAGE_BATCH_SIZE 256   // giới hạn batch insert để tránh quá tải SQLite
#define STORAGE_SLEEP_US 100000  // 100ms

void *storage_manager_thread(void *arg){
    (void)arg;
    log_event("[STORAGE] Storage manager thread started\n");

    sqlite3 *db = NULL;
    int tries = 0;

    // --- 1. Khởi tạo kết nối CSDL có retry ---
    while (tries < 3 && !stop_flag) {
        int rc = db_init_and_open(&db);
        if (rc == SQLITE_OK) {
            log_event("[SQL] Connection to SQLite established.");
            break;
        }
        log_event("[SQL] Failed to connect to SQLite (attempt %d)", ++tries);
        sleep(1);
    }
    if (!db) {
        log_event("[SQL] Could not open database after startup retries. Exiting gateway.");
        exit(EXIT_FAILURE);
    }

    // --- 2. Chuẩn bị câu lệnh sẵn (prepared statement) ---
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO sensor_data(id, type, value, ts) VALUES (?1, ?2, ?3, datetime('now'));";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        log_event("[SQL] Failed to prepare statement: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    // --- 3. Vòng lặp chính ---
    while (!stop_flag) {
        sensor_packet_t batch[STORAGE_BATCH_SIZE];
        size_t count = 0;

        // lấy các node chưa lưu
        sbuffer_node_t *node;
        while ((node = sbuffer_find_for_storage(&sbuffer)) != NULL && count < STORAGE_BATCH_SIZE) {
            batch[count++] = node->pkt;
            sbuffer_mark_storage_done(&sbuffer, node);
        }

        if (count == 0) {
            usleep(STORAGE_SLEEP_US);
            continue;
        }

        // --- 4. Transaction batch insert ---
        sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
        for (size_t i = 0; i < count; ++i) {
            sensor_packet_t *pkt = &batch[i];

            sqlite3_reset(stmt);
            sqlite3_bind_int(stmt, 1, pkt->id);
            sqlite3_bind_int(stmt, 2, pkt->type);
            sqlite3_bind_double(stmt, 3, pkt->value);

            int rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                log_event("[SQL] Insert failed (id=%d): %s", pkt->id, sqlite3_errmsg(db));
                sqlite3_reset(stmt);

                // thử reconnect
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                db = NULL;

                int reconnect_try = 0;
                while (reconnect_try < 3 && !stop_flag) {
                    int rc2 = db_init_and_open(&db);
                    if (rc2 == SQLITE_OK) {
                        log_event("[SQL] Reconnected successfully.");
                        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
                            break;
                    }
                    log_event("[SQL] Reconnect attempt %d failed.", ++reconnect_try);
                    sleep(1);
                }
                if (!db) {
                    log_event("[SQL] Reconnect failed after 3 attempts. Exiting gateway.");
                    exit(EXIT_FAILURE);
                }
            }
        }
        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

        log_event("[SQL] Stored %zu records in batch.", count);

        usleep(STORAGE_SLEEP_US);
    }

    // --- 5. Cleanup ---
    if (stmt) sqlite3_finalize(stmt);

    if (db) {
        sqlite3_close(db);
        log_event("[SQL] Database connection closed.");
    }
    log_event("[STORAGE] Storage manager thread exiting\n");
    return NULL;
}
