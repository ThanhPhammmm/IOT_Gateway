#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "main.h"

extern volatile sig_atomic_t stop_flag;

#define LOCAL_BUFFER_SIZE 1500
#define POLL_DELAY_MS 100

// Sensor thresholds
#define TEMP_HOT 25.5
#define TEMP_COLD 15.0
#define HUMID_HIGH 80.0
#define HUMID_LOW 30.0
#define LIGHT_BRIGHT 800.0
#define LIGHT_DIM 200.0

void *data_manager_thread(void *arg);

#endif