# Example vxWorks startup file

# Following must be added for many board support packages
#cd <full path to target bin directory>

< cdCommands

#< ../nfsCommands

#The following sets timezone properly on vxWorks
#YOU MUST enter correct values for name,minutesWest,start daylight,end daylight
#putenv("TIMEZONE=<name>::<minutesWest>:<start daylight>:<end daylight>")
#For example
#putenv("TIMEZONE=US/Central::360:040102:100102")

cd topbin
ld < exampleLibrary.munch

#The following uses drvTS for vxWorks
#ld < <base>/bin/vxWorks-<arch>/drvTS.o
#TSinit

cd top
dbLoadDatabase("dbd/example.dbd")
registerRecordDeviceDriver(pdbbase)
dbLoadRecords("db/dbExample1.db","user=_USER_")
dbLoadRecords("db/dbExample2.db","user=_USER_,no=1,scan=1 second")
dbLoadRecords("db/dbExample2.db","user=_USER_,no=2,scan=2 second")
dbLoadRecords("db/dbExample2.db","user=_USER_,no=3,scan=5 second")

cd startup
iocInit
#seq &snctest,"user=_USER_"
