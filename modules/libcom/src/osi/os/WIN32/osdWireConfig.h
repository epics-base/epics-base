/*
 * WIN32 version of
 * osdWireConfig.h
 *
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef osdWireConfig_h
#define osdWireConfig_h

/* for now, assume that win32 runs only on generic little endian */
#define EPICS_BYTE_ORDER EPICS_ENDIAN_LITTLE
#define EPICS_FLOAT_WORD_ORDER EPICS_BYTE_ORDER

#endif /* ifdef osdWireConfig_h */
