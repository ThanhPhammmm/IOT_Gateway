#include "client_thread.h"
#include "sbuffer.h"
#include "logger.h"

void *client_thread_func(void *arg){
    client_info_t *ci = (client_info_t*)arg;
    int fd = ci->client_fd;

    // read lines: "<id> <type> <value>\n"
    char buf[512];
    ssize_t n;
    int first_id = -1;
    char readbuf[1024];
    size_t readbuf_len = 0;

    while(!stop_flag){
        n = read(fd, buf, sizeof(buf)-1);
        if(n <= 0){
            break;
        }
        // append to readbuf
        if(readbuf_len + n >= sizeof(readbuf)-1){
            readbuf_len = 0; // overflow -> reset
        }
        memcpy(readbuf + readbuf_len, buf, n);
        readbuf_len += n;
        readbuf[readbuf_len] = '\0';

        // process lines
        char *line_start = readbuf;
        char *nl;
        while((nl = memchr(line_start, '\n', (readbuf + readbuf_len) - line_start))){
            *nl = '\0';
            int sensor_id, sensor_type;
            double sensor_value;
            int parsed = sscanf(line_start, "%d %d %lf", &sensor_id, &sensor_type, &sensor_value);
            if(parsed == 3){
                if(first_id == -1){
                    first_id = sensor_id;
                    log_event("A sensor node with ID:%d from %s:%d has opened a new connection", first_id, inet_ntoa(ci->addr.sin_addr), ntohs(ci->addr.sin_port));
                }
                sensor_packet_t pkt;
                pkt.id = sensor_id;
                pkt.type = sensor_type;
                pkt.value = sensor_value;
                pkt.ts = time(NULL);
                
                log_event("Received data with ID:%d type=%d val=%.2f from %s:%d", sensor_id, sensor_type, sensor_value, inet_ntoa(ci->addr.sin_addr), ntohs(ci->addr.sin_port));

                sbuffer_insert(&sbuffer, &pkt);
            } 
            else{
                log_event("Received sensor data with invalid sensor node ID or format");
            }
            line_start = nl + 1;
        }
        // move leftover to beginning
        size_t leftover = (readbuf + readbuf_len) - line_start;
        memmove(readbuf, line_start, leftover);
        readbuf_len = leftover;
    }
    if(first_id != -1){
        log_event("The sensor node with ID:%d from %s:%d has closed the connection", first_id, inet_ntoa(ci->addr.sin_addr), ntohs(ci->addr.sin_port));
    } 
    else{
        log_event("A sensor node (unknown ID) has closed the connection");
    }

    free(ci);
    close(fd);
    return NULL;
}

void update_running_avg(int id, int type, double val, double *out_avg){
    pthread_mutex_lock(&stats_mutex);
    sensor_stat_t *cur = stats_head;
    while(cur){
        if(cur->id == id && cur->type == type)
            break;
        cur = cur->next;
    }
    if(!cur){
        cur = calloc(1, sizeof(*cur));
        if(!cur){
            pthread_mutex_unlock(&stats_mutex);
            fprintf(stderr, "Memory allocation failed in update_running_avg\n");
            return;
        }
        cur->id = id;
        cur->type = type;
        cur->avg = 0.0;
        cur->count = 0;
        cur->next = stats_head;
        stats_head = cur;
    }
    cur->avg = (cur->avg * (double)cur->count + val) / (double)(cur->count + 1);
    cur->count++;
    if(out_avg) *out_avg = cur->avg;
    pthread_mutex_unlock(&stats_mutex);
}

void stats_free_all(){
    sensor_stat_t *n = stats_head;
    while(n){
        sensor_stat_t *nx = n->next;
        free(n);
        n = nx;
    }
    stats_head = NULL;
}