/*
    Copyright 2006,2007,2008,2012 Martin Pärtel <martin.partel@gmail.com>

    This file is part of bindfs.

    bindfs is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    bindfs is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with bindfs.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "userinfo.h"
#include "misc.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

struct uid_cache_entry {
    uid_t uid;
    gid_t main_gid;
    int username_offset; /* allocated in cache_memory_block */
};

struct gid_cache_entry {
    gid_t gid;
    int uid_count;
    int uids_offset; /* allocated in cache_memory_block */
};

static pthread_rwlock_t cache_lock = PTHREAD_RWLOCK_INITIALIZER;

static struct uid_cache_entry *uid_cache_by_uid = NULL;
static struct uid_cache_entry *uid_cache_by_name = NULL;
static int uid_cache_size = 0;
static int uid_cache_by_uid_capacity = 0;
static int uid_cache_by_name_capacity = 0;

static struct gid_cache_entry *gid_cache = NULL;
static int gid_cache_size = 0;
static int gid_cache_capacity = 0;

static struct memory_block cache_memory_block = MEMORY_BLOCK_INITIALIZER;

static volatile int cache_clear_requested = 1;

static struct uid_cache_entry *read_through_uid_cache(uid_t uid);
static struct uid_cache_entry *read_through_uid_by_name_cache(const char* name);
static struct gid_cache_entry *read_through_gid_cache(gid_t gid);
static int rebuild_uid_cache();
static int rebuild_gid_cache();
static struct uid_cache_entry* append_to_uid_cache(struct passwd *pw);
static struct gid_cache_entry* append_to_gid_cache(struct group *gr);
static void clear_uid_cache();
static void clear_gid_cache();
static int uid_cache_name_sortcmp(const void *key, const void *entry);
static int uid_cache_name_searchcmp(const void *key, const void *entry);
static int uid_cache_uid_sortcmp(const void *key, const void *entry);
static int uid_cache_uid_searchcmp(const void *key, const void *entry);
static int gid_cache_gid_sortcmp(const void *key, const void *entry);
static int gid_cache_gid_searchcmp(const void *key, const void *entry);

static struct uid_cache_entry *read_through_uid_cache(uid_t uid)
{
    struct uid_cache_entry *entry = (struct uid_cache_entry *)bsearch(
        &uid,
        uid_cache_by_uid,
        uid_cache_size,
        sizeof(struct uid_cache_entry),
        uid_cache_uid_searchcmp
    );
    if (entry != NULL) {
        return entry;
    }

    pthread_rwlock_unlock(&cache_lock);
    pthread_rwlock_wrlock(&cache_lock);

    struct passwd* pw = getpwuid(uid);
    if (pw != NULL) {
        entry = append_to_uid_cache(pw);
        insertion_sort_last(uid_cache_by_uid, uid_cache_size, sizeof(struct uid_cache_entry), uid_cache_uid_sortcmp);
        insertion_sort_last(uid_cache_by_name, uid_cache_size, sizeof(struct uid_cache_entry), uid_cache_name_sortcmp);
    } else {
        DPRINTF("Failed to getpwuid(%lld)", (long long)uid);
    }

    pthread_rwlock_unlock(&cache_lock);
    pthread_rwlock_rdlock(&cache_lock);
    return entry;
}

static struct uid_cache_entry *read_through_uid_by_name_cache(const char* name)
{
    struct uid_cache_entry *entry = (struct uid_cache_entry *)bsearch(
        name,
        uid_cache_by_name,
        uid_cache_size,
        sizeof(struct uid_cache_entry),
        uid_cache_name_searchcmp
    );
    if (entry != NULL) {
        return entry;
    }

    pthread_rwlock_unlock(&cache_lock);
    pthread_rwlock_wrlock(&cache_lock);

    struct passwd* pw = getpwnam(name);
    if (pw != NULL) {
        entry = append_to_uid_cache(pw);
        insertion_sort_last(uid_cache_by_uid, uid_cache_size, sizeof(struct uid_cache_entry), uid_cache_uid_sortcmp);
        insertion_sort_last(uid_cache_by_name, uid_cache_size, sizeof(struct uid_cache_entry), uid_cache_name_sortcmp);
    } else {
        DPRINTF("Failed to getpwnam(%lld)", (long long)uid);
    }

    pthread_rwlock_unlock(&cache_lock);
    pthread_rwlock_rdlock(&cache_lock);
    return entry;
}

static struct gid_cache_entry *read_through_gid_cache(gid_t gid)
{
    struct gid_cache_entry *entry = (struct gid_cache_entry *)bsearch(
        &gid,
        gid_cache,
        gid_cache_size,
        sizeof(struct gid_cache_entry),
        gid_cache_gid_searchcmp
    );
    if (entry != NULL) {
        return entry;
    }

    pthread_rwlock_unlock(&cache_lock);
    pthread_rwlock_wrlock(&cache_lock);

    struct group* gr = getgrgid(gid);
    if (gr != NULL) {
        entry = append_to_gid_cache(gr);
        insertion_sort_last(gid_cache, gid_cache_size, sizeof(struct gid_cache_entry), gid_cache_gid_sortcmp);
    } else {
        DPRINTF("Failed to getgrgid(%lld)", (long long)gid);
    }

    pthread_rwlock_unlock(&cache_lock);
    pthread_rwlock_rdlock(&cache_lock);
    return entry;
}

static int rebuild_uid_cache()
{
    struct passwd *pw;

    uid_cache_size = 0;

    while (1) {
        errno = 0;
        pw = getpwent();
        if (pw == NULL) {
            if (errno == 0) {
                break;
            } else {
                goto error;
            }
        }

        append_to_uid_cache(pw);
    }

    qsort(uid_cache_by_uid, uid_cache_size, sizeof(struct uid_cache_entry), uid_cache_uid_sortcmp);
    qsort(uid_cache_by_name, uid_cache_size, sizeof(struct uid_cache_entry), uid_cache_name_sortcmp);

    endpwent();
    return 1;
error:
    endpwent();
    clear_uid_cache();
    DPRINTF("Failed to rebuild uid cache");
    return 0;
}

static int rebuild_gid_cache()
{
    struct group *gr;

    gid_cache_size = 0;

    while (1) {
        errno = 0;
        gr = getgrent();
        if (gr == NULL) {
            if (errno == 0) {
                break;
            } else {
                goto error;
            }
        }

        append_to_gid_cache(gr);
    }

    qsort(gid_cache, gid_cache_size, sizeof(struct gid_cache_entry), gid_cache_gid_sortcmp);

    endgrent();
    return 1;
error:
    endgrent();
    clear_gid_cache();
    DPRINTF("Failed to rebuild gid cache");
    return 0;
}

static struct uid_cache_entry* append_to_uid_cache(struct passwd *pw)
{
    if (uid_cache_size == uid_cache_by_uid_capacity) {
        grow_array(&uid_cache_by_uid, &uid_cache_by_uid_capacity, sizeof(struct uid_cache_entry));
    }
    if (uid_cache_size == uid_cache_by_name_capacity) {
        grow_array(&uid_cache_by_name, &uid_cache_by_name_capacity, sizeof(struct uid_cache_entry));
    }

    struct uid_cache_entry *entry = &uid_cache_by_uid[uid_cache_size];
    entry->uid = pw->pw_uid;
    entry->main_gid = pw->pw_gid;

    int username_len = strlen(pw->pw_name) + 1;
    entry->username_offset = append_to_memory_block(&cache_memory_block, pw->pw_name, username_len);

    uid_cache_by_name[uid_cache_size] = *entry;
    ++uid_cache_size;

    return entry;
}

static struct gid_cache_entry* append_to_gid_cache(struct group *gr)
{
    if (gid_cache_size == gid_cache_capacity) {
        grow_array(&gid_cache, &gid_cache_capacity, sizeof(struct gid_cache_entry));
    }

    struct gid_cache_entry *entry = &gid_cache[gid_cache_size++];
    entry->gid = gr->gr_gid;
    entry->uid_count = 0;
    entry->uids_offset = cache_memory_block.size;

    int i;
    for (i = 0; gr->gr_mem[i] != NULL; ++i) {
        struct uid_cache_entry *uid_entry = read_through_uid_by_name_cache(gr->gr_mem[i]);
        if (uid_entry != NULL) {
            grow_memory_block(&cache_memory_block, sizeof(uid_t));
            ((uid_t *)MEMORY_BLOCK_GET(cache_memory_block, entry->uids_offset))[entry->uid_count++] = uid_entry->uid;
        }
    }

    return entry;
}

static void clear_uid_cache()
{
    free(uid_cache_by_uid);
    free(uid_cache_by_name);
    uid_cache_by_uid = NULL;
    uid_cache_by_name = NULL;
    uid_cache_size = 0;
    uid_cache_by_uid_capacity = 0;
    uid_cache_by_name_capacity = 0;
}

static void clear_gid_cache()
{
    free(gid_cache);
    gid_cache = NULL;
    gid_cache_size = 0;
    gid_cache_capacity = 0;
}

static int uid_cache_name_sortcmp(const void *a, const void *b)
{
    int name_a_off = ((struct uid_cache_entry *)a)->username_offset;
    int name_b_off = ((struct uid_cache_entry *)b)->username_offset;
    const char *name_a = (const char *)MEMORY_BLOCK_GET(cache_memory_block, name_a_off);
    const char *name_b = (const char *)MEMORY_BLOCK_GET(cache_memory_block, name_b_off);
    return strcmp(name_a, name_b);
}

static int uid_cache_name_searchcmp(const void *key, const void *entry)
{
    int name_off = ((struct uid_cache_entry *)entry)->username_offset;
    const char *name = (const char *)MEMORY_BLOCK_GET(cache_memory_block, name_off);
    return strcmp((const char *)key, name);
}

static int uid_cache_uid_sortcmp(const void *a, const void *b)
{
    return (long)((struct uid_cache_entry *)a)->uid - (long)((struct uid_cache_entry *)b)->uid;
}

static int uid_cache_uid_searchcmp(const void *key, const void *entry)
{
    return (long)*((uid_t *)key) - (long)((struct uid_cache_entry *)entry)->uid;
}

static int gid_cache_gid_sortcmp(const void *a, const void *b)
{
    return (long)((struct gid_cache_entry *)a)->gid - (long)((struct gid_cache_entry *)b)->gid;
}

static int gid_cache_gid_searchcmp(const void *key, const void *entry)
{
    return (long)*((gid_t *)key) - (long)((struct gid_cache_entry *)entry)->gid;
}


int user_uid(const char *username, uid_t *ret)
{
    struct passwd pwbuf, *pwbufp = NULL;
    char *buf;
    size_t buflen;
    int res;

    uid_t uid;
    char *endptr;

    /* Check whether the string is a numerical UID */
    uid = strtol(username, &endptr, 10);
    if (*endptr == '\0') {
        buflen = 1024;
        buf = malloc(buflen);
        res = getpwuid_r(uid, &pwbuf, buf, buflen, &pwbufp);
        if (res != 0) {
            if (res != ERANGE) { /* don't care if buffer was too small */
                free(buf);
                return 0;
            }
        }
        free(buf);
        *ret = uid;
        return 1;
    }

    /* Process user name */
    buflen = 1024;
    buf = malloc(buflen);

    res = getpwnam_r(username, &pwbuf, buf, buflen, &pwbufp);
    while(res == ERANGE) {
        buflen *= 2;
        buf = realloc(buf, buflen);
        res = getpwnam_r(username, &pwbuf, buf, buflen, &pwbufp);
    }

    if (pwbufp == NULL) {
        free(buf);
        return 0;
    }

    *ret = pwbuf.pw_uid;
    free(buf);
    return 1;
}


int group_gid(const char *groupname, gid_t *ret)
{
    struct group gbuf, *gbufp = NULL;
    char *buf;
    size_t buflen;
    int res;

    gid_t gid;
    char *endptr;

    /* Check whether the string is a numerical GID */
    gid = strtol(groupname, &endptr, 10);
    if (*endptr == '\0') {
        buflen = 1024;
        buf = malloc(buflen);
        res = getgrgid_r(gid, &gbuf, buf, buflen, &gbufp);
        if (res != 0) {
            if (res != ERANGE) { /* don't care if buffer was too small */
                free(buf);
                return 0;
            }
        }
        free(buf);
        *ret = gid;
        return 1;
    }

    /* Process group name */
    buflen = 1024;
    buf = malloc(buflen);

    res = getgrnam_r(groupname, &gbuf, buf, buflen, &gbufp);
    while(res == ERANGE) {
        buflen *= 2;
        buf = realloc(buf, buflen);
        res = getgrnam_r(groupname, &gbuf, buf, buflen, &gbufp);
    }

    if (gbufp == NULL) {
        free(buf);
        return 0;
    }

    *ret = gbuf.gr_gid;
    free(buf);
    return 1;
}

int user_belongs_to_group(uid_t uid, gid_t gid)
{
    int ret = 0;
    int i;
    uid_t *uids;

    pthread_rwlock_rdlock(&cache_lock);

    if (cache_clear_requested) {
        pthread_rwlock_unlock(&cache_lock);

        pthread_rwlock_wrlock(&cache_lock);
        if (cache_clear_requested) {
            DPRINTF("Clearing user/group cache");
            cache_clear_requested = 0;
            clear_uid_cache();
            clear_gid_cache();
        }
        pthread_rwlock_unlock(&cache_lock);

        pthread_rwlock_rdlock(&cache_lock);
    }

    struct uid_cache_entry *uent = read_through_uid_cache(uid);
    if (uent != NULL && uent->main_gid == gid) {
        ret = 1;
        goto done;
    }

    struct gid_cache_entry *gent = read_through_gid_cache(gid);
    if (gent) {
        uids = (uid_t*)MEMORY_BLOCK_GET(cache_memory_block, gent->uids_offset);
        for (i = 0; i < gent->uid_count; ++i) {
            if (uids[i] == uid) {
                ret = 1;
                goto done;
            }
        }
    }

done:
    pthread_rwlock_unlock(&cache_lock);
    return ret;
}

void rebuild_user_caches()
{
    pthread_rwlock_wrlock(&cache_lock);

    clear_uid_cache();
    clear_gid_cache();
    free_memory_block(&cache_memory_block);
    init_memory_block(&cache_memory_block, 1024);
    rebuild_uid_cache();
    rebuild_gid_cache();

    pthread_rwlock_unlock(&cache_lock);
}

void invalidate_user_caches()
{
    cache_clear_requested = 1;
}

void clear_user_caches()
{
    pthread_rwlock_wrlock(&cache_lock);

    clear_uid_cache();
    clear_gid_cache();
    free_memory_block(&cache_memory_block);
    init_memory_block(&cache_memory_block, 1024);

    pthread_rwlock_unlock(&cache_lock);
}
