#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include "main.h"

#define LISTEN_BACKLOG 16
#define SELECT_TIMEOUT_SEC 1
#define MAX_CONCURRENT_CLIENTS 50 

void *connection_manager_thread(void *arg);

#endif
