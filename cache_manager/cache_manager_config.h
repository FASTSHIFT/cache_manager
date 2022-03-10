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

#ifndef __CACHE_MANAGER_CONFIG_H__
#define __CACHE_MANAGER_CONFIG_H__

#include <stdio.h>
#include <stdlib.h>

/*Enable log*/
#define CACHE_MANAGER_USE_LOG 1

#if CACHE_MANAGER_USE_LOG
#define CM_LOG(format, ...) printf("[CM]" format "\r\n", ##__VA_ARGS__)
#define CM_LOG_INFO(format, ...) CM_LOG("[Info] " format, ##__VA_ARGS__)
#define CM_LOG_WARN(format, ...) CM_LOG("[Warn] " format, ##__VA_ARGS__)
#define CM_LOG_ERROR(format, ...) CM_LOG("[Error] " format, ##__VA_ARGS__)
#else
#define CM_LOG_INFO(...)
#define CM_LOG_WARN(...)
#define CM_LOG_ERROR(...)
#endif

#define CACHE_MANAGER_MALLOC(size) malloc(size)
#define CACHE_MANAGER_REALLOC(ptr, size) realloc(ptr, size)
#define CACHE_MANAGER_FREE(ptr) free(ptr)
#define CACHE_MANAGER_RAND() rand()

#endif
