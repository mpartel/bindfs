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

#include "misc.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int count_chars(const char *s, char ch)
{
    int count = 0;
    assert(s != NULL);
    while (*s != '\0') {
        if (*s == ch)
            ++count;
        ++s;
    }
    return count;
}

int count_substrs(const char *s, const char *sub)
{
    int count = 0;
    int sublen = strlen(sub);
    int left = strlen(s);

    assert(s != NULL && sub != NULL);

    while (left > sublen) {
        if (strncmp(s, sub, sublen) == 0)
            ++count;
        --left;
        ++s;
    }

    return count;
}

char *strdup_until(const char *s, const char *endchars)
{
    char *endloc = strpbrk(s, endchars);
    char *ret;
    if (!endloc) {
        ret = malloc((strlen(s) + 1) * sizeof(char));
        strcpy(ret, s);
        return ret;
    } else {
        ret = malloc((endloc - s + 1) * sizeof(char));
        memcpy(ret, s, (endloc - s) * sizeof(char));
        ret[(endloc - s)] = '\0';
        return ret;
    }
}

char *sprintf_new(const char *format, ...)
{
    va_list ap;
    size_t buffer_size = strlen(format) + 32;
    char *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        return NULL;
    }

    while (1) {
        va_start(ap, format);
        size_t len = (size_t)vsnprintf(buffer, buffer_size, format, ap);
        va_end(ap);
        if (len < buffer_size) {
            return buffer;
        }
        free(buffer);
        buffer_size *= 2;
        buffer = malloc(buffer_size);
        if (buffer == NULL) {
            return NULL;
        }
    }
}

const char *my_basename(const char *path)
{
    const char *p;

    if (path == NULL)
        return NULL;

    p = strrchr(path, '/');
    if (p != NULL)
        return p + 1;
    else
        return path;
}

const char *my_dirname(char *path)
{
    if (strcmp(path, ".") == 0) {
        return "..";
    } else if (strcmp(path, "/") == 0) {
        return "/";
    } else {
        int len = strlen(path);
        char *p = path + len - 1;
        while (p > path) {
            if (*p == '/') {
                break;
            }
            --p;
        }
        if (p > path) {
            *p = '\0';
            return path;
        } else if (*path == '/') {
            return "/";
        } else {
            return ".";
        }
    }
}

void insertion_sort_last(void *base, size_t nmemb, size_t size, int (*compar)(const void*, const void*))
{
    void *const last = base + (nmemb - 1) * size;
    void *const entry = malloc(size);
    memcpy(entry, last, size);

    // Find the location to insert at.
    int n = 0;
    void *p = last - size;
    while (p >= base && compar(entry, p) < 0) {
        p -= size;
        n += 1;
    }

    // We found a *p that's larger or we went below the start of the array, so go back one step.
    p += size;
    // No need to adjust n, it's now the correct number of elements to shift forward

    // Insert at *p.
    memmove(p + size, p, n * size);
    memcpy(p, entry, size);
    free(entry);
}

void grow_array_impl(void **array, int *capacity, int member_size)
{
    int new_cap = *capacity;
    if (new_cap == 0) {
        new_cap = 8;
    } else {
        new_cap *= 2;
    }

    *array = realloc(*array, new_cap * member_size);
    *capacity = new_cap;
}

int parse_byte_count(const char *str, double *result)
{
    char* end;
    double base = strtod(str, &end);
    long long mul = 1;
    if (*end == '\0') {
        mul = 1L;
    } else if (strcmp(end, "k") == 0) {
        mul = 1024L;
    } else if (strcmp(end, "M") == 0) {
        mul = 1024L * 1024L;
    } else if (strcmp(end, "G") == 0) {
        mul = 1024L * 1024L * 1024L;
    } else if (strcmp(end, "T") == 0) {
        mul = 1024LL * 1024LL * 1024LL * 1024LL;
    } else {
        return 0;
    }
    *result = base * mul;
    return 1;
}

void init_memory_block(struct memory_block *a, int initial_capacity)
{
    a->size = 0;
    a->capacity = initial_capacity;
    if (initial_capacity > 0) {
        a->ptr = (char *)malloc(initial_capacity);
    } else {
        a->ptr = NULL;
    }
}

void grow_memory_block(struct memory_block *a, int amount)
{
    int new_cap;

    a->size += amount;
    if (a->size >= a->capacity) {
        new_cap = a->capacity;
        if (new_cap == 0) {
            new_cap = 8;
        } else {
            new_cap *= 2;
        }
        a->ptr = (char *)realloc(a->ptr, new_cap);
        a->capacity = new_cap;
    }
}

int append_to_memory_block(struct memory_block *a, void *src, int src_size)
{
    int dest = a->size;
    grow_memory_block(a, src_size);
    memcpy(&a->ptr[dest], src, src_size);
    return dest;
}

void free_memory_block(struct memory_block *a)
{
    free(a->ptr);
    init_memory_block(a, 0);
}
