eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # registerRecordDeviceDriver 

$file = $ARGV[0];
$numberRecordType = 0;
$numberDeviceSupport = 0;
$numberDriverSupport = 0;

open(INP,"$file") or die "$! opening file";
while(<INP>) {
    if( /recordtype/) {
        /recordtype\s*\(\s*(\w+)/;
        $recordType[$numberRecordType++] = $1;
    }
    if( /device/) {
        /device\s*\(\s*(\s*\w+)\W+\w+\W+(\w+)/;
        $deviceRecordType[$numberDeviceSupport] = $1;
        $deviceSupport[$numberDeviceSupport] = $2;
        $numberDeviceSupport++;
    }
    if( /driver/) {
        /driver\s*\(\s*(\w+)/;
        $driverSupport[$numberDriverSupport++] = $1;
    }
}
close(INP) or die "$! closing file";
# beginning of generated routine

print << "END" ;
/*#registerRecordDeviceDriver.cpp */
/* THIS IS A GENERATED FILE. DO NOT EDIT */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "dbBase.h"
#include "errlog.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "recSup.h"
#include "devSup.h"
#include "drvSup.h"
#include "registryRecordType.h"
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"
#include "ioccrf.h"
END

#definitions for recordtype
if($numberRecordType>0) {
    for ($i=0; $i<$numberRecordType; $i++) {
        print "extern \"C\" struct rset $recordType[$i]RSET;\n";
        print "extern \"C\" int $recordType[$i]RecordSizeOffset(dbRecordType *pdbRecordType);\n";
    #NOTE the following caused a compiler error on vxWorks
    #    print "extern computeSizeOffset $recordType[$i]RecordSizeOffset;\n";
    }
    print "\nstatic const char * const recordTypeNames[$numberRecordType] = {\n";
    for ($i=0; $i<$numberRecordType; $i++) {
        print "    \"$recordType[$i]\"";
        if($i < $numberRecordType-1) { print ",";}
        print "\n";
    }
    print "};\n\n";

    print "static const recordTypeLocation rtl[$i] = {\n";
    for ($i=0; $i<$numberRecordType; $i++) {
        print "    {&$recordType[$i]RSET, $recordType[$i]RecordSizeOffset}";
        if($i < $numberRecordType-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
}

#definitions for device
if($numberDeviceSupport>0) {
    for ($i=0; $i<$numberDeviceSupport; $i++) {
        print "extern struct dset $deviceSupport[$i];\n";
    }
    print "\nstatic const char * const deviceSupportNames[$numberDeviceSupport] = {\n";
    for ($i=0; $i<$numberDeviceSupport; $i++) {
        print "    \"$deviceSupport[$i]\"";
        if($i < $numberDeviceSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";

    print "static const struct dset * const devsl[$i] = {\n";
    for ($i=0; $i<$numberDeviceSupport; $i++) {
        print "    &$deviceSupport[$i]";
        if($i < $numberDeviceSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
}

#definitions for driver
if($numberDriverSupport>0) {
    for ($i=0; $i<$numberDriverSupport; $i++) {
        print "extern struct drvet $driverSupport[$i];\n";
    }
    print "\nstatic char *driverSupportNames[$numberDriverSupport] = {\n";
    for ($i=0; $i<$numberDriverSupport; $i++) {
        print "    \"$driverSupport[$i]\"";
        if($i < $numberDriverSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
    
    print "static struct drvet *drvsl[$i] = {\n";
    for ($i=0; $i<$numberDriverSupport; $i++) {
        print "    &$driverSupport[$i]";
        if($i < $numberDriverSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
}

#Now actual registration code.

print << "END" ;
int registerRecordDeviceDriver(DBBASE *pbase)
{
    int i;

END
if($numberRecordType>0) {
    print << "END" ;
    for(i=0; i< $numberRecordType;  i++ ) {
        recordTypeLocation *precordTypeLocation;
        computeSizeOffset sizeOffset;
        DBENTRY dbEntry;

        if(registryRecordTypeFind(recordTypeNames[i])) continue;
        if(!registryRecordTypeAdd(recordTypeNames[i],&rtl[i])) {
            errlogPrintf(\"registryRecordTypeAdd failed %s\\n\",
                recordTypeNames[i]);
            continue;
        }
        dbInitEntry(pbase,&dbEntry);
        precordTypeLocation = registryRecordTypeFind(recordTypeNames[i]);
        sizeOffset = precordTypeLocation->sizeOffset;
        if(dbFindRecordType(&dbEntry,recordTypeNames[i])) {
            errlogPrintf(\"registerRecordDeviceDriver failed %s\\n\",
                recordTypeNames[i]);
        } else {
            sizeOffset(dbEntry.precordType);
        }
    }
END
}
if($numberDeviceSupport>0) {
    print << "END" ;
    for(i=0; i< $numberDeviceSupport;  i++ ) {
        if(registryDeviceSupportFind(deviceSupportNames[i])) continue;
        if(!registryDeviceSupportAdd(deviceSupportNames[i],devsl[i])) {
            errlogPrintf(\"registryDeviceSupportAdd failed %s\\n\",
                deviceSupportNames[i]);
            continue;
        }
    }
END
}
if($numberDriverSupport>0) {
    print << "END" ;
    for(i=0; i< $numberDriverSupport;  i++ ) {
        if(registryDriverSupportFind(driverSupportNames[i])) continue;
        if(!registryDriverSupportAdd(driverSupportNames[i],drvsl[i])) {
            errlogPrintf(\"registryDriverSupportAdd failed %s\\n\",
                driverSupportNames[i]);
            continue;
        }
    }
END
}
print << "END" ;
    return(0);
}

/* registerRecordDeviceDriver */
static const ioccrfArg registerRecordDeviceDriverArg0 =
                                            {"pdbbase",ioccrfArgPdbbase};
static const ioccrfArg *registerRecordDeviceDriverArgs[1] =
                                            {&registerRecordDeviceDriverArg0};
static const ioccrfFuncDef registerRecordDeviceDriverFuncDef =
                {"registerRecordDeviceDriver",1,registerRecordDeviceDriverArgs};
extern "C" {
static void registerRecordDeviceDriverCallFunc(const ioccrfArgBuf *)
{
    registerRecordDeviceDriver(pdbbase);
}
} //extern "C"

/*
 * Register commands on application startup
 */
class IoccrfReg {
  public:
    IoccrfReg() { ioccrfRegister(&registerRecordDeviceDriverFuncDef,registerRecordDeviceDriverCallFunc);}
};
namespace { IoccrfReg ioccrfReg; }
END
