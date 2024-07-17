#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef TYPES_H_
#define TYPES_H_

typedef struct
{
  bool sessinitiated;
} sail_state_t;

typedef struct
{
  int sockfd;
  struct sockaddr_in sockaddr;
  socklen_t sockaddr_len;
} sail_connection_t;

typedef struct
{
  char *data;
  size_t sz;
} sail_buffer_t;

typedef struct
{
  int key;
  sail_connection_t conn;
  sail_buffer_t in;
  sail_buffer_t out;
  int status;
  sail_state_t state;
} sail_channel_t;

typedef struct
{
  sail_channel_t **channs;
  size_t sz;
} sail_collection_t;

typedef struct
{
  pthread_t id;
} sail_pool_key_t;

typedef struct
{
  sail_pool_key_t *keys;
  size_t sz;
  int counter;
  pthread_mutex_t mut;
  pthread_cond_t readycond;
  pthread_cond_t updatecond;
  void *(*routine) (void *);
  bool active;
  size_t qsz;
  void **q;
} sail_pool_t;

typedef struct
{
  sail_pool_t *pool;
  int keyidx;
} sail_pool_meta_t;

typedef struct
{
  char verb[100];
  char params[100];
} sail_command_t;

#endif