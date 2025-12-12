#ifndef DATABASE_H
#define DATABASE_H

#include "main.h"

extern sbuffer_t sbuffer;

int db_init_and_open(sqlite3 **out_db);
int db_insert_measure(sqlite3 *db, sensor_packet_t *pkt);
int db_insert_measures_batch(sqlite3 *db, sensor_packet_t *packets, size_t count);

#endif