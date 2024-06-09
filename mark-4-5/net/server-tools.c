#include "server-tools.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../util/config.h"
#include "net-config.h"
#include "server-log.h"

typedef enum HandleResult {
    EMPTY_CLIENT_SOCKET,
    DEAD_CLIENT_SOCKET,
    UNKNOWN_SOCKET_ERROR,
} HandleResult;

static HandleResult handle_socket_op_error(const char* cause) {
    switch (errno) {
        case EAGAIN:
            return EMPTY_CLIENT_SOCKET;
        case EPIPE:
        case ENETDOWN:
        case ENETUNREACH:
        case ENETRESET:
        case ECONNABORTED:
        case ECONNRESET:
        case ENOTCONN:
        case ECONNREFUSED:
        case EHOSTDOWN:
        case EHOSTUNREACH:
            return DEAD_CLIENT_SOCKET;
        default:
            app_perror(cause);
            return UNKNOWN_SOCKET_ERROR;
    }
}

static bool is_socket_alive(int sock_fd) {
    int error     = 0;
    socklen_t len = sizeof(error);
    int retval    = getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, &len);
    if (retval != 0) {
        errno = retval;
        app_perror("getsockopt");
        return false;
    }
    if (error != 0) {
        errno = error;
        handle_socket_op_error("is_socket_alive");
        return false;
    }
    return true;
}

static bool setup_server(int server_sock_fd, struct sockaddr_in* server_address,
                         uint16_t server_port) {
    if (-1 == setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &(int){true}, sizeof(int))) {
        app_perror("setsockopt[SOL_SOCKET,SO_REUSEADDR]");
        return false;
    }
   
    if (-1 == setsockopt(server_sock_fd, SOL_SOCKET, SO_BROADCAST, &(int){true}, sizeof(int))) {
        app_perror("setsockopt[SOL_SOCKET,SO_BROADCAST]");
        return false;
    }

    *server_address = (struct sockaddr_in){
        .sin_family      = AF_INET,
        .sin_port        = htons(server_port),
        .sin_addr.s_addr = htonl(INADDR_BROADCAST)
    };
    if (-1 ==
        bind(server_sock_fd, (const struct sockaddr*)server_address, sizeof(*server_address))) {
        app_perror("bind");
        return false;
    }

    return true;
}

bool init_server(Server server, uint16_t server_port) {
    memset(server, 0, sizeof(*server));
    server->sock_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server->sock_fd == -1) {
        app_perror("socket");
        return false;
    }
    if (!setup_server(server->sock_fd, &server->sock_addr, server_port)) {
        close(server->sock_fd);
        return false;
    }
    return true;
}

void deinit_server(Server server) {
    int sock_fd = server->sock_fd;
    assert(sock_fd != -1);
    if (close(sock_fd) == -1) {
        app_perror("close");
    }
}

typedef struct ClientMetaInfo {
    char host[48];
    char port[16];
    char numeric_host[48];
    char numeric_port[16];
} ClientMetaInfo;

static void fill_client_metainfo(ClientMetaInfo* info, const struct sockaddr_in* client_addr) {
    int gai_err = getnameinfo((const struct sockaddr*)client_addr, sizeof(*client_addr), info->host,
                              sizeof(info->host), info->port, sizeof(info->port), 0);
    if (gai_err != 0) {
        fprintf(stderr, "> Could not fetch info about socket address: %s\n", gai_strerror(gai_err));
        strcpy(info->host, "unknown host");
        strcpy(info->port, "unknown port");
    }

    gai_err = getnameinfo((const struct sockaddr*)client_addr, sizeof(*client_addr),
                          info->numeric_host, sizeof(info->numeric_host), info->numeric_port,
                          sizeof(info->numeric_port), NI_NUMERICSERV | NI_NUMERICHOST);
    if (gai_err != 0) {
        fprintf(stderr, "> Could not fetch info about socket address: %s\n", gai_strerror(gai_err));
        strcpy(info->numeric_host, "unknown host");
        strcpy(info->numeric_port, "unknown port");
    }
}

static void handle_log(const Server server, const ServerLog* log) {
    (void)server;
    assert(log->message[0] != '\0');
    puts(log->message);
}

static const struct sockaddr_in* cast_to_sockaddr_in(
    const struct sockaddr_storage* broadcast_address_storage, socklen_t broadcast_address_size) {
    return broadcast_address_size == sizeof(struct sockaddr_in)
               ? (const struct sockaddr_in*)broadcast_address_storage
               : NULL;
}

static bool send_message(const Server server, const UDPMessage* message) {
    bool ok = sendto(server->sock_fd, message, sizeof(*message), 0,
                     (const struct sockaddr*)&server->sock_addr,
                     sizeof(server->sock_addr)) == sizeof(*message);
    if (!ok) {
        app_perror("sendto");
    }
    return ok;
}

static bool server_handle_pin_from_first_stage_worker(const Server server, Pin pin) {
    ServerLog log;
    int ret = snprintf(log.message, sizeof(log.message),
                       "> Transferring pin[pin_id=%d] to the second stage workers\n", pin.pin_id);
    if (ret <= 0) {
        app_perror("snprintf");
    }
    handle_log(server, &log);

    UDPMessage message = {
        .sender_type         = COMPONENT_TYPE_SERVER,
        .receiver_type       = COMPONENT_TYPE_SECOND_STAGE_WORKER,
        .message_type        = MESSAGE_TYPE_PIN_TRANSFERRING,
        .message_content.pin = pin,
    };
    return send_message(server, &message);
}

static bool server_handle_pin_from_second_stage_worker(const Server server, Pin pin) {
    ServerLog log;
    int ret = snprintf(log.message, sizeof(log.message),
                       "> Transferring pin[pin_id=%d] to the third stage workers\n", pin.pin_id);
    if (ret <= 0) {
        app_perror("snprintf");
    }
    handle_log(server, &log);

    UDPMessage message = {
        .sender_type         = COMPONENT_TYPE_SERVER,
        .receiver_type       = COMPONENT_TYPE_THIRD_STAGE_WORKER,
        .message_type        = MESSAGE_TYPE_PIN_TRANSFERRING,
        .message_content.pin = pin,
    };
    return send_message(server, &message);
}

static void server_handle_invalid_pin_source(ComponentType pin_source, const Server server, Pin pin,
                                             const ClientMetaInfo* info) {
    ServerLog log;
    int ret = snprintf(log.message, sizeof(log.message),
                       "> Error: invalid source %s[address=%s:%s | %s:%s] of the pin[pin_id=%d]\n",
                       component_type_to_string(pin_source), info->host, info->port,
                       info->numeric_host, info->numeric_port, pin.pin_id);
    if (ret <= 0) {
        app_perror("snprintf");
    }
    handle_log(server, &log);
}

static bool server_handle_pin_transferring(const Server server, const UDPMessage* message,
                                           const ClientMetaInfo* info) {
    if (message->receiver_type != COMPONENT_TYPE_SERVER) {
        return true;
    }

    Pin pin = message->message_content.pin;
    ServerLog log;
    int ret = snprintf(log.message, sizeof(log.message),
                       "> Received pin[pin_id=%d] from the\n> %s[address=%s:%s | %s:%s]\n",
                       pin.pin_id, component_type_to_string(message->sender_type), info->host,
                       info->port, info->numeric_host, info->numeric_port);
    if (ret <= 0) {
        app_perror("snprintf");
    }
    handle_log(server, &log);
    if (message->receiver_type != COMPONENT_TYPE_SERVER) {
        return true;
    }

    switch (message->sender_type) {
        case COMPONENT_TYPE_FIRST_STAGE_WORKER:
            return server_handle_pin_from_first_stage_worker(server, pin);
        case COMPONENT_TYPE_SECOND_STAGE_WORKER:
            return server_handle_pin_from_second_stage_worker(server, pin);
        default:
            server_handle_invalid_pin_source(message->sender_type, server, pin, info);
            return true;
    }
}

static void server_handle_new_client(const Server server, const UDPMessage* message,
                                     const ClientMetaInfo* info) {
    const char* client_type_str = component_type_to_string(message->sender_type);
    ServerLog log;
    int ret =
        snprintf(log.message, sizeof(log.message),
                 "> New client with type \"%s\"[address=%s:%s | %s:%s] sent signal of presence\n",
                 client_type_str, info->host, info->port, info->numeric_host, info->numeric_port);
    if (ret <= 0) {
        app_perror("snprintf");
    }
    handle_log(server, &log);
}

static void server_handle_shutdown_signal(const Server server, const UDPMessage* message,
                                          const ClientMetaInfo* info) {
    ServerLog log = {0};
    int ret       = snprintf(log.message, sizeof(log.message),
                             "> Warning: received shutdown signal from %s[address=%s:%s | %s:%s]\n",
                             component_type_to_string(message->sender_type), info->host, info->port,
                             info->numeric_host, info->numeric_port);
    if (ret <= 0) {
        app_perror("snprintf");
    }
    handle_log(server, &log);
}

static void server_handle_invalid_message_type(const Server server, const UDPMessage* message,
                                               const ClientMetaInfo* info) {
    ServerLog log;
    int ret = snprintf(
        log.message, sizeof(log.message),
        "> Error: invalid message type %s[value=%u] from the\n> %s[address=%s:%s | %s:%s]\n",
        message_type_to_string(message->message_type), (uint32_t)message->message_type,
        component_type_to_string(message->sender_type), info->host, info->port, info->numeric_host,
        info->numeric_port);
    if (ret <= 0) {
        app_perror("snprintf");
    }
    handle_log(server, &log);
}

bool nonblocking_poll(const Server server) {
    UDPMessage message                                = {0};
    struct sockaddr_storage broadcast_address_storage = {0};
    socklen_t broadcast_address_size                  = sizeof(broadcast_address_storage);
    ssize_t received_size =
        recvfrom(server->sock_fd, &message, sizeof(message), 0,
                 (struct sockaddr*)&broadcast_address_storage, &broadcast_address_size);
    if (received_size < 0) {
        app_perror("recvfrom");
        return false;
    }

    const struct sockaddr_in* sock_addr =
        cast_to_sockaddr_in(&broadcast_address_storage, broadcast_address_size);
    if (sock_addr == NULL) {
        fprintf(stderr, "> Unknown message sender of size %u\n", broadcast_address_size);
        return true;
    }

    ClientMetaInfo info;
    fill_client_metainfo(&info, sock_addr);
    switch (message.message_type) {
        case MESSAGE_TYPE_PIN_TRANSFERRING:
            return server_handle_pin_transferring(server, &message, &info);
        case MESSAGE_TYPE_NEW_CLIENT:
            server_handle_new_client(server, &message, &info);
            break;
        case MESSAGE_TYPE_SHUTDOWN_MESSAGE:
            server_handle_shutdown_signal(server, &message, &info);
            break;
        default:
            server_handle_invalid_message_type(server, &message, &info);
            break;
    }

    return true;
}

void send_shutdown_signal_to_all(const Server server) {
    UDPMessage message = {
        .sender_type           = COMPONENT_TYPE_SERVER,
        .receiver_type         = COMPONENT_TYPE_ANY_WORKER,
        .message_type          = MESSAGE_TYPE_SHUTDOWN_MESSAGE,
        .message_content.bytes = {0},
    };
    send_message(server, &message);
}
