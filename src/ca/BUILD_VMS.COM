$!========================================================================
$!
$! Name      : BUILD_VMS
$!
$! Purpose   : To build the CHANNEL_ACCESS library and test programs for
$!             VAX/VMS. This procedure assumes the following:
$!             - You have copied *.c and *.h from the Epics channel access 
$!               source directory  (base/src/ca) into this VMS directory
$!             - You have copied *.c and *.h from the Epics
$!               libCom directory into this VMS directory
$!             - You have copied *.h from the base/include directory into this
$!               VMS directory
$!             - You are using Multinet for TCP/IP access. If not, the logical
$!               name definitions below will need to be changed
$!              
$!
$! Arguments : 	optional source file name 
$!
$! Created  	16-NOV-1993   	Mark L. Rivers
$!		05-MAY-1994	Jeff O. Hill	Updated for EPICS 3.12
$!		07-MAR-1995	Jeff O. Hill	Added FTP script	
$!
$!========================================================================
$!
$! Example FTP script moves sources from UNIX to VMS
$! (remove "$!" comment delimiters)
$!
$! user XXXXXXX 
$! cd [.ca]
$! prompt
$! lcd ~/epics/base/src/ca
$! mput *.c
$! mput *.h
$! put BUILD_VMS.COM
$! lcd ../libCom
$! mput *.c
$! mput *.h
$! lcd O.sun4
$! mput envData.c
$! lcd ../../../include
$! mput *.h
$!========================================================================
$!
$ define /nolog sys multinet_root:[multinet.include.sys]
$ define /nolog vms multinet_root:[multinet.include.vms]
$ define /nolog net multinet_root:[multinet.include.net]
$ define /nolog netinet multinet_root:[multinet.include.netinet]
$ define /nolog arpa multinet_root:[multinet.include.arpa]
$ define /nolog tcp multinet_root:[multinet.include]
$!
$! Compile the functions and test programs
$! Define symbol for the CC command
$ call list_define
$ call set_cc_command
$ if (p1 .nes. "")
$ then
$ cc_command 'p1'
$ else 
$ cc_command -
ACCESS, -
CONN, -  
CONVERT, -  
FLOW_CONTROL, -
IOCINF, -
REPEATER, - 
CAREPEATER, - 
SERVICE, - 
SYNCGRP,-
TEST_EVENT, -
BSD_DEPEN, - 
IF_DEPEN, - 
VMS_DEPEN, -
[-.libcom]ELLLIB, -
[-.libcom]BUCKETLIB, -
[-.libcom]ENVSUBR, -
[-.libcom]TSSUBR, -
NEXTFIELDSUBR, -
ASSERTUNIX, -
[-.libcom]ENVDATA, -
[-.ca]CATIME, - 
ACCTST 
$ endif
$
$!
$! Build an object library
$ library /create channel_access -
IOCINF, -
ACCESS, -
CONN, -
CONVERT, -
FLOW_CONTROL, -
REPEATER, -
TEST_EVENT, -
SYNCGRP, -
SERVICE, -
IF_DEPEN, - 
VMS_DEPEN, -
BSD_DEPEN, -
BUCKETLIB, -
TSSUBR, -
ENVSUBR, -
NEXTFIELDSUBR, -
ENVDATA, -
ASSERTUNIX, -
ELLLIB
$! Link the test programs
$ call link acctst
$ call link catime
$ call link carepeater
$
$! Setup DCL Foreign Command for UNIX cmd line params
$ acctst == "$ EPICS_EXE:acctst.exe"
$ catime == "$ EPICS_EXE:catime.exe"
$
$!
$ link: subroutine
$! Link differently for VAX and AXP
$ if f$getsyi("HW_MODEL") .ge. 1024
$ then
$   link 'p1', sys$input/options
    channel_access/lib
    multinet_socket_library/share
$ else
$   link 'p1', sys$input/options
    channel_access/lib
    multinet_socket_library/share
    sys$share:vaxcrtl/share
$ endif
$ endsubroutine
$
$
$! This subroutine sets up "cc_command" to use different switches for 
$! VAX (assumes VAX C compiler) and AXP (DEC C compiler).
$ set_cc_command : subroutine
$ if f$getsyi("HW_MODEL") .ge. 1024
$ then
$! turn of no prototype messages because MULTINET does not
$! supply prototypes.
$ cc_command:== cc /warn=(disable=IMPLICITFUNC)/float=d_float -
/define=("__STDC__=1", "MULTINET=1") -
/include=([], [-.include], [-.include.os.vms], [-.libcom], [-.db]) 
$ else
$ cc_command:== cc /define=("__STDC__=1", "MULTINET=1") 'listing' -
/include=([], [-.include], [-.include.os.vms], [-.libcom], [-.db]) 
$ endif
$
$ endsubroutine
$ list_define: subroutine
$ listing == ""
$ if p1 .eqs. "L" then listing == "/lis=list.lis /sho=incl"
$ endsubroutine
$
$! ************************************************************


