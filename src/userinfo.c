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
    int username_offset; /* arena-allocated */
};

struct gid_cache_entry {
    gid_t gid;
    int uid_count;
    int uids_offset; /* arena-allocated */
};

static pthread_rwlock_t cache_lock = PTHREAD_RWLOCK_INITIALIZER;

static struct uid_cache_entry *uid_cache = NULL;
static int uid_cache_size = 0;
static int uid_cache_capacity = 0;

static struct gid_cache_entry *gid_cache = NULL;
static int gid_cache_size = 0;
static int gid_cache_capacity = 0;

static struct arena cache_arena = ARENA_INITIALIZER;

static volatile int cache_rebuild_requested = 1;

static void rebuild_cache();
static struct uid_cache_entry *uid_cache_lookup(uid_t key);
static struct gid_cache_entry *gid_cache_lookup(gid_t key);
static int rebuild_uid_cache();
static int rebuild_gid_cache();
static void clear_uid_cache();
static void clear_gid_cache();
static int uid_cache_name_sortcmp(const void *key, const void *entry);
static int uid_cache_name_searchcmp(const void *key, const void *entry);
static int uid_cache_uid_sortcmp(const void *key, const void *entry);
static int uid_cache_uid_searchcmp(const void *key, const void *entry);
static int gid_cache_gid_sortcmp(const void *key, const void *entry);
static int gid_cache_gid_searchcmp(const void *key, const void *entry);

static void rebuild_cache()
{
    free_arena(&cache_arena);
    init_arena(&cache_arena, 1024);
    rebuild_uid_cache();
    rebuild_gid_cache();
    qsort(uid_cache, uid_cache_size, sizeof(struct uid_cache_entry), uid_cache_uid_sortcmp);
    qsort(gid_cache, gid_cache_size, sizeof(struct gid_cache_entry), gid_cache_gid_sortcmp);
}

static struct uid_cache_entry *uid_cache_lookup(uid_t key)
{
    return (struct uid_cache_entry *)bsearch(
        &key,
        uid_cache,
        uid_cache_size,
        sizeof(struct uid_cache_entry),
        uid_cache_uid_searchcmp
    );
}

static struct gid_cache_entry *gid_cache_lookup(gid_t key)
{
    return (struct gid_cache_entry *)bsearch(
        &key,
        gid_cache,
        gid_cache_size,
        sizeof(struct gid_cache_entry),
        gid_cache_gid_searchcmp
    );
}

static int rebuild_uid_cache()
{
    /* We're holding the lock, so we have mutual exclusion on getpwent and getgrent too. */
    struct passwd *pw;
    struct uid_cache_entry *ent;
    int username_len;

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

        if (uid_cache_size == uid_cache_capacity) {
            grow_array(&uid_cache, &uid_cache_capacity, sizeof(struct uid_cache_entry));
        }

        ent = &uid_cache[uid_cache_size++];
        ent->uid = pw->pw_uid;
        ent->main_gid = pw->pw_gid;

        username_len = strlen(pw->pw_name) + 1;
        ent->username_offset = append_to_arena(&cache_arena, pw->pw_name, username_len);
    }

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
    /* We're holding the lock, so we have mutual exclusion on getpwent and getgrent too. */
    struct group *gr;
    struct gid_cache_entry *ent;
    int i;
    struct uid_cache_entry *uid_ent;

    gid_cache_size = 0;

    qsort(uid_cache, uid_cache_size, sizeof(struct uid_cache_entry), uid_cache_name_sortcmp);

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

        if (gid_cache_size == gid_cache_capacity) {
            grow_array(&gid_cache, &gid_cache_capacity, sizeof(struct gid_cache_entry));
        }

        ent = &gid_cache[gid_cache_size++];
        ent->gid = gr->gr_gid;
        ent->uid_count = 0;
        ent->uids_offset = cache_arena.size;

        for (i = 0; gr->gr_mem[i] != NULL; ++i) {
            uid_ent = (struct uid_cache_entry *)bsearch(
                gr->gr_mem[i],
                uid_cache,
                uid_cache_size,
                sizeof(struct uid_cache_entry),
                uid_cache_name_searchcmp
            );
            if (uid_ent != NULL) {
                grow_arena(&cache_arena, sizeof(uid_t));
                ((uid_t *)ARENA_GET(cache_arena, ent->uids_offset))[ent->uid_count++] = uid_ent->uid;
            }
        }
    }

    endgrent();
    return 1;
error:
    endgrent();
    clear_gid_cache();
    DPRINTF("Failed to rebuild uid cache");
    return 0;
}

static void clear_uid_cache()
{
    uid_cache_size = 0;
}

static void clear_gid_cache()
{
    gid_cache_size = 0;
}

static int uid_cache_name_sortcmp(const void *a, const void *b)
{
    int name_a_off = ((struct uid_cache_entry *)a)->username_offset;
    int name_b_off = ((struct uid_cache_entry *)b)->username_offset;
    const char *name_a = (const char *)ARENA_GET(cache_arena, name_a_off);
    const char *name_b = (const char *)ARENA_GET(cache_arena, name_b_off);
    return strcmp(name_a, name_b);
}

static int uid_cache_name_searchcmp(const void *key, const void *entry)
{
    int name_off = ((struct uid_cache_entry *)entry)->username_offset;
    const char *name = (const char *)ARENA_GET(cache_arena, name_off);
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

    if (cache_rebuild_requested) {
        pthread_rwlock_unlock(&cache_lock);

        pthread_rwlock_wrlock(&cache_lock);
        if (cache_rebuild_requested) {
            DPRINTF("Building user/group cache");
            cache_rebuild_requested = 0;
            rebuild_cache();
        }
        pthread_rwlock_unlock(&cache_lock);

        pthread_rwlock_rdlock(&cache_lock);
    }

    struct uid_cache_entry *uent = uid_cache_lookup(uid);
    if (uent && uent->main_gid == gid) {
        ret = 1;
        goto done;
    }

    struct gid_cache_entry *gent = gid_cache_lookup(gid);
    if (gent) {
        uids = (uid_t*)ARENA_GET(cache_arena, gent->uids_offset);
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

void invalidate_user_cache()
{
    cache_rebuild_requested = 1;
}
