#include <stdlib.h>

#include "../util/parser.h"
#include "client-tools.h"
#include "server-log.h"

static void print_server_log(const ServerLog* log) {
    printf("> Received new server log:\n%s\n", log->message);
}

static int start_runtime_loop(Client logs_col) {
    int ret = EXIT_SUCCESS;
    ServerLog log;
    while (!client_should_stop(logs_col)) {
        if (!receive_server_log(logs_col, &log)) {
            ret = EXIT_FAILURE;
            break;
        }

        print_server_log(&log);
    }

    if (ret == EXIT_SUCCESS) {
        printf(
            "+------------------------------------------+\n"
            "| Received shutdown signal from the server |\n"
            "+------------------------------------------+\n");
    }

    printf(
        "+-------------------------------+\n"
        "| Logs collector is stopping... |\n"
        "+-------------------------------+\n");
    return ret;
}

static int run_logs_collector(uint16_t server_port) {
    Client logs_col;
    if (!init_client(logs_col, server_port, COMPONENT_TYPE_LOGS_COLLECTOR)) {
        return EXIT_FAILURE;
    }

    print_client_info(logs_col);
    int ret = start_runtime_loop(logs_col);
    deinit_client(logs_col);
    return ret;
}

int main(int argc, char const* argv[]) {
    ParseResult res = parse_args(argc, argv);
    if (res.status != PARSE_SUCCESS) {
        print_invalid_args_error(res.status, argv[0]);
        return EXIT_FAILURE;
    }

    return run_logs_collector(res.port);
}
