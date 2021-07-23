#include <stdlib.h>
#include "wq.h"
#include "utlist.h"

/* Initializes a work queue WQ. */
void wq_init(wq_t *wq) {

  /* TODO: Make me thread-safe! */
  wq->size = 0;
  wq->head = NULL;
  wq->wq_lock = malloc(sizeof(pthread_mutex_t));
  wq->wq_block_sema = malloc(sizeof(sem_t));
  pthread_mutex_init(wq->wq_lock,NULL);
  sem_init(wq->wq_block_sema,0,0);

}

/* Remove an item from the WQ. This function should block until there
 * is at least one item on the queue. */
int wq_pop(wq_t *wq) {
  /*Thread safe and blocked while there are zero threads in the queue*/
  sem_wait(wq->wq_block_sema);
  pthread_mutex_lock(wq->wq_lock);
  wq_item_t *wq_item = wq->head;
  int client_socket_fd = wq->head->client_socket_fd;
  wq->size--;
  DL_DELETE(wq->head, wq->head);

  free(wq_item);
  pthread_mutex_unlock(wq->wq_lock);

  return client_socket_fd;
}

/* Add ITEM to WQ. */
void wq_push(wq_t *wq, int client_socket_fd) {

  /* TODO: Make me thread-safe! */
  pthread_mutex_lock(wq->wq_lock);

  wq_item_t *wq_item = calloc(1, sizeof(wq_item_t));
  wq_item->client_socket_fd = client_socket_fd;
  DL_APPEND(wq->head, wq_item);
  wq->size++;
  /*When at least one thread is in queue , it gets unlocked*/
  sem_post(wq->wq_block_sema);
  pthread_mutex_unlock(wq->wq_lock);

}
