#ifndef CLIENT_THREAD_H
#define CLIENT_THREAD_H

#include "main.h"

extern sensor_stat_t *stats_head;
extern volatile sig_atomic_t stop_flag;
extern pthread_mutex_t stats_mutex;
extern sbuffer_t sbuffer;

void *client_thread_func(void *arg);
void update_running_avg(int id, int type, double val, double *out_avg);
void stats_free_all();
#endif