#ifdef __linux__

#include <stdio.h>

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

    int fd = open(argv[1], O_RDONLY | O_DIRECT);
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

    while (1) {
        ssize_t amt_read = read(fd, buf, buf_size);
        if (amt_read == 0) {
            break;
        }
        if (amt_read == -1) {
            perror("failed to read file");
            return 1;
        }
        fwrite(buf, 1, amt_read, stdout);
        fflush(stdout);
    }

    return 0;
}

#else  // __linux__

#include <stdio.h>

int main(void) {
    fprintf(stderr, "Not supported on this platform.\n");
    return 1;
}

#endif  // __linux__
