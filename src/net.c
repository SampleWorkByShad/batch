#include "net.h"

#include <arpa/inet.h>
#include <config.h>
#include <errno.h>
#include <libconfig.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void
bat_connection_init (bat_connection_t *conn)
{
  conn->sockaddr_len = sizeof (conn->sockaddr);
}

void
bat_connection_deinit (bat_connection_t *conn)
{
  close (conn->sockfd);
}

int
bat_connection_setup (bat_connection_t *conn, char *host, int port)
{
  int status;
  int rv;
  struct protoent *proto;
  int opt;

  status = 0;

  proto = getprotobyname ("tcp");
  if (proto == NULL)
    {
      perror ("getprotobyname() failed");
      status = -1;
      goto end;
    }

  rv = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, proto->p_proto);
  if (rv == -1)
    {
      perror ("socket() failed");
      status = -1;
      goto end;
    }
  conn->sockfd = rv;

  opt = 1;
  rv = setsockopt (conn->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
  if (rv == -1)
    {
      perror ("setsockopt() failed");
    }
  bat_connection_init (conn);

  memset (&conn->sockaddr, 0, conn->sockaddr_len);
  conn->sockaddr.sin_family = AF_INET;
  conn->sockaddr.sin_port = htons (port);
  inet_aton (host, &conn->sockaddr.sin_addr);

  rv = bind (conn->sockfd, (struct sockaddr *)&conn->sockaddr,
             conn->sockaddr_len);
  if (rv == -1)
    {
      perror ("bind() failed");
      status = -1;
      goto end;
    }

  rv = listen (conn->sockfd, 50);
  if (rv == -1)
    {
      perror ("listen() failed");
      status = -1;
      goto end;
    }

end:
  return status;
}

bat_channel_t *
bat_channel_create ()
{
  bat_channel_t *chan;
  chan = (bat_channel_t *)calloc (1, sizeof (bat_channel_t));
  return chan;
}

void
bat_channel_init (bat_channel_t *chan)
{
  chan->state = 0;
}

void
bat_channel_destroy (bat_channel_t *chan)
{
  free (chan);
}

void
bat_channel_list_init (bat_channel_list_t *list)
{
  list->listsz = BAT_MAX_CONNECTIONS;
  memset (&list->items, 0, list->listsz);
}

int
bat_channel_list_slot (bat_channel_list_t *list)
{
  int slot;
  int i, len;

  slot = -1;

  for (i = 0; i < list->listsz; i++)
    {
      if (list->items[i] == 0)
        {
          slot = i;
          break;
        }
    }

  return slot;
}

int
bat_channel_list_insert (bat_channel_list_t *list, int slot,
                         bat_channel_t *chan)
{
  int status;

  status = slot;

  if (slot >= list->listsz || slot < 0 || list->items[slot] != 0)
    {
      status = -1;
      goto end;
    }
  list->items[slot] = chan;

end:
  return status;
}

bat_channel_t *
bat_channel_list_find_by_fd (int fd, bat_channel_list_t *list)
{
  bat_channel_t *chan;
  int i;

  chan = NULL;

  for (i = 0; i < list->listsz; i++)
    {
      if (list->items[i]->conn.sockfd == fd)
        {
          chan = list->items[i];
          break;
        }
    }

  return chan;
}

void
bat_channel_list_destroy (bat_channel_list_t *list)
{
  int i;

  for (i = 0; i < list->listsz; i++)
    {
      if (list->items[i] != NULL)
        {
          bat_connection_deinit (&list->items[i]->conn);
          bat_channel_destroy (list->items[i]);
        }
    }
}