# Example vxWorks startup file

# Following must be added for many board support packages
#cd <full path to target bin directory>

< cdCommands

#< nfsCommands

cd appbin
ld < iocCore
ld < seq
ld < exampleLib

cd startup
dbLoadDatabase("../../dbd/exampleApp.dbd")
dbLoadRecords("../../exampleApp/Db/dbExample.db","user=_USER_")
iocInit
seq &snctest
