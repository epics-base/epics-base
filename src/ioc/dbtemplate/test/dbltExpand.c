/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
* National Laboratory.
* EPICS Base is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
\*************************************************************************/

/* This is a simple version of msi for testing the dbLoadTemplate() code.
 *
 * It calls dbLoadTemplate() to parse the substitution file, but replaces
 * dbLoadRecords() with its own version that reads the template file,
 * expands any macros in the text and prints the result to stdout.
 *
 * This technique won't work on Windows, dbLoadRecords() has to be
 * epicsShare... decorated and loaded from a shared library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "macLib.h"
#include "dbLoadTemplate.h"


#define BUFFER_SIZE 0x10000

static char *input_buffer, *output_buffer;

int dbLoadRecords(const char *file, const char *macros)
{
    MAC_HANDLE *macHandle = NULL;
    char **macPairs;
    FILE *fp;
    size_t input_len;

    if (macCreateHandle(&macHandle, NULL)) {
        fprintf(stderr, "macCreateHandle failed\n");
        exit(1);
    }

    macSuppressWarning(macHandle, 1);
    macParseDefns(macHandle, macros, &macPairs);
    if (!macPairs) {
        macDeleteHandle(macHandle);
        macHandle = NULL;
    } else {
        macInstallMacros(macHandle, macPairs);
        free(macPairs);
    }

    fp = fopen(file, "r");
    if (!fp) {
        fprintf(stderr, "fopen('%s') failed: %s\n", file, strerror(errno));
        exit(1);
    }

    input_len = fread(input_buffer, 1, BUFFER_SIZE, fp);
    if (!feof(fp)) {
        fprintf(stderr, "input file > 64K!\n");
        fclose(fp);
        exit(1);
    }
    input_buffer[input_len] = 0;

    if (fclose(fp)) {
        fprintf(stderr, "fclose('%s') failed: %s\n", file, strerror(errno));
        exit(1);
    }

    macExpandString(macHandle, input_buffer, output_buffer, BUFFER_SIZE-1);
    printf("%s", output_buffer);

    if (macHandle) macDeleteHandle(macHandle);

    return 0;
}

int main(int argc, char **argv)
{
    input_buffer = malloc(BUFFER_SIZE);
    output_buffer = malloc(BUFFER_SIZE);

    if (!input_buffer || !output_buffer) {
        fprintf(stderr, "malloc(%d) failed\n", BUFFER_SIZE);
        exit(1);
    }

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file.sub\n", argv[0]);
        exit(1);
    }

    dbLoadTemplate(argv[1], NULL);

    free(output_buffer);
    free(input_buffer);
    return 0;
}
