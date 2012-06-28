
#include <epicsStdio.h>
#include <string.h>

#include "epicsTypes.h"
#include "dbBase.h"
#include "devSup.h"
#include "registryCommon.h"
#include "registryRecordType.h"
#include "dbStaticLib.h"

#include "epicsUnitTest.h"

/*
 * Test database initialization
 */

/* No actual record support code is provided */
static rset xRSET;

/* the following would normally be found in xRecord.h */

typedef struct xRecord {
    char                name[61];   /* Record Name */
    epicsInt32          val;        /* Value */
} xRecord;

typedef enum {
        xRecordNAME = 0,
        xRecordVAL = 1
} xFieldIndex;

static int xRecordSizeOffset(dbRecordType *prt)
{
    xRecord *prec = 0;
    prt->papFldDes[xRecordNAME]->size = sizeof(prec->name);
    prt->papFldDes[xRecordVAL]->size = sizeof(prec->val);
    prt->papFldDes[xRecordNAME]->offset = (char *)&prec->name - (char *)prec;
    prt->papFldDes[xRecordVAL]->offset = (char *)&prec->val - (char *)prec;
    prt->rec_size = sizeof(*prec);
    return 0;
}

/* the following would normally be found in xRecord_registerRecordDeviceDriver.cpp */

static const char * const recordTypeNames[] = {
    "x"
};

static const recordTypeLocation rtl[] = {
    {&xRSET, &xRecordSizeOffset}
};

/* the following wpuld normally be the contents of xRecord.dbd */

static const char xRecord_dbd[] =
"# This is a combined minimal DBD and DB file\n"
"\n"
"recordtype(x) {\n"
"  field(NAME, DBF_STRING) {\n"
"    prompt(\"Record Name\")\n"
"    special(SPC_NOMOD)\n"
"    size(61)\n"
"  }\n"
"  field(VAL, DBF_LONG) {\n"
"    prompt(\"Value\")\n"
"  }\n"
"}\n";

/* the following is a db file which loads a single record instance */

static const char recinst_db[] = "record(x, x) {}\n";

/* The remainder of this file is specific to the test database initialization */

static FILE* makefile(const char* contents)
{
    size_t len=strlen(contents);
    FILE *fp=epicsTempFile();
    if(!fp) {
        perror("epicsTempFile fails");
        return NULL;
    }

    if(fwrite(contents, 1, len, fp)!=len){
        perror("makefile fwrite failed");
        goto fail;
    }

    if(fseek(fp, 0, SEEK_SET)!=0)
        goto fail;

    return fp;
fail:
    fclose(fp);
    return NULL;
}

int loadTestDB(DBBASE **ppbase)
{
    FILE *fp=makefile(xRecord_dbd);
    if(!fp) {
        testFail("Failed to create temp xRecord.dbd");
        return -1;
    }

    if(dbReadDatabaseFP(ppbase, fp, 0, 0)) {
        testFail("Failed to load temp xRecord.dbd");
        goto fail;
    }
    /* fp closed by dbReadDatabaseFP */

    registerRecordTypes(*ppbase, NELEMENTS(rtl), recordTypeNames, rtl);

    fp=makefile(recinst_db);
    if(!fp) {
        testFail("Failed to create temp xinst.db");
        return -1;
    }

    if(dbReadDatabaseFP(ppbase, fp, 0, 0)) {
        testFail("Failed to load temp xinst.db");
        goto fail;
    }
    /* fp closed by dbReadDatabaseFP */

    testPass("Loaded test database");
    return 0;
fail:
    fclose(fp);
    return -1;
}
