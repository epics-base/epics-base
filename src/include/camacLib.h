/* camacLib.h -- Prototypes for camacLib.o
 *
 * Marty Wise
 * 10/11/93
 *
 * $Log$
 * Revision 1.1  1994/10/20  20:16:16  tang
 * Commit adding of Camac Driver h file.
 *
 * Revision 1.4  94/05/11  13:52:57  13:52:57  wise (Marty Wise)
 * Added low-level diagnostics
 * 
 * Revision 1.3  94/03/27  10:52:29  10:52:29  wise (Marty Wise)
 * *** empty log message ***
 * 
 * Revision 1.1  93/11/05  06:39:35  06:39:35  wise (Marty Wise)
 * Initial revision
 * 
 * Revision 0.3  93/10/19  09:03:45  09:03:45  wise (Marty Wise)
 * Added cssa and csmad
 * 
 * Revision 0.2  93/10/12  07:33:01  07:33:01  wise (Marty Wise)
 * cfubc working, cfmad working, almost ready for alpha release.
 * 
 * Revision 0.1  93/10/11  10:31:48  10:31:48  wise (Marty Wise)
 * Initial revision
 * 
 *
 *
 */

static char ht2992_h_RCSID[] = "$Header$";

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

