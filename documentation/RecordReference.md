# Record Reference Documentation

The following documentation for the record types and menus include with Base was converted from the old EPICS Wiki pages and updated. This list does not include all of the available record types as some have not been documented yet.

## Record Types

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
* [64bit Integer Input Record (int64in)](int64inRecord.html)
* [64bit Integer Output Record (int64out)](int64outRecord.html)
* [Long Input Record (longin)](longinRecord.html)
* [Long Output Record (longout)](longoutRecord.html)
* [Multi-Bit Binary Input Direct Record (mbbiDirect)](mbbiDirectRecord.html)
* [Multi-Bit Binary Input Record (mbbi)](mbbiRecord.html)
* [Multi-Bit Binary Output Direct Record (mbboDirect)](mbboDirectRecord.html)
* [Multi-Bit Binary Output Record (mbbo)](mbboRecord.html)
* [Permissive Record (permissive)](permissiveRecord.html)
* [Select Record (sel)](selRecord.html)
* [Sequence Record (seq)](seqRecord.html)
* [State Record (state)](stateRecord.html)
* [String Input Record (stringin)](stringinRecord.html)
* [String Output Record (stringout)](stringoutRecord.html)
* [Sub-Array Record (subArray)](subArrayRecord.html)
* [Subroutine Record (sub)](subRecord.html)
* [Waveform Record (waveform)](waveformRecord.html)

## Menu Definitions

* [Alarm Severity Menu](menuAlarmSevr.html)
* [Alarm Status Menu](menuAlarmStat.html)
* [Analog Conversions Menu](menuConvert.html)
* [Field Type Menu](menuFtype.html)
* [Invalid Value Output Action Menu](menuIvoa.html)
* [Output Mode Select Menu](menuOmsl.html)
* [Scan Menu](menuScan.html)
* [Simulation Mode Menu](menuSimm.html)
* [Yes/No Menu](menuYesNo.html)

## Corrections and Updates

Corrections to these documents can be submitted as patch files to the EPICS core developers, or as merge requests or pull requests to the 7.0 branch of epics-base. The document sources can be found in the `modules/database/src/std/rec` and `modules/database/src/ioc/db` directories in files with extension `.dbd.pod`. The documentation format is an extended version of Perl POD, run `perldoc pod` for details.
