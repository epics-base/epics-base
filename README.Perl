Perl
====

A number of shell scripts are now Perl scripts,
most important:

tools/installEpics.pl
include/makeEpicsVersion.pl
libCom/bldEnvData.pl
libCom/makeStatSymTbl.pl
sequencer/makeVersion.pl

WIN32 also uses
tools/cp.pl, mv.pl, rm.pl, mkdir.pl
which should behave like the UNIX cp, mv, rm, rmdir.
All systems could use the Perl versions but that seems
to be overkill for UNIX.

Advantages:
* no need for ls, date, basename, grep, awk, sed, ...,
  which made the make UNIX-specific.
* Perl is currently available for Unix, OS/2, WIN32, VMS
  and the scripts should work on all those systems

Disadvantage:
* You have to get Perl, look e.g. at http://www.perl.com
* Perl version 5.003_02 works, we don't know about earlier versions.


Compile Perl:
No big deal,
* on UNIX say 'configure' and 'gnumake',
* on Windows use MX Visual C++,
  use File/Open Workspace to open the Makefile {PERL}/perl5i.mak,
  use Build/Batch build to select eiher the Release or Debug targets.

  You can do the same with the Makefile {Perl}/Ext/WIN32/win32.mak
  - nice tools but not necessary for EPICS.

  Then call {PERL}/bin/perlw32-install.bat

In any case, read the texts that come with perl!


