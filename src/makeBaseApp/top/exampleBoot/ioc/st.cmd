# Example vxWorks startup file

# Following must be added for many board support packages
#cd <full path to target bin directory>

< cdCommands

#< ../nfsCommands

cd appbin
ld < exampleLib

cd startup
dbLoadDatabase("../../dbd/exampleApp.dbd")
dbLoadRecords("../../db/dbExample1.db","user=mrk")
dbLoadRecords("../../db/dbExample2.db","user=mrk,no=1,scan=1 second")
dbLoadRecords("../../db/dbExample2.db","user=mrk,no=2,scan=2 second")
dbLoadRecords("../../db/dbExample2.db","user=mrk,no=3,scan=5 second")

iocInit
#seq &snctest
