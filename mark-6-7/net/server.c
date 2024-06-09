#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "../util/config.h"
#include "../util/parser.h"
#include "client-tools.h"
#include "net-config.h"
#include "pin.h"
#include "server-tools.h"

/// @brief We use global variables so it can be accessed through
static struct Server server                      = {0};
static volatile bool is_poller_running           = true;
static volatile bool is_logger_running           = true;
static volatile bool is_managers_handler_running = true;
static volatile pthread_t app_threads[3]         = {(pthread_t)-1, (pthread_t)-1, (pthread_t)-1};
static volatile atomic_size_t app_threads_size   = 0;

static void stop_all_threads(void) {
    static volatile atomic_bool disposed = false;

    bool value_if_first_call = false;
    bool disposed_equals_false =
        atomic_compare_exchange_strong(&disposed, &value_if_first_call, true);
    if (!disposed_equals_false) {
        return;
    }

    is_poller_running           = false;
    is_logger_running           = false;
    is_managers_handler_running = false;
    for (size_t i = 0; i < sizeof(app_threads) / sizeof(app_threads[0]); i++) {
        if (app_threads[i] != (pthread_t)-1) {
            int err_code = pthread_cancel(app_threads[i]);
            if (err_code != 0) {
                errno = err_code;
                app_perror("at stop_all_threads(): at pthread_cancel()");
            }
        }
    }
}
static void signal_handler(int sig) {
    fprintf(stderr, "> Received signal %d\n", sig);
    stop_all_threads();
}
static void setup_signal_handler(void) {
    const int handled_signals[] = {
        SIGABRT, SIGINT, SIGTERM, SIGSEGV, SIGQUIT, SIGKILL,
    };
    for (size_t i = 0; i < sizeof(handled_signals) / sizeof(handled_signals[0]); i++) {
        signal(handled_signals[i], signal_handler);
    }
}

static void* workers_poller(void* unused) {
    (void)unused;
    const struct timespec sleep_time = {
        .tv_sec  = 1,
        .tv_nsec = 0,
    };

    while (is_poller_running) {
        if (!nonblocking_poll(&server)) {
            fprintf(stderr, "> Could not poll clients\n");
            break;
        }
        if (nanosleep(&sleep_time, NULL) == -1) {
            if (errno != EINTR) {
                // if not interrupted by the signal
                app_perror("nanosleep");
            }
            break;
        }
    }

    int32_t ret = is_poller_running ? EXIT_FAILURE : EXIT_SUCCESS;
    stop_all_threads();
    return (void*)(uintptr_t)(uint32_t)ret;
}

static void* logs_sender(void* unused) {
    (void)unused;
    const struct timespec sleep_time = {
        .tv_sec  = 1,
        .tv_nsec = 0,
    };

    ServerLog log = {0};
    while (is_logger_running) {
        if (!dequeue_log(&server, &log)) {
            fputs("> Could not get next log\n", stderr);
            break;
        }

        if (!send_server_log(&server, &log)) {
            fputs("> Could not send log\n", stderr);
            break;
        }

        if (nanosleep(&sleep_time, NULL) == -1) {
            if (errno != EINTR) {  // if not interrupted by the signal
                perror("nanosleep");
            }
            break;
        }
    }

    int32_t ret = is_logger_running ? EXIT_FAILURE : EXIT_SUCCESS;
    if (is_logger_running) {
        stop_all_threads();
    }
    return (void*)(uintptr_t)(uint32_t)ret;
}

static bool create_thread(pthread_t* pthread_id, void*(handler)(void*)) {
    int ret = pthread_create(pthread_id, NULL, handler, NULL);
    if (ret != 0) {
        stop_all_threads();
        errno = ret;
        app_perror("pthread_create");
        return false;
    }
    assert(app_threads_size < (sizeof(app_threads) / sizeof(app_threads[0])));
    app_threads[app_threads_size++] = *pthread_id;
    return true;
}

static int join_thread(pthread_t pthread_id) {
    void* poll_ret       = NULL;
    int pthread_join_ret = pthread_join(pthread_id, &poll_ret);
    if (pthread_join_ret != 0) {
        errno = pthread_join_ret;
        app_perror("pthread_join");
    }
    return (int)(uintptr_t)poll_ret;
}

static int start_runtime_loop(void) {
    pthread_t poll_thread;
    if (!create_thread(&poll_thread, &workers_poller)) {
        return EXIT_FAILURE;
    }
    printf("> Started polling thread\n");

    pthread_t logs_thread;
    if (!create_thread(&logs_thread, &logs_sender)) {
        pthread_cancel(poll_thread);
        return EXIT_FAILURE;
    }
    printf("> Started logging thread\n");

    const int ret = join_thread(poll_thread);
    printf("> Joined polling thread\n");
    const int ret_1 = join_thread(logs_thread);
    printf("> Joined logging thread\n");

    printf("> Started sending shutdown signals to all clients\n");
    send_shutdown_signal_to_all(&server);
    printf("> Sent shutdown signals to all clients\n");

    return ret | ret_1;
}

static int run_server(uint16_t server_port) {
    if (!init_server(&server, server_port)) {
        return EXIT_FAILURE;
    }

    int ret = start_runtime_loop();
    deinit_server(&server);
    printf("> Deinitialized server resources\n");
    return ret;
}

int main(int argc, const char* argv[]) {
    setup_signal_handler();

    ParseResult res = parse_args(argc, argv);
    if (res.status != PARSE_SUCCESS) {
        print_invalid_args_error(res.status, argv[0]);
        return EXIT_FAILURE;
    }

    return run_server(res.port);
}
