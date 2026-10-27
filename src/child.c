#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool is_prime(int number){
    if (number < 2) {
        return false;
    }
    for (int divisor = 2; divisor <= number / divisor; ++divisor) {
        if (number % divisor == 0) {
            return false;
        }
    }
    return true;
}

int parse_number(const char *line, size_t length, int *number){
    if (memchr(line, '\0', length) != NULL) {
        return -1;
    }
    char *end;
    errno = 0;
    long value = strtol(line, &end, 10);
    if (end == line || errno == ERANGE || value < INT_MIN || value > INT_MAX) {
        return -1;
    }
    while (isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0') {
        return -1;
    }
    *number = (int)value;
    return 0;
}

int main(){
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "child PID=%ld, parent PID=%ld\n", (long)getpid(), (long)getppid());

    struct stat info;
    if (fstat(STDIN_FILENO, &info) == -1) {
        perror("fstat");
        return EXIT_FAILURE;
    }
    if (!S_ISREG(info.st_mode) || info.st_size < 0 ||
        (uintmax_t)info.st_size > SIZE_MAX - 2) {
        fprintf(stderr, "child: expected a regular file of supported size\n");
        return EXIT_FAILURE;
    }
    size_t size = (size_t)info.st_size + 2;
    if ((off_t)size <= 0 || (uintmax_t)(off_t)size != size) {
        fprintf(stderr, "child: file too large\n");
        return EXIT_FAILURE;
    }
    if (ftruncate(STDOUT_FILENO, (off_t)size) == -1) {
        perror("ftruncate");
        return EXIT_FAILURE;
    }
    char *data = mmap(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, STDOUT_FILENO, 0);
    if (data == MAP_FAILED) {
        perror("child: mmap");
        return EXIT_FAILURE;
    }
    size_t used = 0;

    char *line = NULL;
    size_t capacity = 0;
    size_t line_number = 0;
    int result = EXIT_SUCCESS;

    while (1) {
        ssize_t length = getline(&line, &capacity, stdin);
        if (length == -1) {
            if (!feof(stdin)) {
                perror("child: input");
                result = EXIT_FAILURE;
            }
            break;
        }
        ++line_number;

        int number;
        if (parse_number(line, (size_t)length, &number) == -1) {
            fprintf(stderr, "child: invalid integer at line %zu\n", line_number);
            result = EXIT_FAILURE;
            break;
        }
        if (number < 0 || is_prime(number)) {
            fprintf(stderr, "child: stopped at number %d\n", number);
            break;
        }
        if (number >= 2) {
            int count = snprintf(data + used, size - used, "%d\n", number);
            if (count < 0 || (size_t)count >= size - used) {
                fprintf(stderr, "child: output does not fit\n");
                result = EXIT_FAILURE;
                break;
            }
            used += (size_t)count;
        }
    }

    free(line);
    if (munmap(data, size) == -1) {
        perror("munmap");
        result = EXIT_FAILURE;
    }
    if (ftruncate(STDOUT_FILENO, (off_t)used) == -1) {
        perror("ftruncate");
        result = EXIT_FAILURE;
    }
    if (fclose(stdin) == EOF) {
        perror("child: close input");
        result = EXIT_FAILURE;
    }
    if (fclose(stdout) == EOF) {
        perror("child: close output");
        result = EXIT_FAILURE;
    }

    return result;
}
