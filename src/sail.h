#include "types.h"
#include <arpa/inet.h>
#include <config.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef SAIL_H_
#define SAIL_H_

#define SAIL_CHANNEL_STATUS_READY 0
#define SAIL_CHANNEL_STATUS_PROCESSING 1

#define SAIL_MAX_CLIENT_CONNECTIONS 10000

#define SAIL_EPOLL_MAX_EVENTS 1000

extern sail_connection_t serverconn;
extern sail_collection_t clientchans;
extern pthread_mutex_t mut;

#define SAIL_LOCK() pthread_mutex_lock (&mut)
#define SAIL_UNLOCK() pthread_mutex_unlock (&mut)

int sail_buffer_allocate (sail_buffer_t *, size_t);
void sail_buffer_reset (sail_buffer_t *);

void sail_connection_init (sail_connection_t *);

sail_channel_t *sail_channel_create ();
void sail_channel_destroy (sail_channel_t *);

int sail_collection_init (sail_collection_t *, size_t);
int sail_collection_add (sail_collection_t *, sail_channel_t *);
void sail_collection_remove (sail_collection_t *, sail_channel_t *);
sail_channel_t *sail_collection_get_by_sockfd (sail_collection_t *, int);
int sail_collection_deinit (sail_collection_t *);

int sail_pool_init (sail_pool_t *, size_t, size_t, void *(void *));
void *sail_pool_routine (void *);
void sail_pool_activate (sail_pool_t *);
void sail_pool_deactivate (sail_pool_t *);
void sail_pool_ready (sail_pool_t *);
int sail_pool_queue_add (sail_pool_t *, void *);
void sail_pool_notify (sail_pool_t *);
void sail_pool_winddown (sail_pool_t *);
int sail_pool_deinit (sail_pool_t *);

void sail_init ();
void sail_deinit ();
void sail_terminate_channel (sail_channel_t *);
void *sail_greet_routine (void *);
void *sail_proc_routine (void *);

#endif