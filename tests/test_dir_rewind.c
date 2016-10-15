// Tests that opening the current directory, reading its entries
// rewinding and reading its entries again gives the same entries both times.
//
// https://github.com/mpartel/bindfs/issues/41

#ifdef __linux__

#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BUF_SIZE 4096

int main()
{
    int fd = open(".", O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        perror("failed to open '.'");
        return 1;
    }

    char buf1[BUF_SIZE];
    char buf2[BUF_SIZE];
    memset(buf1, 0, BUF_SIZE);
    memset(buf2, 0, BUF_SIZE);

    int amt_read1 = syscall(SYS_getdents, fd, buf1, BUF_SIZE);
    if (amt_read1 <= 0) {
        fprintf(stderr, "amt_read1=%d\n", amt_read1);
        close(fd);
        return 1;
    }

    off_t seek_res = lseek(fd, 0, SEEK_SET);
    if (seek_res == (off_t)-1) {
        perror("failed to lseek to 0");
        close(fd);
        return 1;
    }

    int amt_read2 = syscall(SYS_getdents, fd, buf2, BUF_SIZE);
    if (amt_read2 <= 0) {
        fprintf(stderr, "amt_read2=%d\n", amt_read2);
        close(fd);
        return 1;
    }

    if (amt_read1 != amt_read2) {
        fprintf(stderr,
                "First read gave %d bytes, second read gave %d bytes.\n",
                amt_read1, amt_read2);
        close(fd);
        return 1;
    }
    if (memcmp(buf1, buf2, BUF_SIZE) != 0) {
        fprintf(stderr, "First and second read results differ.\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

#else  // #ifdef __linux__

int main()
{
    printf("This test (probably) only compiles on Linux.\n");
    printf("Skipping by just returning successfully.\n");
    return 0;
}

#endif  // #ifdef __linux__
