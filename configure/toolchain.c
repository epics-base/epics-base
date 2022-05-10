#ifdef _COMMENT_
/* Extract compiler pre-defined macros as Make variables
 *
 * Expanded as $(INSTALL_CFG)/TOOLCHAIN.$(EPICS_HOST_ARCH).$(T_A)
 *
 * Must be careful not to #include any C definitions
 * into what is really a Makefile snippet
 *
 * cf. https://sourceforge.net/p/predef/wiki/Home/
 */
/* GCC preprocessor drops C comments from output.
 * MSVC preprocessor emits C comments in output
 */
#endif

#if defined(__GNUC__) && !defined(__clang__)
GCC_MAJOR = __GNUC__
GCC_MINOR = __GNUC_MINOR__
GCC_PATCH = __GNUC_PATCHLEVEL__

#elif defined(__clang__)
CLANG_MAJOR = __clang_major__
CLANG_MINOR = __clang_minor__
CLANG_PATCH = __clang_patchlevel__

#ifdef _COMMENT_
/* CLANG version detection, similar scheme as used for BASE_3_15 etc.
 * Needed to disable warnings added in later Clang versions.
 * In a Makefile
 *   ifdef CLANG_12
 *     means the compiler is Clang 12.x or later,
 *   ifeq ($(CLANG_12),1)
 *     means the compiler is Clang 12.x.
 * Add other versions as needed.
 */
#endif

#if __clang_major__ >= 12
  #if __clang_major__ == 12
    CLANG_12 = 1
  #else
    CLANG_12 = 0
    #if __clang_major__ == 13
      CLANG_13 = 1
    #else
      CLANG_13 = 0
      #if __clang_major__ == 14
        CLANG_14 = 1
      #endif
    #endif
  #endif
#endif

#elif defined(_MSC_VER)
MSVC_VER = _MSC_VER
#endif

#ifdef __rtems__
#include <rtems/score/cpuopts.h>
#  if __RTEMS_MAJOR__>=5
OS_API = posix
#  else
OS_API = score
#  endif
#endif
