/* base/include $Id$ */

/*
 * pal.h
 *
 * PAL emulator header file
 * AT-8 hardware design
 *
 * .01 09-11-96 joh fixed warnings and protoized
 */

/*
 * general constants
 */
#define INTBITS		32	/* # of bits in an integer */
#define NORMAL		0	/* normal polarity */
#define INVERTED	1	/* inverted polarity */
#define REGISTERED	1	/* registered output */
#define COMBINATORIAL	0	/* combinatorial output */
#define RETRY		3	/* file open retry count */

/*
 * PAL data structures
 */
struct rowsel
	{
	int prow;			/* primary fuse row */
	int arow;			/* alternate fuse row */
	int sel;			/* macrocell select bit */
	int psel;			/* sel value for prow */
	};

struct blk
	{
	unsigned int *array;		/* fuse array */
	int blksiz;			/* pal block size (in product terms) */
	int loc;			/* starting fuse address */
	int pol;			/* output polarity */
	int typ;			/* combinatorial or registered */
	int val;			/* current value */
	int row;			/* feedback row */
	int nrofst;			/* row offset for negative feedback */
	struct rowsel *rsel;		/* feedback row select info */
	};

struct thresh
	{
	double hithresh;		/* logic "1" threshold */
	double lothresh;		/* logic "0" threshold */
	int val;			/* current value */
	int row;			/* fuse row */
	int nrofst;			/* row offset for regative feedback */
	struct rowsel *rsel;		/* input row select info */
	};

struct palinit
	{
	int mc[8];		/* macrocell config data */
	char *path;		/* path to files */
	char *name;		/* PAL device type */
	char *def;		/* JEDEC definition file */
	int blk0;		/* starting fuse address of first pal block */
	int polloc;		/* starting fuse address of polarity info */
	int typloc;		/* starting fuse address of output type info */
	int s0;			/* starting fuse address of macrocell config bits */
	int snum;		/* number of config bits */
	};

struct pal
	{
	struct pal *flink;	/* link to next PAL */
	char *recnam;		/* record name */
	struct blk *bptr;	/* PAL block data */
	struct thresh *ithresh;	/* input limit data */
	int *scan;		/* scan order data */
	struct palinit *iptr;	/* initialization data */
	int blkelm;		/* longwords per product term */
	int blkbits;		/* bits per product term */
	int blknum;		/* number of pal blocks in device */
	int inp;		/* number of inputs */
	int blktyp;		/* macrocell type */
	};

int getkey(FILE *file, char **list, int num);
int paldef(struct pal *defpal);
int readblk(FILE *datfile, struct blk *curblk, int blkelm);
int readinp(FILE *datfile, struct thresh *curinp);
int getbit(FILE *datfile);
struct pal * palinit(char *file, char *recname, double *thresh);
int palconfig(FILE *datfile, struct pal *curpal, int fusenum);
int pal(struct pal *pptr, double *in, int inum, unsigned int *result);
int inbitload(struct thresh *tptr, unsigned int *inarray, unsigned int val);
int outbitload(struct blk *lptr, unsigned int *inarray, unsigned int val);
int eval(struct blk *lptr, unsigned int *inarray, int blkelm);


