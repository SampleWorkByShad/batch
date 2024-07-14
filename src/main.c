#include <arpa/inet.h>
#include <config.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "chann.h"
#include "jiffy.h"

#define BAT_MAX_EVENTS 100

pthread_mutex_t mut;

#define bat_lock() pthread_mutex_lock (&mut);
#define bat_unlock() pthread_mutex_unlock (&mut);

chan_connection_t serverconn;
chan_collection_t clientchans;

static void
bat_init ()
{
  pthread_mutex_init (&mut, NULL);
  chan_collection_init (&clientchans, 10);
}

static void
bat_deinit ()
{
  chan_collection_deinit (&clientchans);
  pthread_mutex_destroy (&mut);
}

static void
bat_terminate (chan_channel_t *chan)
{
  bat_lock ();
  epoll_ctl (serverconn.sockfd, EPOLL_CTL_DEL, chan->conn.sockfd, NULL);
  close (chan->conn.sockfd);
  clientchans.channs[chan->key] = 0;
  chan_destroy (chan);
  bat_unlock ();
}

static void *
bat_routine_read (void *arg)
{
  chan_channel_t *chan;
  int rc;
  int state;
  bool quit;

  chan = (chan_channel_t *)arg;

  state = 0;
  chan->insz = 100;
  chan->in = (char *)calloc (chan->insz, sizeof (char));
  rc = read (chan->conn.sockfd, chan->in, chan->insz);
  while (rc > 0)
    {
      if (strstr (chan->in, "quit") != NULL)
        {
          quit = true;
          break;
        }
      memset (chan->in, '\0', chan->insz);
      rc = read (chan->conn.sockfd, chan->in, chan->insz);
    }
  free (chan->in);
  chan->insz = 0;

  if (quit)
    {
      bat_terminate (chan);
    }
  else
    {
      bat_lock ();
      chan->state = state;
      bat_unlock ();
    }
}

int
main (int argc, char *argv[])
{
  int retval;
  int rv;
  int sfd, cfd, epollfd, nfds;
  int i;
  struct sockaddr_in caddr;
  socklen_t caddr_len;
  struct protoent *proto;
  int sockopt;
  chan_channel_t *chan, *newchan;
  jiff_pool_t readpool;
  struct epoll_event ev, events[BAT_MAX_EVENTS];

  proto = getprotobyname ("tcp");

  if (proto == NULL)
    {
      perror ("getprotobyname() failed");
      retval = EXIT_FAILURE;
      goto end;
    }
  sfd = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, proto->p_proto);

  if (sfd == -1)
    {
      perror ("socket() failed");
      retval = EXIT_FAILURE;
      goto end;
    }
  serverconn.sockfd = sfd;
  sockopt = 1;
  rv = setsockopt (serverconn.sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt,
                   sizeof (sockopt));

  if (rv == -1)
    {
      perror ("setsockopt() failed");
    }
  chan_connection_init (&serverconn);
  memset (&serverconn.sockaddr, 0, serverconn.sockaddr_len);
  serverconn.sockaddr.sin_family = AF_INET;
  serverconn.sockaddr.sin_port = htons (3000);
  inet_aton ("0.0.0.0", &serverconn.sockaddr.sin_addr);
  rv = bind (serverconn.sockfd, (struct sockaddr *)&serverconn.sockaddr,
             serverconn.sockaddr_len);

  if (rv == -1)
    {
      perror ("bind() failed");
      retval = EXIT_FAILURE;
      goto cleanup;
    }
  rv = listen (serverconn.sockfd, 50);

  if (rv == -1)
    {
      perror ("listen() failed");
      retval = EXIT_FAILURE;
      goto cleanup;
    }
  retval = EXIT_SUCCESS;
  bat_init ();
  jiff_init (&readpool, 10, 10, &bat_routine_read);
  jiff_activate (&readpool);
  jiff_ready (&readpool);
  caddr_len = sizeof (caddr);
  epollfd = epoll_create1 (0);

  if (epollfd == -1)
    {
      perror ("epoll_create1() failed");
      retval = EXIT_FAILURE;
      goto cleanup;
    }
  ev.events = EPOLLIN;
  ev.data.fd = serverconn.sockfd;
  rv = epoll_ctl (epollfd, EPOLL_CTL_ADD, serverconn.sockfd, &ev);

  if (rv == -1)
    {
      perror ("epoll_ctl() failed");
      retval = EXIT_FAILURE;
      goto cleanup;
    }

  while (true)
    {
      nfds = epoll_wait (epollfd, events, BAT_MAX_EVENTS, -1);

      if (nfds == -1)
        {
          perror ("epoll_wait() failed");
          retval = EXIT_FAILURE;
          goto cleanup;
        }

      for (i = 0; i <= nfds; ++i)
        {
          if (events[i].data.fd == serverconn.sockfd)
            {
              cfd = accept (events[i].data.fd, (struct sockaddr *)&caddr,
                            &caddr_len);

              if (cfd == -1)
                {

                  perror ("accept() failed");
                  continue;
                }
              printf ("accepted() = %d\n", cfd);
              fcntl (cfd, F_SETFL, O_NONBLOCK);
              ev.events = EPOLLIN | EPOLLET;
              ev.data.fd = cfd;
              rv = epoll_ctl (epollfd, EPOLL_CTL_ADD, cfd, &ev);

              if (rv == -1)
                {
                  perror ("epoll_wait() failed");
                  retval = EXIT_FAILURE;
                  goto cleanup;
                }
              newchan = chan_create ();
              chan_init (newchan);
              memcpy (&newchan->conn.sockaddr, &caddr,
                      newchan->conn.sockaddr_len);
              newchan->conn.sockfd = cfd;
              bat_lock ();
              rv = chan_collection_add (&clientchans, newchan);

              if (rv != -1)
                {
                  newchan->key = rv;
                }
              bat_unlock ();

              if (rv == -1)
                {
                  perror ("chan_collection_add() failed");
                  close (cfd);
                  chan_destroy (newchan);
                }
            }
          else
            {
              if (events[i].events & EPOLLIN)
                {
                  bat_lock ();
                  chan = chan_collection_find_by_sockfd (&clientchans,
                                                         events[i].data.fd);
                  if (chan != NULL && chan->state == 0)
                    {
                      rv = jiff_queue_add (&readpool, (void *)chan);

                      if (rv != -1)
                        {
                          chan->state = 1;
                          jiff_notify (&readpool);
                        }
                    }
                  bat_unlock ();
                }
            }
        }
    }
cleanup:
  jiff_deactivate (&readpool);
  jiff_notify (&readpool);
  jiff_winddown (&readpool);
  jiff_deinit (&readpool);
  bat_deinit ();

end:
  return retval;
}