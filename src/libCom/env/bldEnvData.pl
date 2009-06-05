#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
#
# 	Author: Kay-Uwe Kasemir
# 	based on bldEnvData shell scripts, Andrew Johnson (RGO)
# 	Date:	1-30-97
# 
# 	Experimental Physics and Industrial Control System (EPICS)
# 
#	tool to build envData.c from envDefs.h and config/CONFIG*ENV

use Cwd 'abs_path';

#	We need exactly one argument:
$usage="Usage:\tbldEnvData <config-Directory>";
die $usage unless $#ARGV==0;

$config_dir      = abs_path($ARGV[0]);
$config_env      = "${config_dir}/CONFIG_ENV";
$config_site_env = "${config_dir}/CONFIG_SITE_ENV";

$env_dir    = abs_path("../env");
$env_defs   = "${env_dir}/envDefs.h";

$out_name   = "envData.c";

#	$tool = basename of this script
$tool=$0;
$tool=~ s'.*/'';


#	Start by extracting the ENV_PARAM declarations from $env_defs
#	i.e. gather the names of params we are interested in:
#
open SRC, "<$env_defs" or die "Cannot open $env_defs";
while (<SRC>) {
	if (m/epicsShareExtern\s+const\s+ENV_PARAM\s+([A-Za-z_]\w*)\s*;/) {
		$need_var{$1} = 1;
	}
}
close SRC;


# Read the default values from the config file into shell variables
sub GetVars {
	my ($filename) = @_;
	open IN, "<$filename" or die "Cannot read $filename";
	while (<IN>) {
		# Discard comments, carriage returns and trailing whitespace
		next if m/^ \s* \#/x;
		chomp;
		if (m/^ \s* ([A-Za-z_]\w*) \s* = \s* ( \S* | ".*" ) \s* $/x) {
			my ($var, $val) = ($1, $2);
			next unless $need_var{$var};
			$val =~ s/^"(.*)"$/$1/;
			$value{$var} = $val;
		}
	}
	close IN;
}

GetVars ($config_env);
GetVars ($config_site_env);

#	Generate header file
#

print "Generating $out_name\n";

open OUT, ">$out_name" or die "cannot create $out_name";

#	Write header
print OUT "/* $out_name\n",
	  " *\n",
	  " * Created " . localtime() . "\n",
	  " * by $tool from files:\n",
	  " *\t$env_defs\n",
	  " *\t$config_env\n",
	  " *\t$config_site_env\n",
	  " */\n",
	  "\n",
	  "#define epicsExportSharedSymbols\n",
	  "#include \"envDefs.h\"\n",
	  "\n";


#	Print variables
#
@vars = sort keys %need_var;
foreach $var (@vars) {
	$default = exists $value{$var} ? $value{$var} : "";
	print "Warning: No default value found for $var\n"
	    unless exists $value{$var};

	print OUT "epicsShareDef const ENV_PARAM $var =\n",
		  "\t{\"$var\", \"$default\"};\n";
}

# Now create an array pointing to all parameters

print OUT "\n",
	  "epicsShareDef const ENV_PARAM* env_param_list[] = {\n";

# Contents are the addresses of each parameter
foreach $var (@vars) {
	print OUT "\t&$var,\n";
}

# Finally finish list with 0
print OUT "\t0\n",
	  "};\n",
	  "\n",
	  "/*\tEOF $out_name */\n";

close OUT;

#	EOF bldEnvData.pl
