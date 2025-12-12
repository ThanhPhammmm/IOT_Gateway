#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "main.h"

extern volatile sig_atomic_t stop_flag;
extern sbuffer_t sbuffer;

void *storage_manager_thread(void *arg);

#endif