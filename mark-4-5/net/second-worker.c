#include <stdint.h>  
#include <stdio.h>   
#include <stdlib.h>  

#include "../util/parser.h"  
#include "client-tools.h"
#include "pin.h"  

static void log_received_pin(Pin pin) {
    printf(
        "+-------------------------------------------------\n"
        "| Second worker received pin[pin_id=%d]\n"
        "| and started sharpening it...\n"
        "+-------------------------------------------------\n",
        pin.pin_id);
}

static void log_sharpened_pin(Pin pin) {
    printf(
        "+-------------------------------------------------\n"
        "| Second worker sharpened pin[pin_id=%d].\n"
        "+-------------------------------------------------\n",
        pin.pin_id);
}

static void log_sent_pin(Pin pin) {
    printf(
        "+-------------------------------------------------\n"
        "| Second worker sent sharpened\n"
        "| pin[pin_id=%d] to the third workers.\n"
        "+-------------------------------------------------\n",
        pin.pin_id);
}

static int start_runtime_loop(Client worker) {
    int ret = EXIT_SUCCESS;
    while (!client_should_stop(worker)) {
        Pin pin;
        if (!receive_not_crooked_pin(worker, &pin)) {
            ret = EXIT_FAILURE;
            break;
        }
        log_received_pin(pin);

        sharpen_pin(pin);
        log_sharpened_pin(pin);

        if (client_should_stop(worker)) {
            break;
        }
        if (!send_sharpened_pin(worker, pin)) {
            ret = EXIT_FAILURE;
            break;
        }
        log_sent_pin(pin);
    }

    if (ret == EXIT_SUCCESS) {
        printf(
            "+------------------------------------------+\n"
            "| Received shutdown signal from the server |\n"
            "+------------------------------------------+\n");
    }

    printf(
        "+------------------------------+\n"
        "| Second worker is stopping... |\n"
        "+------------------------------+\n");
    return ret;
}

static int run_worker(uint16_t fserver_port) {
    Client worker;
    if (!init_client(worker, fserver_port, COMPONENT_TYPE_SECOND_STAGE_WORKER)) {
        return EXIT_FAILURE;
    }

    print_client_info(worker);
    int ret = start_runtime_loop(worker);
    deinit_client(worker);
    return ret;
}

int main(int argc, const char* argv[]) {
    ParseResult res = parse_args(argc, argv);
    if (res.status != PARSE_SUCCESS) {
        print_invalid_args_error(res.status, argv[0]);
        return EXIT_FAILURE;
    }

    return run_worker(res.port);
}
