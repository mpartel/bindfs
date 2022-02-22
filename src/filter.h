#ifndef INC_BINDFS_FILTER_H
#define INC_BINDFS_FILTER_H

#include <config.h>
#include <sys/stat.h>

#define MODET_TO_BITMASK(m) ( 1 << (m>>12) )

struct FileFilter;
typedef struct FileFilter FileFilter;

typedef enum FileFilterStatus {
    filefilter_status_found = -1,
    filefilter_status_ok = 0,
    filefilter_status_notfound = 1,
    filefilter_status_incorrect_name = 2,
    filefilter_status_incorrect_mode = 3,
    filefilter_status_addfail = 4,
    filefilter_status_dupfound = 5
} FFStatus;

extern char *ffstatus_str_arr[];
#define ffstatus_str(s) ffstatus_str_arr[s+1]

typedef enum FFType {
    FFT_SCK = 1 << (S_IFSOCK>>12),
    FFT_LNK = 1 << (S_IFLNK>>12),
    FFT_REG = 1 << (S_IFREG>>12),
    FFT_BLK = 1 << (S_IFBLK>>12),
    FFT_DIR = 1 << (S_IFDIR>>12),
    FFT_CHR = 1 << (S_IFCHR>>12),
    FFT_PIP = 1 << (S_IFIFO>>12),
    FFT_ANY = (FFT_SCK|FFT_LNK|FFT_REG|FFT_BLK|FFT_DIR|FFT_CHR|FFT_PIP)
} FFType;

FileFilter *filefilter_create();
void filefilter_destroy(FileFilter *f);

FFStatus filefilter_add(FileFilter *f, char *spec, FFType type);
FFStatus filefilter_find_match(FileFilter *f, char *fn, mode_t type);

#endif
