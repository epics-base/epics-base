/*************************************************************************\
* Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Authors: Jun-ichi Odagiri, Marty Kraimer, Eric Norum,
 *          Mark Rivers, Andrew Johnson, Ralph Lange
 */

/**
 * \file epicsString.h
 *
 * \brief Collection of string utility functions
 */

#ifndef INC_epicsString_H
#define INC_epicsString_H

#include <stdio.h>
#include "epicsTypes.h"
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Converts C-style escape sequences to their binary form
 * 
 * Copies characters from string to an output buffer and converts C-style escape sequences to 
 * their binary form.  Since the output string can never be longer than the source, it is legal for 
 * \p inbuf and \p outbuf to point to the same buffer and \p outsize and \p inlen
 * be equal, thus performing the character translation in-place.
 * 
 * \param outbuf  buffer to copy string to. The resulting string will be zero-terminated as long as 
 *                \p outsize is non-zero. 
 * \param outsize length of output buffer not including the null-terminator. 
 * \param inbuf   buffer to copy from. Null byte terminates the string.
 * \param inlen   maximum number of characters to copy from input buffer.
 *
 * \return  number of characters that were written into output buffer, not counting the null terminator. 
**/
LIBCOM_API int epicsStrnRawFromEscaped(char *outbuf,      size_t outsize,
                                           const char *inbuf, size_t inlen);

/** \brief Converts non-printable characters into C-style escape sequences
 *
 * Copies characters from the string into a output buffer converting non-printable characters into C-style 
 * escape sequences.   In-place translations are not allowed since the escaped results will usually be larger
 * than the input string.
 *
 * The following escaped character constants will be used in the output:
 * \verbatim  \a \b \f \n \r \t \v \\ \’ \" \0  \endverbatim
 * All other non-printable characters appear in form `\xHH` where HH are two hex digits.
 * Non-printable characters are determined by the C runtime library’s isprint() function.
 *
 * \param outbuf  buffer to copy string to. The resulting string will be zero-terminated as long as 
 *                @p outsize is non-zero. 
 * \param outsize length of output buffer not including the null-terminator.
 * \param inbuf   buffer to copy from. Null byte will not terminates the string.
 * \param inlen   Number of characters to copy from input buffer.
 *
 * \return number of characters that would have been stored in the output buffer if it were large enough, 
 * or a negative value if \p outbuf == \p inbuf. 
 *
 */
LIBCOM_API int epicsStrnEscapedFromRaw(char *outbuf,      size_t outsize,
                                           const char *inbuf, size_t inlen);

/** \brief Scans string and returns size of output buffer needed to escape that string
 *
 * Scans up to \p len characters of the string that may contain non-printable characters, and returns 
 * the size of the output buffer that would be needed to escape that string. 
 * This routine is faster than calling epicsStrnEscapedFromRaw() with a zero length output buffer; both 
 * should return the same result.
 *
 *\param buf string to scan
 *\param len length of input string
 *
 *\return size of the output buffer that would be needed for converting to escape characters, not 
 *        including the null terminator. 
 */
LIBCOM_API size_t epicsStrnEscapedFromRawSize(const char *buf, size_t len);

/** \brief Does case-insensitive comparison of two strings
 *
 * Implements strcmp from the C standard library, except is case insensitive
 */
LIBCOM_API int epicsStrCaseCmp(const char *s1, const char *s2);

/** \brief Does case-insensitive comparision of two strings
 *
 * Implements strncmp from the C standard library, except is case insensitive
 */
LIBCOM_API int epicsStrnCaseCmp(const char *s1, const char *s2, size_t len);

/** \brief Duplicates a string
 *
 * Implements strdup from the C standard library.  Calls mallocMustSucceed() to allocate memory
 */
LIBCOM_API char * epicsStrDup(const char *s);

/** \brief Duplicates a string
 *
 * implements strndup from the C standard library. Calls mallocMustSucceed() to allocate memory
 */
LIBCOM_API char * epicsStrnDup(const char *s, size_t len);

/** \brief Prints escaped version of string
 *
 * Prints the contents of its input buffer to given file descriptor,  substituting escape sequences 
 * for non-printable characters.
 *
 * \param fp  File descriptor to print to
 * \param s   String to print
 * \param n   Length of string
 */
LIBCOM_API int epicsStrPrintEscaped(FILE *fp, const char *s, size_t n);
    
#define epicsStrSnPrintEscaped epicsStrnEscapedFromRaw

/** \brief Calculates length of string
 *
 * Implements strnlen from the C standard library.    
 */
LIBCOM_API size_t epicsStrnLen(const char *s, size_t maxlen);

/** \brief Matches a string against a pattern.
 *
 * Checks if str matches the glob style pattern, which may contain ? or * wildcards.
 * A ? matches any single character.
 * A * matched any sub-string.
 *
 * @returns 1 if str matches the pattern, 0 if not.
 *
 * @since EPICS 3.14.7
 */
LIBCOM_API int epicsStrGlobMatch(const char *str, const char *pattern);

/** \brief Matches a string against a pattern.
 *
 * Like epicsStrGlobMatch() but with limited string length.
 * If the length of str is less than len, the full string is matched.
 *
 * @returns 1 if the first len characters of str match the pattern, 0 if not.
 *
 * @since 7.0.6
 */
LIBCOM_API int epicsStrnGlobMatch(const char *str, size_t len, const char *pattern);

 /** \brief Extract tokens from string
  * 
  * Implements strtok_r from the C standard library
  */
LIBCOM_API char * epicsStrtok_r(char *s, const char *delim, char **lasts);

/** \brief Calculates a hash of a null-terminated string
 *
 * Calculates a hash of a null-terminated string.  Initial seed may be provided which 
 * permits multiple strings to be combined into a single hash result.
 *
 *\param str  null-terminated string
 *\param seed Optionally provide seed to combine multiple strings in a single hash.  Otherwise
 *            set to 0.
 *
 *\return Hash value for string
 */    
LIBCOM_API unsigned int epicsStrHash(const char *str, unsigned int seed);

 /** \brief Calculates a hash of a memory buffer
  *
  * Calculates a hash of a memory buffer that may contain null values.  Initial seed may be provided which 
  * permits multiple buffers to be combined into a single hash result.
  *
  *\param str    buffer
  *\param length size of buffer
  *\param seed Optionally provide seed to combine multiple buffers in a single hash.  Otherwise
  *            set to 0.
  *
  *\return Hash value for buffer
  */    
LIBCOM_API unsigned int epicsMemHash(const char *str, size_t length,
                                         unsigned int seed);
/** \brief Compare two strings and return a number in the range [0.0, 1.0] or -1.0 on error.
 *
 * Computes a normalized edit distance representing the similarity between two strings.
 *
 * @returns 1.0 when A and B are identical, down to 0.0 when A and B are unrelated,
 *          or < 0.0 on error.
 *
 * @since EPICS 7.0.5
 */
LIBCOM_API double epicsStrSimilarity(const char *A, const char *B);

/** \brief DEPRECATED
 * \deprecated dbTranslateEscape is deprecated, use epicsStrnRawFromEscaped() instead 
 */
LIBCOM_API int dbTranslateEscape(char *s, const char *ct);

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsString_H */
