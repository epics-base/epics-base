/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *	list fuctions attached to the interrupt vector table
 *
 *	Created 28Mar89	Jeffrey O. Hill
 *	johill@lanl.gov
 *	(505) 665 1831
 *
 */

#include "vxWorks.h"
#include "stdio.h"
#include "string.h"
#include "intLib.h"
#include "vxLib.h"
#include "iv.h"
#include "ctype.h"
#include "sysSymTbl.h"

/*
 *
 * VME bus dependent
 *
 */
#define NVEC 0x100

static char *ignore_list[] = {"_excStub","_excIntStub"};

int		veclist(int);
int		cISRTest(FUNCPTR proutine, FUNCPTR *ppisr, void **pparam);
static void 	*fetch_pointer(unsigned char *);


/*
 *
 * veclist()
 *
 */
int veclist(int all)
{
  	int		vec;
	int		value;
	SYM_TYPE	type;
	char		name[MAX_SYS_SYM_LEN];
	char		function_type[10];
	FUNCPTR		proutine;
	FUNCPTR		pCISR;
	int		cRoutine;
	void		*pparam;
	int		status;
	unsigned	i;

  	for(vec=0; vec<NVEC; vec++){
    		proutine = intVecGet((FUNCPTR *)INUM_TO_IVEC(vec));

		status = cISRTest(proutine, &pCISR, &pparam);
		if(status == OK){
			cRoutine = TRUE;
			proutine = pCISR;
			strcpy(function_type, "C");
		}
		else{
			cRoutine = FALSE;
			strcpy(function_type, "MACRO");
			pCISR = NULL;
		}
 
		status = symFindByValue(
				sysSymTbl, 
				(int)proutine, 
				name,
				&value,
				&type);
		if(status<0 || value != (int)proutine){
			sprintf(name, "0x%X", (unsigned int) proutine);
		}
		else if(!all){
			int	match = FALSE;

			for(i=0; i<NELEMENTS(ignore_list); i++){
				if(!strcmp(ignore_list[i],name)){
					match = TRUE;
					break;
				}
			}
			if(match){
				continue;
			}
		}
       		printf(	"vec 0x%02X %5s ISR %s", 
			vec, 
			function_type,
			name);
		if(cRoutine){
			printf("(0x%X)", (unsigned int) pparam);
		}
		printf("\n");
	}

	return OK;
}



/*
 * cISRTest()
 *
 * test to see if a C routine is attached
 * to this interrupt vector
 */
#define ISR_PATTERN	0xaaaaaaaa
#define PARAM_PATTERN	0x55555555 
int	cISRTest(FUNCPTR proutine, FUNCPTR *ppisr, void **pparam)
{
	static FUNCPTR	handler = NULL;
	STATUS		status;
	unsigned char	*pchk;
	unsigned char	*pref;
	unsigned char	val;
	int		found_isr;
	int		found_param;

	if(handler == NULL){
#if CPU_FAMILY != PPC
		handler = (FUNCPTR) intHandlerCreate(
				(FUNCPTR) ISR_PATTERN, 
				PARAM_PATTERN);
#endif
		if(handler == NULL){
			return ERROR;
		}
	}

	found_isr = FALSE;
	found_param = FALSE;
	pchk = (unsigned char *) proutine;
	pref = (unsigned char *) handler; 
	for(	;
		found_isr==FALSE || found_param==FALSE;
		pchk++, pref++){

		status = vxMemProbe(	
				(char *) pchk,
				READ,
				sizeof(val),
				(char *) &val);
		if(status < 0){
			return ERROR;
		}

		if(val != *pref){
			if(*pref == (unsigned char) ISR_PATTERN){
				*ppisr = (FUNCPTR) fetch_pointer(pchk);
				pref += sizeof(*ppisr)-1;
				pchk += sizeof(*ppisr)-1;
				found_isr = TRUE;	
			}
			else if(*pref == (unsigned char) PARAM_PATTERN){
				*pparam = fetch_pointer(pchk);
				pref += sizeof(*pparam)-1;
				pchk += sizeof(*pparam)-1;
				found_param = TRUE;
			}
			else{	
				return ERROR;
			}
		}
	}

	return OK;
}



/*
 * fetch_pointer()
 *
 * fetch pointer given low byte with correct byte ordering
 *
 */
struct char_array{
	unsigned char byte[4];
};
union pointer{
	void 			*ptr_overlay;
	struct char_array 	char_overlay;
};

static
void *fetch_pointer(unsigned char *plow_byte)
{
	union pointer	p;
	size_t		i;

	for(i=0; i < sizeof(p); i++){
		p.char_overlay.byte[i] = plow_byte[i];
	}

	return p.ptr_overlay;
}
