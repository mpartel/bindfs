
#define _BSD_SOURCE /* For atoll */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char* argv[])
{
    struct timespec times[2];

    if (argc != 6) {
        fprintf(stderr, "Usage: utimens_nofollow path atime atime_nsec mtime mtime_nsec\n");
        return 1;
    }

    times[0].tv_sec = (time_t)atoll(argv[2]);
    times[0].tv_nsec = atoll(argv[3]);
    times[1].tv_sec = (time_t)atoll(argv[4]);
    times[1].tv_nsec = atoll(argv[5]);

    if (utimensat(AT_FDCWD, argv[1], times, AT_SYMLINK_NOFOLLOW) == -1) {
        perror("failed to utimensat the given path");
        return 2;
    }

    return 0;
}
