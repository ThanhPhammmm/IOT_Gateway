#include "cloud_manager.h"

// Callback: When connect
static void on_connect(struct mosquitto *mosq, void *userdata, int rc){
    cloud_client_t *c = userdata;
    if(rc == 0){
        c->connected = 1;
        printf("[MQTT] Sensor %d connected to ThingsBoard broker\n", c->id);
    } 
    else{
        printf("[MQTT] Sensor %d connection failed: %s\n", c->id, mosquitto_connack_string(rc));
    }
}

// Callback: When disconnect
static void on_disconnect(struct mosquitto *mosq, void *userdata, int rc){
    cloud_client_t *c = userdata;
    c->connected = 0;
    printf("[MQTT] Sensor %d disconnected\n", c->id);
}

void cloud_clients_init(void){
    mosquitto_lib_init();
    for(size_t i = 0; i < NUM_CLIENTS; i++){
        clients[i].mosq = mosquitto_new(NULL, true, &clients[i]);
        if(!clients[i].mosq){
            printf("[MQTT] Failed to create client for sensor %d\n", clients[i].id);
            continue;
        }
        mosquitto_connect_callback_set(clients[i].mosq, on_connect);
        mosquitto_disconnect_callback_set(clients[i].mosq, on_disconnect);
        mosquitto_username_pw_set(clients[i].mosq, clients[i].token, NULL);
        int rc = mosquitto_connect(clients[i].mosq, "demo.thingsboard.io", 1883, 60);
        if(rc != MOSQ_ERR_SUCCESS){
            printf("[MQTT] Sensor %d connect failed: %s\n", clients[i].id, mosquitto_strerror(rc));
            continue;
        }        
        mosquitto_loop_start(clients[i].mosq);
    }
}


void cloud_clients_cleanup(void){
    for(size_t i = 0; i < NUM_CLIENTS; i++){
        if(clients[i].mosq){
            mosquitto_disconnect(clients[i].mosq);
            mosquitto_loop_stop(clients[i].mosq, true);
            mosquitto_destroy(clients[i].mosq);
        }
    }
    mosquitto_lib_cleanup();
}
