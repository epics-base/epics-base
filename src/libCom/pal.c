/*
 * pal.c
 *
 * pal emulator
 * AT-8 hardware design
 *
 * .01 09-11-96 joh fixed warnings and protoized
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pal.h"

/*
 * globals
 */
	struct pal *phead;	/* start of PAL list */

/*
 * getbit - get an ascii bit (0 or 1) from a JEDEC file
 */
int getbit(FILE *datfile)
{
	char inchar;
	char temp[80];

	for (inchar = fgetc(datfile); inchar != EOF; inchar = fgetc(datfile))
		if ((inchar == '1') || (inchar =='0'))
			break;
		else if ((inchar =='L') || (inchar =='l'))
			fscanf(datfile,"%s",temp);
	return inchar;
}
/*
 * palinit - initialize PAL data structures
 *
 *	This is the initialization routine which parses a JEDEC file,
 *	determines the file type, reads the device specific chip data,
 *	and initializes the logic arrays for execution. Two data files
 *	are required: the JEDEC file (in UNIX format), and the PAL
 *	definition file.
 *
 *	At present, a third data file is read - the limit definition
 *	file. This file determines the logic "0" and "1" level for
 *	each input. It is anticipated that this function will become
 *	an intrinsic part of the EPICS record in the future.
 *
 */
struct pal * palinit(char *file, char *recname, double *thresh)
{
	FILE *datfile;
	char temp[80], jedfile[80], inchar;
	int i, j, k, l, m, fusenum, itemp, members;
	unsigned int *elptr, tmask;
	struct thresh *tptr;
	struct blk *fptr;
	static struct pal *curpal;

	strcpy(jedfile,file);
	strcat(jedfile,".jed");
	printf("palinit - opening JEDEC file %s\n",jedfile);
	for (i = 0; i < RETRY; i++)
		if ((datfile = fopen(jedfile, "r")) != NULL)
			break;
	if (i == RETRY)
		{
		printf("palinit:cannot open JEDEC file %s - aborting\n",jedfile);
		return (struct pal *)0;
		}
/*
 * allocate memory for new PAL
 */
	if (!phead)
		phead = curpal = (struct pal *)malloc(sizeof (struct pal));
	else
		{
		curpal->flink = (struct pal *)malloc(sizeof (struct pal));
		curpal = curpal->flink;
		}
	curpal->bptr = (struct blk *)0;
	curpal->flink = (struct pal *)0;
	curpal->iptr = (struct palinit *)malloc(sizeof (struct palinit));
	curpal->iptr->def = (char *)malloc(strlen(file)+1);
	strcpy(curpal->iptr->def,file);
	curpal->recnam = (char *)malloc(strlen(recname)+1);
	strcpy(curpal->recnam,recname);
/*
 * look for PAL type - this info is marked with the character value 02
 */
	for (; (inchar = fgetc(datfile)) != EOF;)
		if (inchar == 2)
			break;
	if (inchar == 2)
		{
		fscanf(datfile,"%s",temp);
		curpal->iptr->name = (char *)malloc(strlen(temp)+1);
		strcpy(curpal->iptr->name,temp);
		strcpy(temp,file);		/* strip off path info from JEDEC input file */
		for (i = strlen(temp); i > 0; i--)
			if (temp[i] == '/')
			break;
		if (i)
			temp[i+1] = '\0';
		else
			temp[0] = '\0';
		curpal->iptr->path = (char *)malloc(strlen(temp)+1);
		strcpy(curpal->iptr->path,temp);
		}
	else if (inchar == EOF)
		{
		printf("palinit:cannot find PAL device type in %s - aborting\n",jedfile);
		return (struct pal *)0;
		}
/*
 * find PAL device specific data
 */
	if (paldef(curpal))
		return (struct pal *)0;
/*
 * load input threshold data
 */
	for (i = j = 0, tptr = curpal->ithresh; i < curpal->inp; i++, tptr++)
		{
		tptr->lothresh = thresh[j++];
		tptr->hithresh = thresh[j++];
		}
/*
 * look for start of fuse data
 */
	for (i = fscanf(datfile,"%s",temp); (i != EOF) && (strcmp(temp,"L0000")); i = fscanf(datfile,"%s",temp));
	if (i == EOF)
		{
		printf("palinit:cannot find fuse data in %s - aborting\n",jedfile);
		return (struct pal *)0;
		}
	fusenum = 0;
/*
 * initialize fuse data for each pal block
 */
	for (i = 0; i < curpal->blknum; i++)								/* for each PAL block */
		{
		if (fusenum != curpal->bptr[i].loc)
			{
			for (; fusenum != curpal->bptr[i].loc; fusenum++)
				if (getbit(datfile) == EOF)
					{
					printf("palinit:cannot find start of PAL block %d file %s - aborting\n",i,jedfile);
					return (struct pal *)0;
					}
			}
		for (j = 0, elptr = curpal->bptr[i].array; j < curpal->bptr[i].blksiz; j++)	/* for each block element */
			{
			members = curpal->blkbits/INTBITS;					/* # members per element */
			if (curpal->blkbits%INTBITS)						/* check for partially filled member */
				members++;
			for (k = 0; k < members; k++, elptr++)					/* for each element member */
				{
				if (k < curpal->blkbits/INTBITS)				/* last member may be partially filled */
					itemp = INTBITS;					/* if not do 32 bits worth */
				else
					itemp = curpal->blkbits%INTBITS;			/* if so how much */
				for (l = 0, *elptr = 0; l < itemp; l++)				/* do it */
					{
					if ((inchar = getbit(datfile)) == EOF)			/* get next data bit */
						{
						printf("palinit:unexpected EOF in JEDEC file (pal blocks)\n");
						return (struct pal *)0;
						}
					else if (inchar == '0')
						tmask = 1;					/* set bit in element */
					else
						tmask = 0;					/* clear bit in element */
					*elptr |= tmask<<l;					/* shift to proper position */
					fusenum++;						/* increment fuse # */
					}
				}	
			}
		}
/*
 * read in device specific info
 */
	if (palconfig(datfile,curpal,fusenum))
		return (struct pal *)0;
/*
 * find and disable unused product terms
 */
	for (i = 0; i < curpal->blknum; i++)
		for (j = 0, elptr = curpal->bptr[i].array; j < curpal->bptr[i].blksiz; j++, elptr++)
			{
			for (k = 0, l = 0; k < curpal->blkbits; k++)
				{
				if (k > ((INTBITS-1)+(INTBITS*l)))
						l ++;
				if (!(*(elptr+l) & (1<<(k-(l*INTBITS)))))
					break;
				}
			if (k == curpal->blkbits)
				for(m = 0; m < curpal->blkelm; m++)
					*(elptr + m) = 0;
			elptr += curpal->blkelm - 1;
			}
/*
 * determine scan order - initialize list with combinatorial logic first
 */
	for (i = 0, j = 0; i < curpal->blknum; i++)
		if (curpal->bptr[i].typ == COMBINATORIAL)
			curpal->scan[j++] = i;
	for (i = 0; i < curpal->blknum; i++)
		if (curpal->bptr[i].typ == REGISTERED)
			curpal->scan[j++] = i;
/*
 * delete uneeded structures
 */
	for (i = 0, fptr = curpal->bptr; i < curpal->blknum; i++, fptr++)
		{
		free(fptr->rsel);
		fptr->rsel = (struct rowsel *)0;
		}
	for (i = 0, tptr = curpal->ithresh; i < curpal->inp; i++, tptr++)
		{
		free(tptr->rsel);
		tptr->rsel = (struct rowsel *)0;
		}
	free(curpal->iptr);
	curpal->iptr = (struct palinit *)0;
/*
 * temporary diagnostic - print out PAL info
 *
	printf("record name %s\n",curpal->recnam);
	printf("%d blocks %d bits %d elements %d inputs\n",curpal->blknum,curpal->blkbits,
		curpal->blkelm,curpal->inp);
	for (i = 0; i < curpal->blknum; i++)
		{
		printf("block %d size %d polarity %d type %d\n",i,curpal->bptr[i].blksiz,
			curpal->bptr[i].pol,curpal->bptr[i].typ);
		printf("fuse %d row %d neg offset %d\n",curpal->bptr[i].loc,
			curpal->bptr[i].row,curpal->bptr[i].nrofst);
		for (j = 0, elptr = curpal->bptr[i].array; j < curpal->bptr[i].blksiz; j++, elptr++)
			{
			for (k = 0, l = 0; k < curpal->blkbits; k++)
				{
				if (k > (INTBITS-1)+l)
					{
					l += 32;
					elptr++;
					}
				if (*elptr & (1<<(k-l)))
					printf("1");
				else
					printf("0");
				}
			printf("\n");
			}
		}
 * end diagnostic */
/*
 * initialization done
 */
	fclose(datfile);
	return curpal;
}
/*
 * palconfig
 *
 * read in device specific configuration information
 *
 */
int palconfig(FILE *datfile, struct pal *curpal, int fusenum)
{
	int i, j, k;
	struct thresh *tptr;
	struct blk *fptr;
	char inchar;

	if (!strcmp(curpal->iptr->name,"PAL16V8"))
		{	
		/*
 		 * The PAL16V8 has a complex macrocell involving 2 configuration
		 * fuses for feedback, 8 seperate polarity, and 8 seperate type fuses.
		 * Look for polarity data first
 		 */
		for (; fusenum != curpal->iptr->polloc; fusenum++)
			if (getbit(datfile) == EOF)
				{
				printf("palconfig:cannot find polarity data\n");
				return -1;
				}
		/*
		 * read in polarity data for each pal block
		 */
		for (i = 0; i < curpal->blknum; i++)
			{
			if ((inchar = getbit(datfile)) == EOF)
				{
				printf("palconfig:unexpected EOF in JEDEC file %s (polarity)\n",curpal->iptr->def);
				return -1;
				}
			else if (inchar == '0')
				curpal->bptr[i].pol = INVERTED;
			else
				curpal->bptr[i].pol = NORMAL;
			fusenum++;
			}
		/*
		 * look for output type data
		 */
		for (; fusenum != curpal->iptr->typloc; fusenum++)
			if (getbit(datfile) == EOF)
				{
				printf("palconfig:cannot find type data\n");
				return -1;
				}
		/*
		 * read in type data for each pal block
		 */
		for (i = 0; i < curpal->blknum; i++)
			{
			if ((inchar = getbit(datfile)) == EOF)
				{
				printf("palconfig:unexpected EOF in JEDEC file %s (type)\n",curpal->iptr->def);
				return -1;
				}
			if (inchar == '1')
				curpal->bptr[i].typ = COMBINATORIAL;
			else
				curpal->bptr[i].typ = REGISTERED;
			fusenum++;
			}
		/*
		 * look for global macrocell configuration data
		 */
		for (; fusenum != curpal->iptr->s0; fusenum++)
			if (getbit(datfile) == EOF)
				{
				printf("palconfig:cannot find macrocell data\n");
				return -1;
				}
		/*
		 * read in global macrocell data
		 */
		for (i = 0; i < curpal->iptr->snum; i++)
			{
			if ((inchar = getbit(datfile)) == EOF)
				{
				printf("palconfig:unexpected EOF in JEDEC file %s (macrocell)\n",curpal->iptr->def);
				return -1;
				}
			if (inchar == '1')
				curpal->iptr->mc[i] = 1;
			else
				curpal->iptr->mc[i] = 0;
			fusenum++;
			}
		/*
		 * determine input fuse assignments
		 */
		for (i = 0, tptr = curpal->ithresh; i < curpal->inp; i++, tptr++)
			if (tptr->rsel->arow != -1)
				if (curpal->iptr->mc[tptr->rsel->sel] == tptr->rsel->psel)
					tptr->row = tptr->rsel->prow;
				else
					tptr->row = tptr->rsel->arow;
			else
				tptr->row = tptr->rsel->prow;
		/*
		 * determine logic block feedback fuse assignments
		 */
		for (i = 0, fptr = curpal->bptr; i < curpal->blknum; i++, fptr++)
			if (fptr->rsel->arow != -1)
				if (curpal->iptr->mc[fptr->rsel->sel] == fptr->rsel->psel)
					fptr->row = fptr->rsel->prow;
				else
					fptr->row = fptr->rsel->arow;
			else
				fptr->row = fptr->rsel->prow;
		}
	else if (!strcmp(curpal->iptr->name,"PAL22V10"))
		{
		/*
		 * The 22V10 has a simple macrocell where polarity and type are
		 * controlled by two configuration fuses.
		 * Look for global macrocell configuration data first
		 */
		for (; fusenum != curpal->iptr->s0; fusenum++)
			if (getbit(datfile) == EOF)
				{
				printf("palconfig:cannot find macrocell data\n");
				return -1;
				}
		/*
		 * read in global macrocell data
		 */
		for (j = 0; j < curpal->blknum; j++)
			{
			for (i = 0, k = 0; i < curpal->iptr->snum; i++, k<<1)
				{
				if ((inchar = getbit(datfile)) == EOF)
					{
					printf("palconfig:unexpected EOF in JEDEC file %s (macrocell)\n",curpal->iptr->def);
					return -1;
					}
				else if (inchar == '1')
					k |= 1<<i;
				fusenum++;
				}
			if (k & 1)
				curpal->bptr[j].pol = NORMAL;
			else
				curpal->bptr[j].pol = INVERTED;
			if (k & 2)
				curpal->bptr[j].typ = COMBINATORIAL;
			else
				curpal->bptr[j].typ = REGISTERED;
			}
		/*
		 * determine input fuse assignments
		 */
		for (i = 0, tptr = curpal->ithresh; i < curpal->inp; i++, tptr++)
			tptr->row = tptr->rsel->prow;
		/*
		 * determine logic block feedback fuse assignments
		 */
		for (i = 0, fptr = curpal->bptr; i < curpal->blknum; i++, fptr++)
			fptr->row = fptr->rsel->prow;
		}
	return 0;			
}
/*
 * pal - PAL emulator
 *
 */
int pal(struct pal *pptr, double *in, int inum, unsigned int *result)
{
	int i;
	struct thresh *tptr;
	struct blk *lptr;
	unsigned int inarray[2];
	unsigned int tval;
	
	inarray[0] = inarray[1] = tval = 0;
	if (!pptr)
		return -1;
	for (i = 0, tptr = pptr->ithresh; (i < pptr->inp) && (i < inum); i++, tptr++)	/* load input bits */
		if (in[i] < tptr->lothresh)
			inbitload(tptr,inarray,0);
		else if (in[i] > tptr->hithresh)
			inbitload(tptr,inarray,1);
	for (i = 0, lptr = pptr->bptr; i < pptr->blknum; i++, lptr++)	/* load feedback bits */
		if ((!pptr->blktyp) || (!lptr->typ))			/* if not registered 22v10 */
			outbitload(lptr,inarray,(*result>>i)&1);	/* feedback from VAL field */
		else
			outbitload(lptr,inarray,!((*result>>i)&1));
/*
 * temporary diagnostic - print out array inputs
 *
	for (i = 0, j = 0; i < pptr->blkbits; i++)
		{
		if (i > ((INTBITS-1)+(INTBITS*j)))
			j ++;
		if (inarray[j] & (1<<(i-(j*INTBITS))))
			printf("1");
		else
			printf("0");
		}
	printf("\t");
* end diagnostic */
	*result = 0;							/* clear VAL field */
	for (i = 0, lptr = pptr->bptr + *(pptr->scan); i < pptr->blknum; i++, lptr = pptr->bptr + *(pptr->scan + i))	/* evaluate blocks */
		{
		tval = eval(lptr,inarray,pptr->blkelm);			/* evaluate PAL logic block */
		*result |= tval<<(*(pptr->scan+i));			/* load bits into VAL */
		}
	return 0;
}
/*
 * inbitload - load bits in input array
 *
 */
int inbitload(struct thresh *tptr, unsigned int *inarray, unsigned int val)
{
	int i;

	tptr->val = val;
	if (tptr->row == -1)
		return 0;
	if (tptr->row < INTBITS)
		{
		inarray[0] |= val<<tptr->row;
		inarray[0] |= (!val)<<(tptr->row + tptr->nrofst);
		}
	else
		{
		i = tptr->row - INTBITS;
		inarray[1] |= val<<i;
		inarray[1] |= (!val)<<(i + tptr->nrofst);
		}
	return 0;
}
/*
 * outbitload - load feedback bits in input array
 *
 */
int outbitload(struct blk *lptr, unsigned int *inarray, unsigned int val)
{
	int i;

	if (lptr->row == -1)
		return 0;
	if (lptr->row < INTBITS)
		if (val)
			{
			inarray[0] |= val<<lptr->row;
			inarray[0] &= (val<<(lptr->row + lptr->nrofst)) ^ 0xffffffff;
			}
		else
			{
			inarray[0] &= ((!val)<<lptr->row) ^ 0xffffffff;
			inarray[0] |= (!val)<<(lptr->row + lptr->nrofst);
			}
	else
		if (val)
			{
			i = lptr->row - INTBITS;
			inarray[1] |= val<<i;
			inarray[1] &= (val<<(i + lptr->nrofst)) ^ 0xffffffff;
			}
		else
			{
			i = lptr->row - INTBITS;
			inarray[1] &= ((!val)<<i) ^ 0xffffffff;
			inarray[1] |= (!val)<<(i + lptr->nrofst);
			}
	return 0;
}
/*
 * eval - evaluate logic block
 *
 */
int eval(struct blk *lptr, unsigned int *inarray, int blkelm)
{
	int i, j;
	unsigned int *eptr;
	int tval, t[2];

	for (i = tval = 0, eptr = lptr->array; i < lptr->blksiz; i++)
		{
		for (j = t[0] = t[1] = 0; j < blkelm; j++, eptr++)
			if ((inarray[j] & *eptr) == *eptr)
				{
				t[j] = 1;
				if (*eptr)
					tval = 1;
				}
		for (j = 0; j < blkelm; j++)
			tval &= t[j];
		if (tval)
			{
			if (lptr->pol == INVERTED)
				tval = 0;
			lptr->val = tval;
			if (lptr->typ == COMBINATORIAL)
				outbitload(lptr,inarray,lptr->val);
			return lptr->val;
			}
		}
	if (lptr->pol == INVERTED)
		tval = 1;
	lptr->val = tval;
	if (lptr->typ == COMBINATORIAL)
		outbitload(lptr,inarray,lptr->val);
	return lptr->val;
}
