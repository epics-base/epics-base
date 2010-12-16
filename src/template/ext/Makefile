TOP=../..

include $(TOP)/configure/CONFIG

TEMPLATES_DIR = makeBaseExt

TEMPLATES += top/Makefile
TEMPLATES += top/README
TEMPLATES += top/configure/CONFIG
TEMPLATES += top/configure/CONFIG_SITE
TEMPLATES += top/configure/Makefile
TEMPLATES += top/configure/RELEASE
TEMPLATES += top/configure/RULES
TEMPLATES += top/configure/RULES_DIRS
TEMPLATES += top/configure/RULES_TOP
TEMPLATES += top/configure/RULES_PYTHON
TEMPLATES += top/configure/RULES_IDL

TEMPLATES += $(subst ../,,$(wildcard ../top/configure/os/CONFIG*))

TEMPLATES += top/src/Makefile

TEMPLATES += top/exampleExt/Makefile
TEMPLATES += top/exampleExt/caExample.c
TEMPLATES += top/exampleExt/RELEASE_NOTES.HTM

TEMPLATES += top/simpleExt/Makefile

SCRIPTS_HOST += makeBaseExt.pl

include $(TOP)/configure/RULES

