#include "chann.h"

void
chan_connection_init (chan_connection_t *conn)
{
  conn->sockaddr_len = sizeof (conn->sockaddr);
}

chan_channel_t *
chan_create ()
{
  chan_channel_t *chan;

  chan = (chan_channel_t *)malloc (sizeof (*chan));

  return chan;
}

void
chan_init (chan_channel_t *chan)
{
  chan->state = 0;
  chan_connection_init (&chan->conn);
}

void
chan_destroy (chan_channel_t *chan)
{
  free (chan);
}

int
chan_collection_init (chan_collection_t *c, size_t sz)
{
  int retval;

  c->channs = (chan_channel_t **)calloc (sz, sizeof (*c->channs));

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
chan_collection_add (chan_collection_t *c, chan_channel_t *chan)
{
  int i;

  for (i = 0; i < c->sz; ++i)
    {
      if (c->channs[i] == 0)
        {
          c->channs[i] = chan;
          break;
        }
    }

  return i == c->sz ? -1 : i;
}

chan_channel_t *
chan_collection_find_by_sockfd (chan_collection_t *c, int sockfd)
{
  chan_channel_t *chan;
  int i;

  chan = NULL;

  for (i = 0; i < c->sz; ++i)
    {
      if (c->channs[i] != 0 && c->channs[i]->conn.sockfd == sockfd)
        {
          chan = c->channs[i];
          break;
        }
    }

  return chan;
}

int
chan_collection_deinit (chan_collection_t *c)
{
  free (c->channs);
}