#pragma once

#include "net-config.h"

enum { MAX_SERVER_LOG_SIZE = UDP_MESSAGE_BUFFER_SIZE };

typedef struct ServerLog {
    char message[MAX_SERVER_LOG_SIZE];
} ServerLog;
