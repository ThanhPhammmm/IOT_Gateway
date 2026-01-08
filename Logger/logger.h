#ifndef LOGGER_H
#define LOGGER_H

#define _GNU_SOURCE 200809L

#include "main.h"

#define MAX_LOG_SIZE   (5 * 1024 * 1024)
#define FLUSH_INTERVAL 50

extern pthread_mutex_t log_mutex;
extern const char *fifo_path;
extern volatile sig_atomic_t stop_flag;

void log_event(const char *fmt, ...);
void run_logger_process();
void close_logger_process(void);

#endif