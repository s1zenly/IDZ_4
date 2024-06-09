#pragma once

#include <stdint.h>

typedef enum ParseStatus {
    PARSE_SUCCESS,
    PARSE_INVALID_ARGC,
    PARSE_INVALID_IP_ADDRESS,
    PARSE_INVALID_PORT,
} ParseStatus;

typedef struct ParseResult {
    const char* ip_address;
    uint16_t port;
    ParseStatus status;
} ParseResult;

ParseResult parse_args(int argc, const char* argv[]);
void print_invalid_args_error(ParseStatus status, const char* program_path);
