/*
 *	$Id$
 *
 *	list fuctions attached to the interrupt vector table
 *
 *	Created 28Mar89	Jeffrey O. Hill
 *	hill@atdiv.lanl.gov
 *	(505) 665 1831
 *
 */

/*
 * 
 *	makefile
 *
 *
 * V5VW = /.../vx/v502b
 * V4VW = /.../vx/v402
 *
 * veclist.o:
 *	cc68k -c -DV5_vxWorks -DCPU_FAMILY=MC680X0 -I$(V5VW)/h veclist.c
 * 
 * veclistV4.o: veclist.c
 *	cc68k -c -DV4_vxWorks -I$(V4VW)/h veclist.c -o veclistV4.o
 *
 */

#include "vxWorks.h"
#include "a_out.h"
#if defined(V4_vxWorks)
#	include "iv68k.h"
#elif defined(V5_vxWorks)
#	include "iv.h"
#else
#	include "iv68k.h"
#endif
#include "ctype.h"
#include "sysSymTbl.h"

static char *sccsID = 
	"$Id$\t$Date$ written by Jeffrey O. Hill hill@atdiv.lanl.gov";

/*
 *
 * VME bus dependent
 *
 */
#define NVEC 0x100

static char *ignore_list[] = {"_excStub","_excIntStub"};

int	veclist(int);
int	cISRTest(void (*)(), void (**)(), void **);
void 	*fetch_pointer(unsigned char *);


/*
 *
 * veclist()
 *
 */
veclist(int all)
{
  	int		vec;
	int		value;
#if defined(V4_vxWorks)
	UTINY		type;
#else
	SYM_TYPE	type;
#endif
	char		name[MAX_SYS_SYM_LEN];
	char		function_type[10];
	void		(*proutine)();
	void		(*pCISR)();
	void		*pparam;
	int		status;
	int		i;

  	for(vec=0; vec<NVEC; vec++){
    		proutine = (void *) intVecGet(INUM_TO_IVEC(vec));

		status = cISRTest(proutine, &pCISR, &pparam);
		if(status == OK){
			proutine = pCISR;
			strcpy(function_type, "C");
		}
		else{
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
			sprintf(name, "0x%X", proutine);
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
       		printf(	"vec 0x%2X %5s ISR %s", 
			vec, 
			function_type,
			name);
		if(pCISR){
			printf("(0x%X)", pparam);
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
LOCAL
int	cISRTest(void (*proutine)(), void (**ppisr)(), void **pparam)
{
	static FUNCPTR	handler = NULL;
	STATUS		status;
	unsigned char	*pchk;
	unsigned char	*pref;
	unsigned char	val;

	if(handler == NULL){
		handler = (FUNCPTR) intHandlerCreate(
				(FUNCPTR) ISR_PATTERN, 
				PARAM_PATTERN);
		if(handler == NULL){
			return ERROR;
		}
	}

	*ppisr = NULL;
	*pparam = NULL;
	pchk = (unsigned char *) proutine;
	pref = (unsigned char *) handler; 
	for(	;
		*ppisr==NULL || *pparam==NULL;
		pchk++, pref++){

		status = vxMemProbe(	
				pchk,
				READ,
				sizeof(val),
				&val);
		if(status < 0){
			return ERROR;
		}

		if(val != *pref){
			if(*pref == (unsigned char) ISR_PATTERN){
				*ppisr = fetch_pointer(pchk);
				if(!*ppisr){
					return ERROR;
				}
				pref += sizeof(*ppisr)-1;
				pchk += sizeof(*ppisr)-1;
			}
			else if(*pref == (unsigned char) PARAM_PATTERN){
				*pparam = fetch_pointer(pchk);
				if(!*pparam){
					return ERROR;
				}
				pref += sizeof(*pparam)-1;
				pchk += sizeof(*pparam)-1;
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

LOCAL
void *fetch_pointer(unsigned char *plow_byte)
{
	union pointer	p;
	int		i;

	for(i=0; i < sizeof(p); i++){
		p.char_overlay.byte[i] = plow_byte[i];
	}

	return p.ptr_overlay;
}
