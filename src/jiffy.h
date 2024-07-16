#ifndef JIFFY_H_
#define JIFFY_H_

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
  pthread_t id;
} jiff_key_t;

typedef struct
{
  jiff_key_t *keys;
  size_t sz;
  int counter;
  pthread_mutex_t mut;
  pthread_cond_t readycond;
  pthread_cond_t updatecond;
  void *(*routine) (void *);
  bool active;
  size_t qsz;
  void **q;
} jiff_pool_t;

typedef struct
{
  jiff_pool_t *pool;
  int keyidx;
} jiff_meta_t;

int jiff_init (jiff_pool_t *, size_t, size_t, void *(void *));

void *jif_routine (void *);

void jiff_activate (jiff_pool_t *);

void jiff_deactivate (jiff_pool_t *);

void jiff_ready (jiff_pool_t *);

int jiff_queue_add (jiff_pool_t *, void *);

void jiff_notify (jiff_pool_t *);

void jiff_winddown (jiff_pool_t *);

int jiff_deinit (jiff_pool_t *);

#endif