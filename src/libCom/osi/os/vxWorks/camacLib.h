/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* camacLib.h -- Prototypes for camacLib.o
 *
 * Marty Wise
 * 10/11/93
 *
 */

/********************************/
/* GLOBAL DATA			*/
/********************************/
extern	int	debug_hook;

extern	struct	glob_dat {
	int	total;
	int	read_error[5];
	int	write_error[5];
	int	cmd_error[5];
	int	total_err;
	int	lam_count[12];
} debug_dat;


/********************************/
/* FUNCTION PROTOTYPES		*/
/********************************/

void	cdreg(int *ext, int b, int c, int n, int a);
void	cfsa(int f, int ext, int *dat, int *q);
void	cssa(int f, int ext, short *dat, int *q);
void	ccci(int ext, int l);
void	cccz(int ext);
void	cccc(int ext);
void	ccinit(int b);
void	ctci(int ext, int *l);
void	cgreg(int ext, int *b, int *c, int *n, int *a);
void	cfmad(int f, int extb[2], int *intc, int cb[4]);
void	cfubc(int f, int ext, int *intc, int cb[4]);
void	cfubc(int f, int ext, int *intc, int cb[4]);
void	csmad(int f, int extb[2], short *intc, int cb[4]);
void	ctcd(int ext, int *l);
void	cccd(int ext, int l);
void	csga(int fa[], int exta[], unsigned short intc[], int qa[], int cb[4]);
void	cfga(int fa[], int exta[], int intc[], int qa[], int cb[4]);
void	cfubr(int f, int ext, int intc[], int cb[4]);
void	csubc(int f, int ext, unsigned short *intc, int cb[4]);
void	csubr(int f, int ext, int intc[], int cb[4]);
void	print_reg(int ext);

