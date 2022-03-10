/*
 * MIT License
 * Copyright (c) 2022 _VIFEXTech
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "cache_manager/cache_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct {
    char name[32];
    int time_cost;
} user_data_t;

static user_data_t user_data_arr[] = {
    { "A", 1 },
    { "BB", 2 },
    { "CCC", 3 },
    { "DDDD", 4 },
    { "EEEEE", 5 },
    { "FFFFFF", 6 },
    { "GGGGGGG", 7 },
    { "1", 1 },
    { "22", 2 },
    { "333", 3 },
    { "4444", 4 },
    { "55555", 5 },
    { "666666", 6 },
    { "7777777", 7 },
};

static bool create_cb(cache_manager_node_t* node)
{
    int id = node->id - 1;

    if (id >= ARRAY_SIZE(user_data_arr)) {
        return false;
    }

    user_data_t* user_data_p = malloc(sizeof(user_data_t));

    *user_data_p = user_data_arr[id];

    usleep(user_data_arr[id].time_cost * 1000);

    node->context.ptr = user_data_p;
    node->context.size = sizeof(user_data_t);

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
        id = rand() % (ARRAY_SIZE(user_data_arr) - 1);
    } else {
        id++;
    }

    id %= (ARRAY_SIZE(user_data_arr) - 1);

    return id + 1;
}

int main(int argc, char* argv[])
{
    printf("cache_manager test\n");

    cache_manager_t* cm = cm_create(
        6,
        CACHE_MANAGER_MODE_LRU,
        create_cb,
        delete_cb,
        custom_tick_get,
        NULL);

    srand(custom_tick_get());

    for (int i = 0; i < 1000; i++) {
        int id = gen_id(true);

        cache_manager_node_t* node;

        if (cm_open(cm, id, &node) == CACHE_MANAGER_RES_OK) {
            user_data_t* user_data_p = node->context.ptr;
            printf("id:%d open success, data = %s\n", id, user_data_p->name);
        } else {
            printf("id:%d open failed\n", id);
        }
    }

    printf("cache hit rate: %0.1f%%\n", (double)cm_get_cache_hit_rate(cm) / 10);

    cm_delete(cm);

    return 0;
}
