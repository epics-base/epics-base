
/*
 * Use compiler specific key words to set up shareable library
 * external symbols and entry points
 */

#ifdef WIN32
#	ifdef _WINDLL
#		define epicsShareExtern __declspec(dllexport) 
#	else
#		define epicsShareExtern __declspec(dllimport) extern
#	endif
#	define epicsShareAPI __stdcall
#else
#	define epicsShareAPI
#	define epicsShareExtern extern
#endif

