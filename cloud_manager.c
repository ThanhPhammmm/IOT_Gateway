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
    int len = snprintf(payload, sizeof(payload), "{\"sensor_id\":%d,\"type\":%d,\"avg\":%.2f,\"count\":%lu,\"timestamp\":%ld}", stat->id, stat->type, stat->avg, stat->count, time(NULL));
    
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

// Helper: Process network loop for client
static void process_mqtt_loop(cloud_client_t *client){
    if(!client || !client->mosq) return;
    
    for(int i = 0; i < LOOP_ITERATIONS; i++){
        mosquitto_loop(client->mosq, LOOP_DELAY_MS, 1);
        usleep(LOOP_DELAY_MS * 1000);
    }
}

// Helper: Mark all cloud-ready buffer nodes as done
static void mark_cloud_buffer_done(void){
    sbuffer_node_t *node;
    while((node = sbuffer_find_for_cloud(&sbuffer)) != NULL){
        sbuffer_mark_upcloud_done(&sbuffer, node);
    }
}

void *cloud_manager_thread(void *arg){
    (void)arg;
    
    log_event("[CLOUD] Cloud uploader thread started");
    
    // Initialize MQTT clients
    cloud_clients_init();
    
    size_t total_uploaded = 0;
    size_t total_failed = 0;
    
    // Main upload loop
    while(!stop_flag){
        pthread_mutex_lock(&stats_mutex);
        
        sensor_stat_t *stat = stats_head;
        size_t batch_uploaded = 0;
        size_t batch_failed = 0;
        
        while(stat){
            // Find corresponding cloud client
            cloud_client_t *client = find_client_by_id(stat->id);
            
            if(!client){
                log_event("[CLOUD] No client found for sensor ID %d", stat->id);
                stat = stat->next;
                continue;
            }
            
            if(!client->token){
                log_event("[CLOUD] No token for sensor ID %d, skipping", stat->id);
                stat = stat->next;
                continue;
            }
            
            if(!client->connected){
                log_event("[CLOUD] Sensor %d not connected yet, skipping", stat->id);
                stat = stat->next;
                continue;
            }
            
            // Upload sensor data
            if(upload_sensor_data(client, stat) == 0){
                batch_uploaded++;
            } 
            else{
                batch_failed++;
            }
            
            // Process MQTT network loop
            process_mqtt_loop(client);
            
            stat = stat->next;
        }
        
        // Mark buffer nodes as processed
        mark_cloud_buffer_done();
        
        pthread_mutex_unlock(&stats_mutex);
        
        // Update statistics
        total_uploaded += batch_uploaded;
        total_failed += batch_failed;
        
        if(batch_uploaded > 0 || batch_failed > 0){
            log_event("[CLOUD] Upload batch complete: %zu uploaded, %zu failed", batch_uploaded, batch_failed);
        }
        
        // Wait before next upload cycle
        sleep(UPLOAD_INTERVAL_SEC);
    }
    
    // Cleanup
    cloud_clients_cleanup();
    
    log_event("[CLOUD] Cloud uploader thread exiting. Total: %zu uploaded, %zu failed", total_uploaded, total_failed);
    
    return NULL;
}