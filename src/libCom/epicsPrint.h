
#ifdef __cplusplus
extern "C" {
#define epicsPrintUseProtoANSI
#endif

#ifdef __STDC__
#ifndef epicsPrintUseProtoANSI
#define epicsPrintUseProtoANSI
#endif
#endif

#ifdef vxWorks
#	ifdef epicsPrintUseProtoANSI
#		include <stdarg.h>
		int epicsPrintf(const char *pFormat, ...);
		int epicsVprintf (const char *pFormat, va_list pvar);
		int iocLogVPrintf(const char *pFormat, va_list pvar);
		int iocLogPrintf(const char *pFormat, ...);
#	else /* not epicsPrintUseProtoANSI */
		int epicsPrintf();
		int epicsVprintf ();
		int iocLogVPrintf();
		int iocLogPrintf();
#	endif /* ifdef epicsPrintUseProtoANSI */
#else /* not vxWorks */
#	define epicsPrintf printf
#	define epicsVprintf vprintf
#endif /* ifdef vxWorks */

#ifdef __cplusplus
}
#endif

