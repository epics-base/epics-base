/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.6  1997/06/13 09:26:04  jhill
 * fixed generated conversion functions
 *
 * Revision 1.5  1997/05/01 19:54:50  jhill
 * updated dll keywords
 *
 * Revision 1.4  1996/11/02 01:24:41  jhill
 * strcpy => styrcpy (shuts up purify)
 *
 * Revision 1.3  1996/08/13 15:07:44  jbk
 * changes for better string manipulation and fixes for the units field
 *
 * Revision 1.2  1996/06/27 14:33:13  jbk
 * changes data type to string conversions to use installString(), not copy()
 *
 * Revision 1.1  1996/06/25 19:11:30  jbk
 * new in EPICS base
 *
 * Revision 1.3  1996/06/17 15:24:05  jbk
 * many mods, string class corrections.
 * gdd operator= protection.
 * dbmapper uses aitString array for menus now
 *
 * Revision 1.2  1996/06/13 21:31:52  jbk
 * Various fixes and correction - including ref_cnt change to unsigned short
 *
 * Revision 1.1  1996/05/31 13:15:19  jbk
 * add new stuff
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "aitTypes.h"

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

#define FILE_NAME "aitConvertGenerated.cc"
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

int main(int argc,char* argv[])
{
	int i,j,k;

	if((dfd=fopen(FILE_NAME,"w"))==NULL)
	{
		pr(stderr,"file %s failed to open\n",FILE_NAME);
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
			MakeStringFuncTo(i,aitEnumString,k);
			MakeStringFuncFrom(aitEnumString,i,k);
			MakeFStringFuncTo(i,aitEnumFixedString,k);
			MakeFStringFuncFrom(aitEnumFixedString,i,k);
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

	pr(dfd,"static int %s%s%s(void* d,const void* s,aitIndex c)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitIndex i;\n");
	pr(dfd,"\tint status=0;\n");
	pr(dfd,"\tchar temp[AIT_FIXED_STRING_SIZE];\n");
	pr(dfd,"\taitString* out=(aitString*)d;\n");
	pr(dfd,"\t%s* in=(%s*)s;\n",aitName[j],aitName[j]);

	pr(dfd,"\tfor(i=0;i<c;i++) {\n");
	pr(dfd,"\t\tint nChar;\n");
	pr(dfd,"\t\tnChar = sprintf(temp, \"%%%s\",in[i]);\n",
			aitPrintf[j]);
	pr(dfd,"\t\tif (nChar>=0) {\n");
	pr(dfd,"\t\t\tstatus += nChar;\n");
	pr(dfd,"\t\t\tout[i].copy(temp);\n");
	pr(dfd,"\t\t}\n");
	pr(dfd,"\t\telse {\n");
	pr(dfd,"\t\t\treturn -1;\n");
	pr(dfd,"\t\t}\n");
	pr(dfd,"\t}\n");
	pr(dfd,"\treturn status;\n");
	pr(dfd,"}\n");
}

void MakeStringFuncTo(int i,int j,int k)
{
	/* assumes that void* d in an array of char pointers of length c */
	/* takes string data from source j and convert it to numeric in dest i */

	pr(dfd,"static int %s%s%s(void* d,const void* s,aitIndex c)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitIndex i;\n");
	pr(dfd,"\taitString* in=(aitString*)s;\n");
	pr(dfd,"\t%s* out=(%s*)d;\n",aitName[i],aitName[i]);

	pr(dfd,"\tfor(i=0;i<c;i++) {\n");
	pr(dfd,"\t\tif(in[i].string()) {\n");
	pr(dfd,"\t\t\tint j;\n");
	pr(dfd,"\t\t\tdouble ftmp;\n");

	pr(dfd,"\t\t\tj = sscanf(in[i],\"%%lf\",&ftmp);\n");
	pr(dfd,"\t\t\tif (j==1) {\n");
	pr(dfd,"\t\t\t\tif (ftmp>=%g && ftmp<=%g) {\n",
			aitMin[i], aitMax[i]);
	pr(dfd,"\t\t\t\t\tout[i] = (%s) ftmp;\n", aitName[i]);
	pr(dfd,"\t\t\t\t}\n");
	pr(dfd,"\t\t\t\telse {\n");
	pr(dfd,"\t\t\t\t\treturn -1;\n");
	pr(dfd,"\t\t\t\t}\n");
	pr(dfd,"\t\t\t}\n");
	pr(dfd,"\t\t\telse {\n");
	pr(dfd,"\t\t\t\tunsigned long itmp;\n");
	pr(dfd,"\t\t\t\tj = sscanf(in[i],\"%%lx\",&itmp);\n");
	pr(dfd,"\t\t\t\tif (j==1) {\n");
	pr(dfd,"\t\t\t\t\tif (%g<=(double)itmp && %g>=(double)itmp) {\n",
			aitMin[i], aitMax[i]);
	pr(dfd,"\t\t\t\t\t\tout[i] = (%s) itmp;\n", aitName[i]);
	pr(dfd,"\t\t\t\t\t}\n");
	pr(dfd,"\t\t\t\t\telse {\n");
	pr(dfd,"\t\t\t\t\t\treturn -1;\n");
	pr(dfd,"\t\t\t\t\t}\n");
	pr(dfd,"\t\t\t\t}\n");
	pr(dfd,"\t\t\t\telse {\n");
	pr(dfd,"\t\t\t\t\treturn -1;\n");
	pr(dfd,"\t\t\t\t}\n");
	pr(dfd,"\t\t\t}\n");
	pr(dfd,"\t\t}\n");
	pr(dfd,"\t}\n");
	pr(dfd,"\treturn (int) (sizeof(%s)*c);\n}\n", aitName[i]);
}

void MakeFStringFuncFrom(int i,int j,int k)
{
	/* assumes that void* d in an array of char pointers of length c */
	/* takes numeric data from source j and convert it to string in dest i */

	pr(dfd,"static int %s%s%s(void* d,const void* s,aitIndex c)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitIndex i;\n");
	pr(dfd,"\taitFixedString* out=(aitFixedString*)d;\n");
	pr(dfd,"\t%s* in=(%s*)s;\n",aitName[j],aitName[j]);

#if 0
	if(j==aitEnumInt8)
	{
		pr(dfd,"\n\t// assume source s is string if count c is 1\n");
		pr(dfd,"\n\tif(c==1) {\n");
		pr(dfd,"\t\tstrcpy(out->fixed_string,(char*)in);\n");
		pr(dfd,"\t\treturn;\n");
		pr(dfd,"\t}\n\n");
	}
#endif

	pr(dfd,"\tfor(i=0;i<c;i++) {\n");
	pr(dfd,"\t\tint nChar;\n");
	pr(dfd,"\t\tnChar = sprintf(out[i].fixed_string, \"%%%s\",in[i]);\n",
				aitPrintf[j]);
	pr(dfd,"\t\tif (nChar>=0) {\n");
	pr(dfd,"\t\t\tnChar = min(nChar,AIT_FIXED_STRING_SIZE-1)+1;\n");
	pr(dfd,"\t\t\t/* shuts up purify */\n");
	pr(dfd,"\t\t\tmemset(&out[i].fixed_string[nChar],\'\\0\',AIT_FIXED_STRING_SIZE-nChar);\n");
	pr(dfd,"\t\t}\n");
	pr(dfd,"\t\telse {\n");
	pr(dfd,"\t\t\treturn -1;\n");
	pr(dfd,"\t\t}\n");
	pr(dfd,"\t}\n\treturn c*AIT_FIXED_STRING_SIZE;\n}\n");
}

void MakeFStringFuncTo(int i,int j,int k)
{
	/* assumes that void* d in an array of char pointers of length c */
	/* takes string data from source j and convert it to numeric in dest i */
	pr(dfd,"static int %s%s%s(void* d,const void* s,aitIndex c)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitIndex i;\n");
	pr(dfd,"\taitFixedString* in=(aitFixedString*)s;\n");
	pr(dfd,"\t%s* out=(%s*)d;\n",aitName[i],aitName[i]);

	pr(dfd,"\tfor(i=0;i<c;i++) {\n");
	pr(dfd,"\t\tint j;\n");
	pr(dfd,"\t\tdouble ftmp;\n");

	pr(dfd,"\t\tj = sscanf(in[i].fixed_string,\"%%lf\",&ftmp);\n");
	pr(dfd,"\t\tif (j==1) {\n");
	pr(dfd,"\t\t\tif (ftmp>=%g && ftmp<=%g) {\n",
			aitMin[i], aitMax[i]);
	pr(dfd,"\t\t\t\tout[i] = (%s) ftmp;\n", aitName[i]);
	pr(dfd,"\t\t\t}\n");
	pr(dfd,"\t\t\telse {\n");
	pr(dfd,"\t\t\t\treturn -1;\n");
	pr(dfd,"\t\t\t}\n");
	pr(dfd,"\t\t}\n");
	pr(dfd,"\t\telse {\n");
	pr(dfd,"\t\t\tunsigned long itmp;\n");
	pr(dfd,"\t\t\tj = sscanf(in[i].fixed_string,\"%%lx\",&itmp);\n");
	pr(dfd,"\t\t\tif (j==1) {\n");
	pr(dfd,"\t\t\t\tif (%g<=(double)itmp && %g>=(double)itmp) {\n",
			aitMin[i], aitMax[i]);
	pr(dfd,"\t\t\t\t\tout[i] = (%s) itmp;\n", aitName[i]);
	pr(dfd,"\t\t\t\t}\n");
	pr(dfd,"\t\t\t\telse {\n");
	pr(dfd,"\t\t\t\t\treturn -1;\n");
	pr(dfd,"\t\t\t\t}\n");
	pr(dfd,"\t\t\t}\n");
	pr(dfd,"\t\t\telse {\n");
	pr(dfd,"\t\t\t\treturn -1;\n");
	pr(dfd,"\t\t\t}\n");
	pr(dfd,"\t\t}\n");
	pr(dfd,"\t}\n");
	pr(dfd,"\treturn (int) (sizeof(%s)*c);\n}\n", aitName[i]);
}

void GenName(int i,int j,int k)
{
	const char* i_name = &((aitName[i])[3]);
	const char* j_name = &((aitName[j])[3]);

	pr(dfd,"static int %s%s%s(void* d,const void* s,aitIndex c)\n",
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

