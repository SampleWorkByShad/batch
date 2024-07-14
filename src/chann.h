#ifndef CHANN_H_
#define CHANN_H_

#include <netinet/in.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct
{
  int sockfd;
  struct sockaddr_in sockaddr;
  socklen_t sockaddr_len;
} chan_connection_t;

typedef struct
{
  int key;
  chan_connection_t conn;
  char *in;
  size_t insz;
  char *out;
  size_t outsz;
  int state;
} chan_channel_t;

typedef struct
{
  chan_channel_t **channs;
  size_t sz;
} chan_collection_t;

void chan_connection_init (chan_connection_t *);

chan_channel_t *chan_create ();

void chan_init (chan_channel_t *);

void chan_destroy (chan_channel_t *);

int chan_collection_init (chan_collection_t *, size_t);

int chan_collection_add (chan_collection_t *, chan_channel_t *);

chan_channel_t *chan_collection_find_by_sockfd (chan_collection_t *, int);

int chan_collection_deinit (chan_collection_t *);

#endif