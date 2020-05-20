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

/**
 * \file yajl_gen.h
 * \brief Interface to YAJL's JSON generation facilities.
 * \author Lloyd Hilaiel
 */

#ifndef __YAJL_GEN_H__
#define __YAJL_GEN_H__

#include "yajl_common.h"

#ifdef __cplusplus
extern "C" {
#endif
    /** Generator status codes */
    typedef enum {
        /** No error */
        yajl_gen_status_ok = 0,
        /** At a point where a map key is generated, a function other than
         *  yajl_gen_string() was called */
        yajl_gen_keys_must_be_strings,
        /** YAJL's maximum generation depth was exceeded.  see
         *  \ref YAJL_MAX_DEPTH */
        yajl_max_depth_exceeded,
        /** A generator function (yajl_gen_XXX) was called while in an error
         *  state */
        yajl_gen_in_error_state,
        /** A complete JSON document has been generated */
        yajl_gen_generation_complete,
        /** yajl_gen_double() was passed an invalid floating point value
         *  (infinity or NaN). */
        yajl_gen_invalid_number,
        /** A print callback was passed in, so there is no internal
         * buffer to get from */
        yajl_gen_no_buf,
        /** Returned from yajl_gen_string() when the yajl_gen_validate_utf8()
         *  option is enabled and an invalid was passed by client code.
         */
        yajl_gen_invalid_string
    } yajl_gen_status;

    /** An opaque handle to a generator */
    typedef struct yajl_gen_t * yajl_gen;

    /** A callback used for "printing" the results. */
    typedef void (*yajl_print_t)(void * ctx,
                                 const char * str,
                                 size_t len);

    /** Configuration parameters for the parser, these may be passed to
     *  yajl_gen_config() along with option specific argument(s).  In general,
     *  all configuration parameters default to *off*. */
    typedef enum {
        /** Generate indented (beautiful) output */
        yajl_gen_beautify = 0x01,
        /**
         * Set an indent string which is used when yajl_gen_beautify()
         * is enabled.  Maybe something like \\t or some number of
         * spaces.  The default is four spaces ' '.
         */
        yajl_gen_indent_string = 0x02,
        /**
         * Set a function and context argument that should be used to
         * output generated json.  the function should conform to the
         * yajl_print_t() prototype while the context argument is a
         * void * of your choosing.
         *
         * Example: \code{.cpp}
         *   yajl_gen_config(g, yajl_gen_print_callback, myFunc, myVoidPtr);
	 * \endcode
         */
        yajl_gen_print_callback = 0x04,
        /**
         * Normally the generator does not validate that strings you
         * pass to it via yajl_gen_string() are valid UTF8.  Enabling
         * this option will cause it to do so.
         */
        yajl_gen_validate_utf8 = 0x08,
        /**
         * The forward solidus (slash or '/' in human) is not required to be
         * escaped in json text.  By default, YAJL will not escape it in the
         * iterest of saving bytes.  Setting this flag will cause YAJL to
         * always escape '/' in generated JSON strings.
         */
        yajl_gen_escape_solidus = 0x10
    } yajl_gen_option;

    /** Allow the modification of generator options subsequent to handle
     *  allocation (via yajl_alloc())
     *  \returns zero in case of errors, non-zero otherwise
     */
    YAJL_API int yajl_gen_config(yajl_gen g, yajl_gen_option opt, ...);

    /** Allocate a generator handle
     *  \param allocFuncs An optional pointer to a structure which allows
     *                    the client to overide the memory allocation
     *                    used by yajl.  May be NULL, in which case
     *                    malloc(), free() and realloc() will be used.
     *
     *  \returns an allocated handle on success, NULL on failure (bad params)
     */
    YAJL_API yajl_gen yajl_gen_alloc(const yajl_alloc_funcs * allocFuncs);

    /** Free a generator handle */
    YAJL_API void yajl_gen_free(yajl_gen handle);

    /** Generate an integer number. */
    YAJL_API yajl_gen_status yajl_gen_integer(yajl_gen hand, long long int number);
    /** Generate a floating point number. \p number may not be infinity or
     *  NaN, as these have no representation in JSON.  In these cases the
     *  generator will return \ref yajl_gen_invalid_number */
    YAJL_API yajl_gen_status yajl_gen_double(yajl_gen hand, double number);
    /** Generate a number from the string given in \p num. */
    YAJL_API yajl_gen_status yajl_gen_number(yajl_gen hand,
                                             const char * num,
                                             size_t len);
    /** Generate a string value or map key from \p str. */
    YAJL_API yajl_gen_status yajl_gen_string(yajl_gen hand,
                                             const unsigned char * str,
                                             size_t len);
    /** Generate a \c null value. */
    YAJL_API yajl_gen_status yajl_gen_null(yajl_gen hand);
    /** Generate a \c true or \c false value from \p boolean. */
    YAJL_API yajl_gen_status yajl_gen_bool(yajl_gen hand, int boolean);
    /** Start generating a JSON map. This should be followed by calls to
     *  yajl_gen_string() to provide a key and another yajl_gen routine
     *  to provide the value associated with that key. */
    YAJL_API yajl_gen_status yajl_gen_map_open(yajl_gen hand);
    /** Finish generating a JSON map. */
    YAJL_API yajl_gen_status yajl_gen_map_close(yajl_gen hand);
    /** Start generating a JSON array. */
    YAJL_API yajl_gen_status yajl_gen_array_open(yajl_gen hand);
    /** Finish generating a JSON array. */
    YAJL_API yajl_gen_status yajl_gen_array_close(yajl_gen hand);

    /** Access the null terminated generator buffer.  If incrementally
     *  outputing JSON, one should call yajl_gen_clear() to clear the
     *  buffer.  This allows stream generation. */
    YAJL_API yajl_gen_status yajl_gen_get_buf(yajl_gen hand,
                                              const unsigned char ** buf,
                                              size_t * len);

    /** Clear yajl's output buffer, but maintain all internal generation
     *  state.  This function will not "reset" the generator state, and is
     *  intended to enable incremental JSON outputing. */
    YAJL_API void yajl_gen_clear(yajl_gen hand);

#ifdef __cplusplus
}
#endif

#endif
