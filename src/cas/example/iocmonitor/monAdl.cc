
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "monNode.h"
#include "monServer.h"

#define WIDTH		180
#define HEIGHT		10
#define MAX_WIDTH	1000
#define MAX_HEIGHT	700
#define TOTAL_X		(MAX_WIDTH/WIDTH)
#define TOTAL_Y		(MAX_HEIGHT/HEIGHT)

int monAdl(monNode** head, char* fname,const char* prefix)
{
	FILE *fdout;
	int dh;
	int i,j,k;
	monNode* node;

	if((fdout=fopen(fname,"w"))==NULL)
	{
		fprintf(stderr,"Cannot open the ADL file %s\n",fname);
		return -1;
	}

	dh=monNode::total/TOTAL_X;
	if((dh*TOTAL_X)!=monNode::total) ++dh;

	fprintf(fdout,"\nfile { name=\"ioc_status.adl\" version=020209 }\n");
	fprintf(fdout,"display {\n");
	fprintf(fdout,"  object { x=0 y=0 width=%d height=%d }\n",
		MAX_WIDTH,dh*HEIGHT+40);
	fprintf(fdout,"  clr=37 bclr=14 cmap=\"\"\n}\n");

	fprintf(fdout,"\"color map\" { ncolors=65 colors {\n");
	fprintf(fdout,"ffffff, ececec, dadada, c8c8c8, bbbbbb, aeaeae, 9e9e9e,\n");
	fprintf(fdout,"919191, 858585, 787878, 696969, 5a5a5a, 464646, 2d2d2d,\n");
	fprintf(fdout,"000000, 00d800, 1ebb00, 339900, 2d7f00, 216c00, fd0000,\n");
	fprintf(fdout,"de1309, be190b, a01207, 820400, 5893ff, 597ee1, 4b6ec7,\n");
	fprintf(fdout,"3a5eab, 27548d, fbf34a, f9da3c, eeb62b, e19015, cd6100,\n");
	fprintf(fdout,"ffb0ff, d67fe2, ae4ebc, 8b1a96, 610a75, a4aaff, 8793e2,\n");
	fprintf(fdout,"6a73c1, 4d52a4, 343386, c7bb6d, b79d5c, a47e3c, 7d5627,\n");
	fprintf(fdout,"58340f, 99ffff, 73dfff, 4ea5f9, 2a63e4, 0a00b8, ebf1b5,\n");
	fprintf(fdout,"d4db9d, bbc187, a6a462, 8b8239, 73ff6b, 52da3b, 3cb420,\n");
	fprintf(fdout,"289315, 1a7309,\n");
	fprintf(fdout,"} }\n");

	// ----------------------- heading ----------------------------------
	i=(MAX_WIDTH/2)-(300/2);
	fprintf(fdout," text { object { x=%d y=2 width=300 height=25 }\n",i);
	fprintf(fdout," \"basic attribute\" { clr=37 }\n");
	fprintf(fdout," textix=\"%d IOC STATUS MONITOR\"\n",monNode::total);
	fprintf(fdout," align=\"horiz. centered\"\n}\n");

	// ---------------------- generate new file button -------------------
	fprintf(fdout,"\"message button\" { object\n");
	fprintf(fdout," { x=%d y=2 width=180 height=22 }\n",MAX_WIDTH-180);
	fprintf(fdout," control { chan=\"%smakeScreen\" clr=31 bclr=47 }\n",prefix);
	fprintf(fdout," label=\"Make New Screen\" press_msg=\"1\"\n}\n");

	fprintf(fdout,"\"text update\" { object { x=1 y=2 width=180 height=22 }\n");
	fprintf(fdout," monitor { chan=\"%siocCount\" clr=31 bclr=14 }\n",prefix);
	fprintf(fdout,"}\n");

	// -------------- make all the buttons
	i=0;j=0;
	for(k=0;k<ADDR_TOTAL;k++)
	{
		for(node=head[k];node;node=node->next)
		{
			fprintf(fdout,"\"text update\" {\n");
			fprintf(fdout,"  object { x=%d y=%d width=%d height=%d }\n",
				i*WIDTH,j*HEIGHT+30,WIDTH,HEIGHT);

			if(++i>=TOTAL_X) { i=0; ++j; }

			fprintf(fdout,"  monitor { chan=\"%s\" clr=0 bclr=14 }\n",
				node->name);
			fprintf(fdout,"  clrmod=\"alarm\"\n}\n");
		}
	}

	fclose(fdout);
	return 0;
}

