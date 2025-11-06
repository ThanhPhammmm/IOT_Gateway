#include "storage_manager.h"
#include "sbuffer.h"
#include "logger.h"
#include "database.h"

void *storage_manager_thread(void *arg){
    log_event("[STORAGE] Storage manager thread started\n");
    (void)arg;
    sqlite3 *db = NULL;
    int tries = 0;
    while(tries < 3){
        int rc = db_init_and_open(&db);
        if(rc == SQLITE_OK){
            log_event("Connection to SQL server established.");
            break;
        }
        else{
            tries++;
            log_event("Unable to connect to SQL server (attempt %d)", tries);
            sleep(1);
        }
    }
    if(!db){
        log_event("Unable to connect to SQL server. Exiting gateway.");
        exit(EXIT_FAILURE);
    }

    while(!stop_flag){
        sbuffer_node_t *node = sbuffer_find_for_storage(&sbuffer);
        if(!node) break;
        sensor_packet_t pkt = node->pkt;
        int rc = db_insert_measure(db, &pkt);
        if(rc == SQLITE_OK){
            sbuffer_mark_storage_done(&sbuffer, node);
        }
        else{
            log_event("Connection to SQL server lost.");
            sqlite3_close(db);
            db = NULL;
            int tries2 = 0;
            while(tries2 < 3 && !stop_flag){
                int rc2 = db_init_and_open(&db);
                if(rc2 == SQLITE_OK){
                    log_event("Connection to SQL server established.");
                    break;
                }
                tries2++;
                log_event("Unable to connect to SQL server (reconnect attempt %d)", tries2);
                sleep(1);
            }
            if(!db){
                log_event("Unable to connect to SQL server after retries. Exiting gateway.");
                exit(EXIT_FAILURE);
            }
            else{
                rc = db_insert_measure(db, &pkt);
                if(rc == SQLITE_OK){
                    sbuffer_mark_storage_done(&sbuffer, node);
                }
                else{
                    log_event("DB insert failed after reconnect. Skipping measurement id=%d", pkt.id);
                    sbuffer_mark_storage_done(&sbuffer, node);
                }
            }
        }
    }

    if(db){
        sqlite3_close(db);
    }
    log_event("[STORAGE] Storage manager thread exsisting\n");
    return NULL;
}