#!/usr/bin/perl
#makeBaseApp 

use Cwd;
use Getopt::Std;
use File::Copy;

$user=GetUser();

#get options and check for valid combinations
unless($len = @ARGV) { Cleanup(1);}
unless(getopts('eib:a:')) {Cleanup(1,"Illegal option");}
if($opt_e) {
    if($len = @ARGV) { Cleanup(1,"no arguments allowed with -e");}
    @ARGV = ("example");
}
if(!("opt_i" || "opt_e") && "opt_a") {
    Cleanup(1,"-a not valid when generating xxxApp only");
}

#locate epics_base
if($opt_b) { #first choice is -b base
    $epics_base = $opt_b;
} elsif(-r "config/RELEASE") { #second choice is config/RELEASE
    open(IN,"config/RELEASE") or die "Cannot open config/RELEASE";
    while ($line = <IN>) {
	chomp($line);
	if($line =~ /EPICS_BASE/) {
            $line =~ s/EPICS_BASE=//;
	    $epics_base = $line;
	    break;
	}
    }
    close IN;
} else { #assume script was called with full path to base
    $epics_base = $0;
    $epics_base =~ s:(/.*)/bin/.*makeBaseApp.*:$1:;
}
unless("$epics_base") {Cleanup (1,"Cannot find EPICS base");}

$top = $epics_base . "/templates/makeBaseApp/top";

#copy <top>/Makefile and config files if not present
unless (-f 'Makefile')
{
    copy("${top}/Makefile","Makefile") or die "$! Copying ${top}/Makefile";
}
unless (-d 'config')
{
    mkdir('config', 0777) or die "Cannot create config directory";
    foreach $fullname (glob("${top}/config/*")) {
	$name = GetFilename($fullname);
	if($name eq "RELEASE") {#must substitute for epics_base
	    open(IN,$fullname) or die "Cannot open $fullname";
	    open OUT, ">config/RELEASE" or die "Cannot create RELEASE";
	    while ($line =<IN>) {
		$line =~ s/\${epics_base}/$epics_base/;
		print OUT $line;
	    }
	    close OUT;
	    close IN;
	} else {
	    copy("$fullname","config/$name") or die "$! Copying $fullname";
	}
    }
}

if($opt_i || $opt_e) { #Create ioc directories
    unless(-d "iocBoot") {
        mkdir("iocBoot", 0777) or die "Cannot create iocBoot directory";
	$fromDir = "$top/iocBoot"; $toDir = "iocBoot";
	foreach $file ("Makefile","nfsCommands") {
	    copy("$fromDir/$file","$toDir/$file")
		or die "$! Copying $fromDir/$file";
	}
    }
    @IOC = @ARGV;
    foreach $ioc ( @IOC ) {
	$ioc =~ s/^ioc//;
	$ioc = "ioc" . $ioc;
        if (-d "iocBoot/$ioc") {
	    print "ioc iocBoot/$ioc is already there!\n";
	    next;
        }
        mkdir("iocBoot/$ioc", 0777) or die "Cannot create dir iocBoot/$ioc";
	$fromDir = "$top/iocBoot/ioc"; $toDir = "iocBoot/$ioc";
	if($opt_a) {
	    $arch = $opt_a;
	} else {
	    print "What architecture do you want to use for your IOC,";
	    print "e.g. pc486, mv167 ? ";
	    $arch=<STDIN>;
	    chomp($arch);
	}
        open(IN,"${fromDir}/Makefile") or die "Cannot open ${fromDir}/Makefile";
        open OUT, ">${toDir}/Makefile" or die "Cannot create ${toDir}/Makefile";
        while ($line = <IN>) {
	    $line =~ s/\${arch}/${arch}/;
	    print OUT $line;
        }
        close OUT;
        close IN;
	if($opt_e) {
            open(IN,"${fromDir}/st.cmdExample")
	        or die "Cannot open ${fromDir}/st.cmdExample";
            open OUT, ">${toDir}/st.cmd"
	        or die "Cannot create ${toDir}/st.cmd";
            while ($line = <IN>) {
	        $line =~ s/\${user}/${user}/;
	        print OUT $line;
            }
            close OUT;
            close IN;
	} else {
	    copy("$fromDir/st.cmd","$toDir/st.cmd")
	        or die "$! Copying $fromDir/st.cmd";
	}
    }
    if($opt_i) {exit 0;} #If -i was specified dont generate any xxxApps
}

#	Generate app dirs (if any names given)
foreach $app ( @ARGV) {
    $appname = $app . "App";
    if (-d "$appname") {
	print "App $appname is already there!\n";
	next;
    }
    mkdir("$appname", 0777) or die "Cannot create dir $appname";
    copy("$top/App/Makefile","$appname/Makefile")
	or die "$! Copying $top/App/Makefile";
    mkdir("${appname}/src", 0777) or die "Cannot create dir ${appname}/src";
    $fromDir = "$epics_base/dbd"; $toDir = "${appname}/src";
    foreach $file ("base.dbd","baseLIBOBJS") {
	copy("$fromDir/$file","$toDir/$file")
	    or die "$! Copying $fromDir/$file";
    }
    $fromDir = "$top/App/src"; $toDir = "${appname}/src";
    copy("$fromDir/Makefile","$toDir/Makefile")
	or die "$! Copying $fromDir/Makefile";
    if($opt_e) {$example = "Example";} else {$example = "";}
    foreach $ext ("Host","Vx") {
	copy("$fromDir/Makefile${example}.$ext","${toDir}/Makefile.$ext")
	    or die "$! Copying $fromDir/Makefile${example}.$extension";
    }
    if($opt_e) {
	@filelist = (glob("${fromDir}/*.c"));
	@filelist = (@filelist,glob("${fromDir}/*.dbd"));
	@filelist = (@filelist,glob("${fromDir}/*.st"));
	foreach $fullname (@filelist) {
	    $name = GetFilename($fullname);
	    if($name eq "sncExample.st") {
		open(IN,"${fromDir}/sncExample.st")
		    or die "Cannot open ${fromDir}/sncExample.st";
		open(OUT, ">${toDir}/sncExample.st")
		    or die "Cannot open ${toDir}/sncExample.st";
		while ($line =<IN>) {
		    $line =~ s/\${user}/${user}/;
		    print OUT $line;
		}
		close OUT;
		close IN;
	    } else {
	        copy("${fromDir}/${name}","${toDir}/${name}")
	    	    or die "$! \nCopying ${fromDir}/${name}\n";
	    }
	}
    }
    mkdir("${appname}/Db", 0777) or die "Cannot create dir ${appname}/Db";
    $fromDir = "$top/App/Db"; $toDir = "${appname}/Db";
    copy("${fromDir}/Makefile","${toDir}/Makefile")
	or die "$! Copying ${fromDir}/Makefile";
    if ($opt_e) {
        copy("${fromDir}/dbExample.db","${toDir}/dbExample.db")
	    or die "$! Copying ${fromDir}/dbExample.db";
    }
}

#	Cleanup (return-code [ messsage-line1, line 2, ... ])
sub Cleanup
{
    my ($rtncode, @message) = @_;

    foreach $line ( @message )
    {
	print "$line\n";
    }

    print "\nUsage:\n\n",
	"$0 -e\n",
	"      or create ioc directories\n",
	"$0 -i [-b base] [-a arch] ioc ...\n",
	"      or create App directories\n",
	"$0 [-b base] app ...\n",
	"\n",
	"where\n\n",
	"-e  Create an example app. No other arguments can be given\n",
	"-i  Create ioc directories only. Each argument is an iocname\n",
	"    If -i is not specified App directories are created\n",
	"-b  The location of epics base (full path name).\n",
	"    If not specified and config exists it is found in config/RELEASE\n",
	"    If config does not exist it is taken from command\n",
	"-a  The architecture, e.g. mv167.\n",
	"    If not given you will be prompted\n";

    exit $rtncode;
}

sub GetFilename # full name including path
{
    my ($name) = @_;
    $name =~ s:.*/(.*):$1: ;
    return($name);
}

sub GetUser # no args
{
    my ($user);

    # add to this list if new possibilities arise,
    # currently it's UNIX and WIN32:
    $user=$ENV{USER} || $ENV{USERNAME} || Win32::LoginName();

    unless ($user)
    {
	print "I cannot figure out your user name.\n";
	print "What shall you be called ?\n";
	print ">";
	$user = <STDIN>;
	chomp $user;
    }
    die "No user name" unless $user;
    return $user;
}
