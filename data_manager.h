#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "main.h"

extern volatile sig_atomic_t stop_flag;

void *data_manager_thread(void *arg);

#endif