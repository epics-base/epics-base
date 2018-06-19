/*
 * vxWorks version of
 * osdWireConfig.h
 *
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef osdWireConfig_h
#define osdWireConfig_h

#include <vxWorks.h>
#include <types/vxArch.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN
#   define EPICS_BYTE_ORDER EPICS_ENDIAN_LITTLE
#elif _BYTE_ORDER == _BIG_ENDIAN
#   define EPICS_BYTE_ORDER EPICS_ENDIAN_BIG
#else
#   error EPICS hasnt been ported to _BYTE_ORDER specified by vxWorks <types/vxArch.h>
#endif

/* for now, assume that vxWorks doesnt run on weird arch like ARM NWFP */
#define EPICS_FLOAT_WORD_ORDER EPICS_BYTE_ORDER

#endif /* ifdef osdWireConfig_h */
