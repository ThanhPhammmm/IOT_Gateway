#ifndef CLIENT_THREAD_H
#define CLIENT_THREAD_H

#include "main.h"

#define READ_BUFFER_SIZE 1024
#define RECV_BUFFER_SIZE 512

typedef struct {
    int id;
    int type;
    double value;
} stat_update_t;

typedef struct{
    uint8_t client_fd;
    struct sockaddr_in addr;
} client_info_t;

extern sensor_stat_t *stats_head;
extern volatile sig_atomic_t stop_flag;
extern pthread_mutex_t stats_mutex;
extern sbuffer_t sbuffer;
extern volatile sig_atomic_t active_clients;

void *client_thread_func(void *arg);
//void update_running_avg(int id, int type, double val, double *out_avg);
void stats_free_all();
void update_running_avg_batch(stat_update_t *updates, size_t count, double *out_avgs);

#endif