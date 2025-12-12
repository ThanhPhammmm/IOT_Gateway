#ifndef UTILITIES_H
#define UTILITIES_H

#include "main.h"

extern pthread_mutex_t log_mutex;
extern const char *fifo_path;
extern sbuffer_t sbuffer;
extern volatile sig_atomic_t stop_flag;

void sigint_handler(int sig);
void ensure_fifo_exists(void);

#endif