// Takes two files and exits with 0 if fcntl-locking one fcntl-locks the other.
// If the files don't fcntl-lock each other, returns 1.
// If any other error occurs, returns 2.

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "expecting exactly two arguments\n");
        return 2;
    }

    int fd1 = -1;
    int fd2 = -1;
    int result = 2;

    fd1 = open(argv[1], O_RDWR);
    if (fd1 == -1) {
        perror("failed to open the first file");
        goto exit;
    }
    fd2 = open(argv[2], O_RDWR);
    if (fd2 == -1) {
        perror("failed to open the second file");
        goto exit;
    }

    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl(fd1, F_SETLK, &lock) == -1) {
        perror("fcntl F_SETLK on the first file failed");
        goto exit;
    }

    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl(fd2, F_SETLK, &lock) == -1) {
        if (errno == EACCES || errno == EAGAIN) {
            result = 0;
            goto exit;
        } else {
            perror("fcntl F_SETLK on the second file failed");
            goto exit;
        }
    } else {
        result = 1;
        goto exit;
    }

exit:
    close(fd1);
    close(fd2);
    return result;
}
