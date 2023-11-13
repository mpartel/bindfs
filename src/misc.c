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
#include <stdint.h>
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
        size_t len = strlen(path);
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

static const char* find_last_char_between(const char* start, const char* end, char ch) {
    assert(start != NULL && end != NULL);
    const char* p = end - 1;
    while (p >= start) {
        if (*p == ch) {
            return p;
        }
        --p;
    }
    return NULL;
}

bool path_starts_with(const char *path, const char* prefix, size_t prefix_len)
{
    size_t path_len = strlen(path);
    while (prefix_len > 0 && prefix[prefix_len - 1] == '/') {
        --prefix_len;
    }
    while (path_len > 0 && path[path_len - 1] == '/') {
        --path_len;
    }

    if (strncmp(path, prefix, prefix_len) == 0) {
        // We still need to check that the last path component of
        // 'path' does not simply start with the last path component of 'prefix'.
        const char* prefix_slash = find_last_char_between(prefix, prefix + prefix_len, '/');
        const char* prefix_part = prefix_slash ? prefix_slash + 1 : prefix;
        size_t prefix_part_len = (prefix + prefix_len - prefix_part);

        const char* path_part = path + (prefix_part - prefix);
        const char* path_slash = strchr(path_part, '/');
        size_t path_part_len = path_slash ? (size_t)(path_slash - path_part) : path_len - (path_part - path);

        return prefix_part_len == path_part_len;
    }

    return false;
}

static char **dup_argv(int argc, const char * const *argv, struct arena *arena)
{
    char **pointer_list = arena_malloc(arena, (argc + 1) * sizeof(char*));
    char **next_ptr = pointer_list;

    for (int i = 0; i < argc; ++i) {
        size_t len = strlen(argv[i]);
        char *str = arena_malloc(arena, len + 1);
        memcpy(str, argv[i], len + 1);
        *next_ptr = str;
        ++next_ptr;
    }
    *next_ptr = NULL;

    return pointer_list;
}

/* Converts all ("-o", "...") into ("-o..."). */
static void merge_o_args(
    int *argc,
    char **argv,
    struct arena *arena
)
{
    int i = 0;
    while (i < *argc) {
        char *arg = argv[i];
        if (strcmp(arg, "-o") == 0) {
            if (i + 1 < *argc) {
                char *merged = arena_malloc(arena, 2 + strlen(argv[i + 1]) + 1);
                merged[0] = '-';
                merged[1] = 'o';
                strcpy(&merged[2], argv[i + 1]);
                argv[i] = merged;

                for (int j = i + 1; j < *argc - 1; ++j) {
                    argv[j] = argv[j + 1];
                }
            }
            --*argc;
        }
        ++i;
    }
}

void filter_o_opts(
    bool (*keep)(const char* opt),
    int orig_argc,
    const char * const *orig_argv,
    int *new_argc,
    char ***new_argv,
    struct arena* arena
)
{
    int argc = orig_argc;
    char **argv = dup_argv(argc, orig_argv, arena);

    merge_o_args(&argc, argv, arena);

    for (int i = 0; i < argc; i++) {
        char *arg = argv[i];
        if (strncmp(arg, "-o", 2) == 0) {
            char *filtered = arena_malloc(arena, strlen(arg) + 1);
            char *filtered_end = filtered;

            const char *tok = strtok(arg + 2, ",");
            while (tok != NULL) {
                size_t tok_len = strlen(tok);
                if ((*keep)(tok)) {
                    if (filtered_end == filtered) {
                        *(filtered_end++) = '-';
                        *(filtered_end++) = 'o';
                    } else {
                        *(filtered_end++) = ',';
                    }
                    memcpy(filtered_end, tok, tok_len + 1);
                    filtered_end += tok_len;  // We'll overwrite the null terminator if we append more.
                }
                tok = strtok(NULL, ",");
            }

            if (filtered != filtered_end) {
                argv[i] = filtered;
            } else {
                for (int j = i; j < argc - 1; ++j) {
                    argv[j] = argv[j + 1];
                }
                --argc;
            }
        }
    }

    *new_argc = argc;
    *new_argv = argv;
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


void init_memory_block(struct memory_block *a, size_t initial_capacity)
{
    a->size = 0;
    a->capacity = initial_capacity;
    if (initial_capacity > 0) {
        a->ptr = (char *)malloc(initial_capacity);
    } else {
        a->ptr = NULL;
    }
}

void grow_memory_block(struct memory_block *a, size_t amount)
{
    size_t new_cap;

    a->size += amount;
    if (a->size >= a->capacity) {
        new_cap = a->capacity;
        while (new_cap < a->size) {
            if (new_cap == 0) {
                new_cap = 8;
            } else {
                if (new_cap > SIZE_MAX / 2) {
                    fprintf(stderr, "Memory block too large.");
                    abort();
                }
                new_cap *= 2;
            }
        }
        a->ptr = (char *)realloc(a->ptr, new_cap);
        a->capacity = new_cap;
    }
}

int append_to_memory_block(struct memory_block *a, const void *src, size_t src_size)
{
    size_t dest = a->size;
    grow_memory_block(a, src_size);
    memcpy(&a->ptr[dest], src, src_size);
    return dest;
}

void free_memory_block(struct memory_block *a)
{
    free(a->ptr);
    init_memory_block(a, 0);
}
