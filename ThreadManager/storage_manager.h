#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "main.h"

#define MAX_RECONNECT_ATTEMPTS 3
#define RECONNECT_DELAY_SEC 1
#define BATCH_SIZE 100
#define POLL_DELAY_MS 100

extern volatile sig_atomic_t stop_flag;
extern sbuffer_t sbuffer;

void *storage_manager_thread(void *arg);

#endif