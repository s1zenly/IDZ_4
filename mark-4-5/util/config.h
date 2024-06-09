#pragma once

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum {
    MIN_SLEEP_TIME = 1,
    MAX_SLEEP_TIME = 9,
};

__attribute__((__cold__)) static inline void app_perror_impl(const char* cause, const char* file,
                                                             uint32_t line, const char* func_name) {
    int errno_val                = errno;
    static char buffer[PATH_MAX] = {0};
    ssize_t bytes_written        = readlink("/proc/self/exe", buffer, sizeof(buffer) / 2);
    if (bytes_written < 0) {
        perror("at app_perror_impl(): readlink()");
        bytes_written = 0;
    }
    bytes_written =
        snprintf(&buffer[bytes_written], sizeof(buffer) - (size_t)bytes_written,
                 ": %s:%u: %s: %s: %s\n", file, line, func_name, cause, strerror(errno_val));
    if (bytes_written <= 0) {
        perror("at app_perror_impl(): snprintf()");
    }
    fputs(buffer, stderr);
}

#if defined __GNUC__ && defined __GNUC_MINOR__ && \
    ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((2) << 16) + (4))
#define FUNCTION_NAME_MACRO __extension__ __PRETTY_FUNCTION__
#else
#define FUNCTION_NAME_MACRO __func__
#endif

#define app_perror(cause) app_perror_impl(cause, __FILE__, __LINE__, FUNCTION_NAME_MACRO)
