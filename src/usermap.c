
#include "usermap.h"
#include "userinfo.h"
#include <stdlib.h>

struct UserMap {
    uid_t *user_from;
    uid_t *user_to;
    gid_t *group_from;
    gid_t *group_to;
    int user_capacity;
    int group_capacity;
    int user_size;
    int group_size;
};

UserMap *usermap_create()
{
    UserMap* map = (UserMap*)malloc(sizeof(UserMap));
    map->user_from = NULL;
    map->user_to = NULL;
    map->group_from = NULL;
    map->group_to = NULL;
    map->user_capacity = 0;
    map->group_capacity = 0;
    map->user_size = 0;
    map->group_size = 0;
    return map;
}

void usermap_destroy(UserMap *map)
{
    free(map->user_from);
    free(map->user_to);
    free(map->group_from);
    free(map->group_to);
    free(map);
}

UsermapStatus usermap_add_uid(UserMap *map, uid_t from, uid_t to)
{
    int i;
    if (from == to) {
        return usermap_status_ok;
    }
    if (map->user_size == map->user_capacity) {
        map->user_capacity *= 2;
        map->user_from = (uid_t*)realloc(map->user_from, map->user_capacity * sizeof(uid_t));
        map->user_to = (uid_t*)realloc(map->user_to, map->user_capacity * sizeof(uid_t));
    }
    if (usermap_get_uid_or_default(map, from, -1) != -1) {
        return usermap_status_duplicate_key;
    }
    i = map->user_size;
    map->user_from[i] = from;
    map->user_to[i] = to;
    map->user_size += 1;
    return usermap_status_ok;
}

UsermapStatus usermap_add_gid(UserMap *map, gid_t from, gid_t to)
{
    int i;
    if (from == to) {
        return usermap_status_ok;
    }
    if (map->group_size == map->group_capacity) {
        map->group_capacity *= 2;
        map->group_from = (gid_t*)realloc(map->group_from, map->group_capacity * sizeof(gid_t));
        map->group_to = (gid_t*)realloc(map->group_to, map->group_capacity * sizeof(gid_t));
    }
    if (usermap_get_gid_or_default(map, from, -1) != -1) {
        return usermap_status_duplicate_key;
    }
    i = map->group_size;
    map->group_from[i] = from;
    map->group_to[i] = to;
    map->group_size += 1;
    return usermap_status_ok;
}

const char* usermap_errorstr(UsermapStatus status)
{
    switch (status) {
        case usermap_status_ok: return "ok";
        case usermap_status_duplicate_key: return "user mapped twice";
        default: return "unknown error";
    }
}

uid_t usermap_get_uid_or_default(UserMap *map, uid_t u, uid_t deflt)
{
    int i;
    for (i = 0; i < map->user_size; ++i) {
        if (map->user_from[i] == u) {
            return map->user_to[i];
        }
    }
    return deflt;
}

gid_t usermap_get_gid_or_default(UserMap *map, gid_t g, gid_t deflt)
{
    int i;
    for (i = 0; i < map->group_size; ++i) {
        if (map->group_from[i] == g) {
            return map->group_to[i];
        }
    }
    return deflt;
}
