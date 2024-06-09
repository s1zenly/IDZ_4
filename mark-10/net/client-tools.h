#pragma once

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../util/config.h"
#include "net-config.h"
#include "pin.h"
#include "server-log.h"

typedef struct Client {
    int client_sock_fd;
    ComponentType type;
    struct sockaddr_in server_broadcast_sock_addr;
} Client[1];

bool init_client(Client client, uint16_t server_port, ComponentType type);
void deinit_client(Client client);

static inline bool is_worker(const Client client) {
    switch (client->type) {
        case COMPONENT_TYPE_FIRST_STAGE_WORKER:
        case COMPONENT_TYPE_SECOND_STAGE_WORKER:
        case COMPONENT_TYPE_THIRD_STAGE_WORKER:
            return true;
        default:
            return false;
    }
}

bool client_should_stop(const Client client);
void print_sock_addr_info(const struct sockaddr* address, socklen_t sock_addr_len);
static inline void print_client_info(const Client client) {
    print_sock_addr_info((const struct sockaddr*)&client->server_broadcast_sock_addr,
                         sizeof(client->server_broadcast_sock_addr));
}
Pin receive_new_pin(void);
bool check_pin_crookness(Pin pin);
bool send_not_croocked_pin(const Client worker, Pin pin);
bool receive_not_crooked_pin(const Client worker, Pin* rec_pin);
void sharpen_pin(Pin pin);
bool send_sharpened_pin(const Client worker, Pin pin);
bool receive_sharpened_pin(const Client worker, Pin* rec_pin);
bool check_sharpened_pin_quality(Pin sharpened_pin);
bool receive_server_log(const Client logs_collector, ServerLog* log);
ServerCommandResult send_manager_command_to_server(const Client manager, ServerCommand command);
