TOP=../../..
 
include $(TOP)/configure/CONFIG
 
HOST_WARN=NO
 
YACC = $(EYACC)
YACCOPT = -l -d

SKELETON_FILE=include/flex.skel.static

#	ARCH_CLASS hp700 needs special flags,
#	everything else uses DEFAULT:
#
USR_CPPFLAGS_DEFAULT := -DUSG
USR_CPPFLAGS_hp700 := -DSCO_UNIX

#	This applies to all arch.classes:
#
ifndef BORLANDC
USR_CPPFLAGS = -DDEFAULT_SKELETON_FILE=\"$(SKELETON_FILE)\"
endif

INC += flex.skel.static

# main.c is included in parse.c
#
SRCS += ccl.c
SRCS += dfa.c
SRCS += ecs.c
SRCS += gen.c
SRCS += misc.c
SRCS += nfa.c
SRCS += sym.c
SRCS += tblcmp.c
SRCS += parse.c

PROD_HOST = e_flex

include $(TOP)/configure/RULES

clean::
	$(RM) parse.c parse.h

depends: $(INSTALL_PROD)

#	EOF Makefile.Host for base/src/toolsComm/flex
