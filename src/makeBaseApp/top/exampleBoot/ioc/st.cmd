# Example vxWorks startup file

# Following must be added for many board support packages
#cd <full path to target bin directory>

< cdCommands

#< ../nfsCommands

cd appbin
ld < iocCore
ld < seq
ld < exampleLib

cd startup
dbLoadDatabase("../../dbd/exampleApp.dbd")
dbLoadRecords("../../db/dbExample1.db","user=_USER_")
dbLoadRecords("../../db/dbExample2.db")

iocInit
seq &snctest
