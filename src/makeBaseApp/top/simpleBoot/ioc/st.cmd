# Example vxWorks startup file
#Following must be added for many board support packages
#cd <full path to target bin directory>

< cdcmds
 
#< nfsCommands

cd appbin
ld < iocCore
ld < seq
#ld < <some>Lib

cd startup
#dbLoadDatabase("../../dbd/<some>App.dbd")
#dbLoadRecords("../../<some>App/Db/dbExample.db")
iocInit
#seq &<some snc program>
