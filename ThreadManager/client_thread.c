#include "client_thread.h"
#include "sbuffer.h"
#include "logger.h"

void *client_thread_func(void *arg){
    client_info_t *client_info = (client_info_t*)arg;
    int client_fd = client_info->client_fd;

    // Set timeout
    struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
    if(setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
        log_event("[CLIENT] Failed to set timeout: %s", strerror(errno));
        close(client_fd);
        free(client_info);
        return NULL;
    }

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

        if(bytes_read < 0){
            // Error happens
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                // Timeout for 5s, client connected to server but does not send any data    
                log_event("[CLIENT] Connection timeout for %s:%d", client_ip, client_port);
            } 
            else{
                // (Network error, etc)
                log_event("[CLIENT] Read error for %s:%d: %s", client_ip, client_port, strerror(errno));
            }
            break;
        }
        else if(bytes_read == 0){
            // Client disconnected
            break;
        }
        
        // Check buffer overflow BEFORE appending
        if(buffer_len + bytes_read >= sizeof(read_buffer)){
            // Try to salvage by processing up to last complete line
            char *last_newline = memrchr(read_buffer, '\n', buffer_len);
            
            if(last_newline){
                // Process complete lines first
                *last_newline = '\0';
                
                char *line_start = read_buffer;
                char *newline;
                
                while((newline = strchr(line_start, '\n')) || 
                      (line_start < last_newline)){
                    if(newline){
                        *newline = '\0';
                    }
                    
                    // Parse sensor data
                    int sensor_id, sensor_type;
                    double sensor_value;
                    int parsed = sscanf(line_start, "%d %d %lf", &sensor_id, &sensor_type, &sensor_value);
                    
                    if(parsed == 3){
                        if(first_sensor_id == -1){
                            first_sensor_id = sensor_id;
                            log_event("[CLIENT] Sensor node ID %d from %s:%d opened new connection", first_sensor_id, client_ip, client_port);
                        }
                        
                        sensor_packet_t packet = {
                            .id = sensor_id,
                            .type = sensor_type,
                            .value = sensor_value,
                            .ts = time(NULL)
                        };
                        
                        sbuffer_insert(&sbuffer, &packet);
                        packets_received++;

                        log_event("[CLIENT] Received data ID %d type %d value %.2f from %s:%d", sensor_id, sensor_type, sensor_value, client_ip, client_port);
                    }
                    
                    if(newline){
                        line_start = newline + 1;
                    } 
                    else{
                        break;
                    }
                }
                
                // Keep remainder after last newline
                size_t remainder_len = buffer_len - (last_newline - read_buffer + 1);
                if(remainder_len > 0){
                    memmove(read_buffer, last_newline + 1, remainder_len);
                    buffer_len = remainder_len;
                } 
                else{
                    buffer_len = 0;
                }
            } 
            else{
                // No newline found - truly a line too long
                log_event("[CLIENT] Protocol violation: line exceeds buffer size from %s:%d", client_ip, client_port);
                buffer_len = 0;
            }
            
            // Check again after cleanup
            if(buffer_len + bytes_read >= sizeof(read_buffer)){
                log_event("[CLIENT] Still overflow after cleanup, dropping data from %s:%d", client_ip, client_port);
                continue;  // Skip this chunk
            }
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
            
            // Parse sensor data
            int sensor_id, sensor_type;
            double sensor_value;
            int parsed = sscanf(line_start, "%d %d %lf", &sensor_id, &sensor_type, &sensor_value);
            
            if(parsed == 3){
                if(first_sensor_id == -1){
                    first_sensor_id = sensor_id;
                    log_event("[CLIENT] Sensor node ID %d from %s:%d opened new connection", first_sensor_id, client_ip, client_port);
                }
                
                sensor_packet_t packet = {
                    .id = sensor_id,
                    .type = sensor_type,
                    .value = sensor_value,
                    .ts = time(NULL)
                };
                
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
    
    close(client_fd);
    free(client_info);

    // Decrement counter on exit
    __sync_fetch_and_sub(&active_clients, 1);

    return NULL;
}

// void update_running_avg(int id, int type, double val, double *out_avg){
//     pthread_mutex_lock(&stats_mutex);
    
//     // Find existing stat entry
//     sensor_stat_t *stat = stats_head;
//     while(stat){
//         if(stat->id == id && stat->type == type){
//             break;
//         }
//         stat = stat->next;
//     }
    
//     // Create new entry if not found
//     if(!stat){
//         stat = malloc(sizeof(sensor_stat_t));
//         if(!stat){
//             log_event("[STATS] Memory allocation failed for sensor %d type %d", id, type);
//             pthread_mutex_unlock(&stats_mutex);
//             return;
//         }
        
//         stat->id = id;
//         stat->type = type;
//         stat->avg = 0.0;
//         stat->count = 0;
//         stat->last_uploaded = 0; // for tracking uploading
//         stat->last_uploaded_count = 0;
//         stat->next = stats_head;
//         stats_head = stat;
//     }
    
//     // Update running average
//     stat->avg = (stat->avg * stat->count + val) / (stat->count + 1);
//     stat->count++;
    
//     if(out_avg){
//         *out_avg = stat->avg;
//     }
    
//     pthread_mutex_unlock(&stats_mutex);
// }

void update_running_avg_batch(stat_update_t *updates, size_t count, double *out_avgs){
    if(!updates || count == 0) return;
    
    pthread_mutex_lock(&stats_mutex);
    
    for(size_t i = 0; i < count; i++){
        // Find existing stat entry
        sensor_stat_t *stat = stats_head;
        while(stat){
            if(stat->id == updates[i].id && stat->type == updates[i].type){
                break;
            }
            stat = stat->next;
        }
        
        // Create new entry if not found
        if(!stat){
            stat = malloc(sizeof(sensor_stat_t));
            if(!stat){
                log_event("[STATS] Memory allocation failed for sensor %d type %d", updates[i].id, updates[i].type);
                if(out_avgs) out_avgs[i] = 0.0;
                continue;
            }
            
            stat->id = updates[i].id;
            stat->type = updates[i].type;
            stat->avg = 0.0;
            stat->count = 0;
            stat->last_uploaded = 0;
            stat->last_uploaded_count = 0;
            stat->next = stats_head;
            stats_head = stat;
        }
        
        // Update running average
        stat->avg = (stat->avg * stat->count + updates[i].value) / (stat->count + 1);
        stat->count++;
        
        if(out_avgs){
            out_avgs[i] = stat->avg;
        }
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