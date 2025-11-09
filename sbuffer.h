#ifndef SBUFFER_H
#define SBUFFER_H

#include "main.h"

extern volatile sig_atomic_t stop_flag;

void sbuffer_init(sbuffer_t *b);
void sbuffer_free_all(sbuffer_t *b);
void sbuffer_insert(sbuffer_t *b, sensor_packet_t *pkt);
static inline int sbuffer_wait_until_data(sbuffer_t *b);
static sbuffer_node_t *sbuffer_find_generic(sbuffer_t *b, int (*predicate)(sbuffer_node_t *));
static int need_data(sbuffer_node_t *n);
static int need_storage(sbuffer_node_t *n);
static int need_cloud(sbuffer_node_t *n);
sbuffer_node_t* sbuffer_find_for_storage(sbuffer_t *b);
sbuffer_node_t* sbuffer_find_for_data(sbuffer_t *b);
sbuffer_node_t* sbuffer_find_for_cloud(sbuffer_t *b);
static void sbuffer_try_cleanup(sbuffer_t *b, sbuffer_node_t *node);
void sbuffer_mark_storage_done(sbuffer_t *b, sbuffer_node_t *node);
void sbuffer_mark_data_done(sbuffer_t *b, sbuffer_node_t *node) ;
void sbuffer_mark_upcloud_done(sbuffer_t *b, sbuffer_node_t *node);

#endif