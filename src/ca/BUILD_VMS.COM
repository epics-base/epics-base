$!========================================================================
$!
$! Name      : BUILD_VMS
$!
$! Purpose   : To build the CHANNEL_ACCESS library and test programs for
$!             VAX/VMS. This procedure assumes the following:
$!             - You have copied *.c and *.h from the Epics channel access 
$!               source directory  (.../src/ca) into this VMS directory
$!             - You have copied envSubr.c, ellLib.c, and bucketLib.c 
$!			from the Epics
$!               libCom directory into this VMS directory
$!             - You have copied *.h from the /src/epicsH directory into this
$!               VMS directory
$!             - You are using Multinet for TCP/IP access. If not, the logical
$!               name definitions below will need to be changed
$!              
$!
$! Arguments : None
$!
$! Created  	16-NOV-1993   	Mark L. Rivers
$!		05-MAY-1994	Jeff O. Hill	Updated for EPICS 3.12
$!
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
$ if (p1 .nes. "")
$ then
$ cc /defines=(__STDC__)/include=([]) 'p1' 
$ else 
$ cc /defines=(__STDC__)/include=([])  -
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
ELLLIB, - 
BUCKETLIB, -
ENVSUBR, -
CATIME, - 
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
ENVSUBR, -
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
$ link 'p1', sys$input/options
channel_access/lib
multinet_socket_library/share
sys$share:vaxcrtl/share
$ endsubroutine
$! ************************************************************

