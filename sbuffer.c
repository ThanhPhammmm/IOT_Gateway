#include"sbuffer.h"

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

void sbuffer_insert(sbuffer_t *b, sensor_packet_t *pkt){
    sbuffer_node_t *n = calloc(1, sizeof(*n));
    if(!n){
        return;
    }
    n->pkt = *pkt;
    n->pkt.ts = time(NULL);
    n->refcount = 3; // data manager + storage manager + cloud manager
    n->processed_by_data = 0;
    n->processed_by_storage = 0;
    n->next = NULL;

    pthread_mutex_lock(&b->mutex);
    if(b->tail){
        b->tail->next = n;
    }
    else{
        b->head = n;
    }
    b->tail = n;
    pthread_mutex_unlock(&b->mutex);
}

// find first node not yet processed by storage; returns pointer (do NOT remove).
// Blocks until such node exists or stop_flag set.
sbuffer_node_t* sbuffer_find_for_storage(sbuffer_t *b){
    pthread_mutex_lock(&b->mutex);
    while(!stop_flag){
        sbuffer_node_t *cur = b->head;
        while(cur){
            if(!cur->processed_by_storage){
                // return cur without removing
                pthread_mutex_unlock(&b->mutex);
                return cur;
            }
            cur = cur->next;
        }
        // nothing suitable yet -> wait
        pthread_cond_wait(&b->cond, &b->mutex);
    }
    pthread_mutex_unlock(&b->mutex);
    return NULL;
}

void sbuffer_mark_storage_done(sbuffer_t *b, sbuffer_node_t *node){
    pthread_mutex_lock(&b->mutex);
    if(!node->processed_by_storage){
        node->processed_by_storage = 1;
        node->refcount--;
        // if refcount==0 -> remove node from list and free
        if(node->refcount <= 0){
            // remove node from list (could be anywhere)
            sbuffer_node_t *prev = NULL, *cur = b->head;
            while(cur){
                if(cur == node){
                    if(prev) prev->next = cur->next;
                    else b->head = cur->next;
                    if(b->tail == cur) b->tail = prev;
                    free(cur);
                    break;
                }
                prev = cur;
                cur = cur->next;
            }
        }
    }
    pthread_mutex_unlock(&b->mutex);
}

void sbuffer_mark_data_done(sbuffer_t *b, sbuffer_node_t *node){
    pthread_mutex_lock(&b->mutex);
    if(!node->processed_by_data){
        node->processed_by_data = 1;
        node->refcount--;   
        // if refcount==0 -> remove & free
        if(node->refcount <= 0){
            sbuffer_node_t *prev = NULL, *cur = b->head;
            while(cur){
                if(cur == node){
                    if(prev) prev->next = cur->next;
                    else b->head = cur->next;
                    if(b->tail == cur) b->tail = prev;
                    free(cur);
                    break;
                }
                prev = cur;
                cur = cur->next;
            }
        }
    }
    pthread_cond_broadcast(&b->cond);
    pthread_mutex_unlock(&b->mutex);
}
