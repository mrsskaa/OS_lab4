#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int close_fd(int fd){
    if (close(fd) == -1) {
        perror("close");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]){
    const char *child_program;
    if (argc > 1) {
        child_program = argv[1];
    } else {
        child_program = "./child";
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        return EXIT_FAILURE;
    }

    char *filename = NULL;
    size_t capacity = 0;
    fprintf(stderr, "Input file: ");

    ssize_t length = getline(&filename, &capacity, stdin);
    if (length == -1) {
        if (feof(stdin)) {
            fprintf(stderr, "parent: expected a file name\n");
        } else {
            perror("getline");
        }
        free(filename);
        return EXIT_FAILURE;
    }
    if (memchr(filename, '\0', (size_t)length) != NULL) {
        fprintf(stderr, "parent: invalid file name\n");
        free(filename);
        return EXIT_FAILURE;
    }
    filename[strcspn(filename, "\r\n")] = '\0';

    int input_fd = open(filename, O_RDONLY);
    free(filename);
    if (input_fd == -1) {
        perror("parent: open");
        return EXIT_FAILURE;
    }

    char name[] = "/tmp/os_lab4_XXXXXX";
    int output_fd = mkstemp(name);
    if (output_fd == -1) {
        perror("mkstemp");
        close_fd(input_fd);
        return EXIT_FAILURE;
    }
    if (unlink(name) == -1) {
        perror("unlink");
        close_fd(input_fd);
        close_fd(output_fd);
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close_fd(input_fd);
        close_fd(output_fd);
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        if (dup2(input_fd, STDIN_FILENO) == -1 || dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            _exit(EXIT_FAILURE);
        }

        int result = close_fd(input_fd);
        result |= close_fd(output_fd);
        if (result != EXIT_SUCCESS) {
            _exit(EXIT_FAILURE);
        }

        execl(child_program, "child", (char *)NULL);
        perror("exec");
        _exit(EXIT_FAILURE);
    }

    int result = close_fd(input_fd);
    fprintf(stderr, "parent PID=%ld, child PID=%ld\n",(long)getpid(), (long)pid);

    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }
        perror("waitpid");
        close_fd(output_fd);
        return EXIT_FAILURE;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
        fprintf(stderr, "parent: child failed\n");
        close_fd(output_fd);
        return EXIT_FAILURE;
    }

    struct stat info;
    if (fstat(output_fd, &info) == -1) {
        perror("fstat");
        close_fd(output_fd);
        return EXIT_FAILURE;
    }
    if (info.st_size > 0) {
        size_t size = (size_t)info.st_size;
        char *data = mmap(NULL, size, PROT_READ, MAP_SHARED, output_fd, 0);
        if (data == MAP_FAILED) {
            perror("parent: mmap");
            close_fd(output_fd);
            return EXIT_FAILURE;
        }
        if (fwrite(data, 1, size, stdout) != size) {
            perror("parent: output");
            result = EXIT_FAILURE;
        }
        if (munmap(data, size) == -1) {
            perror("munmap");
            result = EXIT_FAILURE;
        }
    }
    result |= close_fd(output_fd);
    if (fflush(stdout) == EOF) {
        perror("parent: flush");
        result = EXIT_FAILURE;
    }
    return result;
}
