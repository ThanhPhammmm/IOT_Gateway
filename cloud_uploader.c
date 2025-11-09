#include "cloud_manager.h"
#include "logger.h"

// Callback: When connect
static void on_connect(struct mosquitto *mosq, void *userdata, int rc){
    (void)mosq;
    cloud_client_t *c = (cloud_client_t*)userdata;
    
    if(rc == 0){
        c->connected = 1;
        log_event("[MQTT] Sensor %d connected to ThingsBoard broker", c->id);
    } 
    else{
        log_event("[MQTT] Sensor %d connection failed: %s", c->id, mosquitto_connack_string(rc));
    }
}

// Callback: When disconnect
static void on_disconnect(struct mosquitto *mosq, void *userdata, int rc){
    (void)mosq;
    (void)rc;
    cloud_client_t *c = (cloud_client_t*)userdata;
    
    c->connected = 0;
    log_event("[MQTT] Sensor %d disconnected", c->id);
}

// Callback: When publish complete
static void on_publish(struct mosquitto *mosq, void *userdata, int mid){
    (void)mosq;
    (void)mid;
    cloud_client_t *c = (cloud_client_t*)userdata;
    log_event("[MQTT] Sensor %d message published successfully (mid=%d)", c->id, mid);
}

void cloud_clients_init(void){
    int rc = mosquitto_lib_init();
    if(rc != MOSQ_ERR_SUCCESS){
        log_event("[MQTT] Failed to initialize mosquitto library: %s", mosquitto_strerror(rc));
        return;
    }
    
    log_event("[MQTT] Initializing %d cloud clients", NUM_CLIENTS);
    
    size_t success_count = 0;
    
    for(size_t i = 0; i < NUM_CLIENTS; i++){
        // Create mosquitto client
        clients[i].mosq = mosquitto_new(NULL, true, &clients[i]);
        if(!clients[i].mosq){
            log_event("[MQTT] Failed to create client for sensor %d", clients[i].id);
            continue;
        }
        
        // Set callbacks
        mosquitto_connect_callback_set(clients[i].mosq, on_connect);
        mosquitto_disconnect_callback_set(clients[i].mosq, on_disconnect);
        mosquitto_publish_callback_set(clients[i].mosq, on_publish);
        
        // Set authentication
        rc = mosquitto_username_pw_set(clients[i].mosq, clients[i].token, NULL);
        if(rc != MOSQ_ERR_SUCCESS){
            log_event("[MQTT] Sensor %d failed to set credentials: %s", clients[i].id, mosquitto_strerror(rc));
            mosquitto_destroy(clients[i].mosq);
            clients[i].mosq = NULL;
            continue;
        }
        
        // Connect to broker
        rc = mosquitto_connect(clients[i].mosq, MQTT_BROKER, MQTT_PORT, MQTT_KEEPALIVE);
        if(rc != MOSQ_ERR_SUCCESS){
            log_event("[MQTT] Sensor %d connect failed: %s", clients[i].id, mosquitto_strerror(rc));
            mosquitto_destroy(clients[i].mosq);
            clients[i].mosq = NULL;
            continue;
        }
        
        // Start network loop
        rc = mosquitto_loop_start(clients[i].mosq);
        if(rc != MOSQ_ERR_SUCCESS){
            log_event("[MQTT] Sensor %d failed to start loop: %s", clients[i].id, mosquitto_strerror(rc));
            mosquitto_disconnect(clients[i].mosq);
            mosquitto_destroy(clients[i].mosq);
            clients[i].mosq = NULL;
            continue;
        }
        
        success_count++;
        log_event("[MQTT] Sensor %d initialization started", clients[i].id);
    }
    
    log_event("[MQTT] Cloud clients initialization complete: %zu/%d successful", success_count, NUM_CLIENTS);
}

void cloud_clients_cleanup(void){
    log_event("[MQTT] Cleaning up cloud clients");
    
    for(size_t i = 0; i < NUM_CLIENTS; i++){
        if(clients[i].mosq){
            // Disconnect gracefully
            mosquitto_disconnect(clients[i].mosq);
            
            // Stop loop (wait for completion)
            mosquitto_loop_stop(clients[i].mosq, true);
            
            // Destroy client
            mosquitto_destroy(clients[i].mosq);
            clients[i].mosq = NULL;
            clients[i].connected = 0;
            
            log_event("[MQTT] Sensor %d cleaned up", clients[i].id);
        }
    }
    
    // Cleanup library
    mosquitto_lib_cleanup();
    
    log_event("[MQTT] Cloud clients cleanup complete");
}