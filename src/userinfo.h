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

#ifndef INC_BINDFS_USERINFO_H
#define INC_BINDFS_USERINFO_H

#include <config.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <pwd.h>
#include <grp.h>

/* Misc. reentrant helpers for handling user data.
   Return non-zero on success/true and 0 on failure/false. */

int user_uid(const char *username, uid_t *ret);
int group_gid(const char *groupname, gid_t *ret);

int user_belongs_to_group(uid_t uid, gid_t gid);

#endif
