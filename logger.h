#ifndef LOGGER_H
#define LOGGER_H

#define _GNU_SOURCE 200809L

#include "main.h"

extern pthread_mutex_t log_mutex;
extern const char *fifo_path;
extern volatile sig_atomic_t stop_flag;

void log_event(const char *fmt, ...);
void run_logger_process();

#endif