/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
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

	pr(dfd,"static void %s%s%s(void* d,const void* s,aitIndex c)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitIndex i;\n");
	pr(dfd,"\tchar temp[AIT_FIXED_STRING_SIZE];\n");

	if(j==aitEnumInt8)
		pr(dfd,"\taitInt32 itmp;\n");
	else if(j==aitEnumUint8)
		pr(dfd,"\taitUint32 itmp;\n");

	pr(dfd,"\taitString* out=(aitString*)d;\n");
	pr(dfd,"\t%s* in=(%s*)s;\n",aitName[j],aitName[j]);
	pr(dfd,"\tfor(i=0;i<c;i++) {\n");

	if(j==aitEnumInt8)
		pr(dfd,"\t\titmp=(aitInt32)in[i];\n");
	else if(j==aitEnumUint8)
		pr(dfd,"\t\titmp=(aitUint32)in[i];\n");

	if(j==aitEnumInt8)
		pr(dfd,"\t\tsprintf(temp,aitStringType[aitEnumInt32],itmp);\n");
	else if(j==aitEnumUint8)
		pr(dfd,"\t\tsprintf(temp,aitStringType[aitEnumUint32],itmp);\n");
	else
		pr(dfd,"\t\tsprintf(temp,aitStringType[%d],in[i]);\n",j);

	pr(dfd,"\t\tout[i].copy(temp);\n");
	pr(dfd,"\t}\n");
	pr(dfd,"}\n");
}

void MakeStringFuncTo(int i,int j,int k)
{
	/* assumes that void* d in an array of char pointers of length c */
	/* takes string data from source j and convert it to numeric in dest i */

	pr(dfd,"static void %s%s%s(void* d,const void* s,aitIndex c)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitIndex i;\n");
	pr(dfd,"\taitString* in=(aitString*)s;\n");

	/* I special cased the Int8 and Uint8 - yuck */

	if(i==aitEnumInt8)
		pr(dfd,"\taitInt32 itmp;\n");
	else if(i==aitEnumUint8)
		pr(dfd,"\taitUint32 itmp;\n");

	pr(dfd,"\t%s* out=(%s*)d;\n",aitName[i],aitName[i]);
	pr(dfd,"\tfor(i=0;i<c;i++) {\n");
	pr(dfd,"\t\tif(in[i].string()) {\n");

	if(i==aitEnumInt8)
	{
		pr(dfd,"\t\t\tsscanf(in[i],aitStringType[aitEnumInt32],&itmp);\n");
		pr(dfd,"\t\t\tout[i]=(aitInt8)itmp;\n");
	}
	else if(i==aitEnumUint8)
	{
		pr(dfd,"\t\t\tsscanf(in[i],aitStringType[aitEnumUint32],&itmp);\n");
		pr(dfd,"\t\t\tout[i]=(aitUint8)itmp;\n");
	}
	else
		pr(dfd,"\t\t\tsscanf(in[i].string(),aitStringType[%d],&out[i]);\n",i);

	pr(dfd,"\t\t} else\n");
	pr(dfd,"\t\t\tout[i]=0;\n");
	pr(dfd,"\t}\n}\n");
}

void MakeFStringFuncFrom(int i,int j,int k)
{
	/* assumes that void* d in an array of char pointers of length c */
	/* takes numeric data from source j and convert it to string in dest i */

	pr(dfd,"static void %s%s%s(void* d,const void* s,aitIndex c)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitIndex i;\n");

	if(j==aitEnumInt8)
		pr(dfd,"\taitInt32 itmp;\n");
	else if(j==aitEnumUint8)
		pr(dfd,"\taitUint32 itmp;\n");

	pr(dfd,"\taitFixedString* out=(aitFixedString*)d;\n");
	pr(dfd,"\t%s* in=(%s*)s;\n",aitName[j],aitName[j]);
	pr(dfd,"\tfor(i=0;i<c;i++) {\n");

	if(j==aitEnumInt8)
		pr(dfd,"\t\titmp=(aitInt32)in[i];\n");
	else if(j==aitEnumUint8)
		pr(dfd,"\t\titmp=(aitUint32)in[i];\n");

	if(j==aitEnumInt8)
		pr(dfd,"\t\tsprintf(out[i].fixed_string,aitStringType[aitEnumInt32],itmp);\n");
	else if(j==aitEnumUint8)
		pr(dfd,"\t\tsprintf(out[i].fixed_string,aitStringType[aitEnumUint32],itmp);\n");
	else
		pr(dfd,"\t\tsprintf(out[i].fixed_string,aitStringType[%d],in[i]);\n",j);

	pr(dfd,"\t}\n}\n");
}

void MakeFStringFuncTo(int i,int j,int k)
{
	/* assumes that void* d in an array of char pointers of length c */
	/* takes string data from source j and convert it to numeric in dest i */

	pr(dfd,"static void %s%s%s(void* d,const void* s,aitIndex c)\n",
		table_type[k],&(aitName[i])[3],&(aitName[j])[3]);
	pr(dfd,"{\n");
	pr(dfd,"\taitIndex i;\n");
	pr(dfd,"\taitFixedString* in=(aitFixedString*)s;\n");

	/* I special cased the Int8 and Uint8 - yuck */

	if(i==aitEnumInt8)
		pr(dfd,"\taitInt32 itmp;\n");
	else if(i==aitEnumUint8)
		pr(dfd,"\taitUint32 itmp;\n");

	pr(dfd,"\t%s* out=(%s*)d;\n",aitName[i],aitName[i]);
	pr(dfd,"\tfor(i=0;i<c;i++) {\n");

	if(i==aitEnumInt8)
	{
		pr(dfd,"\t\tsscanf(in[i].fixed_string,aitStringType[aitEnumInt32],&itmp);\n");
		pr(dfd,"\t\tout[i]=(aitInt8)itmp;\n");
	}
	else if(i==aitEnumUint8)
	{
		pr(dfd,"\t\tsscanf(in[i].fixed_string,aitStringType[aitEnumUint32],&itmp);\n");
		pr(dfd,"\t\tout[i]=(aitUint8)itmp;\n");
	}
	else
		pr(dfd,"\t\tsscanf(in[i].fixed_string,aitStringType[%d],&out[i]);\n",i);

	pr(dfd,"\t}\n}\n");
}

void GenName(int i,int j,int k)
{
	const char* i_name = &((aitName[i])[3]);
	const char* j_name = &((aitName[j])[3]);

	pr(dfd,"static void %s%s%s(void* d,const void* s,aitIndex c)\n",
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
	pr(dfd,"}\n");
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
	pr(dfd,"}\n");
}

