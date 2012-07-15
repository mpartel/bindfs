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

#ifndef INC_BINDFS_DEBUG_H
#define INC_BINDFS_DEBUG_H

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* Handier to lock and use strerror() than allocate buffer for strerror_r(). */
extern pthread_mutex_t strerror_lock;

#if BINDFS_DEBUG
#include <stdio.h>
#define DPRINTF(fmt, ...) do { \
        pthread_mutex_lock(&strerror_lock); \
        fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__); \
        pthread_mutex_unlock(&strerror_lock); \
    } while (0)
#else
#define DPRINTF(...)
#endif

#endif

