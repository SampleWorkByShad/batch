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

sail_command_registry_t commandregistry
    = { .actions = {
            { .keyname = "helo", .handler = &sail_helo_action_handler },
            { .keyname = "ehlo", .handler = &sail_ehlo_action_handler },
            { .keyname = "mail", .handler = &sail_mail_action_handler },
            { .keyname = "rcpt", .handler = &sail_rcpt_action_handler },
            { .keyname = "data", .handler = &sail_data_action_handler },
            { .keyname = "rset", .handler = &sail_rset_action_handler },
            { .keyname = "noop", .handler = &sail_noop_action_handler },
            { .keyname = "quit", .handler = &sail_quit_action_handler },
            { .keyname = "vrfy", .handler = &sail_vrfy_action_handler },
        } };
struct sail_server serverinst;

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
  struct epoll_event ev, events[SAIL_EPOLL_MAX_EVENTS];

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
  serverinst.conn.sockfd = sfd;
  sockopt = 1;
  rv = setsockopt (serverinst.conn.sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt,
                   sizeof (sockopt));

  if (rv == -1)
    {
      perror ("setsockopt() failed");
    }
  sail_init_connection (&serverinst.conn);
  memset (&serverinst.conn.sockaddr, 0, serverinst.conn.sockaddr_len);
  serverinst.conn.sockaddr.sin_family = AF_INET;
  serverinst.conn.sockaddr.sin_port = htons (3000);
  inet_aton ("0.0.0.0", &serverinst.conn.sockaddr.sin_addr);
  rv = bind (serverinst.conn.sockfd,
             (struct sockaddr *)&serverinst.conn.sockaddr,
             serverinst.conn.sockaddr_len);

  if (rv == -1)
    {
      perror ("bind() failed");
      retval = -1;
      goto cleanup;
    }
  rv = listen (serverinst.conn.sockfd, 50);

  if (rv == -1)
    {
      perror ("listen() failed");
      retval = -1;
      goto cleanup;
    }
  retval = EXIT_SUCCESS;
  sail_init ();
  sail_init_pool (&greetpool, 3, 1000, &sail_greet_routine);
  sail_activate_pool (&greetpool);
  sail_pool_ready (&greetpool);
  sail_init_pool (&procpool, 7, 1000, &sail_proc_routine);
  sail_activate_pool (&procpool);
  sail_pool_ready (&procpool);
  caddr_len = sizeof (caddr);
  epollfd = epoll_create1 (0);

  if (epollfd == -1)
    {
      perror ("epoll_create1");
      goto cleanup;
    }
  ev.events = EPOLLIN;
  ev.data.fd = serverinst.conn.sockfd;
  rv = epoll_ctl (epollfd, EPOLL_CTL_ADD, serverinst.conn.sockfd, &ev);

  if (rv == -1)
    {
      perror ("epoll_ctl() failed");
      goto cleanup;
    }

  while (true)
    {
      nfds = epoll_wait (epollfd, events, SAIL_EPOLL_MAX_EVENTS, -1);

      if (nfds == -1)
        {
          perror ("epoll_wait");
          goto cleanup;
        }

      for (i = 0; i <= nfds; ++i)
        {
          if (events[i].data.fd == serverinst.conn.sockfd)
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
              newchan = sail_create_channel ();
              memcpy (&newchan->conn.sockaddr, &caddr,
                      newchan->conn.sockaddr_len);
              newchan->conn.sockfd = cfd;
              SAIL_LOCK ();
              rv = sail_add_collection_channel (&serverinst.clients, newchan);
              if (rv != -1)
                {
                  rv = sail_add_pool_queue_channel (&greetpool,
                                                    (void *)newchan);

                  if (rv != -1)
                    {
                      newchan->status = SAIL_CHANNEL_STATUS_PROCESSING;
                      sail_notify_pool (&greetpool);
                    }
                }
              else
                {
                  perror ("sail_add_collection_channel() failed");
                  close (cfd);
                  sail_destroy_channel (newchan);
                }
              SAIL_UNLOCK ();
            }
          else
            {
              if (events[i].events & EPOLLIN)
                {
                  SAIL_LOCK ();
                  chan = sail_get_collection_channel_by_sockfd (
                      &serverinst.clients, events[i].data.fd);
                  if (chan != NULL
                      && chan->status == SAIL_CHANNEL_STATUS_READY)
                    {
                      rv = sail_add_pool_queue_channel (&procpool, chan);

                      if (rv != -1)
                        {
                          chan->status = SAIL_CHANNEL_STATUS_PROCESSING;
                          sail_notify_pool (&procpool);
                        }
                    }
                  SAIL_UNLOCK ();
                }
            }
        }
    }

cleanup:
  sail_deactivate_pool (&procpool);
  sail_notify_pool (&procpool);
  sail_winddown_pool (&procpool);
  sail_deinit_pool (&procpool);
  sail_deactivate_pool (&greetpool);
  sail_notify_pool (&greetpool);
  sail_winddown_pool (&greetpool);
  sail_deinit_pool (&greetpool);
  sail_deinit ();

end:
  return retval;
}