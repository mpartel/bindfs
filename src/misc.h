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

#ifndef INC_BINDFS_MISC_H
#define INC_BINDFS_MISC_H

#include <stdbool.h>
#include <stdlib.h>

#include "arena.h"

/* Counts the number of times ch occurs in s. */
int count_chars(const char *s, char ch);

/* Counts the number of times sub occurs in s. */
int count_substrs(const char *s, const char *sub);

/* Creates a duplicate string of all the characters in s before
   an end character is reached. */
char *strdup_until(const char *s, const char *endchars);

/* Like sprintf but writes to an automatically malloc'ed buffer. */
char *sprintf_new(const char *format, ...);

/* Returns a pointer to the first character after the
   final slash of path, or path itself if it contains no slashes.
   If the path ends with a slash, then the result is an empty
   string.
   Returns NULL if path is NULL. */
const char *my_basename(const char *path);

/* A thread-safe version of dirname, with slightly different behavior.
   If path is ".", returns "..".
   If path is "/", returns "/".
   If path has an initial slash but no other slashes, returns "/".
   If path contains a slash, replaces the last slash with a '\0' and returns path.
   Otherwise, returns ".". */
const char *my_dirname(char *path);

/* Returns true if the initial path components of 'path' are the path components of 'prefix'.
   Assumes 'strlen(prefix) == prefix_len'. */
bool path_starts_with(const char *path, const char* prefix, size_t prefix_len);

/* Filters arguments in comma-separated lists prefixed by '-o'.
 * Allocates 'new_argv' and its strings, as well as some temporary data, into 'arena'. */
void filter_o_opts(
    bool (*keep)(const char* opt),
    int argc,
    const char * const *argv,
    int *new_argc,
    char ***new_argv,
    struct arena *arena
);

/* Reallocs `*array` (may be NULL) to be at least one larger
   than `*capacity` (may be 0) and stores the new capacity
   in `*capacity`. */
#define grow_array(array, capacity, member_size) grow_array_impl((void**)(array), (capacity), (member_size))
void grow_array_impl(void **array, int *capacity, int member_size);

/* Returns 1 on success, 0 on syntax error. */
int parse_byte_count(const char *str, double *result);

/* An allocation of contiguous memory with convenient functions for
   growing it and appending to it. */
struct memory_block {
    char *ptr;
    size_t size;
    size_t capacity;
};

#define MEMORY_BLOCK_INITIALIZER { NULL, 0, 0 }

void init_memory_block(struct memory_block *a, size_t initial_capacity);
void grow_memory_block(struct memory_block *a, size_t amount);
int append_to_memory_block(struct memory_block *a, const void *src, size_t src_size);
void free_memory_block(struct memory_block *a);

#define MEMORY_BLOCK_GET(a, offset) (&(a).ptr[(offset)])

#endif
