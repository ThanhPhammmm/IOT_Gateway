#include "sbuffer.h"
#include "logger.h"

void sbuffer_init(sbuffer_t *b){
    b->head = b->tail = NULL;
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->cond, NULL);
}

void sbuffer_free_all(sbuffer_t *b){
    pthread_mutex_lock(&b->mutex);
    sbuffer_node_t *n = b->head;
    while(n){
        sbuffer_node_t *nx = n->next;
        free(n);
        n = nx;
    }
    b->head = b->tail = NULL;
    pthread_mutex_unlock(&b->mutex);
}

// Helper function: chờ cho đến khi buffer có dữ liệu hoặc stop_flag bật
static inline int sbuffer_wait_until_data(sbuffer_t *b){
    while(b->head == NULL && !stop_flag){
        pthread_cond_wait(&b->cond, &b->mutex);
    }
    return stop_flag;
}

void sbuffer_insert(sbuffer_t *b, sensor_packet_t *pkt){
    sbuffer_node_t *n = calloc(1, sizeof(*n));
    if (!n) return;

    n->pkt = *pkt;
    n->pkt.ts = time(NULL);
    n->refcount = 3;  // data + storage + cloud
    n->next = NULL;

    pthread_mutex_lock(&b->mutex);
    if (b->tail)
        b->tail->next = n;
    else
        b->head = n;
    b->tail = n;
    pthread_cond_broadcast(&b->cond);
    pthread_mutex_unlock(&b->mutex);
}

/* ===========================
 *   Find node functions
 * =========================== */

static sbuffer_node_t *sbuffer_find_generic(sbuffer_t *b,
        int (*predicate)(sbuffer_node_t *)){

    pthread_mutex_lock(&b->mutex);

    // Wait until data arrives or stop_flag
    if(sbuffer_wait_until_data(b)){
        log_event("[SBUFFER] stop_flag set, returning NULL\n");
        pthread_mutex_unlock(&b->mutex);
        return NULL;
    }

    for(sbuffer_node_t *cur = b->head; cur; cur = cur->next){
        if(predicate(cur)){
            pthread_mutex_unlock(&b->mutex);
            return cur;
        }
    }

    pthread_mutex_unlock(&b->mutex);
    return NULL;
}

// Define predicate functions
static int need_data(sbuffer_node_t *n){
    return !n->processed_by_data;
}
static int need_storage(sbuffer_node_t *n){
    return !n->processed_by_storage && n->processed_by_data == 1;
}
static int need_cloud(sbuffer_node_t *n){
    return !n->processed_by_cloud && n->processed_by_data == 1 && n->processed_by_storage == 1;
}

// Wrappers
sbuffer_node_t* sbuffer_find_for_data(sbuffer_t *b){
    return sbuffer_find_generic(b, need_data);
}
sbuffer_node_t* sbuffer_find_for_storage(sbuffer_t *b){
    return sbuffer_find_generic(b, need_storage);
}
sbuffer_node_t* sbuffer_find_for_cloud(sbuffer_t *b){
    return sbuffer_find_generic(b, need_cloud);
}

/* ===========================
 *   Mark functions
 * =========================== */

void sbuffer_mark_data_done(sbuffer_t *b, sbuffer_node_t *node){
    pthread_mutex_lock(&b->mutex);
    if(!node->processed_by_data){
        node->processed_by_data = 1;
        node->refcount--;
        pthread_cond_broadcast(&b->cond);
    }
    pthread_mutex_unlock(&b->mutex);
}

void sbuffer_mark_storage_done(sbuffer_t *b, sbuffer_node_t *node){
    pthread_mutex_lock(&b->mutex);
    if(!node->processed_by_storage && node->processed_by_data){
        node->processed_by_storage = 1;
        node->refcount--;
        pthread_cond_broadcast(&b->cond);
    }
    pthread_mutex_unlock(&b->mutex);
}

void sbuffer_mark_upcloud_done(sbuffer_t *b, sbuffer_node_t *node){
    pthread_mutex_lock(&b->mutex);

    if(!node->processed_by_cloud &&
        node->processed_by_data && node->processed_by_storage){
        node->processed_by_cloud = 1;
        node->refcount--;
    }

    // cleanup nếu mọi thread đã xử lý xong
    if(node->refcount <= 0){
        sbuffer_node_t *prev = NULL, *cur = b->head;
        while(cur){
            if(cur == node){
                if (prev) prev->next = cur->next;
                else b->head = cur->next;
                if (b->tail == cur) b->tail = prev;
                free(cur);
                break;
            }
            prev = cur;
            cur = cur->next;
        }
    }

    pthread_cond_broadcast(&b->cond);
    pthread_mutex_unlock(&b->mutex);
}
