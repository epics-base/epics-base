#!/usr/bin/perl
#
# 	Author: Kay-Uwe Kasemir
# 	based on bldEnvData shell scripts, Andrew Johnson (RGO)
# 	Date:	1-30-97
# 
# 	Experimental Physics and Industrial Control System (EPICS)
# 
#	tool to build envData.c from envDefs.h and config/CONFIG*ENV

use Cwd;

#	We need exactly one argument:
$usage="Usage:\tbldEnvData <config-Directory>";
die $usage unless $#ARGV==0;

$start_dir=cwd();
$config_dir=$ARGV[0];

#	Don't see a reason for this directory hopping,
#	it's copied from the original:
chdir $config_dir or die "cannot change dir to $config_dir";
$config_dir=cwd();
chdir $start_dir;

$SRC      = "../envDefs.h";
$env_data = "${config_dir}/CONFIG_ENV";
$site_data= "${config_dir}/CONFIG_SITE_ENV";
$out_name = "envData.c";
$OUT      = "> $out_name";

#	$tool = basename of this script
$tool=$0;
$tool=~ s'.*/'';


#	Start by extracting the ENV_PARAM declarations from $SRC
#	i.e. gather the names of params we are interested in:
#
open SRC or die "Cannot open $SRC";
while (<SRC>)
{
	if (m'epicsShareExtern[ \t]+READONLY[ \t]+ENV_PARAM[ \t]+([A-Za-z_]+)[ \t;]*')
	{
		$need_var{$1} = 1;
	}
}
close SRC;



# Read the default values from the config file into shell variables
sub GetVars
{
	my ($filename) = @_;

	open IN, $filename or die "Cannot read $filename";
	while (<IN>)
	{
		#      word       space = space rest
		if (m'([A-Za-z_]+)[ \t]*=[ \t]*(.*)')
		{
			$var = $1;
			# Check if we need that variable:
			next unless $need_var{$var};

			# cosmetics:
			# Some vars are given as "",
			# so that $value{$var} is empty (=undefined).
			# To avoid "no value for .." warning I use %have_value
			$have_value{$var} = 1;
			$value{$var} = $2;
			# remove '"'
			if ($value{$var} =~ m'"(.*)"')
			{
				$value{$var} = $1;
			}
		}
	}
	close IN;
}

GetVars ($env_data);
GetVars ($site_data);

#	Generate header file
#

print "Generating $out_name\n";

open OUT or die "cannot create $out_name";

#	Write header
print OUT "/*\t$out_name\n";
print OUT " *\n";
print OUT " *\tcreated by $tool\n";
print OUT " *\n";
print OUT " *\tfrom:\n";
print OUT " *\t$SRC\n";
print OUT " *\t$env_data\n";
print OUT " *\t$site_data\n";
print OUT " *\n";
print OUT " *\t" . localtime() . "\n";
print OUT " *\n";
print OUT " */\n";
print OUT "\n";
print OUT "#define epicsExportSharedSymbols\n";
print OUT "#include \"envDefs.h\"\n";
print OUT "\n";


#	Print variables
#
foreach $var ( sort keys %need_var )
{
	if ($have_value{$var})
	{
		$default = $value{$var};
	}
	else
	{
		$default = "";
		print "Cannot find value for $var\n";
	}

	printf OUT "epicsShareDef READONLY ENV_PARAM %s = { \"%s\", \"%s\" };\n",
		$var, $var, $default;
}

# Now create an array pointing to all parameters

print OUT "\n";
print OUT "epicsShareDef READONLY ENV_PARAM* env_param_list[EPICS_ENV_VARIABLE_COUNT+1] =\n";
print OUT "{\n";

# Contents are the addresses of each parameter
foreach $var ( sort keys %need_var )
{
	print OUT "\t&$var,\n";
}

# Finally finish list with 0
print OUT "\t0\n";
print OUT "};\n";
print OUT "\n";
print OUT "/*\tEOF $out_name */\n";

close OUT;

#	EOF bldEnvData.pl
