/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "epicsStdio.h"

#ifndef SEEK_END
#define SEEK_END 2
#endif

/*
 * truncate to specified size (we don't use truncate()
 * because it is not portable)
 */
LIBCOM_API enum TF_RETURN  truncateFile (const char *pFileName, unsigned long size)
{
    long filePos;
    FILE *pFile;
    FILE *ptmp;
    int status;
    int c;
    unsigned charNo;

    /*
     * see cast of size to long below
     */
    if (size>LONG_MAX) {
        return TF_ERROR;
    }

    pFile = fopen(pFileName, "r");
    if (!pFile) {
        fprintf (stderr,
            "File access problems to `%s' because `%s'\n",
            pFileName,
            strerror(errno));
        return TF_ERROR;
    }

    /*
     * This is not required under UNIX but does appear
     * to be required under WIN32.
     */
    status = fseek (pFile, 0L, SEEK_END);
    if (status!=TF_OK) {
        fclose (pFile);
        return TF_ERROR;
    }

    filePos = ftell(pFile);
    if (filePos <= (long) size) {
        fclose (pFile);
        return TF_OK;
    }

    ptmp = epicsTempFile();
    if (!ptmp) {
        fprintf (stderr,
            "File access problems to temp file because `%s'\n",
            strerror(errno));
        fclose (pFile);
        return TF_ERROR;
    }
    rewind (pFile);
    charNo = 0u;
    while (charNo<size) {
        c = getc (pFile);
        if (c==EOF) {
            fprintf (stderr,
                "File access problems to temp file because `%s'\n",
                strerror(errno));
            fclose (pFile);
            fclose (ptmp);
            return TF_ERROR;
        }
        status = putc (c, ptmp);
        if (status==EOF) {
            fprintf(stderr,
                "File access problems to temp file because `%s'\n",
                strerror(errno));
            fclose (pFile);
            fclose (ptmp);
            return TF_ERROR;
        }
        charNo++;
    }
    fclose (pFile);
    pFile = fopen(pFileName, "w");
    if (!pFile) {
        fprintf (stderr,
            "File access problems to `%s' because `%s'\n",
            pFileName,
            strerror(errno));
        fclose (ptmp);
        return TF_ERROR;
    }
    rewind (ptmp);
    charNo = 0u;
    while (charNo<size) {
        c = getc (ptmp);
        if (c==EOF) {
            fprintf (stderr,
                "File access problems to temp file because `%s'\n",
                strerror(errno));
            fclose (pFile);
            fclose (ptmp);
            return TF_ERROR;
        }
        status = putc (c, pFile);
        if (status==EOF) {
            fprintf(stderr,
                "File access problems to `%s' because `%s'\n",
                pFileName,
                strerror(errno));
            fclose (pFile);
            fclose (ptmp);
            return TF_ERROR;
        }
        charNo++;
    }
    fclose(ptmp);
    fclose(pFile);
    return TF_OK;
}

