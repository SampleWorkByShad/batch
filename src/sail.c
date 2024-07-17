#include "sail.h"

int
sail_buffer_allocate (sail_buffer_t *buf, size_t sz)
{
  int retval;

  buf->data = (char *)calloc (sz, sizeof (char));

  if (buf->data == NULL)
    {
      retval = -1;
      goto end;
    }
  buf->sz = sz;

  retval = 0;
end:
  return retval;
}

void
sail_buffer_reset (sail_buffer_t *buf)
{
  free (buf->data);
  buf->sz = 0;
}

void
sail_connection_init (sail_connection_t *conn)
{
  conn->sockaddr_len = sizeof (conn->sockaddr);
}

sail_channel_t *
sail_channel_create ()
{
  sail_channel_t *chan;

  chan = (sail_channel_t *)malloc (sizeof (*chan));

  if (chan != NULL)
    {
      memset (chan, 0, sizeof (*chan));
      sail_connection_init (&chan->conn);
      chan->state.sessinitiated = false;
    }

  return chan;
}

void
sail_channel_destroy (sail_channel_t *chan)
{
  free (chan->in.data);
  free (chan->out.data);
  free (chan);
}

int
sail_collection_init (sail_collection_t *c, size_t sz)
{
  int retval;

  c->channs = (sail_channel_t **)calloc (sz, sizeof (*c->channs));

  if (c->channs == NULL)
    {
      retval = -1;
      goto end;
    }
  c->sz = sz;
  c->mersenne = 0x7FFFFFFF;
  c->midpoint = 0x3FFFFFFE;

  retval = 0;

end:
  return retval;
}

int
sail_collection_add (sail_collection_t *c, sail_channel_t *chan)
{
  int idx;

  idx = c->midpoint % (c->midpoint - chan->conn.sockfd);
  c->channs[idx] = chan;

  return idx;
}

sail_channel_t *
sail_collection_get_by_sockfd (sail_collection_t *c, int sockfd)
{
  sail_channel_t *chan;
  int idx;

  idx = c->midpoint % (c->midpoint - sockfd);
  chan = c->channs[idx];

  return chan;
}

int
sail_collection_deinit (sail_collection_t *c)
{
  free (c->channs);
}

int
sail_pool_init (sail_pool_t *pool, size_t sz, size_t qsz,
                void *routine (void *arg))
{
  int retval;

  pool->keys = (sail_pool_key_t *)calloc (sz, sizeof (*pool->keys));
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

  return retval;
}

void *
sail_pool_routine (void *arg)
{
  sail_pool_meta_t *meta;
  void *item;
  int i;

  meta = (sail_pool_meta_t *)arg;
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
sail_pool_activate (sail_pool_t *pool)
{
  int i;
  sail_pool_meta_t *arg;

  pool->active = true;

  for (i = 0; i < pool->sz; i++)
    {
      arg = (sail_pool_meta_t *)malloc (sizeof (*arg));

      if (arg == NULL)
        continue;
      arg->pool = pool;
      arg->keyidx = i;
      pool->counter++;
      pthread_create (&pool->keys[i].id, NULL, &sail_pool_routine, arg);
    }
}

void
sail_pool_deactivate (sail_pool_t *pool)
{
  pthread_mutex_lock (&pool->mut);
  pool->active = false;
  pthread_mutex_unlock (&pool->mut);
}

void
sail_pool_ready (sail_pool_t *pool)
{
  pthread_mutex_lock (&pool->mut);

  while (pool->counter > 0)
    {
      pthread_cond_wait (&pool->readycond, &pool->mut);
    }
  pthread_mutex_unlock (&pool->mut);
}

int
sail_pool_queue_add (sail_pool_t *pool, void *item)
{
  int idx;
  int i;

  pthread_mutex_lock (&pool->mut);

  for (i = 0; i < pool->qsz; i++)
    {
      if (pool->q[i] == 0)
        {
          pool->q[i] = item;
          break;
        }
    }
  pthread_mutex_unlock (&pool->mut);

  if (i < pool->qsz)
    {
      idx = i;
    }
  else
    {
      idx = -1;
    }

  return idx;
}

void
sail_pool_notify (sail_pool_t *pool)
{
  pthread_cond_broadcast (&pool->updatecond);
}

void
sail_pool_winddown (sail_pool_t *pool)
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
sail_pool_deinit (sail_pool_t *pool)
{
  pthread_cond_destroy (&pool->updatecond);
  pthread_cond_destroy (&pool->readycond);
  pthread_mutex_destroy (&pool->mut);
  free (pool->q);
  free (pool->keys);
}

void
sail_init ()
{
  pthread_mutex_init (&mut, NULL);
  sail_collection_init (&clientchans, 10);
}

void
sail_deinit ()
{
  sail_collection_deinit (&clientchans);
  pthread_mutex_destroy (&mut);
}

void
sail_terminate_channel (sail_channel_t *chan)
{
  SAIL_LOCK ();
  epoll_ctl (serverconn.sockfd, EPOLL_CTL_DEL, chan->conn.sockfd, NULL);
  close (chan->conn.sockfd);
  clientchans.channs[chan->key] = 0;
  sail_channel_destroy (chan);
  SAIL_UNLOCK ();
}

void *
sail_greet_routine (void *arg)
{
  sail_channel_t *chan;
  int rv;

  chan = (sail_channel_t *)arg;
  sail_buffer_allocate (&chan->out, 32);
  rv = snprintf (chan->out.data, chan->out.sz, "220 %s %s\r\n", PACKAGE_NAME,
                 PACKAGE_VERSION);

  if (rv < 0)
    {
      sail_terminate_channel (chan);
      goto end;
    }
  rv = write (chan->conn.sockfd, chan->out.data, strlen (chan->out.data));

  if (rv == -1)
    {
      if (errno != EWOULDBLOCK)
        {
          sail_terminate_channel (chan);
          goto end;
        }
    }
  else
    {
      chan->state.sessinitiated = true;
    }
  sail_buffer_reset (&chan->out);
  SAIL_LOCK ();
  chan->status = SAIL_CHANNEL_STATUS_READY;
  SAIL_UNLOCK ();

end:
}

void *
sail_proc_routine (void *arg)
{
  sail_channel_t *chan;
  int rv;

  chan = (sail_channel_t *)arg;
  sail_buffer_allocate (&chan->in, 32);
  rv = read (chan->conn.sockfd, chan->in.data, chan->in.sz);

  if (rv > 0)
    {
      rv = write (chan->conn.sockfd, chan->in.data, strlen (chan->in.data));
    }
  sail_buffer_reset (&chan->in);
  SAIL_LOCK ();
  chan->status = SAIL_CHANNEL_STATUS_READY;
  SAIL_UNLOCK ();
end:
}
