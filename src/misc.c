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

#include "misc.h"
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
