/*
 * Use compiler specific key words to set up shareable library
 * external symbols and entry points
 *
 * Right now this is only necessary for WIN32 DLL,
 * the approach should be general enough for future systems, however.
 *
 *
 * USAGE:
 *
 * In header files, declare variables, classes and functions
 * to be __exported__ like this:
 *
 *	epicsShareAPI     int a_func (int arg);
 *	epicsShareExtern  int a_var;
 *	class epicsShareClass a_class;
 *
 * Usually the epicsShare... macros expand to
 *  "import this from a DLL"  (on WIN32, on Unix it's a NOP)
 *
 * In the implementation file, however, you write:
 *
 *  #include <other_stuff.h>
 *  #define epicsExportSharedSymbols
 *  #include <what_I_implement.h>
 *  ! no more "foreign" includes from here on ! 
 *
 * The point is: define epicsExportSharedSymbols exacly and only
 * right before you include the prototypes for what you implement!
 * This time the epicsShare... macros expand to
 *  "export this from the DLL that we are building" (again only on WIN32)
 *
 *	8-22-96 -kuk-
 */

#undef epicsShareExtern
#undef epicsShareClass
#undef epicsShareAPI

#ifdef WIN32

#	if defined(_WINDLL)  ||  defined(epicsExportSharedSymbols)
#		define epicsShareExtern __declspec(dllexport) extern
#		define epicsShareClass  __declspec(dllexport) 
#	else
#		define epicsShareExtern __declspec(dllimport) extern
#		define epicsShareClass  __declspec(dllimport) 
#	endif
	/*
	 * Subroutine removes arguments 
	 */
#	define epicsShareAPI __stdcall

#else

/* no WIN32 -> no import/export specifiers */

#	define epicsShareExtern extern
#	define epicsShareAPI
#	define epicsShareClass
#endif

