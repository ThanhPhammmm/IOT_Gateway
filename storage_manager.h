#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "main.h"

extern volatile sig_atomic_t stop_flag;
extern sbuffer_t sbuffer;

static sqlite3* storage_connect_db(int max_attempts);
static int storage_batch_insert_with_retry(sqlite3 **db, sensor_packet_t *batch, size_t count);
void *storage_manager_thread(void *arg);

#endif