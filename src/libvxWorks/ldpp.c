
/*
 * $Log$
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

#ifdef __cplusplus
extern "C" {
#endif

void cpp_main(void);
MODULE_ID ldpp (int syms, BOOL noAbort, char *name);
void* __builtin_new(int sz);
void __builtin_delete (void *ptr);

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
	func_ptr p;
	func_ptr *ctorlist;
	int i;

	if( symFindByName(sysSymTbl,"___CTOR_LIST__",
			(char**)&ctorlist,&stype)==ERROR)
	{
		return;
	}
	else
	{
		for(i=1;ctorlist[i]; i++)
		{
			p=ctorlist[i];
			(*p)();
		}
	}

	if(symRemove(sysSymTbl,"___CTOR_LIST__",stype)==ERROR)
	{
		printf("ctor list just diappeared! - that sucks.\n");
	}

	return;
}

void* __builtin_new(int sz)
{
	void* p;

	if(sz==0) sz=1;

	p=(void*)malloc(sz);

	return p;
}

void __builtin_delete (void *ptr)
{
	if(ptr) free(ptr);
}

