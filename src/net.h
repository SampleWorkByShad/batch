#ifndef NET_H_
#define NET_H_

#include <netinet/in.h>
#include <sys/socket.h>

#define BAT_MAX_CONNECTIONS 1024
#define BAT_READ_BUFFER_SIZE 256

typedef struct
{
  int sockfd;
  struct sockaddr_in sockaddr;
  socklen_t sockaddr_len;
} bat_connection_t;

typedef struct
{
  bat_connection_t conn;
  char *in;
  size_t insz;
  char *out;
  size_t outsz;
  volatile int state;
} bat_channel_t;

typedef struct
{
  bat_channel_t *items[BAT_MAX_CONNECTIONS];
  size_t listsz;
} bat_channel_list_t;

void bat_connection_init (bat_connection_t *);

void bat_connection_deinit (bat_connection_t *);

int bat_connection_setup (bat_connection_t *, char *, int);

bat_channel_t *bat_channel_create ();

void bat_channel_init (bat_channel_t *);

void bat_channel_destroy (bat_channel_t *);

void bat_channel_list_init (bat_channel_list_t *);

int bat_channel_list_slot (bat_channel_list_t *);

int bat_channel_list_insert (bat_channel_list_t *, int, bat_channel_t *);

bat_channel_t *bat_channel_list_find_by_fd (int, bat_channel_list_t *);

void bat_channel_list_destroy (bat_channel_list_t *);

#endif