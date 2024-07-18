#include "types.h"
#include <arpa/inet.h>
#include <config.h>
#include <ctype.h>
#include <errno.h>
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

#ifndef SAIL_H_
#define SAIL_H_

#define SAIL_TEXT_LINE_TOTAL_MAX_LENGTH 1000
#define SAIL_COMMAND_LINE_TOTAL_MAX_LENGTH 512
#define SAIL_REPLY_LINE_TOTAL_MAX_LENGTH 512

#define SAIL_MAX_CLIENT_CONNECTIONS 10000

#define SAIL_EPOLL_MAX_EVENTS 1000

#define SAIL_CHANNEL_STATUS_READY 0
#define SAIL_CHANNEL_STATUS_PROCESSING 1

#define SAIL_COMMAND_EQUALS(c, s) strcmp (c.verb, s) == 0

extern sail_command_registry_t commandregistry;
extern struct sail_server serverinst;

#define SAIL_LOCK() pthread_mutex_lock (&serverinst.mut)
#define SAIL_UNLOCK() pthread_mutex_unlock (&serverinst.mut)

int sail_allocate_buffer (sail_buffer_t *, size_t);
void sail_reset_buffer (sail_buffer_t *);

void sail_init_connection (sail_connection_t *);

sail_channel_t *sail_create_channel ();
void sail_destroy_channel (sail_channel_t *);

int sail_init_collection (sail_collection_t *, size_t);
int sail_add_collection_channel (sail_collection_t *, sail_channel_t *);
void sail_remove_collection_channel (sail_collection_t *, sail_channel_t *);
sail_channel_t *sail_get_collection_channel_by_sockfd (sail_collection_t *,
                                                       int);
int sail_deinit_collection (sail_collection_t *);

int sail_init_pool (sail_pool_t *, size_t, size_t, void (sail_channel_t *));
void *sail_pool_routine (void *);
void sail_activate_pool (sail_pool_t *);
void sail_deactivate_pool (sail_pool_t *);
void sail_pool_ready (sail_pool_t *);
int sail_add_pool_queue_channel (sail_pool_t *, sail_channel_t *);
void sail_notify_pool (sail_pool_t *);
void sail_winddown_pool (sail_pool_t *);
int sail_deinit_pool (sail_pool_t *);
int sail_parse_command (sail_channel_t *);
int sail_reset_command (sail_command_t *);

int sail_helo_action_handler (sail_channel_t *);
int sail_ehlo_action_handler (sail_channel_t *);
int sail_mail_action_handler (sail_channel_t *);
int sail_rcpt_action_handler (sail_channel_t *);
int sail_data_action_handler (sail_channel_t *);
int sail_rset_action_handler (sail_channel_t *);
int sail_noop_action_handler (sail_channel_t *);
int sail_quit_action_handler (sail_channel_t *);
int sail_vrfy_action_handler (sail_channel_t *);

void sail_init ();
void sail_deinit ();
void sail_terminate_channel (sail_channel_t *);
int sail_append_reply (sail_channel_t *, int, int, char, char *);
sail_command_action_t *sail_get_command_action (char *keyname);
void sail_greet_routine (sail_channel_t *);
void sail_proc_routine (sail_channel_t *);

#endif