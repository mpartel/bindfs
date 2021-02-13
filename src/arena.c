#include "arena.h"

struct block {
    size_t size;
    size_t used;
    struct block *next;
    char data_start[];
};

static const size_t min_block_room = 16 * 1024 - sizeof(struct block);

static void add_block(struct arena* a, size_t room_wanted)
{
    if (room_wanted < min_block_room) {
        room_wanted = min_block_room;
    }
    struct block *new_block = malloc(sizeof(struct block) + room_wanted);
    new_block->size = room_wanted;
    new_block->used = 0;
    new_block->next = a->cur_block;
    a->cur_block = new_block;
}

void arena_init(struct arena *a)
{
    a->cur_block = NULL;
}

void* arena_malloc(struct arena *a, size_t amount)
{
    struct block* b = a->cur_block;
    if (b == NULL || b->size - b->used < amount) {
        add_block(a, amount);
        b = a->cur_block;
    }
    void* result = &b->data_start[b->used];
    b->used += amount;
    return result;
}

void arena_free(struct arena *a)
{
    struct block* b = a->cur_block;
    while (b != NULL) {
        struct block* next = b->next;
        free(b);
        b = next;
    }
    a->cur_block = NULL;
}
