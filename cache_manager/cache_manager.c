#include "cache_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_MANAGER_INVALIDATE_ID 0

#define CACHE_MANAGER_USE_LOG 1

#if CACHE_MANAGER_USE_LOG
#include <stdio.h>
#define _CM_LOG(format, ...) printf("[CM]" format "\r\n", ##__VA_ARGS__)
#define CM_LOG_INFO(format, ...) _CM_LOG("[Info] " format, ##__VA_ARGS__)
#define CM_LOG_WARN(format, ...) _CM_LOG("[Warn] " format, ##__VA_ARGS__)
#define CM_LOG_ERROR(format, ...) _CM_LOG("[Error] " format, ##__VA_ARGS__)
#else
#define CM_LOG_INFO(...)
#define CM_LOG_WARN(...)
#define CM_LOG_ERROR(...)
#endif

uint32_t cm_tick_elaps(cache_manager_t* cm, uint32_t prev_tick)
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

static void cm_close_node(cache_manager_node_t* node)
{
    if (node->id == CACHE_MANAGER_INVALIDATE_ID) {
        return;
    }

    cache_manager_t* cm = node->cache_manager;

    if (cm->delete_cb) {
        cm->delete_cb(node);
    }

    CM_LOG_INFO("id:%d closed, ref_cnt = %d", node->id, node->priv.ref_cnt);

    memset(node, 0, sizeof(cache_manager_node_t));
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
    uint32_t index = rand() % cm->cache_num;
    return &(cm->cache_node_array[index]);
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

    default:
        CM_LOG_ERROR("unsupport cache mode: %d", cm->mode);
        break;
    }
    return NULL;
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
        CM_LOG_INFO("time cost: %d", node_tmp.priv.time_to_open);
    }

    if (success) {
        *node = node_tmp;
    } else {
        CM_LOG_WARN("id:%d create failed", id);
    }

    return success;
}

cache_manager_t* cm_create(
    uint32_t cache_num,
    cache_manager_mode_t mode,
    cache_manager_user_cb_t create_cb,
    cache_manager_user_cb_t delete_cb,
    cache_manager_tick_get_cb_t tick_get_cb,
    void* user_data)
{
    cache_manager_t* cm = malloc(sizeof(cache_manager_t));

    if (!cm) {
        return NULL;
    }

    memset(cm, 0, sizeof(cache_manager_t));

    cm->cache_node_array = malloc(sizeof(cache_manager_node_t) * cache_num);

    if (!cm->cache_node_array) {
        free(cm);
        return NULL;
    }

    memset(cm->cache_node_array, 0, sizeof(cache_manager_node_t) * cache_num);

    cm->cache_num = cache_num;
    cm->mode = mode;
    cm->create_cb = create_cb;
    cm->delete_cb = delete_cb;
    cm->tick_get_cb = tick_get_cb;
    cm->user_data = user_data;

    return cm;
}

void cm_set_cache_num(cache_manager_t* cm, uint32_t cache_num)
{
    cm_clear(cm);
    cm->cache_node_array = realloc(cm->cache_node_array, cache_num);

    if (!cm->cache_node_array) {
        return;
    }

    memset(cm->cache_node_array, 0, sizeof(cache_manager_node_t) * cache_num);
    cm->cache_num = cache_num;
}

void cm_delete(cache_manager_t* cm)
{
    cm_clear(cm);
    if(cm->cache_node_array)
    {
        free(cm->cache_node_array);
        cm->cache_node_array = NULL;
    }
    free(cm);
}

cache_manager_res_t cm_open(cache_manager_t* cm, int id, cache_manager_node_t** node_p)
{
    if (id == CACHE_MANAGER_INVALIDATE_ID) {
        return CACHE_MANAGER_RES_ERR_ID_INVALIDATE;
    }

    cm->cache_open_cnt++;

    cache_manager_node_t* node;

    node = cm_find_node(cm, id);
    if (node) {
        node->priv.ref_cnt++;
        *node_p = node;
        cm->cache_hit_cnt++;
        CM_LOG_INFO("id:%d cache hit context %p, ref_cnt = %d", node->id, node->context.ptr, node->priv.ref_cnt);
        return CACHE_MANAGER_RES_OK;
    }

    CM_LOG_INFO("cache miss, find empty node...");

    node = cm_find_empty_node(cm);

    if (node) {
        if (cm_open_node(cm, node, id)) {
            node->priv.ref_cnt++;
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

            node_tmp.priv.ref_cnt++;
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
