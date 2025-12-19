#include "sbuffer.h"
#include "data_manager.h"
#include "logger.h"
#include "client_thread.h"

// Helper: process temperature sensor
static void process_temperature(int sensor_id, double avg){
    if(avg >= TEMP_HOT){
        log_event("[TEMP] Sensor %d reports it's too hot (avg = %.2f°C)", sensor_id, avg);
    } 
    else if (avg < TEMP_COLD){
        log_event("[TEMP] Sensor %d reports it's too cold (avg = %.2f°C)", sensor_id, avg);
    } 
    else{
        log_event("[TEMP] Sensor %d temperature normal (avg = %.2f°C)", sensor_id, avg);
    }
}

// Helper: process humidity sensor
static void process_humidity(int sensor_id, double avg){
    if(avg >= HUMID_HIGH){
        log_event("[HUMID] Sensor %d reports high humidity (avg = %.2f%%)", sensor_id, avg);
    } 
    else if(avg < HUMID_LOW){
        log_event("[HUMID] Sensor %d reports low humidity (avg = %.2f%%)", sensor_id, avg);
    } 
    else{
        log_event("[HUMID] Sensor %d humidity normal (avg = %.2f%%)", sensor_id, avg);
    }
}

// Helper: process light sensor
static void process_light(int sensor_id, double avg){
    if(avg >= LIGHT_BRIGHT){
        log_event("[LIGHT] Sensor %d reports bright light (avg = %.2f lux)", sensor_id, avg);
    } 
    else if(avg < LIGHT_DIM){
        log_event("[LIGHT] Sensor %d reports low light (avg = %.2f lux)", sensor_id, avg);
    } 
    else{
        log_event("[LIGHT] Sensor %d light normal (avg = %.2f lux)", sensor_id, avg);
    }
}

// // Helper: process single sensor packet
// static void process_sensor_packet(sensor_packet_t *pkt){
//     double avg;
    
//     // Update running average for this sensor
//     update_running_avg(pkt->id, pkt->type, pkt->value, &avg);
    
//     // Process based on sensor type
//     switch(pkt->type){
//         case SENSOR_TEMPERATURE:
//             process_temperature(pkt->id, avg);
//             break;
            
//         case SENSOR_HUMIDITY:
//             process_humidity(pkt->id, avg);
//             break;
            
//         case SENSOR_LIGHT:
//             process_light(pkt->id, avg);
//             break;
            
//         default:
//             log_event("[UNKNOWN] Sensor %d has unknown type %d (avg = %.2f)", pkt->id, pkt->type, avg);
//             break;
//     }
// }

void *data_manager_thread(void *arg){
    (void)arg;
    
    log_event("[DATA] Data manager thread started");
    
    sensor_packet_t local_buf[LOCAL_BUFFER_SIZE];
    stat_update_t stat_updates[LOCAL_BUFFER_SIZE];  // Batch buffer
    double stat_avgs[LOCAL_BUFFER_SIZE];            // Output buffer

    size_t total_processed = 0;
    size_t local_count = 0;

    while(!stop_flag){
        
        // Collect all unprocessed packets into local buffer
        sbuffer_node_t *node;
        while((node = sbuffer_find_for_data(&sbuffer)) != NULL){
            if(local_count < LOCAL_BUFFER_SIZE){
                local_buf[local_count] = node->pkt;
                
                // Prepare batch update
                stat_updates[local_count].id = node->pkt.id;
                stat_updates[local_count].type = node->pkt.type;
                stat_updates[local_count].value = node->pkt.value;
                
                local_count++;            
            } 
            else{
                // Buffer full, will process this node in next iteration
                break;
            }
            sbuffer_mark_data_done(&sbuffer, node);
        }
        
        // Process all collected packets
        if(local_count > 0){
            // Single lock for entire batch
            update_running_avg_batch(stat_updates, local_count, stat_avgs);
            
            // Process with updated averages
            for(size_t i = 0; i < local_count; i++){
                double avg = stat_avgs[i];
                
                switch(local_buf[i].type){
                    case SENSOR_TEMPERATURE:
                        process_temperature(local_buf[i].id, avg);
                        break;
                        
                    case SENSOR_HUMIDITY:
                        process_humidity(local_buf[i].id, avg);
                        break;
                        
                    case SENSOR_LIGHT:
                        process_light(local_buf[i].id, avg);
                        break;
                        
                    default:
                        log_event("[UNKNOWN] Sensor %d has unknown type %d (avg = %.2f)", local_buf[i].id, local_buf[i].type, avg);
                        break;
                }
            }
            total_processed += local_count;
            local_count = 0;
        } 
        else{
            usleep(POLL_DELAY_MS * 1000);
        }
    }
    
    log_event("[DATA] Data manager thread exiting. Total processed: %zu measurements", total_processed);
    
    return NULL;
}