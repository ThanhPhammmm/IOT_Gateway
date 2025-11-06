#include "cloud_manager.h"
#include "logger.h"
#include "sbuffer.h"

cloud_client_t *find_client_by_id(int id){
    for(size_t i = 0; i < NUM_CLIENTS; i++)
        if(clients[i].id == id){
            return &clients[i];
        }
    return NULL;
}

void *cloud_manager_thread(void *arg){
    (void)arg;
    printf("Cloud uploader thread started (multi-client persistent mode)\n");
    cloud_clients_init();

    while(!stop_flag){
        pthread_mutex_lock(&stats_mutex);
        sensor_stat_t *cur = stats_head;

        while(cur){
            cloud_client_t *cli = find_client_by_id(cur->id);
            if(!cli->token || !cli->connected){
                log_event("[Cloud] No token for sensor ID %d, skipping", cur->id);
                cur = cur->next;
                continue;
            }
            if(!cli->connected){
                printf("[Cloud] Sensor %d not connected yet, skip\n", cur->id);
                cur = cur->next;
                continue;
            }
            char payload[256];
            snprintf(payload, sizeof(payload), "{\"sensor_id\": %d, \"type\": %d, \"avg\": %.2f, \"count\": %lu, \"timestamp\": %ld}", cur->id, cur->type, cur->avg, cur->count, time(NULL));

            int rc = mosquitto_publish(cli->mosq, NULL, "v1/devices/me/telemetry", strlen(payload), payload, 1, false);
            if(rc == MOSQ_ERR_SUCCESS)
                log_event("[Cloud] Uploaded sensor %d (type=%d, avg=%.2f)", cur->id, cur->type, cur->avg);
            else
                log_event("[Cloud] Publish failed for sensor %d: %s",
                          cur->id, mosquitto_strerror(rc));

            for(int i = 0; i < 5; ++i){
                mosquitto_loop(mosq, 100, 1);
                usleep(100000);
            }
            cur = cur->next;
        }

        pthread_mutex_unlock(&stats_mutex);
        sleep(5);
    }

    cloud_clients_cleanup();
    printf("Cloud uploader thread exiting\n");
    return NULL;
}