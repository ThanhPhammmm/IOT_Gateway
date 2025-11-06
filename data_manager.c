#include "sbuffer.h"
#include "data_manager.h"
#include "logger.h"
#include "client_thread.h"

void *data_manager_thread(void *arg){
    log_event("[DATA] Data manager thread started\n");
    (void)arg;
    while(!stop_flag){
        // copy all nodes not yet processed_by_data
        sensor_packet_t local_buf[1500];
        size_t local_count = 0;

        sbuffer_node_t *cur;
        while((cur = sbuffer_find_for_data(&sbuffer)) != NULL){
            // copy
            if(local_count < sizeof(local_buf)/sizeof(local_buf[0])){
                local_buf[local_count++] = cur->pkt;
            }
            else{
                break;
            }
            sbuffer_mark_data_done(&sbuffer, cur);
        }
        // process local_buf
        for(size_t i = 0; i < local_count; ++i){
            sensor_packet_t *p = &local_buf[i];
            double avg;

            // Update average value for this sensor
            update_running_avg(p->id, p->type, p->value, &avg);

            switch(p->type){
                case SENSOR_TEMPERATURE:
                    if(avg >= 25.5){
                        log_event("[TEMP] Sensor %d reports it's too hot (avg = %.2f°C)", p->id, avg);
                    } 
                    else if(avg < 15.0){
                        log_event("[TEMP] Sensor %d reports it's too cold (avg = %.2f°C)", p->id, avg);
                    } 
                    else{
                        log_event("[TEMP] Sensor %d temperature normal (avg = %.2f°C)", p->id, avg);
                    }
                    break;
                case SENSOR_HUMIDITY:
                    if(avg >= 80.0){
                        log_event("[HUMID] Sensor %d reports high humidity (avg = %.2f%%)", p->id, avg);
                    } 
                    else if(avg < 30.0){
                        log_event("[HUMID] Sensor %d reports low humidity (avg = %.2f%%)", p->id, avg);
                    } 
                    else{
                        log_event("[HUMID] Sensor %d humidity normal (avg = %.2f%%)", p->id, avg);
                    }
                    break;
                case SENSOR_LIGHT:
                    if(avg >= 800.0){
                        log_event("[LIGHT] Sensor %d reports bright light (avg = %.2f lux)", p->id, avg);
                    } 
                    else if(avg < 200.0){
                        log_event("[LIGHT] Sensor %d reports low light (avg = %.2f lux)", p->id, avg);
                    } 
                    else{
                        log_event("[LIGHT] Sensor %d light normal (avg = %.2f lux)", p->id, avg);
                    }
                    break;

                default:
                    log_event("[UNKNOWN] Sensor %d has unknown type %d (avg = %.2f)", p->id, p->type, avg);
                    break;
            }
        }

        // small sleep to avoid busy-wait when no new data
        usleep(100 * 1000);
    }
    log_event("[DATA] Data manager thread exiting\n");
    return NULL;
}