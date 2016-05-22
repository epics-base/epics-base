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
 * Author: Jim Kowalkowski
 * Date: 2/96
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>

#define epicsExportSharedSymbols
#include "aitTypes.h"

void initMinMaxAIT ( void );

/*
 * maximum value for each type - joh
 */
double aitMax[aitTotal];

/*
 * minimum value for each ait type - joh
 */
double aitMin[aitTotal];

/*
   Still need to generate to "FromNet" matrix and rename the "Net" conversions
   to "ToNet".
*/

void MakeNormalFunc(int i,int j,int k);
void MakeToFunc(int i,int j,int k);
void MakeFromFunc(int i,int j,int k);
void GenName(int i,int j,int k);
void GenVars(int i,int j);
void MakeStringFuncFrom(int i,int j,int k);
void MakeStringFuncTo(int i,int j,int k);
void MakeFStringFuncFrom(int i,int j,int k);
void MakeFStringFuncTo(int i,int j,int k);

#define pr fprintf

#define AIT_TO_NET		0
#define AIT_FROM_NET	1
#define AIT_NORMAL		2

#define AIT_SWAP_NONE	0
#define AIT_SWAP_16		1
#define AIT_SWAP_32		2
#define AIT_SWAP_64		3

static const char* table_type[] = {
	"aitConvertToNet",
	"aitConvertFromNet",
	"aitConvert" };
static const char* table_def[] = {
	"#if defined(AIT_TO_NET_CONVERT)",
	"#elif defined(AIT_FROM_NET_CONVERT)",
	"#else /* AIT_CONVERT */" };

static FILE *dfd;

/*
 * maximum, minimum value for each ait type - joh
 */
void initMinMaxAIT (void)
{
    unsigned i;

    for ( i = 0u; i < sizeof ( aitMax ) / sizeof ( aitMax [ 0 ] ); i++ ) {
        aitMax [ i ] = -DBL_MAX;
    }

    for ( i = 0u; i < sizeof ( aitMax ) / sizeof ( aitMax [ 0 ] ); i++ ) {
        aitMin [ i ] = DBL_MAX;
    }

	aitMax [ aitEnumInt8 ] = SCHAR_MAX;
	aitMax [ aitEnumUint8 ] = UCHAR_MAX;
	aitMax [ aitEnumInt16 ] = SHRT_MAX;
	aitMax [ aitEnumUint16 ] = USHRT_MAX;
	aitMax [ aitEnumEnum16 ] = USHRT_MAX;
	aitMax [ aitEnumInt32 ] = INT_MAX;
	aitMax [ aitEnumUint32 ] = UINT_MAX;
	aitMax [ aitEnumFloat32 ] = FLT_MAX;
	aitMax [ aitEnumFloat64 ] = DBL_MAX;

	aitMin [ aitEnumInt8 ] = SCHAR_MIN;
	aitMin [ aitEnumUint8 ] = 0u;
	aitMin [ aitEnumInt16 ] = SHRT_MIN;
	aitMin [ aitEnumUint16 ] = 0u;
	aitMin [ aitEnumEnum16 ] = 0u;
	aitMin [ aitEnumInt32 ] = INT_MIN;
	aitMin [ aitEnumUint32 ] = 0u;
	aitMin [ aitEnumFloat32 ] = -FLT_MAX;
	aitMin [ aitEnumFloat64 ] = -DBL_MAX;
}

int main(int argc,char* argv[])
{
	int i,j,k;

    initMinMaxAIT ();

	if(argc<2)
	{
		fprintf(stderr,"You must enter a file name on command line\n");
		return -1;
	}

	if((dfd=fopen(argv[1],"w"))==NULL)
	{
		pr(stderr,"file %s failed to open\n",argv[1]);
		return -1;
	}

	/* -----------------------------------------------------------------*/
	/* generate basic conversion functions */

	pr(dfd,"\n");

	/* generate each group - to/from/normal */
	for(k=0;k<3;k++)
	{
		pr(dfd,"%s\n",table_def[k]);
		for(i=aitConvertAutoFirst;i<=aitConvertAutoLast;i++)
			for(j=aitConvertAutoFirst;j<=aitConvertAutoLast;j++)
			{
				switch(k)
				{
					case AIT_TO_NET:	MakeToFunc(i,j,k); break;
					case AIT_FROM_NET:	MakeFromFunc(i,j,k); break;
					case AIT_NORMAL:	MakeNormalFunc(i,j,k); break;
					default: break;
				}
			}
	}
	pr(dfd,"#endif\n\n");

	for(k=0;k<3;k++)
	{
		pr(dfd,"%s\n",table_def[k]);
		for(i=aitConvertAutoFirst;i<=aitConvertAutoLast;i++)
		{
            if (i!=aitEnumEnum16) {
			    MakeStringFuncTo(i,aitEnumString,k);
			    MakeStringFuncFrom(aitEnumString,i,k);
			    MakeFStringFuncTo(i,aitEnumFixedString,k);
			    MakeFStringFuncFrom(aitEnumFixedString,i,k);
            }
		}
	}
	pr(dfd,"#endif\n\n");

	/* generate the three conversion table matrices */
	for(k=0;k<3;k++)
	{
		pr(dfd,"%s\n",table_def[k]);
		pr(dfd,"aitFunc %sTable[aitTotal][aitTotal]={\n",table_type[k]);

		/* generate network conversion matrix */
		for(i=aitFirst;i<=aitLast;i++)
		{
			pr(dfd," {\n");
			for(j=aitFirst;j<=aitLast;j++)
			{
				if(i<aitConvertFirst || i>aitConvertLast ||
			   	j<aitConvertFirst || j>aitConvertLast)
					pr(dfd,"aitNoConvert");
				else
					pr(dfd,"%s%s%s",table_type[k],
						&(aitName[i])[3],&(aitName[j])[3]);
	
				if(j<aitLast)
				{
					fputc(',',dfd);
					if((j+1)%2==0) pr(dfd,"\n");
				}
				else
					fputc('\n',dfd);
			}
			pr(dfd," }");
			if(i<aitLast) pr(dfd,",\n"); else fputc('\n',dfd);
		}
		pr(dfd,"};\n\n");
	}
	pr(dfd,"#endif\n");

	fclose(dfd);
	return 0;
}

void MakeStringFuncFrom(int i,int j,int k)
{
	/* assumes that void* d in an array of char pointers of length c */
	/* takes numeric data from source j and convert it to string in dest i */

	pr(dfd,"static int %s%s%s(void* d,const void* s,aitIndex c, const gddEnumStringTable * pEST)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\tchar temp[AIT_FIXED_STRING_SIZE];\n");
	pr(dfd,"\taitString* out=(aitString*)d;\n");
	pr(dfd,"\t%s* in=(%s*)s;\n",aitName[j],aitName[j]);
	pr(dfd,"\tfor(aitIndex i=0;i<c;i++) {\n");
    pr(dfd,"\t\tif ( putDoubleToString ( in[i], pEST, temp, AIT_FIXED_STRING_SIZE ) ) {\n");
    pr(dfd,"\t\t\tout[i].copy ( temp );\n");
    pr(dfd,"\t\t}\n");
    pr(dfd,"\t\telse {\n");
    pr(dfd,"\t\t\treturn -1;\n");
    pr(dfd,"\t\t}\n");
	pr(dfd,"\t}\n\treturn c*AIT_FIXED_STRING_SIZE;\n}\n");
}

void MakeStringFuncTo(int i,int j,int k)
{
	/* assumes that void* d in an array of char pointers of length c */
	/* takes string data from source j and convert it to numeric in dest i */

	pr(dfd,"static int %s%s%s(void* d,const void* s,aitIndex c, const gddEnumStringTable *pEST)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitString* in=(aitString*)s;\n");
	pr(dfd,"\t%s* out=(%s*)d;\n",aitName[i],aitName[i]);
	pr(dfd,"\tfor(aitIndex i=0;i<c;i++) {\n");
    pr(dfd,"\t\tdouble ftmp;\n");
    pr(dfd,"\t\tif ( getStringAsDouble (in[i].string(), pEST, ftmp) ) {\n");
	pr(dfd,"\t\t\tif (ftmp>=%g && ftmp<=%g) {\n",
			aitMin[i], aitMax[i]);
	pr(dfd,"\t\t\t\tout[i] = (%s) ftmp;\n", aitName[i]);
	pr(dfd,"\t\t\t}\n");
	pr(dfd,"\t\t\telse {\n");
	pr(dfd,"\t\t\t\treturn -1;\n");
	pr(dfd,"\t\t\t}\n");
    pr(dfd,"\t\t}\n");
    pr(dfd,"\t\telse {\n");
    pr(dfd,"\t\t\treturn -1;\n");
    pr(dfd,"\t\t}\n");
    pr(dfd,"\t}\n");
	pr(dfd,"\treturn (int) (sizeof(%s)*c);\n}\n", aitName[i]);
}

void MakeFStringFuncFrom(int i,int j,int k)
{
	/* assumes that void* d in an array of char pointers of length c */
	/* takes numeric data from source j and convert it to string in dest i */

	pr(dfd,"static int %s%s%s(void* d,const void* s,aitIndex c, const gddEnumStringTable * pEST)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitFixedString* out=(aitFixedString*)d;\n");
	pr(dfd,"\t%s* in=(%s*)s;\n",aitName[j],aitName[j]);

	pr(dfd,"\tfor(aitIndex i=0;i<c;i++) {\n");
    pr(dfd,"\t\tif ( ! putDoubleToString ( in[i], pEST, out[i].fixed_string, AIT_FIXED_STRING_SIZE ) ) {\n");
    pr(dfd,"\t\t\treturn -1;\n");
    pr(dfd,"\t\t}\n");
	pr(dfd,"\t}\n\treturn c*AIT_FIXED_STRING_SIZE;\n}\n");
}

void MakeFStringFuncTo(int i,int j,int k)
{
	/* assumes that void* d in an array of char pointers of length c */
	/* takes string data from source j and convert it to numeric in dest i */
	pr(dfd,"static int %s%s%s(void* d,const void* s,aitIndex c, const gddEnumStringTable *pEST)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitFixedString* in=(aitFixedString*)s;\n");
	pr(dfd,"\t%s* out=(%s*)d;\n",aitName[i],aitName[i]);
	pr(dfd,"\tfor(aitIndex i=0;i<c;i++) {\n");
    pr(dfd,"\t\tdouble ftmp;\n");
    pr(dfd,"\t\tif ( getStringAsDouble (in[i].fixed_string, pEST, ftmp) ) {\n");
	pr(dfd,"\t\t\tif (ftmp>=%g && ftmp<=%g) {\n",
			aitMin[i], aitMax[i]);
	pr(dfd,"\t\t\t\tout[i] = (%s) ftmp;\n", aitName[i]);
	pr(dfd,"\t\t\t}\n");
	pr(dfd,"\t\t\telse {\n");
	pr(dfd,"\t\t\t\treturn -1;\n");
	pr(dfd,"\t\t\t}\n");
    pr(dfd,"\t\t}\n");
    pr(dfd,"\t\telse {\n");
    pr(dfd,"\t\t\treturn -1;\n");
    pr(dfd,"\t\t}\n");
    pr(dfd,"\t}\n");
	pr(dfd,"\treturn (int) (sizeof(%s)*c);\n}\n", aitName[i]);
}

void GenName(int i,int j,int k)
{
	const char* i_name = &((aitName[i])[3]);
	const char* j_name = &((aitName[j])[3]);

	pr(dfd,"static int %s%s%s(void* d,const void* s,aitIndex c, const gddEnumStringTable *)\n",
			table_type[k],i_name,j_name);
}

void GenVars(int i,int j)
{
	pr(dfd,"\taitIndex i;\n");
	pr(dfd,"\t%s* d_val=(%s*)d;\n",aitName[i],aitName[i]);
	pr(dfd,"\t%s* s_val=(%s*)s;\n\n",aitName[j],aitName[j]);
}

void MakeFromFunc(int i,int j,int k)
{
	int conv_type;
	char *len_msg,*conv_msg;

	/* destination = i, source = j, type = k */
	GenName(i,j,k);
	pr(dfd,"{\n");

	/* check if we need network byte swaps */
	if (strstr(aitName[j],"16"))
		{ len_msg="16"; conv_type=AIT_SWAP_16; }
	else if(strstr(aitName[j],"32"))
		{ len_msg="32"; conv_type=AIT_SWAP_32; }
	else if(strstr(aitName[j],"64"))
		{ len_msg="64"; conv_type=AIT_SWAP_64; }
	else
		{ len_msg=""; conv_type=AIT_SWAP_NONE; }

	if (strstr(aitName[j],"Float"))
		conv_msg="Float";
	else
		conv_msg="Order";

	GenVars(i,j);

	if(i==j || conv_type==AIT_SWAP_NONE)
	{
		if(conv_type!=AIT_SWAP_NONE)
		{
			pr(dfd,"\tfor(i=0;i<c;i++)\n");
			pr(dfd,"\t\taitFromNet%s%s",conv_msg,len_msg);
			pr(dfd,"((aitUint%s*)&d_val[i],",len_msg);
			pr(dfd,"(aitUint%s*)&s_val[i]);\n",len_msg);
		}
		else
		{
			pr(dfd,"\tfor(i=0;i<c;i++)\n");
			pr(dfd,"\t\td_val[i]=(%s)(s_val[i]);\n",aitName[i]);
		}
	}
	else
	{
		/* first swap, then cast to correct type */
		pr(dfd,"\t%s temp;\n\n",aitName[j]);
		pr(dfd,"\tfor(i=0;i<c;i++) {\n");
		pr(dfd,"\t\taitFromNet%s%s",conv_msg,len_msg);
		pr(dfd,"((aitUint%s*)&temp,",len_msg);
		pr(dfd,"(aitUint%s*)&s_val[i]);\n",len_msg);
		pr(dfd,"\t\td_val[i]=(%s)temp;\n",aitName[i]);
		pr(dfd,"\t}\n");
	}
	pr(dfd,"\treturn (int) (sizeof(%s)*c);\n}\n",aitName[i]);
}

void MakeToFunc(int i,int j,int k)
{
	int conv_type;
	char *len_msg,*conv_msg;

	/* destination = i, source = j, type = k */
	GenName(i,j,k);
	pr(dfd,"{\n");

	/* check if we need network byte swaps */
	if (strstr(aitName[i],"16"))
		{ len_msg="16"; conv_type=AIT_SWAP_16; }
	else if(strstr(aitName[i],"32"))
		{ len_msg="32"; conv_type=AIT_SWAP_32; }
	else if(strstr(aitName[i],"64"))
		{ len_msg="64"; conv_type=AIT_SWAP_64; }
	else
		{ len_msg=""; conv_type=AIT_SWAP_NONE; }

	if (strstr(aitName[i],"Float"))
		conv_msg="Float";
	else
		conv_msg="Order";

	GenVars(i,j);

	if(i==j || conv_type==AIT_SWAP_NONE)
	{
		if(conv_type!=AIT_SWAP_NONE)
		{
			pr(dfd,"\tfor(i=0;i<c;i++)\n");
			pr(dfd,"\t\taitToNet%s%s",conv_msg,len_msg);
			pr(dfd,"((aitUint%s*)&d_val[i],",len_msg);
			pr(dfd,"(aitUint%s*)&s_val[i]);\n",len_msg);
		}
		else
		{
			pr(dfd,"\tfor(i=0;i<c;i++)\n");
			pr(dfd,"\t\td_val[i]=(%s)(s_val[i]);\n",aitName[i]);
		}
	}
	else
	{
		/* cast first to correct type, then swap */
		pr(dfd,"\t%s temp;\n\n",aitName[i]);
		pr(dfd,"\tfor(i=0;i<c;i++) {\n");
		pr(dfd,"\t\ttemp=(%s)s_val[i];\n",aitName[i]);
		pr(dfd,"\t\taitToNet%s%s",conv_msg,len_msg);
		pr(dfd,"((aitUint%s*)&d_val[i],",len_msg);
		pr(dfd,"(aitUint%s*)&temp);\n",len_msg);
		pr(dfd,"\t}\n");
	}
	pr(dfd,"\treturn (int) (sizeof(%s)*c);\n",aitName[i]);
	pr(dfd,"}\n");
}

void MakeNormalFunc(int i,int j,int k)
{
	GenName(i,j,k);
	pr(dfd,"{\n");

	if(i==j)
		pr(dfd,"\tmemcpy(d,s,c*sizeof(%s));\n",aitName[i]);
	else
	{
		GenVars(i,j);
		pr(dfd,"\tfor(i=0;i<c;i++)\n");
		pr(dfd,"\t\td_val[i]=(%s)(s_val[i]);\n",aitName[i]);
	}
	pr(dfd,"\treturn (int) (sizeof(%s)*c);\n",aitName[i]);
	pr(dfd,"}\n");
}

