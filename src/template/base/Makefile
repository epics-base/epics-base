TOP=../../..

include $(TOP)/configure/CONFIG

TEMPLATES_DIR = makeBaseApp

TEMPLATES += top/Makefile
TEMPLATES += top/configure/CONFIG
TEMPLATES += top/configure/CONFIG_SITE
TEMPLATES += top/configure/Makefile
TEMPLATES += top/configure/RELEASE
TEMPLATES += top/configure/RULES
TEMPLATES += top/configure/RULES.ioc
TEMPLATES += top/configure/RULES_DIRS
TEMPLATES += top/configure/RULES_TOP

TEMPLATES += top/supportApp/Makefile
TEMPLATES += top/supportApp/Db/Makefile
TEMPLATES += top/supportApp/src/Makefile
TEMPLATES += top/supportApp/src/_APPNAME_.dbd

SCRIPTS_HOST += makeBaseApp.pl

include $(TOP)/configure/RULES
