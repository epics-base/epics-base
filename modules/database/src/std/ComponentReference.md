# IOC Component Reference

This document provides reference information about the record types,
menus, link types and channel filters included with EPICS Base.

## Introduction and IOC Concepts

- [Introduction to EPICS](https://docs.epics-controls.org/en/latest/getting-started/EPICS_Intro.html)
- [Process Database Concepts](https://docs.epics-controls.org/en/latest/process-database/EPICS_Process_Database_Concepts.html)

## Record Type Definitions

These sections describe common aspects of the record types:

```{toctree}
:titlesonly:

dbCommonRecord
dbCommonInput
dbCommonOutput
```

These are the record types supplied with EPICS Base:

```{toctree}
:titlesonly:

aaiRecord
aaoRecord
aiRecord
aoRecord
aSubRecord
biRecord
boRecord
calcoutRecord
calcRecord
compressRecord
dfanoutRecord
eventRecord
fanoutRecord
histogramRecord
int64inRecord
int64outRecord
longinRecord
longoutRecord
lsiRecord
lsoRecord
mbbiDirectRecord
mbbiRecord
mbboDirectRecord
mbboRecord
permissiveRecord
printfRecord
selRecord
seqRecord
stateRecord
stringinRecord
stringoutRecord
subArrayRecord
subRecord
waveformRecord
```

## Menu Definitions

Menu field choices are documented with the record type that defines them, or
here for the global menus that are used by multiple record types:

```{toctree}
:titlesonly:

menuAlarmSevr
menuAlarmStat
menuConvert
menuFtype
menuIvoa
menuOmsl
menuPini
menuPost
menuPriority
menuScan
menuSimm
menuYesNo
```

## Other Components

EPICS Base also comes with extensible sets of server Channel Filters and IOC
Database Link types, which are documented here:

```{toctree}
:titlesonly:

filters
links
```
