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
/*#registerRecordDeviceDriver.c */
/* THIS IS A GENERATED FILE. DO NOT EDIT */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "recSup.h"
#include "devSup.h"
#include "drvSup.h"
#include "registryRecordType.h"
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"

END

#definitions for recordtype
if($numberRecordType>0) {
    for ($i=0; $i<$numberRecordType; $i++) {
        print "extern struct rset $recordType[$i]RSET;\n";
        print "extern int $recordType[$i]RecordSizeOffset(dbRecordType *pdbRecordType);\n";
    #NOTE the followimng caused a compiler error on vxWorks
    #    print "extern computeSizeOffset $recordType[$i]RecordSizeOffset;\n";
    }
    print "\nstatic char *recordTypeNames[$numberRecordType] = {\n";
    for ($i=0; $i<$numberRecordType; $i++) {
        print "    \"$recordType[$i]\"";
        if($i < $numberRecordType-1) { print ",";}
        print "\n";
    }
    print "};\n\n";

    print "static recordTypeLocation rtl[$i] = {\n";
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
    print "\nstatic char *deviceSupportNames[$numberDeviceSupport] = {\n";
    for ($i=0; $i<$numberDeviceSupport; $i++) {
        print "    \"$deviceSupport[$i]\"";
        if($i < $numberDeviceSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";

    print "static struct dset *devsl[$i] = {\n";
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
int registerRecordDeviceDriver(DBBASE *pdbbase)
{
    int i;
    int result;
    static int alreadyCalled = 0;
    if(alreadyCalled) {
        printf(\"registerRecordDeviceDriver called multiple times\\n\");
        return(-1);
    }
END
if($numberRecordType>0) {
    print << "END" ;
    for(i=0; i< $numberRecordType;  i++ ) {
        result = registryRecordTypeAdd(recordTypeNames[i],&rtl[i]);
        if(result) {
            recordTypeLocation *precordTypeLocation;
            int (*sizeOffset)(dbRecordType *pdbRecordType);
            DBENTRY dbEntry;

            dbInitEntry(pdbbase,&dbEntry);
            precordTypeLocation = registryRecordTypeFind(recordTypeNames[i]);
            sizeOffset = precordTypeLocation->sizeOffset;
            if(dbFindRecordType(&dbEntry,recordTypeNames[i])) {
                printf(\"registerRecordDeviceDriver failed %s\\n\",
                    recordTypeNames[i]);
            } else {
                sizeOffset(dbEntry.precordType);
            }
        } else {
            printf(\"registerRecordDeviceDriver failed %s\\n\",recordTypeNames[i]);
        }
    }
END
}
if($numberDeviceSupport>0) {
    print << "END" ;
    for(i=0; i< $numberDeviceSupport;  i++ ) {
        result = registryDeviceSupportAdd(deviceSupportNames[i],devsl[i]);
        if(!result)
            printf(\"registerRecordDeviceDriver failed %s\\n\",
            \"deviceSupportNames[i]\");
    }
END
}
if($numberDriverSupport>0) {
    print << "END" ;
    for(i=0; i< $numberDriverSupport;  i++ ) {
        result = registryDriverSupportAdd(driverSupportNames[i],drvsl[i]);
        if(!result)
            printf(\"registerRecordDeviceDriver failed %s\\n\",
            \"driverSupportNames[i]\");
    }
END
}
print << "END" ;
    return(0);
}
END
