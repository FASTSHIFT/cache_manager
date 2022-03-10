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

#include "cache_manager.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_MANAGER_INVALIDATE_ID 0

/*Enable log*/
#define CACHE_MANAGER_USE_LOG 1

/*Decrement life with this value on every open*/
#define CACHE_MANAGER_AGING 1

/*Boost life by this factor (multiply time_to_open with this value)*/
#define CACHE_MANAGER_LIFE_GAIN 1

/*Don't let life to be greater than this limit because it would require a lot of time to
 * "die" from very high values*/
#define CACHE_MANAGER_LIFE_LIMIT 1000

#define CACHE_MANAGER_REF_CNT_LIMIT 100

#define CACHE_MANAGER_MALLOC(size) malloc(size)
#define CACHE_MANAGER_REALLOC(ptr, size) realloc(ptr, size)
#define CACHE_MANAGER_FREE(ptr) free(ptr)
#define CACHE_MANAGER_RAND() rand()

#if CACHE_MANAGER_USE_LOG
#include <stdio.h>
#define CM_LOG(format, ...) printf("[CM]" format "\r\n", ##__VA_ARGS__)
#define CM_LOG_INFO(format, ...) CM_LOG("[Info] " format, ##__VA_ARGS__)
#define CM_LOG_WARN(format, ...) CM_LOG("[Warn] " format, ##__VA_ARGS__)
#define CM_LOG_ERROR(format, ...) CM_LOG("[Error] " format, ##__VA_ARGS__)
#else
#define CM_LOG_INFO(...)
#define CM_LOG_WARN(...)
#define CM_LOG_ERROR(...)
#endif

static uint32_t cm_tick_elaps(cache_manager_t* cm, uint32_t prev_tick)
{
    uint32_t act_time = cm->tick_get_cb();

    /*If there is no overflow in sys_time simple subtract*/
    if (act_time >= prev_tick) {
        prev_tick = act_time - prev_tick;
    } else {
        prev_tick = UINT32_MAX - prev_tick + 1;
        prev_tick += act_time;
    }

    return prev_tick;
}

static bool cm_open_node(cache_manager_t* cm, cache_manager_node_t* node, int id)
{
    uint32_t start_time = 0;
    cache_manager_node_t node_tmp = { 0 };

    CM_LOG_INFO("id:%d creating...", id);

    if (cm->tick_get_cb) {
        start_time = cm->tick_get_cb();
    }

    node_tmp.cache_manager = cm;
    node_tmp.id = id;
    bool success = cm->create_cb(&node_tmp);

    if (cm->tick_get_cb) {
        node_tmp.priv.time_to_open = cm_tick_elaps(cm, start_time);
        CM_LOG_INFO("time cost: %" PRIu32, node_tmp.priv.time_to_open);
    }

    if (node_tmp.priv.time_to_open == 0) {
        node_tmp.priv.time_to_open = 1;
    }

    if (success) {
        *node = node_tmp;
    } else {
        CM_LOG_WARN("id:%d create failed", id);
    }

    return success;
}

static void cm_close_node(cache_manager_node_t* node)
{
    if (node->id == CACHE_MANAGER_INVALIDATE_ID) {
        return;
    }

    cache_manager_t* cm = node->cache_manager;

    if (cm->delete_cb) {
        cm->delete_cb(node);
    }

    CM_LOG_INFO("id:%d closed, ref_cnt = %" PRIu32, node->id, node->priv.ref_cnt);

    memset(node, 0, sizeof(cache_manager_node_t));
}

static void cm_inc_node_ref_cnt(cache_manager_node_t* node)
{
    node->priv.ref_cnt++;
#if CACHE_MANAGER_REF_CNT_LIMIT
    if (node->priv.ref_cnt > CACHE_MANAGER_REF_CNT_LIMIT) {
        node->priv.ref_cnt = CACHE_MANAGER_REF_CNT_LIMIT;
    }
#endif
}

static cache_manager_node_t* cm_find_node(cache_manager_t* cm, int id)
{
    for (uint32_t i = 0; i < cm->cache_num; i++) {
        cache_manager_node_t* node = &(cm->cache_node_array[i]);
        if (id == node->id) {
            return node;
        }
    }
    return NULL;
}

static cache_manager_node_t* cm_find_empty_node(cache_manager_t* cm)
{
    return cm_find_node(cm, CACHE_MANAGER_INVALIDATE_ID);
}

static cache_manager_node_t* cm_find_reuse_lfu(cache_manager_t* cm)
{
    cache_manager_node_t* reuse_node = NULL;
    uint32_t ref_min = UINT32_MAX;

    for (uint32_t i = 0; i < cm->cache_num; i++) {
        cache_manager_node_t* node = &(cm->cache_node_array[i]);
        if (node->id != CACHE_MANAGER_INVALIDATE_ID) {
            if (node->priv.ref_cnt < ref_min) {
                reuse_node = node;
            }
        }
    }
    return reuse_node;
}

static cache_manager_node_t* cm_find_reuse_random(cache_manager_t* cm)
{
    uint32_t index = CACHE_MANAGER_RAND() % cm->cache_num;
    return &(cm->cache_node_array[index]);
}

static cache_manager_node_t* cm_find_reuse_life(cache_manager_t* cm)
{
    /*Find an entry to reuse. Select the entry with the least life*/
    cache_manager_node_t* reuse_node = NULL;
    int life_min = INT32_MAX;
    for (uint32_t i = 0; i < cm->cache_num; i++) {
        cache_manager_node_t* node = &(cm->cache_node_array[i]);
        if (node->id != CACHE_MANAGER_INVALIDATE_ID
            && node->priv.life < life_min) {
            reuse_node = node;
            life_min = node->priv.life;
        }
    }
    return reuse_node;
}

static cache_manager_node_t* cm_find_reuse_node(cache_manager_t* cm)
{
    switch (cm->mode) {
    case CACHE_MANAGER_MODE_LFU:
        return cm_find_reuse_lfu(cm);
        break;
    case CACHE_MANAGER_MODE_RANDOM:
        return cm_find_reuse_random(cm);
        break;
    case CACHE_MANAGER_MODE_LIFE:
        return cm_find_reuse_life(cm);
        break;

    default:
        CM_LOG_ERROR("unsupport cache mode: %d", cm->mode);
        break;
    }
    return NULL;
}

cache_manager_t* cm_create(
    uint32_t cache_num,
    cache_manager_mode_t mode,
    cache_manager_user_cb_t create_cb,
    cache_manager_user_cb_t delete_cb,
    cache_manager_tick_get_cb_t tick_get_cb,
    void* user_data)
{
    cache_manager_t* cm = CACHE_MANAGER_MALLOC(sizeof(cache_manager_t));

    if (!cm) {
        CM_LOG_ERROR("cache_manager malloc failed");
        return NULL;
    }

    memset(cm, 0, sizeof(cache_manager_t));

    cm->cache_node_array = CACHE_MANAGER_MALLOC(sizeof(cache_manager_node_t) * cache_num);

    if (!cm->cache_node_array) {
        CM_LOG_ERROR("cache_node_array malloc failed");
        CACHE_MANAGER_FREE(cm);
        return NULL;
    }

    memset(cm->cache_node_array, 0, sizeof(cache_manager_node_t) * cache_num);

    cm->cache_num = cache_num;
    cm->mode = mode;
    cm->create_cb = create_cb;
    cm->delete_cb = delete_cb;
    cm->tick_get_cb = tick_get_cb;
    cm->user_data = user_data;

    CM_LOG_INFO("cache_manager create OK, cache mode = %d, cache num = %d", mode, cache_num);

    return cm;
}

void cm_set_cache_num(cache_manager_t* cm, uint32_t cache_num)
{
    cm_clear(cm);
    cm->cache_node_array = CACHE_MANAGER_REALLOC(cm->cache_node_array, cache_num);

    if (!cm->cache_node_array) {
        CM_LOG_ERROR("cache_node_array realloc failed");
        return;
    }

    memset(cm->cache_node_array, 0, sizeof(cache_manager_node_t) * cache_num);
    cm->cache_num = cache_num;
}

void cm_delete(cache_manager_t* cm)
{
    cm_clear(cm);
    if (cm->cache_node_array) {
        CACHE_MANAGER_FREE(cm->cache_node_array);
        cm->cache_node_array = NULL;
    }
    CACHE_MANAGER_FREE(cm);
}

cache_manager_res_t cm_open(cache_manager_t* cm, int id, cache_manager_node_t** node_p)
{
    if (id == CACHE_MANAGER_INVALIDATE_ID) {
        return CACHE_MANAGER_RES_ERR_ID_INVALIDATE;
    }

    cache_manager_node_t* node;

    cm->cache_open_cnt++;

    if (cm->mode == CACHE_MANAGER_MODE_LIFE) {
        /*Decrement all lifes. Make the entries older*/
        node = &cm->cache_node_array[0];
        for (uint32_t i = 0; i < cm->cache_num; i++) {
            if (node->id != CACHE_MANAGER_INVALIDATE_ID
                && node->priv.life > INT32_MIN + CACHE_MANAGER_AGING) {
                node->priv.life -= CACHE_MANAGER_AGING;
            }
            node++;
        }
    }

    node = cm_find_node(cm, id);
    if (node) {
        cm_inc_node_ref_cnt(node);
        *node_p = node;
        cm->cache_hit_cnt++;
        CM_LOG_INFO("id:%d cache hit context %p, ref_cnt = %" PRIu32, node->id, node->context.ptr, node->priv.ref_cnt);

        if (cm->mode == CACHE_MANAGER_MODE_LIFE) {
            node->priv.life += node->priv.time_to_open * CACHE_MANAGER_LIFE_GAIN;
            if (node->priv.life > CACHE_MANAGER_LIFE_LIMIT) {
                node->priv.life = CACHE_MANAGER_LIFE_LIMIT;
            }
        }

        return CACHE_MANAGER_RES_OK;
    }

    CM_LOG_INFO("cache miss, find empty node...");

    node = cm_find_empty_node(cm);

    if (node) {
        if (cm_open_node(cm, node, id)) {
            cm_inc_node_ref_cnt(node);
            *node_p = node;
            return CACHE_MANAGER_RES_OK;
        } else {
            return CACHE_MANAGER_RES_ERR_CREATE_FAILED;
        }
    }

    CM_LOG_INFO("cache array full, find reuse node...");

    node = cm_find_reuse_node(cm);
    if (node) {
        cache_manager_node_t node_tmp = { 0 };

        if (cm_open_node(cm, &node_tmp, id)) {

            cm_close_node(node);
            cm_inc_node_ref_cnt(&node_tmp);

            *node = node_tmp;
            *node_p = node;

            return CACHE_MANAGER_RES_OK;
        } else {
            return CACHE_MANAGER_RES_ERR_CREATE_FAILED;
        }
    }

    CM_LOG_ERROR("can't find reuse node");

    return CACHE_MANAGER_RES_ERR_UNKNOW;
}

cache_manager_res_t cm_invalidate(cache_manager_t* cm, int id)
{
    if (id == CACHE_MANAGER_INVALIDATE_ID) {
        return CACHE_MANAGER_RES_ERR_ID_INVALIDATE;
    }

    for (uint32_t i = 0; i < cm->cache_num; i++) {
        cache_manager_node_t* node = &(cm->cache_node_array[i]);
        if (id == node->id) {
            cm_close_node(node);
            return CACHE_MANAGER_RES_OK;
        }
    }

    return CACHE_MANAGER_RES_ERR_ID_NOT_FOUND;
}

void cm_clear(cache_manager_t* cm)
{
    for (uint32_t i = 0; i < cm->cache_num; i++) {
        cache_manager_node_t* node = &(cm->cache_node_array[i]);
        if (node->id != CACHE_MANAGER_INVALIDATE_ID) {
            cm_close_node(node);
        }
    }
}

int cm_get_cache_hit_rate(cache_manager_t* cm)
{
    if (cm->cache_open_cnt == 0) {
        return 0;
    }

    return cm->cache_hit_cnt * 1000 / cm->cache_open_cnt;
}

void cm_reset_cache_hit_cnt(cache_manager_t* cm)
{
    cm->cache_hit_cnt = 0;
    cm->cache_open_cnt = 0;
}
