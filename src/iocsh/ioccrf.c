/* ioccrf.c */
/* Author:  Marty Kraimer Date: 27APR2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "cantProceed.h"
#include "registry.h"
#include "errlog.h"
#include "dbAccess.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"

#define MAX_BUFFER_SIZE 120

static char *ioccrfID = "ioccrf";
static char inline[MAX_BUFFER_SIZE];

typedef struct argvalue {
    struct argvalue *next;
    union type {
        int ival;
        double dval;
    } type;
}argvalue;

argvalue *argvalueHead = 0;

typedef struct ioccrfFunc {
    ioccrfFuncDef *pioccrfFuncDef;
    ioccrfCallFunc func;
}ioccrfFunc;

static int cvtArg(char **pnextchar,argvalue *pargvalue,ioccrfArg *pioccrfArg)
{
    char *p = *pnextchar;
    char *argend;
    char *endp;
    /*skip leading whitespace */
    while(*p && (isspace(*p))) ++p;
    if(!*p) {
        printf("insufficient number of arguments\n");
        return(0);
    }
    /* look for , or ) */
    argend = p;
    while(*argend && (*argend!=',') && (*argend!=')') ) {
        /* If quote then look for matching quote */
        if(*argend == '\"') {
            ++argend;
            while(*argend && (*argend!='\"') ) ++argend;
            if(!*argend) {
                printf("Illegal string: %s\n",p);
                return(0);
            }
        }
        ++argend;
    }
    if(!*argend) {
        printf("Illegal input starting: %s\n",p);
        return(0);
    }
    *argend = 0;
    *pnextchar = argend + 1;
    /*remove trailing spaces*/
    while(isspace(p[strlen(p)-1])) p[strlen(p)-1] = 0;
    switch(pioccrfArg->type) {
    case ioccrfArgInt:
        pargvalue->type.ival = strtol(p,&endp,0);
        if(*endp) {
            printf("Illegal integer %s\n",p);
            return(0);
        }
        pioccrfArg->value = &pargvalue->type.ival;
        break;
    case ioccrfArgDouble:
        pargvalue->type.dval = strtod(p,&endp);
        if(*endp) {
            printf("Illegal double %s\n",p);
            return(0);
        }
        pioccrfArg->value = &pargvalue->type.dval;
        break;
    case ioccrfArgString:
        /*first and last char should be quote */
        if(*p != '\"' || p[strlen(p)-1] != '\"') {
            /*if just character 0 accept it*/
            if(strcmp(p,"0")==0) {
                pioccrfArg->value = 0;
                break;
            }
            printf("Illegal string argument %s\n",p);
            return(0);
        }
        /* strip off trailing quote */
        p[strlen(p)-1] = 0;
        pioccrfArg->value = p+1;
        break;
    case ioccrfArgPdbbase:
        /*filed must be pdbbase */
        if(strcmp(p,"pdbbase")!=0) {
            printf("Expecting pdbbase; got %s\n",p);
            return(0);
        }
        pioccrfArg->value = pdbbase;
        break;
    default:
        printf("Illegal argument type\n");
        return(0);
    }
    return(1);
}
    

void epicsShareAPI ioccrfRegister(ioccrfFuncDef *pioccrfFuncDef,ioccrfCallFunc func)
{
    ioccrfFunc *pioccrfFunc;

    pioccrfFunc = callocMustSucceed(1,sizeof(ioccrfFunc),"ioccrfRegister");
    pioccrfFunc->pioccrfFuncDef = pioccrfFuncDef;
    pioccrfFunc->func = func;
    if(!registryAdd((void *)ioccrfID,pioccrfFuncDef->name,(void *)pioccrfFunc))
        errlogPrintf("ioccrfRegister failed to add %s\n", pioccrfFuncDef->name);
}

void epicsShareAPI ioccrf(FILE *fp)
{
    char *p;
    char *command = 0;
    ioccrfFunc *pioccrfFunc;
    ioccrfFuncDef *pioccrfFuncDef;
    argvalue *pargvalue;
    ioccrfArg *pioccrfArg;
    argvalue *prevArgvalue;
    int arg;

    while(1) {
        if(fp==stdin) printf("ioccrf: ");
        p= fgets(inline,MAX_BUFFER_SIZE-1,fp);
        if(!p) break;
        if(strlen(p)==1) continue; /* null line */
        inline[strlen(inline)-1] = 0; /*strip newline*/
        if(fp!=stdin) printf("%s\n",inline);
        if(strncmp(p,"exit",4)==0) break;
        /*skip whitespace at beginning of line*/
        while(*p && (isspace(*p))) ++p;
        if(!p) continue;
        command = p;
        /*look for ( and replace with null*/
        while(*p && (*p != '(') ) ++p;
        if(!*p) goto badline;
        *p++ = 0;
        /*remove trailing spaces from command*/
        while(isspace(command[strlen(command)-1]))
            command[strlen(command)-1]=0;
        pioccrfFunc = (ioccrfFunc *)registryFind(ioccrfID,command);
        pioccrfFuncDef = pioccrfFunc->pioccrfFuncDef;
        if(!pioccrfFuncDef) {
            printf("%s: Cant find this command\n",command);
            continue;
        }
        pargvalue = argvalueHead;
        prevArgvalue = 0;
        for(arg=0;arg<pioccrfFuncDef->nargs; arg++) {
            pioccrfArg = pioccrfFuncDef->arg[arg];
            if(!pargvalue) {
                pargvalue = callocMustSucceed(1,sizeof(argvalue),"ioccrf");
                if(prevArgvalue) prevArgvalue->next = pargvalue;
                argvalueHead = pargvalue;
            }
            if(!cvtArg(&p,pargvalue,pioccrfArg)) goto badline;
            prevArgvalue = pargvalue;
            pargvalue = prevArgvalue->next;
        }
        (*pioccrfFunc->func)(pioccrfFuncDef->arg);
        continue;
badline:
        printf("illegal line\n");
    }
}
