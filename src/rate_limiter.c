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

#define _XOPEN_SOURCE 700  /* for gettimeofday() on freebsd, and for nanosleep() */

#include "rate_limiter.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

const double rate_limiter_idle_credit = -0.2;

double gettimeofday_clock()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 0.000001;
}

static void sleep_seconds(double s)
{
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - ts.tv_sec) * 1000000000L);
    /* Guard against float imprecision */
    if (ts.tv_nsec > 999999999L) {
        ts.tv_nsec = 999999999L;
    } else if (ts.tv_nsec < 0L) {
        ts.tv_nsec = 0L;
    }

    nanosleep(&ts, NULL);
}

void rate_limiter_init(RateLimiter *limiter, double rate, double (*clock)())
{
    limiter->rate = rate;
    limiter->clock = clock;
    limiter->last_modified = limiter->clock();
    limiter->accumulated_sleep_time = rate_limiter_idle_credit;

    pthread_mutexattr_t attr;
    int status = pthread_mutexattr_init(&attr);
    assert(status == 0);
    status = pthread_mutex_init(&limiter->mutex, &attr);
    assert(status == 0);
    status = pthread_mutexattr_destroy(&attr);
    assert(status == 0);
}

void rate_limiter_wait(RateLimiter* limiter, size_t size)
{
    double time_to_sleep = rate_limiter_wait_nosleep(limiter, size);
    sleep_seconds(time_to_sleep);
}

double rate_limiter_wait_nosleep(RateLimiter* limiter, size_t size)
{
    int status = pthread_mutex_lock(&limiter->mutex);
    assert(status == 0);

    double time_to_add = size / limiter->rate;

    double now = limiter->clock();
    double elapsed = now - limiter->last_modified;
    if (elapsed < 0) {
        elapsed = 0;
    }

    double time_to_sleep = limiter->accumulated_sleep_time;
    time_to_sleep -= elapsed;
    if (time_to_sleep < rate_limiter_idle_credit) {
        time_to_sleep = rate_limiter_idle_credit;
    }
    time_to_sleep += time_to_add;
    limiter->accumulated_sleep_time = time_to_sleep;

    limiter->last_modified = now;

    status = pthread_mutex_unlock(&limiter->mutex);
    assert(status == 0);

    return time_to_sleep;
}

void rate_limiter_destroy(RateLimiter *limiter)
{
    int status = pthread_mutex_destroy(&limiter->mutex);
    assert(status == 0);
}
