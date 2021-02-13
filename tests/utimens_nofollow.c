
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char* argv[])
{
    if (argc != 6) {
        fprintf(stderr, "Usage: utimens_nofollow path atime atime_nsec mtime mtime_nsec\n");
        return 1;
    }

#ifdef HAVE_UTIMENSAT
    struct timespec times[2];
    times[0].tv_sec = (time_t)atoll(argv[2]);
    times[0].tv_nsec = atoll(argv[3]);
    times[1].tv_sec = (time_t)atoll(argv[4]);
    times[1].tv_nsec = atoll(argv[5]);
    if (utimensat(AT_FDCWD, argv[1], times, AT_SYMLINK_NOFOLLOW) == -1) {
        perror("failed to utimensat the given path");
        return 2;
    }
#elif HAVE_LUTIMES
    struct timeval times[2];
    times[0].tv_sec = (time_t)atoll(argv[2]);
    times[0].tv_usec = (suseconds_t)atoll(argv[3]) / 1000;
    times[1].tv_sec = (time_t)atoll(argv[4]);
    times[1].tv_usec = (suseconds_t)atoll(argv[5]) / 1000;
    if (lutimes(argv[1], times) == -1) {
        perror("failed to lutimes the given path");
        return 2;
    }
#else
#error "No symlink-compatible utime* function available."
#endif

    return 0;
}
