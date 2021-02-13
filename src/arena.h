#ifndef INC_BINDFS_ARENA_H
#define INC_BINDFS_ARENA_H

#include <stdlib.h>

struct block;
struct arena {
    struct block *cur_block;
};

void arena_init(struct arena *a);
void* arena_malloc(struct arena *a, size_t amount);
void arena_free(struct arena *a);

#endif
