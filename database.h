#ifndef DATABASE_H
#define DATABASE_H

#include "main.h"

extern sbuffer_t sbuffer;

int db_init_and_open(sqlite3 **out_db);
int db_insert_measure(sqlite3_stmt *stmt, const sensor_packet_t *pkt);

#endif