eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
	if $running_under_some_shell; # makeMakefileInclude.pl
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# $Id$
#
#  Author: Janet Anderson
# 

sub Usage
{
    my ($txt) = @_;

    print "Usage:\n";
    print "\makeMakefileInclude name outfile\n";
    print "\tcp name [ name2 name3 ...] outfile\n";
    print "\nError: $txt\n" if $txt;

    exit 2;
}

# need at least two args: ARGV[0] and ARGV[1]
Usage("need more args") if $#ARGV < 1;

$outfile=$ARGV[$#ARGV];
@nameList=@ARGV[0..$#ARGV-1];

unlink("${outfile}");
open(OUT,">${outfile}") or die "$! opening ${outfile}";
print OUT "#Do not modify this file.\n";
print OUT "#This file is created during the build.\n";

foreach $name ( @nameList ) {
	print OUT "\n";
	print OUT "ifneq (\$(strip \$(${name}_SRCS_\$(OS_CLASS))),)\n";
	print OUT "${name}_SRCS+=\$(subst -nil-,,\$(${name}_SRCS_\$(OS_CLASS)))\n";
	print OUT "else\n";
	print OUT "ifdef ${name}_SRCS_DEFAULT\n";
	print OUT "${name}_SRCS+=\$(${name}_SRCS_DEFAULT)\n";
	print OUT "endif\n";
	print OUT "endif\n";
	print OUT "ifneq (\$(strip \$(${name}_RCS_\$(OS_CLASS))),)\n";
	print OUT "${name}_RCS+=\$(subst -nil-,,\$(${name}_RCS_\$(OS_CLASS)))\n";
	print OUT "else\n";
	print OUT "ifdef ${name}_RCS_DEFAULT\n";
	print OUT "${name}_RCS+=\$(${name}_RCS_DEFAULT)\n";
	print OUT "endif\n";
	print OUT "endif\n";
	print OUT "ifneq (\$(strip \$(${name}_OBJS_\$(OS_CLASS))),)\n";
	print OUT "${name}_OBJS+=\$(subst -nil-,,\$(${name}_OBJS_\$(OS_CLASS)))\n";
	print OUT "else\n";
	print OUT "ifdef ${name}_OBJS_DEFAULT\n";
	print OUT "${name}_OBJS+=\$(${name}_OBJS_DEFAULT)\n";
	print OUT "endif\n";
	print OUT "endif\n";
	print OUT "ifneq (\$(strip \$(${name}_LDFLAGS_\$(OS_CLASS))),)\n";
	print OUT "${name}_LDFLAGS+=\$(subst -nil-,,\$(${name}_LDFLAGS_\$(OS_CLASS)))\n";
	print OUT "else\n";
	print OUT "ifdef ${name}_LDFLAGS_DEFAULT\n";
	print OUT "${name}_LDFLAGS+=\$(${name}_LDFLAGS_DEFAULT)\n";
	print OUT "endif\n";
	print OUT "endif\n";
	print OUT "ifneq (\$(strip \$(${name}_OBJLIBS_\$(OS_CLASS))),)\n";
	print OUT "${name}_OBJLIBS+=\$(subst -nil-,,\$(${name}_OBJLIBS_\$(OS_CLASS)))\n";
	print OUT "else\n";
	print OUT "ifdef ${name}_OBJLIBS_DEFAULT\n";
	print OUT "${name}_OBJLIBS+=\$(${name}_OBJLIBS_DEFAULT)\n";
	print OUT "endif\n";
	print OUT "endif\n";
	print OUT "ifneq (\$(strip \$(${name}_LDOBJS_\$(OS_CLASS))),)\n";
	print OUT "${name}_LDOBJS+=\$(subst -nil-,,\$(${name}_LDOBJS_\$(OS_CLASS)))\n";
	print OUT "else\n";
	print OUT "ifdef ${name}_LDOBJS_DEFAULT\n";
	print OUT "${name}_LDOBJS+=\$(${name}_LDOBJS_DEFAULT)\n";
	print OUT "endif\n";
	print OUT "endif\n";
	print OUT "${name}_LDLIBS+=\$(${name}_LIBS)\n";
	print OUT "ifneq (\$(strip \$(${name}_LIBS_\$(OS_CLASS))),)\n";
	print OUT "${name}_LDLIBS+=\$(subst -nil-,,\$(${name}_LIBS_\$(OS_CLASS)))\n";
	print OUT "else\n";
	print OUT "ifdef ${name}_LIBS_DEFAULT\n";
	print OUT "${name}_LDLIBS+=\$(${name}_LIBS_DEFAULT)\n";
	print OUT "endif\n";
	print OUT "endif\n";
	print OUT "ifneq (\$(strip \$(${name}_SYS_LIBS_\$(OS_CLASS))),)\n";
	print OUT "${name}_SYS_LIBS+=\$(subst -nil-,,\$(${name}_SYS_LIBS_\$(OS_CLASS)))\n";
	print OUT "else\n";
	print OUT "ifdef ${name}_SYS_LIBS_DEFAULT\n";
	print OUT "${name}_SYS_LIBS+=\$(${name}_SYS_LIBS_DEFAULT)\n";
	print OUT "endif\n";
	print OUT "endif\n";
	print OUT "${name}_OBJS+=\$(addsuffix \$(OBJ),\$(basename \$(${name}_SRCS)))\n";
	print OUT "\n";
	print OUT "ifeq (\$(filter ${name},\$(TESTPROD) \$(PROD)),${name})\n";
	print OUT "ifeq (,\$(strip \$(${name}_OBJS) \$(PRODUCT_OBJS)))\n";
	print OUT "${name}_OBJS+=${name}\$(OBJ)\n";
	print OUT "endif\n";
	print OUT "${name}_RESS+=\$(addsuffix \$(RES),\$(basename \$(${name}_RCS)))\n";
	print OUT "${name}_OBJSNAME+=\$(addsuffix \$(OBJ),\$(basename \$(${name}_OBJS)))\n";
	print OUT "${name}_DEPLIBS=\$(foreach lib, \$(${name}_LDLIBS),\\\n";
	print OUT " \$(firstword \$(wildcard \$(addsuffix /\$(LIB_PREFIX)\$(lib).\*,\\\n";
	print OUT " \$(\$(lib)_DIR) \$(SHRLIB_SEARCH_DIRS)))\\\n";
	print OUT " \$(addsuffix /\$(LIB_PREFIX)\$(lib)\$(LIB_SUFFIX),\\\n";
	print OUT " \$(firstword \$(\$(lib)_DIR) \$(SHRLIB_SEARCH_DIRS)))))\n";
	print OUT "${name}\$(EXE): \$(${name}_OBJSNAME) \$(${name}_RESS) \$(${name}_DEPLIBS)\n";
	print OUT "endif\n";
	print OUT "\n";
	print OUT "ifeq (\$(filter ${name},\$(LIBRARY)),${name})\n";
	print OUT "ifneq (\$(filter ${name},\$(LOADABLE_LIBRARY)),${name})\n";
	print OUT "ifneq (,\$(strip \$(${name}_OBJS) \$(LIBRARY_OBJS)))\n";
	print OUT "BUILD_LIBRARY += ${name}\n";
	print OUT "endif\n";
	print OUT "${name}_RESS+=\$(addsuffix \$(RES),\$(basename \$(${name}_RCS)))\n";
	print OUT "${name}_OBJSNAME+=\$(addsuffix \$(OBJ),\$(basename \$(${name}_OBJS)))\n";
	print OUT "${name}_DEPLIBS=\$(foreach lib, \$(${name}_LDLIBS),\\\n";
	print OUT " \$(firstword \$(wildcard \$(addsuffix /\$(LIB_PREFIX)\$(lib).\*,\\\n";
	print OUT " \$(\$(lib)_DIR) \$(SHRLIB_SEARCH_DIRS)))\\\n";
	print OUT " \$(addsuffix /\$(LIB_PREFIX)\$(lib)\$(LIB_SUFFIX),\\\n";
	print OUT " \$(firstword \$(\$(lib)_DIR) \$(SHRLIB_SEARCH_DIRS)))))\n";
	print OUT "${name}_DLL_DEPLIBS=\$(foreach lib, \$(${name}_DLL_LIBS),\\\n";
	print OUT " \$(firstword \$(wildcard \$(addsuffix /\$(LIB_PREFIX)\$(lib).\*,\\\n";
	print OUT " \$(\$(lib)_DIR) \$(SHRLIB_SEARCH_DIRS)))\\\n";
	print OUT " \$(addsuffix /\$(LIB_PREFIX)\$(lib)\$(LIB_SUFFIX),\\\n";
	print OUT " \$(firstword \$(\$(lib)_DIR) \$(SHRLIB_SEARCH_DIRS)))))\n";
	print OUT "\$(LIB_PREFIX)${name}\$(LIB_SUFFIX):\$(${name}_OBJSNAME) \$(${name}_RESS)\n";
	print OUT "\$(LIB_PREFIX)${name}\$(LIB_SUFFIX):\$(${name}_DEPLIBS)\n";
	print OUT "\$(LIB_PREFIX)${name}\$(SHRLIB_SUFFIX):\$(${name}_OBJSNAME) \$(${name}_RESS)\n";
	print OUT "\$(LIB_PREFIX)${name}\$(SHRLIB_SUFFIX):\$(${name}_DEPLIBS)\n";
	print OUT "\$(LIB_PREFIX)${name}\$(SHRLIB_SUFFIX):\$(${name}_DLL_DEPLIBS)\n";
	print OUT "endif\n";
	print OUT "endif\n";
	print OUT "ifeq (\$(filter ${name},\$(LOADABLE_LIBRARY)),${name})\n";
	print OUT "ifneq (,\$(strip \$(${name}_OBJS) \$(LIBRARY_OBJS)))\n";
	print OUT "LOADABLE_BUILD_LIBRARY += ${name}\n";
	print OUT "endif\n";
	print OUT "${name}_RESS+=\$(addsuffix \$(RES),\$(basename \$(${name}_RCS)))\n";
	print OUT "${name}_OBJSNAME+=\$(addsuffix \$(OBJ),\$(basename \$(${name}_OBJS)))\n";
	print OUT "${name}_DEPLIBS=\$(foreach lib, \$(${name}_LDLIBS),\\\n";
	print OUT " \$(firstword \$(wildcard \$(addsuffix /\$(LIB_PREFIX)\$(lib).\*,\\\n";
	print OUT " \$(\$(lib)_DIR) \$(SHRLIB_SEARCH_DIRS)))\\\n";
	print OUT " \$(addsuffix /\$(LIB_PREFIX)\$(lib)\$(LIB_SUFFIX),\\\n";
	print OUT " \$(firstword \$(\$(lib)_DIR) \$(SHRLIB_SEARCH_DIRS)))))\n";
	print OUT "${name}_DLL_DEPLIBS=\$(foreach lib, \$(${name}_DLL_LIBS),\\\n";
	print OUT " \$(firstword \$(wildcard \$(addsuffix /\$(LIB_PREFIX)\$(lib).\*,\\\n";
	print OUT " \$(\$(lib)_DIR) \$(SHRLIB_SEARCH_DIRS)))\\\n";
	print OUT " \$(addsuffix /\$(LIB_PREFIX)\$(lib)\$(LIB_SUFFIX),\\\n";
	print OUT " \$(firstword \$(\$(lib)_DIR) \$(SHRLIB_SEARCH_DIRS)))))\n";
	print OUT "\$(LOADABLE_SHRLIB_PREFIX)${name}\$(LOADABLE_SHRLIB_SUFFIX):\$(${name}_OBJSNAME) \$(${name}_RESS)\n";
	print OUT "\$(LOADABLE_SHRLIB_PREFIX)${name}\$(LOADABLE_SHRLIB_SUFFIX):\$(${name}_DEPLIBS)\n";
	print OUT "\$(LOADABLE_SHRLIB_PREFIX)${name}\$(LOADABLE_SHRLIB_SUFFIX):\$(${name}_DLL_DEPLIBS)\n";
	print OUT "endif\n";
	print OUT "\n";
}
close OUT or die "Cannot close $outfile: $!";

