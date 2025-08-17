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
#  if defined(RTEMS_NETWORKING)
     /* legacy stack circa RTEMS <= 5 and networking internal to RTEMS */
     RTEMS_LEGACY_NETWORKING_INTERNAL = yes
#  else
#    if !defined(__has_include)
       /* assume old GCC implies RTEMS < 5 with mis-configured BSP */
#      error rebuild BSP with --enable-network
#    elif __has_include(<machine/rtems-net-legacy.h>)
       /* legacy stack circa RTEMS > 5 */
       RTEMS_LEGACY_NETWORKING = yes
#    elif __has_include(<machine/rtems-bsd-version.h>)
       /* libbsd stack */
       RTEMS_BSD_NETWORKING = yes
#    else
#      error Cannot determine RTEMS network configuration
#    endif
#  endif
#endif

#ifdef __has_include
#  if defined(__rtems__) && __RTEMS_MAJOR__<5 && __has_include(<libtecla.h>)
COMMANDLINE_LIBRARY ?= LIBTECLA
#  elif __has_include(<readline/readline.h>)
COMMANDLINE_LIBRARY ?= READLINE
#  else
COMMANDLINE_LIBRARY ?= EPICS
#  endif
#else
COMMANDLINE_LIBRARY ?= $(strip $(if $(wildcard $(if $(GNU_DIR),$(GNU_DIR)/include/readline/readline.h)), READLINE, EPICS))
#endif
