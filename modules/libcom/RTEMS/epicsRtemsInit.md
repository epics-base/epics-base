# RTEMS Initialization

## Starting RTEMS

EPICS on RTEMS is dependent on RTEMS being initialized and providing
specific services for it to run. The approach in this PR to
initialization is a minimal base so IOC dependent services can be
manage using an IOC shell and related shell commands. The approach is
the start of moving the set up and configuration of an IOC away from
EPICS code and local custom patches and into system level scripting.

EPICS needs to have some base level of code to start RTEMS. There is a
conflict between RTEMS's set up and initialization, the ability to
have a shared and common executable in a system or site and ability to
have node specific configurations.

### System Steps

1. Network initialization using environment variables. A BSP can set a
   standard set of environment variables that results in a standard
   network set up. The MVME5500 (`powerpc/beatnik`) and MVME2700
   (`powerpc/mvme2700`) have been updated to support this style of
   initialization. Environment variables provide a standard and stable
   way to export settings from RTEMS and importing them into
   EPICS. New BSPs can use the same approach without needing to update
   EPICS.
1. NFSv4 mount support that provides a backwards compatible way to run
   an IOC `st.cmd` script.
1. Debugger support using `libdebugger`. The IOC shell commands
   `Debug_start` and `Debug_break` provide IOC shell commands to start
   the debugger and to wait for the debugger to connect to before
   continuing.

### Environment Variables

RTEMS Initialize uses environment variables to hold and manage a range
of states and settings. Environment variables can be set in the BSP
when booting or initialization steps can set or edit variables
effecting later steps.

Environment variables use a standard C interface and commands in RTEMS
(`getenv -a`) and EPICS (`epicsEnvShow`) let you print and review the
variables. You can tweak settings during development or testing
avoiding rebuilds for production.

BSP support to set environment variables lets us avoid drivers or C
calls to export data and that simplifies the amount of code we all
need to maintained and any possible version skew.

I recommend all RTEMS Initialize environment variables are prefixed
with `RTEMS_`.  Initialize Steps

Initialization is broken down into an ordered series of steps. A step
is a registered function or handler with a domain string, a name
string an order and the default enable or disable setting. Steps are
registered before initialization starts and new steps cannot be
registered once initialization is active.

Initialization runs part of `POSIX_Init()`. The registered set of
steps is sorted based on the start order then the handler function of
each step is called if enabled and not disabled in order. A simple
message is printed on the console before the step's handler is called
so the start up process can be audited and debugged.

### Naming

An initialization step has a domain name and a step or handler name
that combine to be a step's name. For example the system debugger
initialization step is system.debugger where the domain is system and
the name is debugger. The system domain is reserved for the steps that
are build into EPICS base. A name string can contain . characters to
provide further sub-grouping.

You can register a handler with same name string and a different
domain overriding a system domain entry. This provides a structured
way to override and change an initialization built into EPICS without
needing to modify the base code or rebuild it. It also provides a
simple way to develop and extend a system domain step.  Enable and
Disable

Initialization steps have a default enable state. If the default state
is enabled and it is not disabled the step's handler is called. If the
default enable state is disabled the step needs to be enabled for the
handler to be called.

The enable and disable list is held in the environment variables
RTEMS_INIT_ENABLE and RTEMS_INIT_DISABLE and comma separated names. A
name that appears in both lists will be disabled.

### Using

Add to your local site configuration:

```
RTEMS_INIT = new
```

## Use Cases

The following a few use cases. This approach to initialization
provides a simple framework that allows growth.  Boot loader GEV
variables

The `powerpc/beatnik` and `powerpc/mvme2700` support converting the
GEV variables into environment variables and these can be used to
create an /etc/rc.conf file used to configure the network. The GEV
variables for an MVME5550 are:

````
mot-/dev/enet1-cipa=10.10.10.4
mot-/dev/enet1-sipa=10.10.10.2
mot-/dev/enet1-gipa=10.10.10.1
mot-/dev/enet1-snma=255.255.255.0
mot-/dev/enet1-file=/rtems/app.boot
mot-script-boot
dla=malloc 0x4000000
tftpGet -d/dev/enet1 -f/rtems/app.boot -m255.255.255.0 -g10.10.10.1 -s10.10.10.2 -c10.10.10.4 -adla -r3
netshut
go -adla

epics-nfsmount=10.10.10.10:/opt/src:noirlab/gemini/ntp/ntp-test/iocBoot/iocntp/st.cmd
Total Number of GE Variables =7, Bytes Utilized =404, Bytes Free =3188
````

The `/etc/rc.conf` file contents are:

````
ifconfig_em0="inet 10.10.10.4 netmask 255.255.255.0"
defaultrouter="10.10.10.1"
````

The initialization console messages are:

```
EPICS RTEMS rtems-7.0.0 (PowerPC/Generic (classic FPU)/beatnik)
  Tools: 15.2.0 20250808 (RTEMS 7, Build 2026.06.12, RSB f2eb74759a8d5a313428459e31ca2a429f89c8ca, Newlib 7d4336cf)
  Cores: 1

] Init (200,1/10,enabled): system.log.reset
Startup
] Init (210,2/10,enabled): system.filesys.root
File System: root set up
] Init (250,3/10,enabled): system.filesys.local
] Init (500,4/10,enabled): system.net
nexus0: <RTEMS Nexus device>
pcib0 pcibus 0
pci0: <PCI bus> on pcib0
pci0: <memory> at device 0.0 (no driver attached)
pci0: <memory> at device 0.1 (no driver attached)
pci0: <memory> at device 0.2 (no driver attached)
pci0: <memory> at device 0.3 (no driver attached)
pci0: <memory> at device 0.4 (no driver attached)
pci0: <memory> at device 0.5 (no driver attached)
pci0: <memory> at device 0.6 (no driver attached)
pci0: <memory> at device 0.7 (no driver attached)
pcib1: <PCI-PCI bridge> at device 10.0 on pci0
pci1: <PCI bus> on pcib1
pci1: <bridge> at device 0.0 (no driver attached)
pcib2 pcibus 2
pci2: <PCI bus> on pcib2
pci2: <memory> at device 0.0 (no driver attached)
pci2: <memory> at device 0.1 (no driver attached)
pci2: <memory> at device 0.2 (no driver attached)
pci2: <memory> at device 0.3 (no driver attached)
pci2: <memory> at device 0.4 (no driver attached)
pci2: <memory> at device 0.5 (no driver attached)
pci2: <memory> at device 0.6 (no driver attached)
pci2: <memory> at device 0.7 (no driver attached)
em0: <Intel(R) Legacy PRO/1000 XT 82544EI (Copper)> port 0xf0810000-0xf081001f mem 0xe1100000-0xe111ffff,0xe1120000-0xe113ffff irq 84 at device 10.0 on pci2
em0: Using 1024 TX descriptors and 1024 RX descriptors
info: em0: Ethernet address: 00:01:af:31:a8:5d
cpu0: <legacy CPU>
Setting hostname: Amnesiac.
Starting network:  lo0 info: lo0: link state changed to UP
em0.
 em0: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> metric 0 mtu 1500
        options=48525bb<RXCSUM,TXCSUM,VLAN_MTU,VLAN_HWTAGGING,JUMBO_MTU,VLAN_HWCSUM,TSO4,LRO,WOL_MAGIC,VLAN_HWFILTER,VLAN_HWTSO,NOMAP>
        ether 00:01:af:31:a8:5d
        inet 10.10.10.4 netmask 0xffffff00 broadcast 10.10.10.255
        nd6 options=29<PERFORMNUD,IFDISABLED,AUTO_LINKLOCAL>
        media: Ethernet autoselect
        status: no carrier
lo0: flags=8049<UP,LOOPBACK,RUNNING,MULTICAST> metric 0 mtu 16384
        options=680003<RXCSUM,TXCSUM,LINKSTATE,RXCSUM_IPV6,TXCSUM_IPV6>
        inet6 ::1 prefixlen 128
        inet6 fe80::1%lo0 prefixlen 64 scopeid 0x2
        inet 127.0.0.1 netmask 0xff000000
        nd6 options=21<PERFORMNUD,AUTO_LINKLOCAL>
        groups: lo
add net default: gateway 10.10.10.1
em0: waiting for link (timeout 20s)... info: em0: link state changed to UP
up
] Init (600,5/10,enabled): system.nfs
mount: nfs: 10.10.10.10:/opt/src -> /opt/src
nfs: mount: nfs -> 10.10.10.10:/opt/src
nfs: mount args: 8
  16 addr=10 02 08 01 0a 0a 05 04 00 00 00 00 00 00 00 00
  28 fh=32 e3 66 8a de 57 3f 93 0a 00 09 00 00 00 00 00 d3 00 00 00 00 00 00 00 00 00 00 00
   4 sec=sys
   0 nfsv3
   4 fstype=nfs
   7 fspath=/src-0
  19 hostname=10.10.10.10:/opt/src
   0 rw
nfs: mount: (0) Success
] Init (840,6/10,disabled): system.debugger
] Init (850,7/10,enabled): system.ntp.ip
NTP IP address: 10.10.10.2
] Init (900,8/10,enabled): system.cmds
RTEMS Commands registered
] Init (999,9/10,enabled): system.log.err.flush
] Init (1000,10/10,enabled): system.ioc
IOC work directory: "/opt/src/noirlab/gemini/ntp/ntp-test/iocBoot/iocntp/"

IOC Command line: /rtems/app.boot /opt/src/noirlab/gemini/ntp/ntp-test/iocBoot/iocntp/st.cmd
```

## Disable and Enable

You can add custom steps by creating a module level object of type
`epicsRtemsInitRegister`. In this example the system NTP from IP
address set up is disabled and the debugger is enabled. The code added
my ntpMail.cpp is:

```
#ifdef __rtems__
#include <epicsRtemsInit.h>

static int disableNTP_boot_IP() {
    epicsRtemsInit_disable("system", "ntp.ip");
    epicsRtemsInit_enable("system", "debugger");
    return 0;
}

epicsRtemsInitRegister myInit(
    "test", "ntp", rtemsInit_Order_early + 5, true, disableNTP_boot_IP);
#endif
```

The console output is:

```
EPICS RTEMS rtems-7.0.0 (PowerPC/Generic (classic FPU)/beatnik)
  Tools: 15.2.0 20250808 (RTEMS 7, Build 2026.06.12, RSB f2eb74759a8d5a313428459e31ca2a429f89c8ca, Newlib 7d4336cf)
  Cores: 1

] Init (15,1/11,enabled): test.ntp
] Init (200,2/11,enabled): system.log.reset
Startup
] Init (210,3/11,enabled): system.filesys.root
File System: root set up
] Init (250,4/11,enabled): system.filesys.local
] Init (500,5/11,enabled): system.net
nexus0: <RTEMS Nexus device>
pcib0 pcibus 0
pci0: <PCI bus> on pcib0
pci0: <memory> at device 0.0 (no driver attached)
pci0: <memory> at device 0.1 (no driver attached)
pci0: <memory> at device 0.2 (no driver attached)
pci0: <memory> at device 0.3 (no driver attached)
pci0: <memory> at device 0.4 (no driver attached)
pci0: <memory> at device 0.5 (no driver attached)
pci0: <memory> at device 0.6 (no driver attached)
pci0: <memory> at device 0.7 (no driver attached)
pcib1: <PCI-PCI bridge> at device 10.0 on pci0
pci1: <PCI bus> on pcib1
pci1: <bridge> at device 0.0 (no driver attached)
pcib2 pcibus 2
pci2: <PCI bus> on pcib2
pci2: <memory> at device 0.0 (no driver attached)
pci2: <memory> at device 0.1 (no driver attached)
pci2: <memory> at device 0.2 (no driver attached)
pci2: <memory> at device 0.3 (no driver attached)
pci2: <memory> at device 0.4 (no driver attached)
pci2: <memory> at device 0.5 (no driver attached)
pci2: <memory> at device 0.6 (no driver attached)
pci2: <memory> at device 0.7 (no driver attached)
em0: <Intel(R) Legacy PRO/1000 XT 82544EI (Copper)> port 0xf0810000-0xf081001f mem 0xe1100000-0xe111ffff,0xe1120000-0xe113ffff irq 84 at device 10.0 on pci2
em0: Using 1024 TX descriptors and 1024 RX descriptors
info: em0: Ethernet address: 00:01:af:31:a8:5d
cpu0: <legacy CPU>
Setting hostname: Amnesiac.
Starting network:  lo0 info: lo0: link state changed to UP
em0.
 em0: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> metric 0 mtu 1500
        options=48525bb<RXCSUM,TXCSUM,VLAN_MTU,VLAN_HWTAGGING,JUMBO_MTU,VLAN_HWCSUM,TSO4,LRO,WOL_MAGIC,VLAN_HWFILTER,VLAN_HWTSO,NOMAP>
        ether 00:01:af:31:a8:5d
        inet 10.10.10.4 netmask 0xffffff00 broadcast 10.10.10.255
        nd6 options=29<PERFORMNUD,IFDISABLED,AUTO_LINKLOCAL>
        media: Ethernet autoselect
        status: no carrier
lo0: flags=8049<UP,LOOPBACK,RUNNING,MULTICAST> metric 0 mtu 16384
        options=680003<RXCSUM,TXCSUM,LINKSTATE,RXCSUM_IPV6,TXCSUM_IPV6>
        inet6 ::1 prefixlen 128
        inet6 fe80::1%lo0 prefixlen 64 scopeid 0x2
        inet 127.0.0.1 netmask 0xff000000
        nd6 options=21<PERFORMNUD,AUTO_LINKLOCAL>
        groups: lo
add net default: gateway 10.10.10.1
em0: waiting for link (timeout 20s)... info: em0: link state changed to UP
up
] Init (600,6/11,enabled): system.nfs
mount: nfs: 10.10.10.10:/opt/src -> /opt/src
nfs: mount: nfs -> 10.10.10.10:/opt/src
nfs: mount args: 8
  16 addr=10 02 08 01 0a 0a 05 04 00 00 00 00 00 00 00 00
  28 fh=32 e3 66 8a de 57 3f 93 0a 00 09 00 00 00 00 00 d3 00 00 00 00 00 00 00 00 00 00 00
   4 sec=sys
   0 nfsv3
   4 fstype=nfs
   7 fspath=/src-0
  19 hostname=10.10.10.10:/opt/src
   0 rw
nfs: mount: (0) Success
] Init (840,7/11,enabled): system.debugger
] Init (850,8/11,disabled): system.ntp.ip
] Init (900,9/11,enabled): system.cmds
RTEMS Commands registered
] Init (999,10/11,enabled): system.log.err.flush
] Init (1000,11/11,enabled): system.ioc
IOC work directory: "/opt/src/noirlab/gemini/ntp/ntp-test/iocBoot/iocntp/"

IOC Command line: /rtems/app.boot /opt/src/noirlab/gemini/ntp/ntp-test/iocBoot/iocntp/st.cmd
```

In this example the st.cmd script configures NTP (see NTP PR 528) with
a configuration file and start the debug server:

```
iocInit
NTPTime_Start("config", "/opt/src/noirlab/gemini/ntp/ntp-test/iocBoot/iocntp/ntp.conf")
NTPTime_SyncWait(30)
Debug_Start()
```

The console messages are:

```
iocInit
Starting iocInit
ntpd: RTEMS service init
############################################################################
## EPICS R7.0.10.1-DEV
## Rev. 04cd8af51a4254a046c4
## Rev. Date Git: 2026-07-08 13:17:02 +1000
############################################################################
iocRun: All initialization complete
NTPTime_Start("config", "/opt/src/noirlab/gemini/ntp/ntp-test/iocBoot/iocntp/ntp.conf")
NTP: setenv /opt/src/noirlab/gemini/ntp/ntp-test/iocBoot/iocntp/ntp.conf -> EPICS_TS_NTP_CONF_FILE
NTPTime_SyncWait(30)
RTEMS NTP: waiting 30 seconds for sync
NTP is sync[CPU:0] rtems-db: remote running
[CPU:0] rtems-db: tcp remote: listing on port: 1122
hronized
Debug_Start()
Starting debugger: tcp:1122
```
