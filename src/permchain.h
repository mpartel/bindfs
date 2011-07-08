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

#ifndef INC_BINDFS_PERMCHAIN_H
#define INC_BINDFS_PERMCHAIN_H


#include <config.h>

#define _GNU_SOURCE

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <unistd.h>

struct permchain;

struct permchain *permchain_create();

/* Parses chmod arguments like 0777, a=rX, og-rwx etc.
   Multiple rules may be given, separated with commas or colons.
   Unlike the ordinary chmod command, the octal specification may be
   present in a comma/colon-separated list.
   Returns 0 on success. On failure, pc will not be modified. */
int add_chmod_rules_to_permchain(const char *rule, struct permchain *pc);

/* Links 'right' to the end of 'left'. Don't destroy 'right' after this. */
void permchain_cat(struct permchain *left, struct permchain *right);

mode_t permchain_apply(struct permchain *pc, mode_t tgtmode);

void permchain_destroy(struct permchain *pc);

#endif
