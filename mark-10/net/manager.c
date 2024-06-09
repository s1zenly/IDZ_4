#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../util/parser.h"
#include "client-tools.h"
#include "net-config.h"

static uint32_t next_uint(const char* prompt, uint32_t min_value, uint32_t max_value) {
    while (true) {
        fputs(prompt, stdout);
        uint32_t val = 0;
        if (scanf("%u", &val) == 1 && val - min_value <= max_value - min_value) {
            return val;
        }

        fputs("Invalid input, please, try again\n> ", stdout);
    }
}

static bool handle_server_response(ServerCommandResult res) {
    switch (res) {
        case SERVER_COMMAND_SUCCESS:
            printf("Server response: success\n");
            return true;
        case INVALID_SERVER_COMMAND_ARGS:
            printf("Server response: invalid command arguments\n");
            return true;
        case SERVER_INTERNAL_ERROR:
            printf("Server response: internal error\n");
            return true;
        case NO_CONNECTION:
            printf("No connection to the server\n");
            return false;
        default:
            printf("Unknown server response: %d\n", res);
            return false;
    }
}

static bool next_user_command(void) {
    return next_uint("Enter command:\n> 1. Disable client\n> 2. Exit\n\n> ", 1, 2) == 1;
}

static int start_runtime_loop(Client manager) {
    int ret                     = EXIT_SUCCESS;
    bool exit_requested_by_user = false;
    const char* const prompt =
        "Enter client type:\n"
        "> 1. First stage workers\n"
        "> 2. Second stage workers\n"
        "> 3. Third stage workers\n"
        "> 4. Logs collectors\n"
        "> 5. Managers\n"
        "\n"
        "> ";

    while (!client_should_stop(manager)) {
        if (!next_user_command()) {
            exit_requested_by_user = true;
            break;
        }

        const ServerCommand cmd = {.client_type = (ComponentType){1u << next_uint(prompt, 1, 5)}};
        const ServerCommandResult resp = send_manager_command_to_server(manager, cmd);
        if (!handle_server_response(resp)) {
            ret = EXIT_FAILURE;
            break;
        }
    }

    if (ret == EXIT_SUCCESS && !exit_requested_by_user) {
        printf(
            "+------------------------------------------+\n"
            "| Received shutdown signal from the server |\n"
            "+------------------------------------------+\n");
    }

    printf(
        "+------------------------+\n"
        "| Manager is stopping... |\n"
        "+------------------------+\n");
    return ret;
}

static int run_manager(uint16_t server_port) {
    Client manager;
    if (!init_client(manager, server_port, COMPONENT_TYPE_MANAGER)) {
        return EXIT_FAILURE;
    }

    print_client_info(manager);
    int ret = start_runtime_loop(manager);
    deinit_client(manager);
    return ret;
}

int main(int argc, char const* argv[]) {
    ParseResult res = parse_args(argc, argv);
    if (res.status != PARSE_SUCCESS) {
        print_invalid_args_error(res.status, argv[0]);
        return EXIT_FAILURE;
    }

    return run_manager(res.port);
}
