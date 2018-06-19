/*
 * Solaris version of
 * osdWireConfig.h
 *
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef osdWireConfig_h
#define osdWireConfig_h

#include <sys/isa_defs.h>

#if defined ( _LITTLE_ENDIAN )
#   define EPICS_BYTE_ORDER EPICS_ENDIAN_LITTLE
#elif defined ( _BIG_ENDIAN )
#   define EPICS_BYTE_ORDER EPICS_ENDIAN_BIG
#else
#   error EPICS hasnt been ported to byte order specified by <sys/isa_defs.h> on Solaris
#endif

/* for now, assume that Solaris doesnt run on weird arch like ARM NWFP */
#define EPICS_FLOAT_WORD_ORDER EPICS_BYTE_ORDER

#endif /* ifdef osdWireConfig_h */

