
#include <stdlib.h>

#include <devSup.h>

#include <epicsExport.h>

static
long link_test_extend(struct dbCommon *junk)
{ return 0; }

static dsxt xrecextend = {&link_test_extend, &link_test_extend};

static
long link_test_init(int pass)
{
    if (pass == 0)
        devExtend(&xrecextend);
    return 0;
}

static
long link_test_noop(void *junk)
{ return 0; }



#define DEFDSET(LTYPE) \
    static dset devxLTest ## LTYPE = {4, NULL, &link_test_init, &link_test_noop, &link_test_noop}; \
    epicsExportAddress(dset, devxLTest ## LTYPE);

DEFDSET(JSON_LINK)
DEFDSET(VME_IO)
DEFDSET(CAMAC_IO)
DEFDSET(AB_IO)
DEFDSET(GPIB_IO)
DEFDSET(BITBUS_IO)
DEFDSET(INST_IO)
DEFDSET(BBGPIB_IO)
DEFDSET(RF_IO)
DEFDSET(VXI_IO)
