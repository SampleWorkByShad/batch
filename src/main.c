#include <arpa/inet.h>
#include <config.h>
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

#include "sail.h"
#include "types.h"

#define BAT_MAX_FILE_NAME 255
#define BAT_CONNECTION_STATE_TERMINATE 100
#define BAT_MAX_EVENTS 1000

pthread_mutex_t mut;
sail_connection_t serverconn;
sail_collection_t clientchans;

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
  sail_channel_t *chan, *newchan;
  sail_pool_t greetpool, procpool;
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
  sail_connection_init (&serverconn);
  memset (&serverconn.sockaddr, 0, serverconn.sockaddr_len);
  serverconn.sockaddr.sin_family = AF_INET;
  serverconn.sockaddr.sin_port = htons (3000);
  inet_aton ("0.0.0.0", &serverconn.sockaddr.sin_addr);
  rv = bind (serverconn.sockfd, (struct sockaddr *)&serverconn.sockaddr,
             serverconn.sockaddr_len);

  if (rv == -1)
    {
      perror ("bind() failed");
      retval = -1;
      goto cleanup;
    }
  rv = listen (serverconn.sockfd, 50);

  if (rv == -1)
    {
      perror ("listen() failed");
      retval = -1;
      goto cleanup;
    }
  retval = EXIT_SUCCESS;
  sail_init ();
  sail_pool_init (&greetpool, 3, 1000, &sail_greet_routine);
  sail_pool_activate (&greetpool);
  sail_pool_ready (&greetpool);
  sail_pool_init (&procpool, 7, 1000, &sail_proc_routine);
  sail_pool_activate (&procpool);
  sail_pool_ready (&procpool);
  caddr_len = sizeof (caddr);
  epollfd = epoll_create1 (0);

  if (epollfd == -1)
    {
      perror ("epoll_create1");
      goto cleanup;
    }
  ev.events = EPOLLIN;
  ev.data.fd = serverconn.sockfd;
  rv = epoll_ctl (epollfd, EPOLL_CTL_ADD, serverconn.sockfd, &ev);

  if (rv == -1)
    {
      perror ("epoll_ctl() failed");
      goto cleanup;
    }

  while (true)
    {
      nfds = epoll_wait (epollfd, events, BAT_MAX_EVENTS, -1);

      if (nfds == -1)
        {
          perror ("epoll_wait");
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
              fcntl (cfd, F_SETFL, O_NONBLOCK);
              ev.events = EPOLLIN | EPOLLET;
              ev.data.fd = cfd;

              if (epoll_ctl (epollfd, EPOLL_CTL_ADD, cfd, &ev) == -1)
                {
                  perror ("epoll_ctl() failed");
                  exit (EXIT_FAILURE);
                }
              newchan = sail_channel_create ();
              memcpy (&newchan->conn.sockaddr, &caddr,
                      newchan->conn.sockaddr_len);
              newchan->conn.sockfd = cfd;
              SAIL_LOCK ();
              rv = sail_collection_add (&clientchans, newchan);
              if (rv != -1)
                {
                  newchan->key = rv;
                  rv = sail_pool_queue_add (&greetpool, (void *)newchan);

                  if (rv != -1)
                    {
                      newchan->status = SAIL_CHANNEL_STATUS_PROCESSING;
                      sail_pool_notify (&greetpool);
                    }
                }
              else
                {
                  perror ("sail_collection_add() failed");
                  close (cfd);
                  sail_channel_destroy (newchan);
                }
              SAIL_UNLOCK ();
            }
          else
            {
              if (events[i].events & EPOLLIN)
                {
                  SAIL_LOCK ();
                  chan = sail_collection_get_by_sockfd (&clientchans,
                                                         events[i].data.fd);
                  if (chan != NULL
                      && chan->status == SAIL_CHANNEL_STATUS_READY)
                    {
                      rv = sail_pool_queue_add (&procpool, (void *)chan);

                      if (rv != -1)
                        {
                          chan->status = SAIL_CHANNEL_STATUS_PROCESSING;
                          sail_pool_notify (&procpool);
                        }
                    }
                  SAIL_UNLOCK ();
                }
            }
        }
    }

cleanup:
  sail_pool_deactivate (&procpool);
  sail_pool_notify (&procpool);
  sail_pool_winddown (&procpool);
  sail_pool_deinit (&procpool);
  sail_pool_deactivate (&greetpool);
  sail_pool_notify (&greetpool);
  sail_pool_winddown (&greetpool);
  sail_pool_deinit (&greetpool);
  sail_deinit ();

end:
  return retval;
}