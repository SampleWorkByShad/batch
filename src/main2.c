#include <arpa/inet.h>
#include <config.h>
#include <errno.h>
#include <fcntl.h>
#include <libconfig.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <search.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "jiffy.h"
#include "net.h"

pthread_mutex_t mut;

#define BAT_CONNECTION_STATE_TERMINATE 100

static void *
bat_routine_read (void *arg)
{
  bat_channel_t *chan;
  int rc;
  int state;

  chan = (bat_channel_t *)arg;

  state = 0;

  chan->insz = 100;
  chan->in = (char *)calloc (chan->insz, sizeof (char));
  rc = read (chan->conn.sockfd, chan->in, chan->insz);
  while (rc != -1)
    {
      if (strstr (chan->in, "quit") != NULL)
        {
          state = BAT_CONNECTION_STATE_TERMINATE;
          break;
        }
      memset (chan->in, '\0', chan->insz);
      rc = read (chan->conn.sockfd, chan->in, chan->insz);
    }
  free (chan->in);
  chan->insz = 0;

  pthread_mutex_lock (&mut);
  chan->state = state;
  pthread_mutex_unlock (&mut);
}

int
main (void)
{
  int status;
  int rv;
  int maxfd, cfd;
  int i;
  int slot;
  struct sockaddr_in caddr;
  socklen_t caddr_len;
  fd_set readfds, writefds, exceptfds;
  struct timeval timeout;
  bat_connection_t serverconn;
  bat_channel_list_t clientlist;
  bat_channel_t *chan, *newchan;
  jiffy_t readjf;

  status = EXIT_SUCCESS;

  bat_connection_setup (&serverconn, "0.0.0.0", 3000);

  bat_channel_list_init (&clientlist);

  jiff_init (&readjf, 10, 10, &bat_routine_read);
  jiff_activate (&readjf);
  jiff_ready (&readjf);

  pthread_mutex_init (&mut, NULL);

  caddr_len = sizeof (caddr);

  FD_ZERO (&readfds);
  FD_ZERO (&writefds);
  FD_ZERO (&exceptfds);

  FD_SET (serverconn.sockfd, &readfds);
  maxfd = serverconn.sockfd;

  while (true)
    {
      timeout.tv_sec = 10;

      pthread_mutex_lock (&mut);
      for (i = 0; i < clientlist.listsz; i++)
        {
          chan = clientlist.items[i];
          if (chan != NULL && chan->state == BAT_CONNECTION_STATE_TERMINATE)
            {
              // FD_CLR (chan->conn.sockfd, &readfds);
              // FD_CLR (chan->conn.sockfd, &writefds);
              // FD_CLR (chan->conn.sockfd, &exceptfds);
              // bat_connection_deinit (&chan->conn);
              // bat_channel_destroy (chan);
              // clientlist.items[i] = NULL;
              // printf ("destroyed\n");
            }
        }
      pthread_mutex_unlock (&mut);

      rv = select (maxfd + 1, &readfds, &writefds, &exceptfds, &timeout);
      if (rv == -1)
        {
          perror ("select() failed");
          status = EXIT_FAILURE;
          goto cleanup;
        }
      if (rv == 0)
        {
          printf ("timeout\n");
          continue;
        }

      for (i = 0; i <= maxfd; i++)
        {
          if (FD_ISSET (i, &readfds))
            {
              if (i == serverconn.sockfd)
                {
                  do
                    {
                      cfd = accept (i, (struct sockaddr *)&caddr, &caddr_len);

                      if (cfd == -1)
                        {
                          perror ("accept() failed");
                          continue;
                        }
                      newchan = bat_channel_create ();

                      if (newchan == NULL)
                        {
                          perror ("bat_channel_create() failed");
                          close (cfd);
                          continue;
                        }
                      slot = bat_channel_list_slot (&clientlist);

                      if (slot == -1)
                        {
                          perror ("bat_channel_list_slot() failed");
                          bat_channel_destroy (newchan);
                          continue;
                        }
                      bat_channel_init (newchan);
                      bat_connection_init (&newchan->conn);
                      memcpy (&newchan->conn.sockaddr, &caddr,
                              newchan->conn.sockaddr_len);
                      newchan->conn.sockfd = cfd;
                      rv = bat_channel_list_insert (&clientlist, slot,
                                                    newchan);

                      if (rv == -1)
                        {
                          perror ("bat_channel_list_insert() failed");

                          bat_connection_deinit (&newchan->conn);
                          bat_channel_destroy (newchan);
                          continue;
                        }

                      FD_SET (cfd, &readfds);

                      if (cfd > maxfd)
                        maxfd = cfd;

                      printf ("accepted %d\n", cfd);
                    }
                  while (cfd != -1);
                }
              else
                {
                  chan = bat_channel_list_find_by_fd (i, &clientlist);
                  pthread_mutex_lock (&mut);
                  if (chan->state == 0)
                    {
                      rv = jiffy_queue_add (&readjf, (void *)chan);
                      if (rv != -1)
                        {
                          chan->state = 1;
                          jiff_notify (&readjf);
                        }
                    }
                  pthread_mutex_unlock (&mut);
                }
            }
          if (FD_ISSET (i, &writefds))
            {
              perror ("writefds");
            }
          if (FD_ISSET (i, &exceptfds))
            {
              perror ("exceptfds");
            }
        }
    }

cleanup:
  pthread_mutex_destroy (&mut);

  jiff_deactivate (&readjf);
  jiff_notify (&readjf);
  jiff_winddown (&readjf);
  jiff_deinit (&readjf);

  bat_channel_list_destroy (&clientlist);

  rv = close (serverconn.sockfd) == -1;
  if (rv)
    {
      perror ("failed to close sfd");
    }

end:
  exit (status);
}
