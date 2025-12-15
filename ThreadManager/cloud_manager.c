#include "cloud_manager.h"
#include "logger.h"
#include "sbuffer.h"

#define MQTT_TOPIC "v1/devices/me/telemetry"
#define MQTT_QOS 1
#define UPLOAD_INTERVAL_SEC 5
#define LOOP_ITERATIONS 5
#define LOOP_DELAY_MS 100

// Helper: Find client by sensor ID
cloud_client_t *find_client_by_id(int id){
    for(size_t i = 0; i < NUM_CLIENTS; i++){
        if(clients[i].id == id){
            return &clients[i];
        }
    }
    return NULL;
}

// Helper: Upload sensor data to cloud
static int upload_sensor_data(cloud_client_t *client, sensor_stat_t *stat){
    if(!client || !stat) return -1;
    
    // Build JSON payload
    char payload[256];
    int len = snprintf(payload, sizeof(payload), 
        "{\"sensor_id\":%d,\"type\":%d,\"avg\":%.2f,\"count\":%lu,\"timestamp\":%ld,\"last_upload\":%ld}", 
        stat->id, stat->type, stat->avg, stat->count, time(NULL), stat->last_uploaded);

    if(len < 0 || len >= (int)sizeof(payload)){
        log_event("[CLOUD] Payload too large for sensor %d", stat->id);
        return -1;
    }
    
    // Publish to MQTT broker
    int rc = mosquitto_publish(client->mosq, NULL, MQTT_TOPIC, len, payload, MQTT_QOS, false);
    
    if(rc == MOSQ_ERR_SUCCESS){
        log_event("[CLOUD] Uploaded sensor %d (type=%d, avg=%.2f, count=%lu)", stat->id, stat->type, stat->avg, stat->count);
        return 0;
    } 
    else{
        log_event("[CLOUD] Publish failed for sensor %d: %s", stat->id, mosquitto_strerror(rc));
        return -1;
    }
}

// // Helper: Process network loop for client
// static void process_mqtt_loop(cloud_client_t *client){
//     if(!client || !client->mosq) return;
    
//     for(int i = 0; i < LOOP_ITERATIONS; i++){
//         mosquitto_loop(client->mosq, LOOP_DELAY_MS, 1);
//         usleep(LOOP_DELAY_MS * 1000);
//     }
// }

// // Helper: Mark all cloud-ready buffer nodes as done
// static void mark_cloud_buffer_done(void){
//     sbuffer_node_t *node;
//     while((node = sbuffer_find_for_cloud(&sbuffer)) != NULL){
//         sbuffer_mark_upcloud_done(&sbuffer, node);
//     }
// }

// Helper: Check if sensor has new data since last upload
static int has_new_data(sensor_stat_t *sensor){
    // No data yet
    if(sensor->count == 0){
        return 0;
    }
    
    // Never uploaded before - upload now
    if(sensor->last_uploaded == 0){
        return 1;
    }
    
    // Has count increased since last upload?
    if(sensor->count > sensor->last_uploaded_count){
        // Check if enough time has passed
        time_t now = time(NULL);
        if((now - sensor->last_uploaded) >= UPLOAD_INTERVAL_SEC){
            return 1;  // New data + enough time passed
        }
    }
    
    return 0;  // No new data or too soon
}

void *cloud_manager_thread(void *arg){
    (void)arg;
    
    log_event("[CLOUD] Cloud uploader thread started");
    
    cloud_clients_init();
    
    size_t total_uploaded = 0;
    size_t total_failed = 0;
    size_t upload_cycles = 0;

    // Main upload loop
    while(!stop_flag){
        upload_cycles++;

        pthread_mutex_lock(&stats_mutex);
        
        // Count sensors numbers
        size_t sensor_count = 0;
        sensor_stat_t *stat = stats_head;
        while(stat){
            sensor_count++;
            stat = stat->next;
        }
        
        // Handle empty stats
        if(sensor_count == 0){
            pthread_mutex_unlock(&stats_mutex);
            log_event("[CLOUD] No sensors registered yet (cycle %zu)", upload_cycles);
            sleep(UPLOAD_INTERVAL_SEC);
            continue;
        }

        // Allocate local buffer
        sensor_stat_t *local_stats = malloc(sensor_count * sizeof(sensor_stat_t));
        if(!local_stats){
            pthread_mutex_unlock(&stats_mutex);
            log_event("[CLOUD] Failed to allocate local buffer");
            sleep(UPLOAD_INTERVAL_SEC);
            continue;
        }
        
        // Copy data
        stat = stats_head;
        size_t idx = 0;
        while(stat && idx < sensor_count){
            local_stats[idx] = *stat;  // Copy struct
            stat = stat->next;
            idx++;
        }
        
        // Unlock
        pthread_mutex_unlock(&stats_mutex);
        
        // Upload from local buffer
        size_t batch_uploaded = 0;
        size_t batch_failed = 0;
        size_t batch_skipped = 0;
        
        time_t now = time(NULL);
        
        for(size_t i = 0; i < sensor_count; i++){
            // Check if this sensor has new data
            if(!has_new_data(&local_stats[i])){
                batch_skipped++;
                continue;
            }
            
            // Find and validate client
            cloud_client_t *client = find_client_by_id(local_stats[i].id);
            
            if(!client){
                log_event("[CLOUD] No client found for sensor ID %d", local_stats[i].id);
                batch_failed++;
                continue;
            }
            
            if(!client->token){
                log_event("[CLOUD] No token for sensor ID %d", local_stats[i].id);
                batch_failed++;
                continue;
            }
            
            if(!client->connected){
                log_event("[CLOUD] Sensor %d not connected yet", local_stats[i].id);
                batch_failed++;
                continue;
            }
            
            // Attempt upload
            if(upload_sensor_data(client, &local_stats[i]) == 0){
                // Update both timestamp AND count
                pthread_mutex_lock(&stats_mutex);
                
                sensor_stat_t *stat = stats_head;
                while(stat){
                    if(stat->id == local_stats[i].id && stat->type == local_stats[i].type){
                        stat->last_uploaded = now;
                        stat->last_uploaded_count = stat->count; 
                        break;
                    }
                    stat = stat->next;
                }
                
                pthread_mutex_unlock(&stats_mutex);
                
                batch_uploaded++;

                //printf("Hi from Cloud Manager\n");
            } 
            else{
                batch_failed++;
            }
        }
        
        // Cleanup local buffer
        free(local_stats);
        
        // Update statistics
        total_uploaded += batch_uploaded;
        total_failed += batch_failed;
        
        if(batch_uploaded > 0 || batch_failed > 0){
            log_event("[CLOUD] Cycle %zu complete: %zu uploaded, %zu failed, %zu skipped", upload_cycles, batch_uploaded, batch_failed, batch_skipped);
        } 
        else if(batch_skipped > 0){
            log_event("[CLOUD] Cycle %zu: All %zu sensors skipped (no new data or too soon)", upload_cycles, batch_skipped);
        }

            // Wait before next upload cycle
            sleep(UPLOAD_INTERVAL_SEC);
    }
    
    // Cleanup
    cloud_clients_cleanup();
    
    log_event("[CLOUD] Cloud uploader thread exiting. Total: %zu uploaded, %zu failed", total_uploaded, total_failed);
    
    return NULL;
}