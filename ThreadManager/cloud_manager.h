#ifndef CLOUD_MANAGER_H
#define CLOUD_MANAGER_H

#include "main.h"

#define NUM_CLIENTS 3
#define MQTT_BROKER "demo.thingsboard.io"
#define MQTT_PORT 1883
#define MQTT_KEEPALIVE 60
#define MQTT_TOPIC "v1/devices/me/telemetry"
#define MQTT_QOS 1
#define UPLOAD_INTERVAL_SEC 5
#define LOOP_ITERATIONS 5
#define LOOP_DELAY_MS 100

typedef struct{
    uint8_t id;
    const char *token;
    struct mosquitto *mosq;
    uint8_t connected;
} cloud_client_t;

extern volatile sig_atomic_t stop_flag;
extern pthread_mutex_t stats_mutex;
extern sbuffer_t sbuffer;
extern sensor_stat_t *stats_head;
extern struct mosquitto *mosq;
extern cloud_client_t clients[];

cloud_client_t *find_client_by_id(int id);
void cloud_clients_init(void);
void cloud_clients_cleanup(void);
void *cloud_manager_thread(void *arg);

#endif