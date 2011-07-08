/*
    Copyright 2006,2007,2008 Martin Pärtel <martin.partel@gmail.com>

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
#include <stdlib.h>
#include <string.h>
#include <errno.h>


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
    struct passwd pwbuf, *pwbufp = NULL;
    struct group grbuf, *grbufp = NULL;
    char *buf;
    size_t buflen;

    int member;
    uid_t member_uid;

    int res;

    buflen = 1024;
    buf = malloc(buflen);

    res = getpwuid_r(uid, &pwbuf, buf, buflen, &pwbufp);
    while(res == ERANGE) {
        buflen *= 2;
        buf = realloc(buf, buflen);
        res = getpwuid_r(uid, &pwbuf, buf, buflen, &pwbufp);
    }

    if (pwbufp == NULL)
        goto no;

    if (gid == pwbuf.pw_gid)
        goto yes;

    /* we reuse the same buf because we don't need the passwd info any more */
    res = getgrgid_r(gid, &grbuf, buf, buflen, &grbufp);
    while(res == ERANGE) {
        buflen *= 2;
        buf = realloc(buf, buflen);
        res = getgrgid_r(gid, &grbuf, buf, buflen, &grbufp);
    }

    if (grbufp == NULL)
        goto no;

    for (member = 0; grbuf.gr_mem[member] != NULL; ++member) {
        if (user_uid(grbuf.gr_mem[member], &member_uid))
            if (member_uid == uid)
                goto yes;
    }

    goto no;

yes:
    free(buf);
    return 1;
no:
    free(buf);
    return 0;
}
