$!========================================================================
$!
$! Name      : BUILD_VMS
$!
$! Purpose   : To build the CHANNEL_ACCESS library and test programs for
$!             VAX/VMS. This procedure assumes the following:
$!             - You have copied *.c and *.h from the Epics channel access 
$!               source directory  (.../src/ca) into this VMS directory
$!             - You have copied ellLib.c and bucketLib.c from the Epics
$!               libCom directory into this VMS directory
$!             - You have copied *.h from the /src/epicsH directory into this
$!               VMS directory
$!             - You are using Multinet for TCP/IP access. If not, the logical
$!               name definitions below will need to be changed
$!              
$!
$! Arguments : None
$!
$! Created  16-NOV-1993   Mark L. Rivers
$!
$!========================================================================
$!
$ define /nolog sys multinet_root:[multinet.include.sys]
$ define /nolog vms multinet_root:[multinet.include.vms]
$ define /nolog net multinet_root:[multinet.include.net]
$ define /nolog netinet multinet_root:[multinet.include.netinet]
$ define /nolog tcp multinet_root:[multinet.include]
$!
$! Compile the functions and test programs
$ cc /include=[] -
ACCESS, -
ACCTST, -
BUCKETLIB, -
CATIME, -
CA_PRINTF, -
CONN, -
CONVERT, -
ELLLIB, -
FLOW_CONTROL, -
IFSPEC, -
IOCINF, -
LOOKUP_ADDR_TEST, -
REPEATER, -
SERVICE, -
TEST_EVENT
$!
$! Build an object library
$ library /create channel_access -
ACCESS, -
BUCKETLIB, -
CA_PRINTF, -
CONN, -
CONVERT, -
ELLLIB, -
FLOW_CONTROL, -
IFSPEC, -
IOCINF, -
REPEATER, -
SERVICE, -
TEST_EVENT
$! Link the test programs
$ call link acctst
$ call link catime
$ call link lookup_addr_test
$ call link repeater
$!
$ link: subroutine
$ link 'p1', sys$input/options
channel_access/lib
multinet_socket_library/share
sys$share:vaxcrtl/share
$ endsubroutine
$! ************************************************************

