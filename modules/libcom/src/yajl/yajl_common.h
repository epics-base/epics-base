/*
 * Copyright (c) 2007-2011, Lloyd Hilaiel <lloyd@hilaiel.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __YAJL_COMMON_H__
#define __YAJL_COMMON_H__

#include <stddef.h>
#include <shareLib.h>

#include <epicsVersion.h>

#ifdef __cplusplus
extern "C" {
#endif    

/** YAJL API history in brief
 *
 * Originally macro not defined
 *   YAJL 1.0.12
 *   Bundled with EPICS Base 3.15.0.1
 *
 * YAJL 2.1.0
 *   Changes argument type for yajl_integer() from 'int' to 'long long'
 *   Changes argument type for yajl_string() and yajl_map_key() from 'unsigned' to 'size_t'
 *   Replacement of struct yajl_parser_config with yajl_config()
 *   Replacement of yajl_parse_complete() with yajl_complete_parse()
 */
#define EPICS_YAJL_VERSION VERSION_INT(2,1,0,0)

#define YAJL_MAX_DEPTH 128

#define YAJL_API epicsShareFunc

/** pointer to a malloc function, supporting client overriding memory
 *  allocation routines */
typedef void * (*yajl_malloc_func)(void *ctx, size_t sz);

/** pointer to a free function, supporting client overriding memory
 *  allocation routines */
typedef void (*yajl_free_func)(void *ctx, void * ptr);

/** pointer to a realloc function which can resize an allocation. */
typedef void * (*yajl_realloc_func)(void *ctx, void * ptr, size_t sz);

/** A structure which can be passed to yajl_*_alloc routines to allow the
 *  client to specify memory allocation functions to be used. */
typedef struct
{
    /** pointer to a function that can allocate uninitialized memory */
    yajl_malloc_func malloc;
    /** pointer to a function that can resize memory allocations */
    yajl_realloc_func realloc;
    /** pointer to a function that can free memory allocated using
     *  reallocFunction or mallocFunction */
    yajl_free_func free;
    /** a context pointer that will be passed to above allocation routines */
    void * ctx;
} yajl_alloc_funcs;

#ifdef __cplusplus
}
#endif

#endif
