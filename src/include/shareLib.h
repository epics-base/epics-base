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
 *	epicsShareFunc int epicsShareAPI 
 *			a_func (int arg); 	function prototype
 *	epicsShareExtern  int a_var; 		reference variable 
 *	epicsShareDef int a_var= 4; 		create variable instance 
 *	class epicsShareClass a_class; 		reference a class 
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
#undef epicsShareDef
#undef epicsShareClass
#undef epicsShareFunc
#undef epicsShareAPI
#undef READONLY

#if defined(WIN32)

#	if defined(epicsExportSharedSymbols)
#		define epicsShareExtern __declspec(dllexport) extern
#		define epicsShareClass  __declspec(dllexport) 
#		define epicsShareFunc  __declspec(dllexport)
#	else
#		define epicsShareExtern __declspec(dllimport) extern
#		define epicsShareClass  __declspec(dllimport) 
#		define epicsShareFunc  __declspec(dllimport)
#	endif
	/*
	 * Subroutine removes arguments 
	 * (Bill does not allow __stdcall to be next to
	 * __declspec(xxxx))
	 */
#	define epicsShareAPI __stdcall
#	define epicsShareDef __declspec(dllexport)
#       define READONLY const

#elif defined(VAXC)

	/* 
	 * VAXC creates FORTRAN common blocks when
	 * we use "extern int fred"/"int fred=4". Therefore,
	 * the initialization is not loaded unless we
	 * call a function in that object module.
	 *
	 * DEC CXX does not have this problem.
	 * We suspect (but do not know) that DEC C 
	 * 	also does not have this problem.
	 */
#	define epicsShareExtern globalref 
#	define epicsShareDef globaldef 
#       define READONLY const
#	define epicsShareClass
#	define epicsShareFunc
#	define epicsShareAPI

#else

/* else => no import/export specifiers */

#	define epicsShareExtern extern
#	define epicsShareAPI
#	define epicsShareClass
#	define epicsShareDef 
#	define epicsShareFunc
#	if defined(__STDC__)
#		define READONLY const
#	else
#		define READONLY
#	endif

#endif

