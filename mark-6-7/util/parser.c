#include "parser.h"

#include <arpa/inet.h>   
#include <netinet/in.h>  
#include <stdbool.h>     
#include <stdio.h>       
#include <stdlib.h>      
#include <sys/socket.h>  

static bool verify_ip(const char* ip_address, bool ip4_only) {
    struct sockaddr_in sa4;
    int res = inet_pton(AF_INET, ip_address, &sa4.sin_addr);
    if (res == 1) {
        return true;
    }
    if (ip4_only) {
        return false;
    }

    struct sockaddr_in6 sa6;
    return inet_pton(AF_INET6, ip_address, &sa6.sin6_addr) == 1;
}

static bool parse_port(const char* port_str, uint16_t* port) {
    char* end_ptr            = NULL;
    unsigned long port_value = strtoul(port_str, &end_ptr, 10);
    *port                    = (uint16_t)port_value;
    bool port_value_overflow = *port != port_value;
    bool parse_error         = end_ptr == NULL || *end_ptr != '\0';
    return !parse_error && !port_value_overflow;
}

ParseResult parse_args(int argc, const char* argv[]) {
    ParseResult res = {
        .ip_address = NULL,
        .port       = 0,
        .status     = PARSE_INVALID_ARGC,
    };
    if (argc != 2) {
        return res;
    }

    if (!parse_port(argv[1], &res.port)) {
        res.status = PARSE_INVALID_PORT;
        return res;
    }
    res.status = PARSE_SUCCESS;
    return res;
}

void print_invalid_args_error(ParseStatus status, const char* program_path) {
    const char* error_str;
    switch (status) {
        case PARSE_INVALID_ARGC:
            error_str = "Invalid number of arguments";
            break;
        case PARSE_INVALID_IP_ADDRESS:
            error_str = "Invalid ip address";
            break;
        case PARSE_INVALID_PORT:
            error_str = "Invalid port";
            break;
        default:
            error_str = "Unknown error";
            break;
    }

    fprintf(stderr,
            "CLI args error: %s\n"
            "Usage: %s <server port>\n"
            "Example: %s 31457\n",
            error_str, program_path, program_path);
}
