#pragma once

#include "pin.h"

typedef enum ComponentType {
    COMPONENT_TYPE_SERVER              = 1u << 0,
    COMPONENT_TYPE_FIRST_STAGE_WORKER  = 1u << 1,
    COMPONENT_TYPE_SECOND_STAGE_WORKER = 1u << 2,
    COMPONENT_TYPE_THIRD_STAGE_WORKER  = 1u << 3,
    COMPONENT_TYPE_LOGS_COLLECTOR      = 1u << 4,
    COMPONENT_TYPE_ANY_WORKER          = COMPONENT_TYPE_FIRST_STAGE_WORKER |
                                COMPONENT_TYPE_SECOND_STAGE_WORKER |
                                COMPONENT_TYPE_THIRD_STAGE_WORKER,
    COMPONENT_TYPE_ANY_CLIENT = COMPONENT_TYPE_ANY_WORKER | COMPONENT_TYPE_LOGS_COLLECTOR
} ComponentType;

static inline const char* component_type_to_string(ComponentType type) {
    switch (type) {
        case COMPONENT_TYPE_SERVER:
            return "server";
        case COMPONENT_TYPE_FIRST_STAGE_WORKER:
            return "first stage worker";
        case COMPONENT_TYPE_SECOND_STAGE_WORKER:
            return "second stage worker";
        case COMPONENT_TYPE_THIRD_STAGE_WORKER:
            return "third stage worker";
        case COMPONENT_TYPE_LOGS_COLLECTOR:
            return "logs collector";
        default:
            return "unknown client";
    }
}

typedef enum MessageType {
    MESSAGE_TYPE_PIN_TRANSFERRING,
    MESSAGE_TYPE_NEW_CLIENT,
    MESSAGE_TYPE_SHUTDOWN_MESSAGE,
    MESSAGE_TYPE_LOG,
} MessageType;

static inline const char* message_type_to_string(MessageType type) {
    switch (type) {
        case MESSAGE_TYPE_PIN_TRANSFERRING:
            return "pin transferring message";
        case MESSAGE_TYPE_SHUTDOWN_MESSAGE:
            return "shutdown message";
        case MESSAGE_TYPE_NEW_CLIENT:
            return "new client message";
        case MESSAGE_TYPE_LOG:
            return "log message";
        default:
            return "unknown message";
    }
}

enum {
    UDP_MESSAGE_SIZE          = 512,
    UDP_MESSAGE_METAINFO_SIZE = 2 * sizeof(ComponentType) + sizeof(MessageType),
    UDP_MESSAGE_BUFFER_SIZE   = UDP_MESSAGE_SIZE - UDP_MESSAGE_METAINFO_SIZE,
};

typedef struct UDPMessage {
    ComponentType sender_type;
    ComponentType receiver_type;
    MessageType message_type;
    union {
        Pin pin;
        char bytes[UDP_MESSAGE_BUFFER_SIZE];
    } message_content;
} UDPMessage;
