/*
 * paldef - read PAL type definition file
 *
 *	The PAL definition file contains the device specific
 *	information required to emulate a particular PAL chip
 *	type. It is a simple tab delimited ASCII file. This
 *	routine parses the file and loads the information in
 *	the PAL definition structure.
 *
 * 	.01 09-11-96 joh fixed warningsi and protoized
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pal.h"

/*
 * PAL definition keywords recognised by paldef parser
 */
#define	BLK0		0	/* starting fuse address of first pal block */
#define BLKSIZ		1	/* pal block size */
#define BLKELM		2	/* longwords per product term */
#define BLKBITS		3	/* bits per product term */
#define BLKNUM		4	/* number of pal blocks */
#define POLLOC		5	/* starting fuse address of polarity info */
#define TYPLOC		6	/* starting fuse address of output type info */
#define INPNUM		7	/* number of inputs */
#define INPUT		8	/* start of input definition info */
#define S0		9	/* starting fuse address of macrocell definition */
#define NAME		10	/* PAL device type */
#define BLOCK		11	/* start of PAL logic block info */
#define ROW		12	/* fuse row (bit) location */
#define AROW		13	/* alternate fuse row (bit) location */
#define	POL		14	/* polarity information */
#define SNUM		15	/* number of macrocell definition bits */
#define ENDBLK		16
#define ENDINP		17
#define	SFUSE		18	/* macrocell select bit */
#define SVAL		19	/* select value for primary row */
#define NROW		20	/* negative feedback offset */
#define LOC		21	/* starting address of block fuse array */
#define BLKTYP		22	/* macrocell type */
static char *keylist[] = {"blk0","blksiz","blkelm","blkbits","blknum","polloc",
		  	   	  "typloc","inpnum","input","s0","name","block","row",
		    	  	  "arow","pol","snum","endblk","endinp","sfuse","sval",
				  "nrow","loc","blktyp"};
static int keynum = sizeof(keylist)/sizeof(keylist[0]);
char temp[80];


/*
 * getkey - look for keyword
 *
 */
int getkey(FILE *file, char **list, int num)
{
	int key;

	if (fscanf(file,"%s",temp) == EOF)
		return -1;
	for (key = 0; key < num; key++)
		if (!strcmp(temp,list[key]))
			break;
	return key;
}	

int paldef(struct pal *defpal)
{
	FILE *datfile;
	char filenam[80];
	char inchar;
	int key;
	struct blk *curblk = (struct blk *)0;
	struct thresh *curinp = (struct thresh *)0;
	int blknum, inpnum;
	int i;

	strcpy(filenam,defpal->iptr->path);
	strcat(filenam,defpal->iptr->name);
	strcat(filenam,".def");
	printf("paldef - opening %s\n",filenam);
	for (i = 0; i < RETRY; i++)
		if ((datfile = fopen(filenam,"r")) != NULL)
			break;
	if (i == RETRY)
		{
		printf("paldef:cannot open PAL definition file %s - aborting\n",filenam);
		return -1;
		}
	defpal->iptr->polloc = defpal->iptr->typloc = defpal->iptr->s0 = -1;
	for(key = getkey(datfile,keylist,keynum); key != -1; key = getkey(datfile,keylist,keynum))
		switch (key)
			{
			case BLK0:
				fscanf(datfile,"%d",&defpal->iptr->blk0);
				break;
			case BLKELM:
				fscanf(datfile,"%d",&defpal->blkelm);
				break;
			case BLKBITS:
				fscanf(datfile,"%d",&defpal->blkbits);
				break;
			case BLKNUM:
				if (defpal->blknum)
					{
					printf("paldef:error in %s - redefining number of PAL blocks - aborting\n",filenam);
					return -1;
					}
				fscanf(datfile,"%d",&defpal->blknum);
				defpal->bptr = (struct blk *)malloc((sizeof (struct blk)) * defpal->blknum);
				defpal->scan = (int *)malloc((sizeof (int)) * defpal->blknum);
				break;
			case BLKTYP:
				fscanf(datfile,"%d",&defpal->blktyp);
				break;
			case POLLOC:
				fscanf(datfile,"%d",&defpal->iptr->polloc);
				break;
			case TYPLOC:
				fscanf(datfile,"%d",&defpal->iptr->typloc);
				break;
			case INPNUM:
				fscanf(datfile,"%d",&defpal->inp);
				defpal->ithresh = (struct thresh *)malloc((sizeof (struct thresh)) * defpal->inp);
				break;
			case BLOCK:
				fscanf(datfile,"%d",&blknum);
				if (!defpal->blkelm)
					{
					printf("paldef:error - no current block element size - aborting\n");
					return -1;
					}
				if (blknum < defpal->blknum)
					{
					curblk = defpal->bptr + blknum;
					curblk->array = (unsigned int *)0;
					curblk->pol = curblk->typ = curblk->val = curblk->blksiz = 0;
					curblk->rsel = (struct rowsel *)malloc(sizeof (struct rowsel));
					curblk->rsel->prow = curblk->rsel->arow = curblk->rsel->sel = curblk->rsel->psel = -1;
					}
				readblk(datfile,curblk,defpal->blkelm);
				break;
			case INPUT:
				fscanf(datfile,"%d",&inpnum);
				if (inpnum < defpal->inp)
					{
					curinp = defpal->ithresh + inpnum;
					curinp->rsel = (struct rowsel *)malloc(sizeof (struct rowsel));
					curinp->rsel->prow = curinp->rsel->arow = curinp->rsel->sel = curinp->rsel->psel = -1;
					}
				readinp(datfile,curinp);
				break;
			case S0:
				fscanf(datfile,"%d",&defpal->iptr->s0);
				break;
			case SNUM:
				fscanf(datfile,"%d",&defpal->iptr->snum);
				break;
			default:
				if (temp[0] == ';')
					for (inchar = fgetc(datfile); (inchar != '\n') && (inchar != EOF); inchar = fgetc(datfile));
				break;
			}
	return 0;
}
/*
 * readblk - read PAL logic block
 *
 */
int readblk(FILE *datfile, struct blk *curblk, int blkelm)
{
	int key;
	char inchar;

	for(key = getkey(datfile,keylist,keynum); key != ENDBLK; key = getkey(datfile,keylist,keynum))
		switch (key)
			{
			case BLKSIZ:
				fscanf(datfile,"%d",&curblk->blksiz);
					curblk->array = (unsigned int *)malloc((sizeof (unsigned int)) * curblk->blksiz * blkelm);
				break;
			case ROW:
				fscanf(datfile,"%d",&curblk->rsel->prow);
				break;
			case AROW:
				fscanf(datfile,"%d",&curblk->rsel->arow);
				break;
			case SFUSE:
				fscanf(datfile,"%d",&curblk->rsel->sel);
				break;
			case SVAL:
				fscanf(datfile,"%d",&curblk->rsel->psel);
				break;
			case NROW:
				fscanf(datfile,"%d",&curblk->nrofst);
				break;
			case LOC:
				fscanf(datfile,"%d",&curblk->loc);
				break;
			default:
				if (temp[0] == ';')
					for (inchar = fgetc(datfile); (inchar != '\n') && (inchar != EOF); inchar = fgetc(datfile));
				break;
			}
	return 0;
}
/*
 * readinp - read PAL input info
 *
 */
int readinp(FILE *datfile, struct thresh *curinp)
{
	int key;
	char inchar;

	for(key = getkey(datfile,keylist,keynum); key != ENDINP; key = getkey(datfile,keylist,keynum))
		switch (key)
			{
			case ROW:
				fscanf(datfile,"%d",&curinp->rsel->prow);
				break;
			case AROW:
				fscanf(datfile,"%d",&curinp->rsel->arow);
				break;
			case SFUSE:
				fscanf(datfile,"%d",&curinp->rsel->sel);
				break;
			case SVAL:
				fscanf(datfile,"%d",&curinp->rsel->psel);
				break;
			case NROW:
				fscanf(datfile,"%d",&curinp->nrofst);
			default:
				if (temp[0] == ';')
					for (inchar = fgetc(datfile); (inchar != '\n') && (inchar != EOF); inchar = fgetc(datfile));
				break;
			}
	return 0;
}
