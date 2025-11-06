#ifndef MAIN_H
#define MAIN_H

#define _GNU_SOURCE 200809L
#define FIFO_PATH "logFifo"
#define LOG_FILE "gateway.log"
#define DB_FILE "sensors.db"
#define MAX_LINE 256

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

typedef enum {
    SENSOR_TEMPERATURE = 1,
    SENSOR_HUMIDITY = 2,
    SENSOR_LIGHT = 3
} sensor_type_t;

typedef struct {
    int id;
    int type;
    double value;
    time_t ts;
} sensor_packet_t;

typedef struct sbuffer_node {
    sensor_packet_t pkt;
    int refcount; 
    int processed_by_data;
    int processed_by_storage;
    struct sbuffer_node *next;
} sbuffer_node_t;

typedef struct {
    sbuffer_node_t *head;
    sbuffer_node_t *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} sbuffer_t;

typedef struct {
    int client_fd;
    struct sockaddr_in addr;
} client_info_t;

// --- per-sensor running average table (linked list) ---
typedef struct sensor_stat {
    int id;
    int type;
    double avg;
    unsigned long count;
    struct sensor_stat *next;
} sensor_stat_t;

typedef struct {
    int id;
    const char *token;
    struct mosquitto *mosq;
    int connected;
} cloud_client_t;

#endif