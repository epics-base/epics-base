
/*
 * $Log$
 * Revision 1.1  1996/01/29 17:05:58  jbk
 * Added the simple program to allow loading of C++ object modules.
 *
 * Revision 1.1.1.1  1994/12/12  16:07:06  hideos
 * Version 2 of hideos.
 *
 */

/*
	Author: Jim Kowalkowski
	Date: 6/94
*/

#include <vxWorks.h>
#include <vme.h>

#include <stdio.h>
#include <symLib.h>
#include <ioLib.h>
#include <sysSymTbl.h>
#include <sysLib.h>
#include <moduleLib.h>
#include <usrLib.h>
#include <a_out.h>
#include <taskLib.h>

#ifdef __cplusplus
extern "C" {
#endif

void cpp_main(void);
MODULE_ID ldpp (int syms, BOOL noAbort, char *name);
void* __builtin_new(size_t);
void* __builtin_vec_new(size_t);
void __builtin_delete (void *);
void __builtin_vec_delete(void *);
void __pure_virtual(void);

#ifdef __cplusplus
}
#endif

typedef void (*func_ptr) (void);

/* ------------------------------------------------------------------ */
/*
	C++ loader for vxWorks, it runs constructors and such
*/
/* ------------------------------------------------------------------ */

MODULE_ID ldpp (int syms, BOOL noAbort, char *name)
{
  MODULE_ID ret;

  ret = ld(syms,noAbort,name);
  if(ret) cpp_main();
  return ret;
}

void cpp_main(void)
{
	SYM_TYPE stype;
	func_ptr *ctorlist;

	if( symFindByName(sysSymTbl,"___CTOR_LIST__",
			(char**)&ctorlist, &stype)==OK)
	{
		/* 
		 * this code was copied from gbl-ctors.h
		 * DO_GLOBAL_CTORS_BODY
		 */
		unsigned long nptrs = (unsigned long) ctorlist[0];
		unsigned i;
		if (nptrs == (unsigned long) -1) {
			for (nptrs = 0; ctorlist[nptrs + 1] != 0; nptrs++);
		}
		for (i = nptrs; i >= 1; i--) {
			ctorlist[i] ();
		}

		/*
		 * remove the symbol so that this code isnt run again
		 */
		if(symRemove(sysSymTbl,"___CTOR_LIST__",stype)!=OK)
		{
			printf("ctor list just diappeared! - that sucks.\n");
		}
	}

	return;
}

void* __builtin_new(size_t sz)
{
	void* p;

	if(sz==0u) sz=1u;

	p=(void*)malloc(sz);

	return p;
}

void __builtin_delete (void *ptr)
{
	if(ptr) free(ptr);
}

/*
 * __pure_virtual()
 * joh - 9-5-96
 */
void __pure_virtual(void)
{
        printf("A pure virtual function was called\n");
        taskSuspend(taskIdSelf());
}

/*
 * __builtin_vec_delete()
 * joh - 9-5-96
 */
void __builtin_vec_delete(void *ptr)
{
	__builtin_delete(ptr);
}

/*
 * __builtin_vec_new()
 * joh - 9-5-96
 */
void* __builtin_vec_new(size_t sz)
{
	return __builtin_new (sz);
}

