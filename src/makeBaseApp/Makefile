TOP=../..

include $(TOP)/configure/CONFIG

TEMPLATES_DIR = makeBaseApp

TEMPLATES += top/Makefile
TEMPLATES += top/configure/CONFIG
TEMPLATES += top/configure/CONFIG_APP
TEMPLATES += top/configure/Makefile
TEMPLATES += top/configure/RELEASE
TEMPLATES += top/configure/RULES
TEMPLATES += top/configure/RULES.ioc
TEMPLATES += top/configure/RULES.iocBoot
TEMPLATES += top/configure/RULES_DIRS
TEMPLATES += top/configure/RULES_TOP

TEMPLATES += top/exampleApp/Makefile
TEMPLATES += top/exampleApp/Db/Makefile
TEMPLATES += top/exampleApp/Db/dbExample1.db
TEMPLATES += top/exampleApp/Db/dbExample2.db
TEMPLATES += top/exampleApp/Db/dbSubExample.db
TEMPLATES += top/exampleApp/src/Makefile
TEMPLATES += top/exampleApp/src/base.dbd
TEMPLATES += top/exampleApp/src/devXxxSoft.c
TEMPLATES += top/exampleApp/src/xxxRecord.c
TEMPLATES += top/exampleApp/src/caExample.c
TEMPLATES += top/exampleApp/src/caMonitor.c
TEMPLATES += top/exampleApp/src/exampleInclude.dbd
TEMPLATES += top/exampleApp/src/sncExample.stt
TEMPLATES += top/exampleApp/src/xxxRecord.dbd
TEMPLATES += top/exampleApp/src/exampleMain.cpp
TEMPLATES += top/exampleApp/src/dbSubExample.c

TEMPLATES += top/exampleBoot/Makefile
TEMPLATES += top/exampleBoot/nfsCommands@vxWorks
TEMPLATES += top/exampleBoot/ioc/Makefile@Common
TEMPLATES += top/exampleBoot/ioc/Makefile@vxWorks
TEMPLATES += top/exampleBoot/ioc/st.cmd@Common
TEMPLATES += top/exampleBoot/ioc/st.cmd@vxWorks
TEMPLATES += top/exampleBoot/ioc/st.cmd@RTEMS
TEMPLATES += top/exampleBoot/ioc/README@Common
TEMPLATES += top/exampleBoot/ioc/README@vxWorks

TEMPLATES += top/simpleApp/Makefile
TEMPLATES += top/simpleApp/Db/Makefile
TEMPLATES += top/simpleApp/src/Makefile

TEMPLATES += top/simpleBoot/Makefile
TEMPLATES += top/simpleBoot/nfsCommands@vxWorks
TEMPLATES += top/simpleBoot/ioc/Makefile@Common
TEMPLATES += top/simpleBoot/ioc/Makefile@vxWorks
TEMPLATES += top/simpleBoot/ioc/st.cmd@Common
TEMPLATES += top/simpleBoot/ioc/st.cmd@vxWorks
TEMPLATES += top/simpleBoot/ioc/st.cmd@RTEMS

SCRIPTS_HOST += makeBaseApp.pl

include $(TOP)/configure/RULES

