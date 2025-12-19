#include "storage_manager.h"
#include "sbuffer.h"
#include "logger.h"
#include "database.h"

// Helper: connect to database with retries
static sqlite3* storage_connect_db(int max_attempts){
    sqlite3 *db = NULL;
    
    for(int attempt = 1; attempt <= max_attempts && !stop_flag; attempt++){
        int rc = db_init_and_open(&db);
        if(rc == SQLITE_OK){
            log_event("[SQL] Connection to SQL server established");
            return db;
        }
        
        log_event("[SQL] Unable to connect to SQL server (attempt %d/%d)", attempt, max_attempts);
        
        if(attempt < max_attempts){
            sleep(RECONNECT_DELAY_SEC);
        }
    }
    
    return NULL;
}

// Helper: batch insert with automatic reconnect
static int storage_batch_insert_with_retry(sqlite3 **db, sensor_packet_t *batch, size_t count){
    int rc = db_insert_measures_batch(*db, batch, count);
    
    if(rc == SQLITE_OK){
        return SQLITE_OK;
    }
    
    // Connection lost - attempt reconnect
    log_event("[SQL] Connection to SQL server lost. Attempting reconnect...");
    sqlite3_close(*db);
    *db = NULL;
    
    *db = storage_connect_db(MAX_RECONNECT_ATTEMPTS);
    if(!*db){
        log_event("[SQL] Unable to reconnect to SQL server after %d attempts", MAX_RECONNECT_ATTEMPTS);
        return SQLITE_ERROR;
    }
    
    // Retry batch insert after reconnect
    rc = db_insert_measures_batch(*db, batch, count);
    if(rc == SQLITE_OK){
        log_event("[SQL] Recovered and stored %zu measurements", count);
        return SQLITE_OK;
    }
    
    // Batch still failed - try individual inserts as fallback
    log_event("[SQL] Batch insert failed after reconnect. Inserting individually...");
    size_t success = 0;
    for(size_t i = 0; i < count; i++){
        if(db_insert_measure(*db, &batch[i]) == SQLITE_OK){
            success++;
        }
    }
    
    if(success > 0){
        log_event("[SQL] Individual insert: %zu/%zu successful", success, count);
    }
    
    if(success < count){
        log_event("[SQL][ERROR] Lost %zu measurements", count - success);
    }
    return (success == count) ? SQLITE_OK : SQLITE_ERROR;
}

void *storage_manager_thread(void *arg){
    (void)arg;
    
    log_event("[STORAGE] Storage manager thread started");
    
    // Validate configuration
    if(BATCH_SIZE > 1000){
        log_event("[STORAGE] WARNING: BATCH_SIZE=%d exceeds recommended max (1000)", BATCH_SIZE);
    }
    if(BATCH_SIZE < 10){
        log_event("[STORAGE] WARNING: BATCH_SIZE=%d too small, performance will suffer", BATCH_SIZE);
    }
    
    // Calculate memory footprint
    size_t batch_memory = BATCH_SIZE * sizeof(sensor_packet_t);
    log_event("[STORAGE] Batch buffer size: %zu bytes (%zu packets)", batch_memory, (size_t)BATCH_SIZE);
    
    // Initial connection
    sqlite3 *db = storage_connect_db(MAX_RECONNECT_ATTEMPTS);
    if(!db){
        log_event("[SQL] Unable to connect to SQL server. Exiting gateway");
        exit(EXIT_FAILURE);
    }
    
    // Allocate batch buffer
    sensor_packet_t *batch = malloc(BATCH_SIZE * sizeof(sensor_packet_t));
    if(!batch){
        log_event("[STORAGE] Failed to allocate batch buffer");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    
    size_t batch_count = 0;
    size_t total_inserted = 0;
    size_t total_failed = 0;
    size_t health_check_counter = 0;

    // Main processing loop
    while(!stop_flag){
        // //Flush for the last packet
        // if(batch_count > 0){
        //     // flush batch
        //     if(storage_batch_insert_with_retry(&db, batch, batch_count) == SQLITE_OK){
        //         total_inserted += batch_count;
        //     }
        //     batch_count = 0;
        // }

        sbuffer_node_t *node;
        while((node = sbuffer_find_for_storage(&sbuffer)) != NULL){
            // Collect batch
            if(batch_count < BATCH_SIZE){
                batch[batch_count++] = node->pkt;
            }
            else{
                // Buffer full, will process this node in next iteration
                break;
            }
            sbuffer_mark_storage_done(&sbuffer, node);
        }
        // Flush when batch is full
        if(batch_count > 0){
            if(storage_batch_insert_with_retry(&db, batch, batch_count) == SQLITE_OK){
                total_inserted += batch_count;
                
                // Health check
                health_check_counter += batch_count;
                if(health_check_counter >= 1000){
                    if(db_health_check(db) != 0){
                        log_event("[STORAGE] Database health check failed, attempting reconnect");
                        sqlite3_close(db);
                        db = storage_connect_db(MAX_RECONNECT_ATTEMPTS);
                        if(!db){
                            log_event("[STORAGE] Fatal: unable to reconnect to database");
                            break;
                        }
                    }
                    health_check_counter = 0;
                }
            }
            batch_count = 0;
        }
        else{
            // No data available, sleep to avoid busy-waiting
            usleep(POLL_DELAY_MS * 1000);
        }
    }
    
    // Final flush
    if(batch_count > 0){
        log_event("[STORAGE] Flushing final batch of %zu measurements", batch_count);
        int rc = storage_batch_insert_with_retry(&db, batch, batch_count);
        if(rc == SQLITE_OK){
            total_inserted += batch_count;
        }
        else{
            total_failed += batch_count;
        }
    }
    
    // Cleanup
    free(batch);
    
    if(db){
        sqlite3_close(db);
        log_event("[SQL] Database connection closed");
    }
    
    log_event("[STORAGE] Storage manager thread exiting. Stats: %zu inserted, %zu failed", total_inserted, total_failed);
    
    return NULL;
}