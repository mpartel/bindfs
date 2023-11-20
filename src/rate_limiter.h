/*
    Copyright 2014 Martin Pärtel <martin.partel@gmail.com>

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

#ifndef INC_BINDFS_RATE_LIMITER_H
#define INC_BINDFS_RATE_LIMITER_H

#include <string.h>
#include <pthread.h>

/* When we are idle, we allow some time to be "credited" to the next writer.
 * Otherwise, the short pause between requests would "go to waste", lowering
 * the throughput when there is only one requester. */
extern const double rate_limiter_idle_credit;

typedef struct RateLimiter {
    double rate;  /* bytes / second */
    double (*clock)(void);
    double last_modified;
    double accumulated_sleep_time;
    pthread_mutex_t mutex;
} RateLimiter;

double gettimeofday_clock(void);

/* 0 on success, error number on error. */
void rate_limiter_init(RateLimiter* limiter, double rate, double (*clock)(void));
/* Blocks until the rate limiter clears `size` units. */
void rate_limiter_wait(RateLimiter* limiter, size_t size);
/* Updates the rate limiter like `rate_limiter_wait` but does not actually
 * sleep. Returns the time that the caller is expected to sleep. */
double rate_limiter_wait_nosleep(RateLimiter* limiter, size_t size);
/* Destroys the rate limiter. No wait_for_permit calls may be active. */
void rate_limiter_destroy(RateLimiter* limiter);

#endif
