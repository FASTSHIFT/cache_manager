#include "cache_manager/cache_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

static const char* str_arr[] = {
    "A",
    "BB",
    "CCC",
    "DDDD",
    "EEEEE",
    "FFFFFF",
    "GGGGGGG",
    "1",
    "22",
    "333",
    "4444",
    "55555",
    "666666",
    "7777777",
};

static bool create_cb(cache_manager_node_t* node)
{
    int id = node->id - 1;

    if (id >= ARRAY_SIZE(str_arr)) {
        return false;
    }

    uint32_t size = strlen(str_arr[id]) + 1;
    char* str = malloc(size);

    strcpy(str, str_arr[id]);

    node->context.ptr = str;
    node->context.size = size;

    return true;
}

static bool delete_cb(cache_manager_node_t* node)
{
    free(node->context.ptr);
    return true;
}

static uint32_t custom_tick_get(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    uint32_t tick = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    return tick;
}

static int gen_id(bool rnd)
{
    static int id = 0;

    if (rnd) {
        id = rand() % (ARRAY_SIZE(str_arr) - 1);
    } else {
        id++;
    }

    id %= (ARRAY_SIZE(str_arr) - 1);

    return id + 1;
}

int main(int argc, char* argv[])
{
    printf("cache_manager test\n");

    cache_manager_t* cm = cm_create(
        5,
        CACHE_MANAGER_MODE_RANDOM,
        create_cb,
        delete_cb,
        custom_tick_get,
        NULL);

    srand(custom_tick_get());

    for (int i = 0; i < 10000; i++) {

        int id = gen_id(false);

        cache_manager_node_t* node;

        if (cm_open(cm, id, &node) == CACHE_MANAGER_RES_OK) {
            printf("id:%d open success, data = %s\n", id, (const char*)node->context.ptr);
        } else {
            printf("id:%d open failed\n", id);
        }
    }

    printf("cache hit rate: %d\n", cm_get_cache_hit_rate(cm));

    cm_delete(cm);

    return 0;
}
