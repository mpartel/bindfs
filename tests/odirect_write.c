#ifdef __linux__

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef O_DIRECT
#define O_DIRECT 00040000 /* direct disk access hint */
#endif

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Expected 1 argument: the file to read.\n");
        return 1;
    }

    int fd = open(argv[1], O_WRONLY | O_CREAT | O_DIRECT, 0644);
    if (fd == -1) {
        perror("failed to open file");
        return 1;
    }

    const size_t buf_size = 4096;
    unsigned char* buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if (buf == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    size_t total_size = 0;
    while (1) {
        if (feof(stdin)) {
            break;
        }

        memset(buf, 0, buf_size);
        size_t amt_read = fread(buf, 1, buf_size, stdin);
        if (ferror(stdin)) {
            perror("failed to read stdin");
            return 1;
        }
        if (amt_read == 0) {
            continue;
        }

        total_size += amt_read;

        ssize_t res = write(fd, buf, buf_size);
        if (res == -1) {
            perror("failed to write");
            return 1;
        }
        if ((size_t)res != buf_size) {
            // Too lazy to write a loop here unless it turns out to be necessary.
            fprintf(stderr, "Failed to write exactly %lu bytes", (unsigned long)amt_read);
        }
    }

    munmap(buf, buf_size);

    return 0;
}

#else  // __linux__

#include <stdio.h>

int main() {
    fprintf(stderr, "Not supported on this platform.\n");
    return 1;
}

#endif  // __linux__
