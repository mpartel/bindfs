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

#ifndef INC_BINDFS_MISC_H
#define INC_BINDFS_MISC_H

/* Counts the number of times ch occurs in s. */
int count_chars(const char *s, char ch);

/* Counts the number of times sub occurs in s. */
int count_substrs(const char *s, const char *sub);

/* Creates a duplicate string of all the characters in s before
   an end character is reached. */
char *strdup_until(const char *s, const char *endchars);

/* Returns a pointer to the first character after the
   final slash of path, or path itself if it contains no slashes.
   If the path ends with a slash, then the result is an empty
   string.
   Returns NULL if path is NULL. */
const char *my_basename(const char *path);

#endif
