# EPICS Reference Manual  {#recordrefmanual}

This document provides reference information about the record types,
menus, link types and channel filters included with Base.

Many details about the record and menu definitions are derived automatically
from the source code at build time.

## Introduction and IOC Concepts

These links point to an external website where introductory and overview
documentation is now being published.

* [Introduction to EPICS](https://docs.epics-controls.org/en/latest/guides/EPICS_Intro.html)
* [Process Database Concepts](https://docs.epics-controls.org/en/latest/guides/EPICS_Process_Database_Concepts.html)

## Record Type Definitions

These sections describe common aspects of the record types:

* [Fields Common to All Record Types](dbCommonRecord.html)
* [Fields Common to Input Record Types](dbCommonInput.html)
* [Fields Common to Output Record Types](dbCommonOutput.html)

These are the record types supplied with EPICS Base:

* [Analog Array Input Record (aai)](aaiRecord.html)
* [Analog Array Output Record (aao)](aaoRecord.html)
* [Analog Input Record (ai)](aiRecord.html)
* [Analog Output Record (ao)](aoRecord.html)
* [Array Subroutine Record (aSub)](aSubRecord.html)
* [Binary Input Record (bi)](biRecord.html)
* [Binary Output Record (bo)](boRecord.html)
* [Calculation Output Record (calcout)](calcoutRecord.html)
* [Calculation Record (calc)](calcRecord.html)
* [Compression Record (compress)](compressRecord.html)
* [Data Fanout Record (dfanout)](dfanoutRecord.html)
* [Event Record (event)](eventRecord.html)
* [Fanout Record (fanout)](fanoutRecord.html)
* [Histogram Record (histogram)](histogramRecord.html)
* [64-bit Integer Input Record (int64in)](int64inRecord.html)
* [64-bit Integer Output Record (int64out)](int64outRecord.html)
* [Long Input Record (longin)](longinRecord.html)
* [Long Output Record (longout)](longoutRecord.html)
* [Long String Input Record (lsi)](lsiRecord.html)
* [Long String Output Record (lso)](lsoRecord.html)
* [Multi-Bit Binary Input Direct Record (mbbiDirect)](mbbiDirectRecord.html)
* [Multi-Bit Binary Input Record (mbbi)](mbbiRecord.html)
* [Multi-Bit Binary Output Direct Record (mbboDirect)](mbboDirectRecord.html)
* [Multi-Bit Binary Output Record (mbbo)](mbboRecord.html)
* [Permissive Record (permissive)](permissiveRecord.html)
* [Printf Record (printf)](printfRecord.html)
* [Select Record (sel)](selRecord.html)
* [Sequence Record (seq)](seqRecord.html)
* [State Record (state)](stateRecord.html)
* [String Input Record (stringin)](stringinRecord.html)
* [String Output Record (stringout)](stringoutRecord.html)
* [Sub-Array Record (subArray)](subArrayRecord.html)
* [Subroutine Record (sub)](subRecord.html)
* [Waveform Record (waveform)](waveformRecord.html)

## Menu Definitions

Menu field choices are documented with the record type that defines them, or
here for the global menus that are used by multiple record types:

* [Alarm Severity Menu](menuAlarmSevr.html)
* [Alarm Status Menu](menuAlarmStat.html)
* [Analog Conversions Menu](menuConvert.html)
* [Field Type Menu](menuFtype.html)
* [Invalid Value Output Action Menu](menuIvoa.html)
* [Output Mode Select Menu](menuOmsl.html)
* [Process at iocInit Menu](menuPini.html)
* [Post Monitors Menu](menuPost.html)
* [Priority Menu](menuPriority.html)
* [Scan Menu](menuScan.html)
* [Simulation Mode Menu](menuSimm.html)
* [Yes/No Menu](menuYesNo.html)

## Other Definitions

EPICS Base also comes with extensible sets of link types and channel filters
which are described here:

* [Link Types](links.html)
* [Channel Filters](filters.html)

## Corrections and Updates

Corrections to these documents can be submitted as patch files to the EPICS core
developers, or as merge requests or pull requests to the 7.0 branch of Base.
The document sources can be found in the `modules/database/src/std/rec` and
`modules/database/src/ioc/db` directories in files with extension `.dbd.pod`.
The documentation source format is a combination of the EPICS DBD file format
with an extended version of Perl's POD (plain old documentation); run `perldoc
pod` for details of POD.
