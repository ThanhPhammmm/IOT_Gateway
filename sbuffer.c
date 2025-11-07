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
    
    pthread_mutex_destroy(&b->mutex);
    pthread_cond_destroy(&b->cond);
}

// Helper: wait until buffer has data or stop_flag is set
static inline int sbuffer_wait_until_data(sbuffer_t *b){
    while(b->head == NULL && !stop_flag){
        pthread_cond_wait(&b->cond, &b->mutex);
    }
    return stop_flag;
}

void sbuffer_insert(sbuffer_t *b, sensor_packet_t *pkt){
    sbuffer_node_t *n = malloc(sizeof(*n));
    if(!n){
        log_event("[SBUFFER] malloc failed");
        return;
    }

    n->pkt = *pkt;
    n->pkt.ts = time(NULL);
    n->refcount = 3;  // data + storage + cloud
    n->processed_by_data = 0;
    n->processed_by_storage = 0;
    n->processed_by_cloud = 0;
    n->next = NULL;

    pthread_mutex_lock(&b->mutex);
    if(b->tail){
        b->tail->next = n;
    } 
    else{
        b->head = n;
    }
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
        pthread_mutex_unlock(&b->mutex);
        return NULL;
    }

    sbuffer_node_t *result = NULL;
    for(sbuffer_node_t *cur = b->head; cur; cur = cur->next){
        if(predicate(cur)){
            result = cur;
            break;
        }
    }

    pthread_mutex_unlock(&b->mutex);
    return result;
}

// Predicate functions
static int need_data(sbuffer_node_t *n){
    return !n->processed_by_data;
}

static int need_storage(sbuffer_node_t *n){
    return n->processed_by_data && !n->processed_by_storage;
}

static int need_cloud(sbuffer_node_t *n){
    return n->processed_by_data && n->processed_by_storage && !n->processed_by_cloud;
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

static void sbuffer_try_cleanup(sbuffer_t *b, sbuffer_node_t *node){
    if(node->refcount > 0) return;

    // Remove node from linked list
    if(b->head == node){
        b->head = node->next;
        if(b->tail == node){
            b->tail = NULL;
        }
    } 
    else{
        sbuffer_node_t *prev = b->head;
        while(prev && prev->next != node){
            prev = prev->next;
        }
        if(prev){
            prev->next = node->next;
            if(b->tail == node){
                b->tail = prev;
            }
        }
    }
    
    free(node);
    printf("FREE\n");
}

void sbuffer_mark_data_done(sbuffer_t *b, sbuffer_node_t *node){
    pthread_mutex_lock(&b->mutex);
    if(!node->processed_by_data){
        node->processed_by_data = 1;
        if(--node->refcount == 0){
            sbuffer_try_cleanup(b, node);
        }
        pthread_cond_broadcast(&b->cond);
    }
    pthread_mutex_unlock(&b->mutex);
}

void sbuffer_mark_storage_done(sbuffer_t *b, sbuffer_node_t *node){
    pthread_mutex_lock(&b->mutex);
    if(node->processed_by_data && !node->processed_by_storage){
        node->processed_by_storage = 1;
        if(--node->refcount == 0){
            sbuffer_try_cleanup(b, node);
        }
        pthread_cond_broadcast(&b->cond);
    }
    pthread_mutex_unlock(&b->mutex);
}

void sbuffer_mark_upcloud_done(sbuffer_t *b, sbuffer_node_t *node){
    pthread_mutex_lock(&b->mutex);
    if(node->processed_by_data && node->processed_by_storage && 
        !node->processed_by_cloud){
        node->processed_by_cloud = 1;
        if(--node->refcount == 0){
            sbuffer_try_cleanup(b, node);
        }
        pthread_cond_broadcast(&b->cond);
    }
    pthread_mutex_unlock(&b->mutex);
}