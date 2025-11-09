#include "client_thread.h"
#include "sbuffer.h"
#include "logger.h"

#define READ_BUFFER_SIZE 1024
#define RECV_BUFFER_SIZE 512

void *client_thread_func(void *arg){
    client_info_t *client_info = (client_info_t*)arg;
    int client_fd = client_info->client_fd;
    char *client_ip = inet_ntoa(client_info->addr.sin_addr);
    int client_port = ntohs(client_info->addr.sin_port);
    
    int first_sensor_id = -1;
    char read_buffer[READ_BUFFER_SIZE];
    size_t buffer_len = 0;
    size_t packets_received = 0;
    
    // Main read loop
    while(!stop_flag){
        char recv_buf[RECV_BUFFER_SIZE];
        ssize_t bytes_read = read(client_fd, recv_buf, sizeof(recv_buf) - 1);
        
        if(bytes_read <= 0){
            // Connection closed or error
            break;
        }
        
        // Check buffer overflow
        if(buffer_len + bytes_read >= sizeof(read_buffer)){
            log_event("[CLIENT] Buffer overflow for client %s:%d, resetting buffer", client_ip, client_port);
            buffer_len = 0;
        }
        
        // Append to read buffer
        memcpy(read_buffer + buffer_len, recv_buf, bytes_read);
        buffer_len += bytes_read;
        read_buffer[buffer_len] = '\0';
        
        // Process complete lines
        char *line_start = read_buffer;
        char *newline;
        
        while((newline = memchr(line_start, '\n', (read_buffer + buffer_len) - line_start))){
            *newline = '\0';
            
            // Parse sensor data: "id type value"
            int sensor_id, sensor_type;
            double sensor_value;
            int parsed = sscanf(line_start, "%d %d %lf", &sensor_id, &sensor_type, &sensor_value);
            
            if(parsed == 3){
                // Log first connection
                if(first_sensor_id == -1){
                    first_sensor_id = sensor_id;
                    log_event("[CLIENT] Sensor node ID %d from %s:%d opened new connection", first_sensor_id, client_ip, client_port);
                }
                
                // Create packet
                sensor_packet_t packet = {
                    .id = sensor_id,
                    .type = sensor_type,
                    .value = sensor_value,
                    .ts = time(NULL)
                };
                
                // Insert into shared buffer
                sbuffer_insert(&sbuffer, &packet);
                packets_received++;
                
                log_event("[CLIENT] Received data ID %d type %d value %.2f from %s:%d", sensor_id, sensor_type, sensor_value, client_ip, client_port);
            } 
            else{
                log_event("[CLIENT] Invalid data format from %s:%d: '%s'", client_ip, client_port, line_start);
            }
            
            line_start = newline + 1;
        }
        
        // Move leftover data to beginning
        size_t leftover = (read_buffer + buffer_len) - line_start;
        if(leftover > 0){
            memmove(read_buffer, line_start, leftover);
        }
        buffer_len = leftover;
    }
    
    // Log disconnection
    if(first_sensor_id != -1){
        log_event("[CLIENT] Sensor node ID %d from %s:%d closed connection (%zu packets received)", first_sensor_id, client_ip, client_port, packets_received);
    } 
    else{
        log_event("[CLIENT] Unknown sensor from %s:%d closed connection (no valid data)", client_ip, client_port);
    }
    
    // Cleanup
    close(client_fd);
    free(client_info);
    
    return NULL;
}

void update_running_avg(int id, int type, double val, double *out_avg){
    pthread_mutex_lock(&stats_mutex);
    
    // Find existing stat entry
    sensor_stat_t *stat = stats_head;
    while(stat){
        if(stat->id == id && stat->type == type){
            break;
        }
        stat = stat->next;
    }
    
    // Create new entry if not found
    if(!stat){
        stat = malloc(sizeof(sensor_stat_t));
        if(!stat){
            log_event("[STATS] Memory allocation failed for sensor %d type %d", id, type);
            pthread_mutex_unlock(&stats_mutex);
            return;
        }
        
        stat->id = id;
        stat->type = type;
        stat->avg = 0.0;
        stat->count = 0;
        stat->next = stats_head;
        stats_head = stat;
    }
    
    // Update running average
    stat->avg = (stat->avg * stat->count + val) / (stat->count + 1);
    stat->count++;
    
    if(out_avg){
        *out_avg = stat->avg;
    }
    
    pthread_mutex_unlock(&stats_mutex);
}

void stats_free_all(void){
    pthread_mutex_lock(&stats_mutex);
    
    sensor_stat_t *stat = stats_head;
    size_t freed_count = 0;
    
    while(stat){
        sensor_stat_t *next = stat->next;
        free(stat);
        stat = next;
        freed_count++;
    }
    
    stats_head = NULL;
    
    pthread_mutex_unlock(&stats_mutex);
    
    log_event("[STATS] Freed %zu sensor statistics entries", freed_count);
}