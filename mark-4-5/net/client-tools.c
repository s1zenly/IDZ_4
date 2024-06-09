#include "client-tools.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../util/config.h"
#include "client-tools.h"
#include "net-config.h"

static bool send_client_type_info(const Client client) {
    const UDPMessage message = {
        .sender_type           = client->type,
        .receiver_type         = COMPONENT_TYPE_SERVER,
        .message_type          = MESSAGE_TYPE_NEW_CLIENT,
        .message_content.bytes = {0},
    };
    if (sendto(client->client_sock_fd, &message, sizeof(message), 0,
               (const struct sockaddr*)&client->server_broadcast_sock_addr,
               sizeof(client->server_broadcast_sock_addr)) != sizeof(message)) {
        app_perror("sendto");
        return false;
    }

    printf("Sent type \"%s\" of this client to the server\n",
           component_type_to_string(client->type));
    return true;
}

static bool setup_client(int client_sock_fd, struct sockaddr_in* client_send_address,
                         uint16_t server_port) {
    if (-1 == setsockopt(client_sock_fd, SOL_SOCKET, SO_REUSEADDR, &(int){true}, sizeof(int))) {
        app_perror("setsockopt[SOL_SOCKET,SO_REUSEADDR]");
        return false;
    }

    if (-1 == setsockopt(client_sock_fd, SOL_SOCKET, SO_BROADCAST, &(int){true}, sizeof(int))) {
        app_perror("setsockopt[SOL_SOCKET,SO_BROADCAST]");
        return false;
    }

    *client_send_address = (struct sockaddr_in){
        .sin_family      = AF_INET,
        .sin_port        = htons(server_port),
        .sin_addr.s_addr = htonl(INADDR_BROADCAST)
    };

    if (-1 == bind(client_sock_fd, (const struct sockaddr*)client_send_address,
                   sizeof(*client_send_address))) {
        app_perror("bind");
        return false;
    }
    return true;
}

bool init_client(Client client, uint16_t server_port, ComponentType type) {
    client->type = type;
    int sock_fd = client->client_sock_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd == -1) {
        app_perror("socket");
        return false;
    }

    if (!setup_client(sock_fd, &client->server_broadcast_sock_addr, server_port) ||
        !send_client_type_info(client)) {
        close(sock_fd);
        return false;
    }

    return true;
}

void deinit_client(Client client) {
    int sock_fd = client->client_sock_fd;
    assert(sock_fd != -1);
    close(sock_fd);
}

void print_sock_addr_info(const struct sockaddr* socket_address,
                          const socklen_t socket_address_size) {
    char host_name[1024] = {0};
    char port_str[16]    = {0};
    int gai_err =
        getnameinfo(socket_address, socket_address_size, host_name, sizeof(host_name), port_str,
                    sizeof(port_str), NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
    if (gai_err == 0) {
        printf(
            "Numeric socket address: %s\n"
            "Numeric socket port: %s\n",
            host_name, port_str);
    } else {
        fprintf(stderr, "Could not fetch info about socket address: %s\n", gai_strerror(gai_err));
    }

    gai_err = getnameinfo(socket_address, socket_address_size, host_name, sizeof(host_name),
                          port_str, sizeof(port_str), NI_DGRAM);
    if (gai_err == 0) {
        printf("Socket address: %s\nSocket port: %s\n", host_name, port_str);
    }
}

static bool client_handle_errno(const char* cause) {
    int errno_val = errno;
    switch (errno_val) {
        case EAGAIN:
        case ENETDOWN:
        case EPIPE:
        case ENETUNREACH:
        case ENETRESET:
        case ECONNABORTED:
        case ECONNRESET:
        case ECONNREFUSED:
        case EHOSTDOWN:
        case EHOSTUNREACH:
            printf(
                "+---------------------------------------------+\n"
                "| Server stopped connection. Code: %-10u |\n"
                "+---------------------------------------------+\n",
                (uint32_t)errno_val);
            return true;
        default:
            app_perror(cause);
            return false;
    }
}

enum MessageSkipResult {
    RECEIVED_MESSAGE_FROM_SERVER,
    NO_MESSAGES_IN_SOCKET,
    SOCKET_ERROR,
};

static enum MessageSkipResult skip_messages_not_from_the_server(const Client client,
                                                                UDPMessage* message) {
    while (true) {
        ssize_t bytes_read = recv(client->client_sock_fd, message, sizeof(*message),
                                  MSG_DONTWAIT | MSG_PEEK | MSG_NOSIGNAL);
        if (bytes_read != sizeof(UDPMessage)) {
            const int errno_val = errno;
            if (errno_val == EAGAIN || errno_val == EWOULDBLOCK) {
                return NO_MESSAGES_IN_SOCKET;
            }
            client_handle_errno("client_should_stop[tried to skip message not from the server]");
            return SOCKET_ERROR;
        }
        bool is_message_for_client = message->sender_type == COMPONENT_TYPE_SERVER &&
                                     (message->receiver_type & client->type) != 0;
        if (is_message_for_client) {
            return RECEIVED_MESSAGE_FROM_SERVER;
        }
        bytes_read =
            recv(client->client_sock_fd, message, sizeof(*message), MSG_DONTWAIT | MSG_NOSIGNAL);
        if (bytes_read != sizeof(UDPMessage)) {
            client_handle_errno(
                "client_should_stop[tried to peek message not from the server that is to skip]");
            return SOCKET_ERROR;
        }
    }
}

bool client_should_stop(const Client client) {
    UDPMessage message = {0};
    switch (skip_messages_not_from_the_server(client, &message)) {
        case RECEIVED_MESSAGE_FROM_SERVER:
            break;
        case NO_MESSAGES_IN_SOCKET:
            return false;
        case SOCKET_ERROR:
        default:
            assert(false && "invalid return from skip_messages_not_from_the_server");
            return true;
    }

    assert(message.sender_type == COMPONENT_TYPE_SERVER);
    assert(message.receiver_type & client->type);
    return message.message_type == MESSAGE_TYPE_SHUTDOWN_MESSAGE;
}

Pin receive_new_pin(void) {
    Pin pin = {.pin_id = rand()};
    return pin;
}
bool check_pin_crookness(Pin pin) {
    uint32_t sleep_time = (uint32_t)rand() % (MAX_SLEEP_TIME - MIN_SLEEP_TIME + 1) + MIN_SLEEP_TIME;
    sleep(sleep_time);

    uint32_t x = (uint32_t)pin.pin_id;
#if defined(__GNUC__)
    return __builtin_parity(x) & 1;
#else
    return x & 1;
#endif
}
static bool send_pin(const Client worker, Pin pin) {
    const UDPMessage message = {
        .sender_type         = worker->type,
        .receiver_type       = COMPONENT_TYPE_SERVER,
        .message_type        = MESSAGE_TYPE_PIN_TRANSFERRING,
        .message_content.pin = pin,
    };
    bool success = sendto(worker->client_sock_fd, &message, sizeof(message), MSG_NOSIGNAL,
                          (const struct sockaddr*)&worker->server_broadcast_sock_addr,
                          sizeof(worker->server_broadcast_sock_addr)) == sizeof(message);
    if (!success) {
        client_handle_errno("sendto");
    }
    return success;
}
bool send_not_croocked_pin(const Client worker, Pin pin) {
    assert(is_worker(worker));
    return send_pin(worker, pin);
}
static bool receive_pin(const Client worker, Pin* rec_pin) {
    UDPMessage message = {0};
    do {
        ssize_t read_bytes = recv(worker->client_sock_fd, &message, sizeof(message), MSG_NOSIGNAL);
        if (read_bytes == 0) {
            continue;
        }
        if ((size_t)read_bytes != sizeof(message)) {
            client_handle_errno("recv");
            return false;
        }
        if (message.sender_type != COMPONENT_TYPE_SERVER) {
            continue;
        }
        switch (message.message_type) {
            case MESSAGE_TYPE_PIN_TRANSFERRING:
                break;
            case MESSAGE_TYPE_SHUTDOWN_MESSAGE:
                printf(
                    "+------------------------------------------+\n"
                    "| Received shutdown signal from the server |\n"
                    "+------------------------------------------+\n");
                return false;
            default:
                continue;
        }
    } while (message.receiver_type != worker->type);
    *rec_pin = message.message_content.pin;
    return true;
}
bool receive_not_crooked_pin(const Client worker, Pin* rec_pin) {
    assert(is_worker(worker));
    return receive_pin(worker, rec_pin);
}
void sharpen_pin(Pin pin) {
    (void)pin;
    uint32_t sleep_time = (uint32_t)rand() % (MAX_SLEEP_TIME - MIN_SLEEP_TIME + 1) + MIN_SLEEP_TIME;
    sleep(sleep_time);
}
bool send_sharpened_pin(const Client worker, Pin pin) {
    assert(is_worker(worker));
    return send_pin(worker, pin);
}
bool receive_sharpened_pin(const Client worker, Pin* rec_pin) {
    assert(is_worker(worker));
    return receive_pin(worker, rec_pin);
}
bool check_sharpened_pin_quality(Pin sharpened_pin) {
    uint32_t sleep_time = (uint32_t)rand() % (MAX_SLEEP_TIME - MIN_SLEEP_TIME + 1) + MIN_SLEEP_TIME;
    sleep(sleep_time);
    return cos(sharpened_pin.pin_id) >= 0;
}
