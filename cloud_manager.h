#ifndef CLOUD_MANAGER_H
#define CLOUD_MANAGER_H

#include "main.h"
#define NUM_CLIENTS 3
#define MQTT_BROKER "demo.thingsboard.io"
#define MQTT_PORT 1883
#define MQTT_KEEPALIVE 60

extern volatile sig_atomic_t stop_flag;
extern pthread_mutex_t stats_mutex;
extern sbuffer_t sbuffer;
extern sensor_stat_t *stats_head;

extern struct mosquitto *mosq;
extern cloud_client_t clients[];

void cloud_clients_init(void);
void cloud_clients_cleanup(void);
void *cloud_manager_thread(void *arg);

#endif