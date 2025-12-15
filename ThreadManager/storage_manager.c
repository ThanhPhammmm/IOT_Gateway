#include "storage_manager.h"
#include "sbuffer.h"
#include "logger.h"
#include "database.h"

#define MAX_RECONNECT_ATTEMPTS 3
#define RECONNECT_DELAY_SEC 1
#define BATCH_SIZE 100
#define POLL_DELAY_MS 50

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
    
    return (success > 0) ? SQLITE_OK : SQLITE_ERROR;
}

// Helper: Flush batch to database
static int flush_batch(sqlite3 **db, sensor_packet_t *batch, size_t *batch_count, 
                       size_t *total_inserted, size_t *total_failed){
    if(*batch_count == 0){
        return 0; // Nothing to flush
    }
    
    int rc = storage_batch_insert_with_retry(db, batch, *batch_count);
    
    if(rc == SQLITE_OK){
        log_event("[SQL] Stored batch of %zu measurements", *batch_count);
        *total_inserted += *batch_count;
    } 
    else{
        *total_failed += *batch_count;
        if(!*db){
            log_event("[SQL] Fatal database error during flush");
            return -1; // Fatal error
        }
    }
    
    *batch_count = 0;
    return 0;
}

void *storage_manager_thread(void *arg){
    (void)arg;
    
    log_event("[STORAGE] Storage manager thread started");
    
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
    
    // Main processing loop - TWO PHASE APPROACH
    // PHASE 1: Normal operation (while !stop_flag)
    while(!stop_flag) {
        sbuffer_node_t *node = sbuffer_find_for_storage(&sbuffer);
        
        if(!node) {
            // No data available - flush remaining batch if any
            if(batch_count > 0) {
                if(flush_batch(&db, batch, &batch_count, &total_inserted, &total_failed) < 0){
                    free(batch);
                    exit(EXIT_FAILURE);
                }
            }
            
            // Small delay to avoid busy waiting
            usleep(POLL_DELAY_MS * 1000);
            continue;
        }
        
        // Add to batch
        batch[batch_count++] = node->pkt;
        sbuffer_mark_storage_done(&sbuffer, node);
        
        // Insert when batch is full
        if(batch_count >= BATCH_SIZE){
            if(flush_batch(&db, batch, &batch_count, &total_inserted, &total_failed) < 0){
                free(batch);
                exit(EXIT_FAILURE);
            }
        }
    }
    
    // PHASE 2: Shutdown - drain remaining data from sbuffer
    log_event("[STORAGE] Stop flag detected, draining remaining data from buffer...");
    
    size_t drained_count = 0;
    sbuffer_node_t *node;
    
    // Keep draining until no more data
    // Note: sbuffer_find_for_storage returns NULL immediately when stop_flag=1 AND no data
    while((node = sbuffer_find_for_storage(&sbuffer)) != NULL){
        batch[batch_count++] = node->pkt;
        sbuffer_mark_storage_done(&sbuffer, node);
        drained_count++;
        
        // Flush if batch full
        if(batch_count >= BATCH_SIZE) {
            if(flush_batch(&db, batch, &batch_count, &total_inserted, &total_failed) < 0){
                log_event("[STORAGE] Error during final flush, some data may be lost");
                break;
            }
        }
    }
    
    log_event("[STORAGE] Drained %zu additional measurements from buffer", drained_count);
    
    // PHASE 3: Final flush of remaining batch
    if(batch_count > 0){
        log_event("[STORAGE] Flushing final batch of %zu measurements", batch_count);
        if(flush_batch(&db, batch, &batch_count, &total_inserted, &total_failed) < 0){
            log_event("[STORAGE] Final flush failed");
        }
    }
    
    // Cleanup
    free(batch);
    
    if(db){
        // Force synchronization before closing
        sqlite3_exec(db, "PRAGMA wal_checkpoint(FULL);", NULL, NULL, NULL);
        sqlite3_close(db);
        log_event("[SQL] Database connection closed");
    }
    
    log_event("[STORAGE] Storage manager thread exiting. Stats: %zu inserted, %zu failed", total_inserted, total_failed);
    
    return NULL;
}