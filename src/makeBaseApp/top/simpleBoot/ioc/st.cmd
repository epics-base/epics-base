# Example vxWorks startup file
#Following must be added for many board support packages
#cd <full path to target bin directory>

< cdCommands
 
#< ../nfsCommands

cd appbin
ld < iocCore
ld < seq
#ld < _APPNAME_Lib

cd startup
#dbLoadDatabase("../../dbd/_APPNAME_.dbd")
#dbLoadRecords("../../db/_APPNAME_.db")
#dbLoadTemplate("../../db/_APPNAME_.substitutions")

iocInit
#seq &<some snc program>
