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

#include "permchain.h"
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "misc.h"
#include "debug.h"

/* constants for permchain->flags */
#define PC_APPLY_FILES 1
#define PC_APPLY_DIRS 2
#define PC_FLAGS_DEFAULT ((PC_APPLY_FILES) | (PC_APPLY_DIRS))

struct permchain {
    mode_t mask; /* which permissions to apply to */
    char op; /* one of '=', '+', '-', 'o' (octal) or '\0' */
    union {
        char operands[16]; /* a subset of rwxXstugo */
        unsigned int octal;
    };
    int flags;
    struct permchain *next;
};

struct permchain *permchain_create()
{
    struct permchain *pc = malloc(sizeof(struct permchain));
    pc->mask = 0000;
    pc->op = '\0';
    memset(pc->operands, '\0', sizeof(pc->operands));
    pc->next = NULL;
    pc->flags = PC_FLAGS_DEFAULT;
    return pc;
}


static int add_chmod_rule_to_permchain(const char *start, const char *end,
                                       struct permchain *pc);
static int add_octal_rule_to_permchain(const char *start, const char *end,
                                       struct permchain *pc);
static mode_t modebits_to_all(int perms); /* e.g. 5 -> 0555 */



static int add_chmod_rule_to_permchain(const char *start, const char *end,
                                       struct permchain *pc)
{
    int ret = -1;

    int len = end - start;
    char *buf = alloca((len + 1) * sizeof(char));
    const char *p = buf;

    enum {LHS, RHS} state = LHS;
    struct permchain *newpc = permchain_create();
    char *operands_ptr = newpc->operands;

    newpc->flags = 0; /* Reset to PC_FLAGS_DEFAULT in the end if not modified */

    memcpy(buf, start, len);
    buf[len] = '\0';

    while (*p != '\0') {
        if (state == LHS) {
            switch (*p) {
            case 'u':
                newpc->mask |= 0700;
                break;
            case 'g':
                newpc->mask |= 0070;
                break;
            case 'o':
                newpc->mask |= 0007;
                break;
            case 'a':
                newpc->mask = 0777;
                break;
            case 'f':
                newpc->flags |= PC_APPLY_FILES;
                break;
            case 'd':
                newpc->flags |= PC_APPLY_DIRS;
                break;
            case '=':
            case '+':
            case '-':
                if (p == buf) /* first char -> default to 'a' */
                    newpc->mask = 0777;
                newpc->op = *p;
                state = RHS;
                break;
            default:
                goto error;
            }
        } else {
            switch (*p) {
            case 'r':
            case 'w':
            case 'x':
            case 'X':
            case 'D':
            case 's':
            case 't':
            case 'u':
            case 'g':
            case 'o':
                if (!strchr(newpc->operands, *p)) {
                    *(operands_ptr++) = *p;
                }
                break;
            default:
                goto error;
            }
        }

        ++p;
    }

    ret = 0;
error:
    if (newpc->flags == 0)
        newpc->flags = PC_FLAGS_DEFAULT;
    if (ret == 0)
        permchain_cat(pc, newpc);
    else
        permchain_destroy(newpc);
    return ret;
}

static int add_octal_rule_to_permchain(const char *start, const char *end,
                                       struct permchain *pc)
{
    struct permchain *newpc = permchain_create();
    long mode = strtol(start, NULL, 8);

    if (mode < 0 || mode > 0777) {
        permchain_destroy(newpc);
        return -1;
    }

    newpc->mask = 0777;
    newpc->op = 'o';
    newpc->octal = mode;

    permchain_cat(pc, newpc);
    return 0;
}

int add_chmod_rules_to_permchain(const char *rule, struct permchain *pc)
{
    int ret = -1;
    const char *start, *end;
    struct permchain *newpc = permchain_create();

    assert(rule != 0);

    end = start = rule;

    while (*end != '\0') {
        /* find delimiter or end of list */
        while (*end != ',' && *end != ':' && *end != '\0')
            ++end;

        if (start == end) /* empty rule */
            goto error;

        assert(start < end);

        if (isdigit(*start)) {
            if (add_octal_rule_to_permchain(start, end, newpc) != 0)
                goto error;

        } else {
            if (add_chmod_rule_to_permchain(start, end, newpc) != 0)
                goto error;
        }

        if (*end == ',' || *end == ':')
            start = ++end;
    }

    ret = 0;
error:
    if (ret == 0)
        permchain_cat(pc, newpc);
    else
        permchain_destroy(newpc);
    return ret;
}

void permchain_cat(struct permchain *left, struct permchain *right)
{
    while (left->next != NULL)
        left = left->next;
    left->next = right;
}

mode_t modebits_to_all(int perms)
{
    mode_t m = perms;
    m |= perms << 3;
    m |= perms << 6;
    return m;
}

mode_t permchain_apply(struct permchain *pc, mode_t tgtmode)
{
    mode_t original_mode = tgtmode;
    mode_t mode = 0000;
    const char *p;

    while (pc != NULL) {
        #if BINDFS_DEBUG
        if (pc->op == 'o')
            DPRINTF("STAT MODE: %o, op = %c %o", tgtmode, pc->op, pc->octal);
        else
            DPRINTF("STAT MODE: %o, op = %c%s", tgtmode, pc->op, pc->operands);
        #endif

        if (pc->op == '\0') {
            pc = pc->next;
            continue;
        }

        if ((S_ISDIR(tgtmode) && !(pc->flags & PC_APPLY_DIRS))
            ||
            (!S_ISDIR(tgtmode) && !(pc->flags & PC_APPLY_FILES))) {

            pc = pc->next;
            continue;
        }

        if (pc->op == '=' || pc->op == '+' || pc->op == '-') {

            mode = 0000;

            for (p = pc->operands; *p != '\0'; ++p) {
                switch (*p) {
                case 'r':
                    mode |= 0444;
                    break;
                case 'w':
                    mode |= 0222;
                    break;
                case 'x':
                    mode |= 0111;
                    break;
                case 'X':
                    if (S_ISDIR(original_mode) || ((original_mode & 0111) != 0))
                        mode |= 0111;
                    break;
                case 'D':
                    if (S_ISDIR(original_mode))
                        mode |= 0111;
                    break;
                case 's':
                case 't':
                    /* ignored */
                    break;
                case 'u':
                    mode |= modebits_to_all((original_mode & 0700) >> 6);
                    break;
                case 'g':
                    mode |= modebits_to_all((original_mode & 0070) >> 3);
                    break;
                case 'o':
                    mode |= modebits_to_all(original_mode & 0007);
                    break;
                default:
                    assert(0);
                }
            }
            mode &= pc->mask;
        }

        switch (pc->op) {
        case '=':
            tgtmode = (tgtmode & ~pc->mask) | mode;
            break;
        case '+':
            tgtmode |= mode;
            break;
        case '-':
            tgtmode &= ~0777 | ~mode;
            break;
        case 'o':
            tgtmode = (tgtmode & ~0777) | pc->octal;
            break;
        default:
            assert(0);
        }
        pc = pc->next;
        DPRINTF("       =>: %o", tgtmode);
    }
    return tgtmode;
}

void permchain_destroy(struct permchain *pc)
{
    struct permchain *next;
    while (pc) {
        next = pc->next;
        free(pc);
        pc = next;
    }
}

