#*************************************************************************
# Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/std/Makefile.

SRC_DIRS += $(STDDIR)/rec

stdRecords += aaiRecord
stdRecords += aaoRecord
stdRecords += aiRecord
stdRecords += aoRecord
stdRecords += aSubRecord
stdRecords += biRecord
stdRecords += boRecord
stdRecords += calcRecord
stdRecords += calcoutRecord
stdRecords += compressRecord
stdRecords += dfanoutRecord
stdRecords += eventRecord
stdRecords += fanoutRecord
stdRecords += histogramRecord
stdRecords += int64inRecord
stdRecords += int64outRecord
stdRecords += longinRecord
stdRecords += longoutRecord
stdRecords += lsiRecord
stdRecords += lsoRecord
stdRecords += mbbiRecord
stdRecords += mbbiDirectRecord
stdRecords += mbboRecord
stdRecords += mbboDirectRecord
stdRecords += permissiveRecord
stdRecords += printfRecord
stdRecords += selRecord
stdRecords += seqRecord
stdRecords += stateRecord
stdRecords += stringinRecord
stdRecords += stringoutRecord
stdRecords += subRecord
stdRecords += subArrayRecord
stdRecords += waveformRecord

DBDINC += $(stdRecords)

# Generate stdRecords.dbd, not really by concatenation, see RULES
DBDCAT += stdRecords.dbd
stdRecords_DBD = $(patsubst %,%.dbd,$(stdRecords))

dbRecStd_SRCS += $(patsubst %,%.c,$(stdRecords))

HTMLS += $(patsubst %.dbd.pod,%.html,$(notdir $(wildcard ../rec/*Record.dbd.pod)))
