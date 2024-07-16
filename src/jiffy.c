#include "jiffy.h"

int
jiff_init (jiff_pool_t *pool, size_t sz, size_t qsz, void *routine (void *arg))
{
  int retval;

  pool->keys = (jiff_key_t *)calloc (sz, sizeof (*pool->keys));
  pool->sz = sz;
  pool->counter = 0;
  pool->active = false;
  pool->q = (void **)calloc (qsz, sizeof (*pool->q));
  pool->qsz = qsz;
  pool->routine = routine;

  pthread_mutex_init (&pool->mut, NULL);

  pthread_cond_init (&pool->readycond, NULL);
  pthread_cond_init (&pool->updatecond, NULL);

  retval = 0;

end:
  return retval;
}

void *
jif_routine (void *arg)
{
  jiff_meta_t *meta;
  void *item;
  int i;

  meta = (jiff_meta_t *)arg;

  pthread_mutex_lock (&meta->pool->mut);
  meta->pool->counter--;
  pthread_mutex_unlock (&meta->pool->mut);
  pthread_cond_signal (&meta->pool->readycond);

  pthread_mutex_lock (&meta->pool->mut);
  while (meta->pool->active)
    {
      for (i = 0; i < meta->pool->qsz; i++)
        {
          if (meta->pool->q[i] != 0)
            {
              item = meta->pool->q[i];
              meta->pool->q[i] = 0;
              pthread_mutex_unlock (&meta->pool->mut);

              meta->pool->routine (item);

              pthread_mutex_lock (&meta->pool->mut);
            }
        }
      pthread_cond_wait (&meta->pool->updatecond, &meta->pool->mut);
    }
  pthread_mutex_unlock (&meta->pool->mut);

  free (meta);
}

void
jiff_activate (jiff_pool_t *pool)
{
  int i;
  jiff_meta_t *arg;

  pool->active = true;

  for (i = 0; i < pool->sz; i++)
    {
      arg = (jiff_meta_t *)malloc (sizeof (*arg));

      if (arg == NULL)
        continue;
      arg->pool = pool;
      arg->keyidx = i;

      pool->counter++;

      pthread_create (&pool->keys[i].id, NULL, &jif_routine, arg);
    }
}

void
jiff_deactivate (jiff_pool_t *pool)
{
  pthread_mutex_lock (&pool->mut);
  pool->active = false;
  pthread_mutex_unlock (&pool->mut);
}

void
jiff_ready (jiff_pool_t *pool)
{
  pthread_mutex_lock (&pool->mut);
  while (pool->counter > 0)
    {
      pthread_cond_wait (&pool->readycond, &pool->mut);
    }
  pthread_mutex_unlock (&pool->mut);
}

int
jiff_queue_add (jiff_pool_t *pool, void *item)
{
  int index;
  int i;

  index = -1;

  pthread_mutex_lock (&pool->mut);
  for (i = 0; i < pool->qsz; i++)
    {
      if (pool->q[i] == 0)
        {
          pool->q[i] = item;
          index = i;
          break;
        }
    }
  pthread_mutex_unlock (&pool->mut);

  return index;
}

void
jiff_notify (jiff_pool_t *pool)
{
  pthread_cond_broadcast (&pool->updatecond);
}

void
jiff_winddown (jiff_pool_t *pool)
{
  int i;
  void *res;

  for (i = 0; i < pool->sz; i++)
    {
      pthread_join (pool->keys[i].id, res);

      free (res);
    }
}

int
jiff_deinit (jiff_pool_t *pool)
{
  pthread_cond_destroy (&pool->updatecond);
  pthread_cond_destroy (&pool->readycond);
  pthread_mutex_destroy (&pool->mut);

  free (pool->q);
  free (pool->keys);
}