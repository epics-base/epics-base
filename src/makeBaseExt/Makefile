TOP=../..

include $(TOP)/configure/CONFIG

TEMPLATES_DIR = makeBaseExt

TEMPLATES += top/Makefile
TEMPLATES += top/README
TEMPLATES += top/configure/CONFIG
TEMPLATES += top/configure/CONFIG_EXTENSIONS
TEMPLATES += top/configure/Makefile
TEMPLATES += top/configure/RELEASE
TEMPLATES += top/configure/RULES
TEMPLATES += top/configure/RULES_DIRS
TEMPLATES += top/configure/RULES_TOP

TEMPLATES += $(subst ../,,$(wildcard ../top/configure/os/CONFIG*))

TEMPLATES += top/src/Makefile

TEMPLATES += top/exampleExt/Makefile
TEMPLATES += top/exampleExt/caExample.c

TEMPLATES += top/simpleExt/Makefile

SCRIPTS += makeBaseExt.pl

include $(TOP)/configure/RULES
