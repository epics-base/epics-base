eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # registerRecordDeviceDriver 
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

($file, $subname) = @ARGV;
$numberRecordType = 0;
$numberDeviceSupport = 0;
$numberDriverSupport = 0;

# Eliminate chars not allowed in C symbol names
$c_bad_ident_chars = '[^0-9A-Za-z_]';
$subname =~ s/$c_bad_ident_chars/_/g;

open(INP,"$file") or die "$! opening file";
while(<INP>) {
    next if m/ ^ \s* \# /x;
    if (m/ \b recordtype \s* \( \s* (\w+) \s* \) /x) {
        $recordType[$numberRecordType++] = $1;
    }
    elsif (m/ \b device \s* \( \s* (\w+) \W+ \w+ \W+ (\w+) /x) {
        $deviceRecordType[$numberDeviceSupport] = $1;
        $deviceSupport[$numberDeviceSupport] = $2;
        $numberDeviceSupport++;
    }
    elsif (m/ \b driver \s* \( \s* (\w+) \s* \) /x) {
        $driverSupport[$numberDriverSupport++] = $1;
    }
    elsif (m/ \b registrar \s* \( \s* (\w+) \s* \) /x) {
        push @registrars, $1;
    }
    elsif (m/ \b function \s* \( \s* (\w+) \s* \) /x) {
        push @registrars, "register_func_$1";
    }
    elsif (m/ \b variable \s* \( \s* (\w+) \s* , \s* (\w+) \s* \) /x) {
        $varType{$1} = $2;
        push @variables, $1;
    }
}
close(INP) or die "$! closing file";


# beginning of generated routine
print "/* THIS IS A GENERATED FILE. DO NOT EDIT */\n",
      "/* Generated from $file */\n\n",
      "#include \"registryCommon.h\"\n\n",
      "extern \"C\" {\n";

#definitions for recordtype
if($numberRecordType>0) {
    for ($i=0; $i<$numberRecordType; $i++) {
        print "epicsShareExtern rset *pvar_rset_$recordType[$i]RSET;\n";
        print "epicsShareExtern int (*pvar_func_$recordType[$i]RecordSizeOffset)(dbRecordType *pdbRecordType);\n"
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
        print "    {pvar_rset_$recordType[$i]RSET, pvar_func_$recordType[$i]RecordSizeOffset}";
        if($i < $numberRecordType-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
}

#definitions for device
if($numberDeviceSupport>0) {
    for ($i=0; $i<$numberDeviceSupport; $i++) {
        print "epicsShareExtern dset *pvar_dset_$deviceSupport[$i];\n";
    }
    print "\nstatic const char * const deviceSupportNames[$numberDeviceSupport] = {\n";
    for ($i=0; $i<$numberDeviceSupport; $i++) {
        print "    \"$deviceSupport[$i]\"";
        if($i < $numberDeviceSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";

    print "static const dset * const devsl[$i] = {\n";
    for ($i=0; $i<$numberDeviceSupport; $i++) {
        print "    pvar_dset_$deviceSupport[$i]";
        if($i < $numberDeviceSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
}

#definitions for driver
if($numberDriverSupport>0) {
    for ($i=0; $i<$numberDriverSupport; $i++) {
        print "epicsShareExtern drvet *pvar_drvet_$driverSupport[$i];\n";
    }
    print "\nstatic const char *driverSupportNames[$numberDriverSupport] = {\n";
    for ($i=0; $i<$numberDriverSupport; $i++) {
        print "    \"$driverSupport[$i]\"";
        if($i < $numberDriverSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
    
    print "static struct drvet *drvsl[$i] = {\n";
    for ($i=0; $i<$numberDriverSupport; $i++) {
        print "    pvar_drvet_$driverSupport[$i]";
        if($i < $numberDriverSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
}

#definitions registrar
if(@registrars) {
    foreach $reg (@registrars) {
	print "epicsShareExtern void (*pvar_func_$reg)(void);\n";
    }
    print "\n";
}

if (@variables) {
    foreach $var (@variables) {
        print "epicsShareExtern $varType{$var} *pvar_$varType{$var}_$var;\n";
    }
    %iocshTypes = (
        'int' => 'iocshArgInt',
        'double' => 'iocshArgDouble'
    );
    print "static struct iocshVarDef vardefs[] = {\n";
    foreach $var (@variables) {
        $argType = $iocshTypes{$varType{$var}};
        die "Unknown variable type $varType{$var} for variable $var"
            unless $argType;
        print "\t{\"$var\", $argType, (void * const)pvar_$varType{$var}_$var},\n";
    }
    print "\t{NULL, iocshArgInt, NULL}\n};\n\n";
}

#Now actual registration code.

print "int $subname(DBBASE *pbase)\n{\n";

if($numberRecordType>0) {
    print "    registerRecordTypes(pbase, $numberRecordType, ",
				  "recordTypeNames, rtl);\n";
}
if($numberDeviceSupport>0) {
    print "    registerDevices(pbase, $numberDeviceSupport, ",
				  "deviceSupportNames, devsl);\n";
}
if($numberDriverSupport>0) {
    print "    registerDrivers(pbase, $numberDriverSupport, ",
				  "driverSupportNames, drvsl);\n";
}
foreach $reg (@registrars) {
    print "    (*pvar_func_$reg)();\n";
}

if (@variables) {
    print "    iocshRegisterVariable(vardefs);\n";
}
print << "END" ;
    return 0;
}

/* registerRecordDeviceDriver */
static const iocshArg registerRecordDeviceDriverArg0 =
                                            {"pdbbase",iocshArgPdbbase};
static const iocshArg *registerRecordDeviceDriverArgs[1] =
                                            {&registerRecordDeviceDriverArg0};
static const iocshFuncDef registerRecordDeviceDriverFuncDef =
                {"$subname",1,registerRecordDeviceDriverArgs};
static void registerRecordDeviceDriverCallFunc(const iocshArgBuf *)
{
    $subname(pdbbase);
}

} // extern "C"
/*
 * Register commands on application startup
 */
#include "iocshRegisterCommon.h"
class IoccrfReg {
  public:
    IoccrfReg() {
        iocshRegisterCommon();
        iocshRegister(&registerRecordDeviceDriverFuncDef,registerRecordDeviceDriverCallFunc);
    }
};
#if !defined(__GNUC__) || !(__GNUC__<2 || (__GNUC__==2 && __GNUC_MINOR__<=95))
namespace { IoccrfReg iocshReg; }
#else
IoccrfReg iocshReg;
#endif
END
