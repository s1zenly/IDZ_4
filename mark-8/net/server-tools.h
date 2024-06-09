#pragma once

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>

#include "../util/config.h"
#include "net-config.h"
#include "server-logs-queue.h"

enum {
    MAX_NUMBER_OF_FIRST_WORKERS  = 3,
    MAX_NUMBER_OF_SECOND_WORKERS = 5,
    MAX_NUMBER_OF_THIRD_WORKERS  = 2,

    MAX_WORKERS_PER_SERVER =
        MAX_NUMBER_OF_FIRST_WORKERS + MAX_NUMBER_OF_SECOND_WORKERS + MAX_NUMBER_OF_THIRD_WORKERS,
    MAX_CONNECTIONS_PER_SERVER = MAX_WORKERS_PER_SERVER
};

typedef struct Server {
    int sock_fd;
    struct sockaddr_in sock_addr;
    struct ServerLogsQueue logs_queue;
} Server[1];

bool init_server(Server server, uint16_t server_port);
void deinit_server(Server server);
bool nonblocking_poll(Server server);
void send_shutdown_signal_to_all(const Server server);

bool nonblocking_enqueue_log(Server server, const ServerLog* log);
bool dequeue_log(Server server, ServerLog* log);
bool send_server_log(const Server server, const ServerLog* log);
