#include <stdlib.h>
#include <pthread.h>
#include "queue.h"

// circular array
typedef struct _queue {
    int size;
    int used;
    int first;
    void **data;
    pthread_mutex_t *lock;
    pthread_cond_t *full;
    pthread_cond_t *empty;
} _queue;



queue q_create(int size) {
    queue q = malloc(sizeof(_queue));
    q->lock = malloc(sizeof(pthread_mutex_t)); 
    q->full = malloc(sizeof(pthread_cond_t));
    q->empty = malloc(sizeof(pthread_cond_t));

    q->size  = size;
    q->used  = 0;
    q->first = 0;
    q->data  = malloc(size*sizeof(void *));

    pthread_mutex_init(q->lock,NULL);
    pthread_cond_init(q->full,NULL);
    pthread_cond_init(q->empty,NULL);
    
    return q;
}

int q_elements(queue q) {
    return q->used;
}

int q_insert(queue q, void *elem) {
    pthread_mutex_lock(q->lock);

    while(q->size == q->used){
      pthread_cond_wait(q->full, q->lock);
    }

    q->data[(q->first+q->used) % q->size] = elem;    
     
    if(q->used == 0){
      pthread_cond_broadcast(q->empty);
    }

    q->used++;

    pthread_mutex_unlock(q->lock);
    
    
    return 1;
}

void *q_remove(queue q) {
  
    pthread_mutex_lock(q->lock);

    void *res;
    while(q->used==0){
      pthread_cond_wait(q->empty,q->lock);
    }
    
    res = q->data[q->first];
    
    q->first = (q->first+1) % q->size;

    if(q->used == q->size){
      pthread_cond_broadcast(q->full);
    } 
    
    q->used--;
    
    pthread_mutex_unlock(q->lock);

    return res;
}

void q_destroy(queue q) {
    pthread_mutex_destroy(q->lock);
    pthread_cond_destroy(q->full);
    pthread_cond_destroy(q->empty);
    free(q->lock);
    free(q->empty);
    free(q->full);
    free(q->data);
    free(q);
}
