/*
    Copyright 2012 Martin Pärtel <martin.partel@gmail.com>

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

#ifndef INC_BINDFS_USERMAP_H
#define INC_BINDFS_USERMAP_H

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

/* A map of user IDs to userIDs and group IDs to group IDs. */
struct UserMap;
typedef struct UserMap UserMap;

typedef enum UsermapStatus {
    usermap_status_ok = 0,
    usermap_status_duplicate_key = 1
} UsermapStatus;

UserMap *usermap_create(void);
void usermap_destroy(UserMap *map);

UsermapStatus usermap_add_uid(UserMap *map, uid_t from, uid_t to);
UsermapStatus usermap_add_gid(UserMap *map, gid_t from, gid_t to);

const char* usermap_errorstr(UsermapStatus status);

/* Returns the uid that u is mapped to, or deflt if none. */
uid_t usermap_get_uid_or_default(UserMap *map, uid_t u, uid_t deflt);

/* Returns the gid that g is mapped to, or deflt if none. */
gid_t usermap_get_gid_or_default(UserMap *map, gid_t g, gid_t deflt);

#endif
