# Record reference

This document provides reference information about the record types,
menus,
link types
and channel filters included with Base.

## Record type definitions

These sections describe common aspects of the record types:

```{eval-rst}
.. toctree::
   :maxdepth: 1

   generated/dbCommonRecord
   generated/dbCommonInput
   generated/dbCommonOutput
```

These are the record types supplied with EPICS Base:

```{eval-rst}
.. toctree::
   :maxdepth: 1

   generated/aaiRecord
   generated/aaoRecord
   generated/aiRecord
   generated/aoRecord
   generated/aSubRecord
   generated/biRecord
   generated/boRecord
   generated/calcoutRecord
   generated/calcRecord
   generated/compressRecord
   generated/dfanoutRecord
   generated/eventRecord
   generated/fanoutRecord
   generated/histogramRecord
   generated/int64inRecord
   generated/int64outRecord
   generated/longinRecord
   generated/longoutRecord
   generated/lsiRecord
   generated/lsoRecord
   generated/mbbiDirectRecord
   generated/mbbiRecord
   generated/mbboDirectRecord
   generated/mbboRecord
   generated/permissiveRecord
   generated/printfRecord
   generated/selRecord
   generated/seqRecord
   generated/stateRecord
   generated/stringinRecord
   generated/stringoutRecord
   generated/subArrayRecord
   generated/subRecord
   generated/waveformRecord
```

## Menu definitions

Menu field choices are documented with the record type
that defines them,
or here for the global menus
that are used by multiple record types:

```{eval-rst}
.. toctree::
   :maxdepth: 1

   generated/menuAlarmSevr
   generated/menuAlarmStat
   generated/menuConvert
   generated/menuFtype
   generated/menuIvoa
   generated/menuOmsl
   generated/menuPini
   generated/menuPost
   generated/menuPriority
   generated/menuScan
   generated/menuSimm
   generated/menuYesNo
```

## Other definitions

EPICS Base also comes with extendable sets of link types and channel filters
which are described here:

```{eval-rst}
.. toctree::
   :maxdepth: 1

   generated/links
   generated/filters
```

## Corrections and updates

Corrections to these documents can be submitted as patch files to the EPICS core developers,
or as merge requests
or pull requests to the 7.0 branch of Base.
The document sources can be found in the `modules/database/src/std/rec` and `modules/database/src/ioc/db` directories in files with extension `.dbd.pod`.
The documentation source format is a combination of the EPICS DBD file format
with an extended version of Perl's POD (plain old documentation);
run `perldoc pod` for details of POD.
