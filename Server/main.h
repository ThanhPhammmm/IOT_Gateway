#ifndef MAIN_H
#define MAIN_H

#define _GNU_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sqlite3.h>
#include <mosquitto.h>
#include <poll.h>
#include <sys/syscall.h>

#define FIFO_PATH "../Logger/logFifo"
#define LOG_FILE  "../Record/gateway.log"
#define DB_FILE   "../Database/sensors.db"
#define MAX_LINE 256

typedef struct{
    uint8_t id;
    uint8_t type;
    double value;
    time_t ts;
} sensor_packet_t;

typedef struct sbuffer_node{
    sensor_packet_t pkt;
    uint8_t refcount; 
    uint8_t processed_by_data;
    uint8_t processed_by_storage;
    //uint8_t processed_by_cloud;
    struct sbuffer_node *next;
} sbuffer_node_t;

typedef struct{
    sbuffer_node_t *head;
    sbuffer_node_t *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} sbuffer_t;

// Per-sensor running average table
typedef struct sensor_stat{
    uint8_t id;
    uint8_t type;
    double avg;
    unsigned long count;
    time_t last_uploaded;  // for tracking uploading
    unsigned long last_uploaded_count;
    struct sensor_stat *next;
} sensor_stat_t;

#endif