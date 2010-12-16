$!========================================================================
$!
$! Name      : BUILD_VMS
$!
$! Purpose   : To build the CA server library and test programs for
$!             VAX/VMS. This procedure assumes the following:
$!             - You have copied *.c and *.h from the Epics channel access 
$!               server source directory  (base/src/cas) into this VMS directory
$!             - You have copied *.c from the Epics
$!               base/src/libCom directory into this VMS directory
$!             - You have copied *.h from the base/include directory into this
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
$! Example FTP script moves sources from UNIX to VMS
$! (remove "$!" comment delimeters)
$!
$! open <HOST NAME>
$! user <USER NAME> 
$! mkdir [.cas]
$! cd [.cas]
$! prompt
$! lcd ~/epics/base/src/cas
$! mput *.c
$! mput *.h
$! put BUILD_VMS.COM
$! lcd vms
$! mput *.c
$! mput *.h
$! lcd ../../libCom
$! mput *.c
$! mput *.h
$! lcd ../ca
$! mput *.h
$! mput *.c
$! lcd ../../include
$! mput *.h
$!========================================================================
$!
$ cwd = f$logical("sys$disk") + f$directory()
$ define /nolog sys multinet_root:[multinet.include.sys]
$ define /nolog vms multinet_root:[multinet.include.vms]
$ define /nolog net multinet_root:[multinet.include.net]
$ define /nolog netinet multinet_root:[multinet.include.netinet]
$ define /nolog arpa multinet_root:[multinet.include.arpa]
$ define /nolog tcp multinet_root:[multinet.include]
$!
$! Compile the functions and test programs
$! Define symbol for the CC command
$ call set_cc_command
$ if (p1 .nes. "")
$ then
$ cc_command 'p1' 
$ else 
$ cc_command casCreateCvrt
$ call linktmp casCreateCvrt
$ casCreateCvrt == "$" + cwd + "casCreateCvrt.exe"
$ casCreateCvrt casCvrt.c
$ cc_command -
exampleCaServer, -
singleThread, -
camessage, -
camsgtask, -
caserverio, -
caservertask, -
cast_server, -
online_notify, -
caEventQueue, -
caMemory, -
casAccess, -
casCvrt, -
mitfp, -
pvRead, -
pvWrite, -
cvtfast, -
errPrintfUNIX, -
fdmgr
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
ELLLIB, -
BUCKETLIB, -
ENVSUBR, -
TSSUBR, -
NEXTFIELDSUBR, -
ASSERTUNIX, -
CATIME, -
ACCTST
$ endif
$
$!
$! Build an object library
$ library /create ca_server_library -
casAccess, -
singleThread, -
camessage, -
camsgtask, -
caserverio, -
caservertask, -
cast_server, -
online_notify, -
caEventQueue, -
casCvrt, -
mitfp, -
caMemory, -
pvRead, -
pvWrite, -
cvtfast, -
errPrintfUNIX, -
fdmgr
$ library /create ca_client_library -
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
ASSERTUNIX, -
ELLLIB
$! Link the example server 
$ call link exampleCaServer 
$
$! Setup DCL Foreign Command for UNIX cmd line params
$ excas == "$" + cwd + "exampleCaServer.exe"
$
$!
$ link: subroutine
$! Link differently for VAX and AXP
$ if f$getsyi("HW_MODEL") .ge. 1024
$ then
$   link 'p1', sys$input/options
    ca_server_library/lib
    ca_client_library/lib
    multinet_socket_library/share
$ else
$   link 'p1', sys$input/options
    ca_server_library/lib
    ca_client_library/lib
    multinet_socket_library/share
    sys$share:vaxcrtl/share
$ endif
$ endsubroutine
$
$
$ linktmp: subroutine
$! Link differently for VAX and AXP
$ if f$getsyi("HW_MODEL") .ge. 1024
$ then
$   link 'p1'
$ else
$   link 'p1', sys$input/options
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
$ cc_command:== cc /warn/float=d_float -
	/include=([], [-.include], [-.libcom]) -
	/define=(MULTINET=1)
$ else
$ cc_command:== cc /include=([], [-.include], [-.libcom]) -
	/define=(MULTINET=1)
$ endif
$ endsubroutine
$! ************************************************************

