#pragma once

#include "pin.h"

typedef enum ComponentType {
    COMPONENT_TYPE_SERVER              = 1u << 0,
    COMPONENT_TYPE_FIRST_STAGE_WORKER  = 1u << 1,
    COMPONENT_TYPE_SECOND_STAGE_WORKER = 1u << 2,
    COMPONENT_TYPE_THIRD_STAGE_WORKER  = 1u << 3,
    COMPONENT_TYPE_LOGS_COLLECTOR      = 1u << 4,
    COMPONENT_TYPE_MANAGER             = 1u << 5,
    COMPONENT_TYPE_ANY_WORKER          = COMPONENT_TYPE_FIRST_STAGE_WORKER |
                                COMPONENT_TYPE_SECOND_STAGE_WORKER |
                                COMPONENT_TYPE_THIRD_STAGE_WORKER,
    COMPONENT_TYPE_ANY_CLIENT =
        COMPONENT_TYPE_ANY_WORKER | COMPONENT_TYPE_LOGS_COLLECTOR | COMPONENT_TYPE_MANAGER
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
        case COMPONENT_TYPE_MANAGER:
            return "manager";
        default:
            return "unknown client";
    }
}

typedef struct ServerCommand {
    ComponentType client_type;
} ServerCommand;

typedef enum ServerCommandResult {
    SERVER_COMMAND_SUCCESS,
    INVALID_SERVER_COMMAND_ARGS,
    SERVER_INTERNAL_ERROR,
    NO_CONNECTION,
} ServerCommandResult;

static inline const char* server_command_result_to_string(ServerCommandResult res) {
    switch (res) {
        case SERVER_COMMAND_SUCCESS:
            return "SERVER_COMMAND_SUCCESS";
        case INVALID_SERVER_COMMAND_ARGS:
            return "INVALID_SERVER_COMMAND_ARGS";
        case SERVER_INTERNAL_ERROR:
            return "SERVER_INTERNAL_ERROR";
        case NO_CONNECTION:
            return "NO_CONNECTION";
        default:
            return "unknown command result";
    }
}

typedef enum MessageType {
    MESSAGE_TYPE_PIN_TRANSFERRING,
    MESSAGE_TYPE_NEW_CLIENT,
    MESSAGE_TYPE_MANAGER_COMMAND,
    MESSAGE_TYPE_MANAGER_COMMAND_RESULT,
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
        case MESSAGE_TYPE_MANAGER_COMMAND:
            return "manager command";
        case MESSAGE_TYPE_MANAGER_COMMAND_RESULT:
            return "manager command result";
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
        ServerCommand command;
        ServerCommandResult command_result;
        char bytes[UDP_MESSAGE_BUFFER_SIZE];
    } message_content;
} UDPMessage;
