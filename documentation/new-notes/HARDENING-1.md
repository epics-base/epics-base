### CA Server Hardening

In response to several independent reports of security vulnerabilities,
the CA server in the IOC (RSRV) will more strictly validate messages.
**These changes introduce an added risk of interoperability issues with
older CA clients.**

Sites using older versions of libca or CAJ,
or with other CA client implementations,
are advised to do extra testing prior to deploying this release.
