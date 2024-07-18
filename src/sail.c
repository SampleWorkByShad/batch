#include "sail.h"

int
sail_allocate_buffer (sail_buffer_t *buf, size_t sz)
{
  int retval;

  buf->data = calloc (sz, sizeof (char));

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
sail_reset_buffer (sail_buffer_t *buf)
{
  free (buf->data);
  buf->sz = 0;
}

void
sail_init_connection (sail_connection_t *conn)
{
  conn->sockaddr_len = sizeof (conn->sockaddr);
}

sail_channel_t *
sail_create_channel ()
{
  sail_channel_t *chan;

  chan = malloc (sizeof (chan));

  if (chan != NULL)
    {
      memset (chan, 0, sizeof (chan));
      chan->key = -1;
      sail_init_connection (&chan->conn);
    }

  return chan;
}

void
sail_destroy_channel (sail_channel_t *chan)
{
  free (chan->in.data);
  free (chan->out.data);
  free (chan);
}

int
sail_init_collection (sail_collection_t *c, size_t sz)
{
  int retval;

  c->channs = calloc (sz, sizeof (*c->channs));

  if (c->channs == NULL)
    {
      retval = -1;
      goto end;
    }
  c->sz = sz;
  retval = 0;

end:
  return retval;
}

int
sail_add_collection_channel (sail_collection_t *c, sail_channel_t *chan)
{
  int retval;

  if (chan->conn.sockfd > c->sz)
    {
      retval = -1;
      goto end;
    }
  chan->key = chan->conn.sockfd;
  c->channs[chan->key] = chan;
  retval = 0;

end:
  return retval;
}

void
sail_remove_collection_channel (sail_collection_t *c, sail_channel_t *chan)
{
  if (chan->key == -1)
    {
      goto end;
    }
  c->channs[chan->key] = 0;
  chan->key = -1;

end:
}

sail_channel_t *
sail_get_collection_channel_by_sockfd (sail_collection_t *c, int sockfd)
{
  sail_channel_t *chan;

  if (sockfd > c->sz)
    {
      chan = NULL;
      goto end;
    }
  chan = c->channs[sockfd];

end:
  return chan;
}

int
sail_deinit_collection (sail_collection_t *c)
{
  free (c->channs);
}

int
sail_init_pool (sail_pool_t *pool, size_t sz, size_t qsz,
                void routine (sail_channel_t *chan))
{
  int retval;

  pool->keys = calloc (sz, sizeof (*pool->keys));
  pool->sz = sz;
  pool->counter = 0;
  pool->active = false;
  pool->q = calloc (qsz, sizeof (*pool->q));
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
  sail_channel_t *chan;
  int i;

  meta = (sail_pool_meta_t *)arg;

  pthread_mutex_lock (&meta->pool->mut);
  meta->pool->counter--;
  pthread_mutex_unlock (&meta->pool->mut);
  pthread_cond_signal (&meta->pool->readycond);
  pthread_mutex_lock (&meta->pool->mut);

  while (meta->pool->active)
    {
      for (i = 0; i < meta->pool->qsz; ++i)
        {
          if (meta->pool->q[i] != 0)
            {
              chan = meta->pool->q[i];
              meta->pool->q[i] = 0;
              pthread_mutex_unlock (&meta->pool->mut);
              meta->pool->routine (chan);
              pthread_mutex_lock (&meta->pool->mut);
            }
        }
      pthread_cond_wait (&meta->pool->updatecond, &meta->pool->mut);
    }
  pthread_mutex_unlock (&meta->pool->mut);
  free (meta);
}

void
sail_activate_pool (sail_pool_t *pool)
{
  int i;
  sail_pool_meta_t *arg;

  pool->active = true;

  for (i = 0; i < pool->sz; ++i)
    {
      arg = malloc (sizeof (*arg));

      if (arg == NULL)
        continue;
      arg->pool = pool;
      pool->counter++;
      pthread_create (&pool->keys[i].id, NULL, &sail_pool_routine, arg);
    }
}

void
sail_deactivate_pool (sail_pool_t *pool)
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
sail_add_pool_queue_channel (sail_pool_t *pool, sail_channel_t *chan)
{
  int x;
  int i;

  pthread_mutex_lock (&pool->mut);

  for (i = 0; i < pool->qsz; ++i)
    {
      if (pool->q[i] == 0)
        {
          pool->q[i] = chan;
          break;
        }
    }
  pthread_mutex_unlock (&pool->mut);

  if (i < pool->qsz)
    {
      x = i;
    }
  else
    {
      x = -1;
    }

  return x;
}

void
sail_notify_pool (sail_pool_t *pool)
{
  pthread_cond_broadcast (&pool->updatecond);
}

void
sail_winddown_pool (sail_pool_t *pool)
{
  int i;

  for (i = 0; i < pool->sz; ++i)
    {
      pthread_join (pool->keys[i].id, NULL);
    }
}

int
sail_deinit_pool (sail_pool_t *pool)
{
  pthread_cond_destroy (&pool->updatecond);
  pthread_cond_destroy (&pool->readycond);
  pthread_mutex_destroy (&pool->mut);
  free (pool->q);
  free (pool->keys);
}

int
sail_helo_action_handler (sail_channel_t *chan)
{
  int retval;

  retval = 0;
end:
  return retval;
}

int
sail_ehlo_action_handler (sail_channel_t *chan)
{
  int retval;

  retval = 0;
end:
  return retval;
}

int
sail_mail_action_handler (sail_channel_t *chan)
{
  int retval;

  retval = 0;
end:
  return retval;
}

int
sail_rcpt_action_handler (sail_channel_t *chan)
{
  int retval;

  retval = 0;
end:
  return retval;
}

int
sail_data_action_handler (sail_channel_t *chan)
{
  int retval;

  retval = 0;
end:
  return retval;
}

int
sail_rset_action_handler (sail_channel_t *chan)
{
  int retval;

  retval = 0;
end:
  return retval;
}

int
sail_noop_action_handler (sail_channel_t *chan)
{
  int retval;

  retval = 0;
end:
  return retval;
}

int
sail_quit_action_handler (sail_channel_t *chan)
{
  int retval;

  retval = 0;
end:
  return retval;
}

int
sail_vrfy_action_handler (sail_channel_t *chan)
{
  int retval;

  retval = 0;
end:
  return retval;
}

void
sail_init ()
{
  pthread_mutex_init (&serverinst.mut, NULL);
  sail_init_collection (&serverinst.clients, SAIL_MAX_CLIENT_CONNECTIONS);
}

void
sail_deinit ()
{
  sail_deinit_collection (&serverinst.clients);
  pthread_mutex_destroy (&serverinst.mut);
}

void
sail_terminate_channel (sail_channel_t *chan)
{
  epoll_ctl (serverinst.conn.sockfd, EPOLL_CTL_DEL, chan->conn.sockfd, NULL);
  close (chan->conn.sockfd);
  SAIL_LOCK ();
  sail_remove_collection_channel (&serverinst.clients, chan);
  SAIL_UNLOCK ();
  sail_destroy_channel (chan);
}

int
sail_parse_command (sail_channel_t *chan)
{
  int retval;
  char *eol;
  char *delim;
  int verblen;
  char *args;
  int argslen;
  int i;
  sail_command_t *cmd;

  eol = strstr (chan->in.data, "\r\n");

  if (eol == NULL)
    {
      retval = -1;
      goto end;
    }

  if (((eol - chan->in.data) + 2) > SAIL_COMMAND_LINE_TOTAL_MAX_LENGTH)
    {
      retval = -1;
      goto end;
    }
  delim = strstr (chan->in.data, " ");

  if (delim == NULL)
    {
      args = delim = eol;
    }
  else
    {
      args = delim + 1;
    }
  verblen = delim - chan->in.data;
  chan->ctl.command.verb = calloc (verblen + 1, sizeof (char));
  memcpy (chan->ctl.command.verb, chan->in.data, verblen);

  for (i = 0; i < verblen; ++i)
    {
      chan->ctl.command.verb[i] = tolower (chan->ctl.command.verb[i]);
    }
  argslen = eol - args;

  if (argslen > 0)
    {
      chan->ctl.command.args = calloc (argslen + 1, sizeof (char));
      memcpy (chan->ctl.command.args, args, argslen);
    }
  retval = 0;

end:
  return retval;
}

int
sail_append_reply (sail_channel_t *chan, int linelen, int code, char delim,
                   char *text)
{
  int retval;
  int rv;
  char *offset;

  offset = chan->out.data + strlen (chan->out.data);

  if (text == NULL)
    {
      rv = snprintf (offset, linelen, "%d\r\n", code);
    }
  else
    {
      rv = snprintf (offset, linelen, "%d%c%s\r\n", code, delim, text);
    }

  if (rv < 0)
    {
      retval = -1;
      goto end;
    }
  retval = 0;

end:
  return retval;
}

int
sail_reset_command (sail_command_t *cmd)
{
  free (cmd->verb);
  free (cmd->args);
  memset (cmd, 0, sizeof (*cmd));
}

sail_command_action_t *
sail_get_command_action (char *keyname)
{
  sail_command_action_t *action;
  int i;

  action = NULL;

  for (i = 0; i < SAIL_MAX_REGISTERED_COMMANDS; ++i)
    {
      if (commandregistry->actions[i] != 0
          && strcmp (commandregistry.actions[i].keyname, keyname))
        {
          action = commandregistry.actions[i];
        }
    }

  return action;
}

void
sail_greet_routine (sail_channel_t *chan)
{
  int rv;

  sail_allocate_buffer (&chan->out, 32);
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
      chan->ctl.state.session_initiated = true;
      sail_reset_buffer (&chan->out);
    }
  SAIL_LOCK ();
  chan->status = SAIL_CHANNEL_STATUS_READY;
  SAIL_UNLOCK ();

end:
}

void
sail_proc_routine (sail_channel_t *chan)
{
  int rv;
  int i;

  sail_allocate_buffer (&chan->in, SAIL_COMMAND_LINE_TOTAL_MAX_LENGTH);
  rv = read (chan->conn.sockfd, chan->in.data, chan->in.sz);

  if (rv <= 0)
    {
      goto finish;
    }
  rv = sail_parse_command (chan);

  if (rv == -1)
    {
      goto finish;
    }

  // rv = write (chan->conn.sockfd, chan->out.data, strlen (chan->out.data));

cleanup:
  sail_reset_command (&chan->ctl.command);
  sail_reset_buffer (&chan->out);
finish:
  sail_reset_buffer (&chan->in);
  SAIL_LOCK ();
  chan->status = SAIL_CHANNEL_STATUS_READY;
  SAIL_UNLOCK ();
end:
}
