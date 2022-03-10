#ifndef __CACHE_MANAGER_H__
#define __CACHE_MANAGER_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cache_manager_s;
struct cache_manager_node_s;

typedef bool (*cache_manager_user_cb_t)(struct cache_manager_node_s* node);
typedef uint32_t (*cache_manager_tick_get_cb_t)(void);

typedef struct cache_manager_node_s {
    struct cache_manager_s* cache_manager;
    int id;

    struct {
        void* ptr;
        uint32_t size;
    } context;

    struct {
        int32_t life;
        uint32_t ref_cnt;
        uint32_t time_to_open;
    } priv;
} cache_manager_node_t;

typedef enum cache_manager_mode_e {
    CACHE_MANAGER_MODE_LIFE,
    CACHE_MANAGER_MODE_FIFO, /* first in first out */
    CACHE_MANAGER_MODE_LFU, /* less frequently used */
    CACHE_MANAGER_MODE_LRU, /* least recently used */
    CACHE_MANAGER_MODE_RANDOM, /* random */
    _CACHE_MANAGER_MODE_LAST
} cache_manager_mode_t;

typedef enum cache_manager_res_e {
    CACHE_MANAGER_RES_OK,
    CACHE_MANAGER_RES_ERR_ID_NOT_FOUND,
    CACHE_MANAGER_RES_ERR_ID_EXIST,
    CACHE_MANAGER_RES_ERR_ID_INVALIDATE,
    CACHE_MANAGER_RES_ERR_CREATE_FAILED,
    CACHE_MANAGER_RES_ERR_UNKNOW
} cache_manager_res_t;

typedef struct cache_manager_s {
    cache_manager_node_t* cache_node_array;
    uint32_t cache_num;
    uint32_t cache_hit_cnt;
    uint32_t cache_open_cnt;
    cache_manager_mode_t mode;

    cache_manager_user_cb_t create_cb;
    cache_manager_user_cb_t delete_cb;
    cache_manager_tick_get_cb_t tick_get_cb;

    void* user_data;
} cache_manager_t;

cache_manager_t* cm_create(
    uint32_t cache_num,
    cache_manager_mode_t mode,
    cache_manager_user_cb_t create_cb,
    cache_manager_user_cb_t delete_cb,
    cache_manager_tick_get_cb_t tick_get_cb,
    void* user_data);
void cm_set_cache_num(cache_manager_t* cm, uint32_t cache_num);
void cm_delete(cache_manager_t* cm);
cache_manager_res_t cm_open(cache_manager_t* cm, int id, cache_manager_node_t** node_p);
cache_manager_res_t cm_invalidate(cache_manager_t* cm, int id);
void cm_clear(cache_manager_t* cm);

int cm_get_cache_hit_rate(cache_manager_t* cm);
void cm_reset_cache_hit_cnt(cache_manager_t* cm);

#ifdef __cplusplus
}
#endif

#endif /* __CACHE_MANAGER_H__ */
