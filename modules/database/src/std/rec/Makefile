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
aiRecord_CFLAGS += -DUSE_TYPED_DSET
aoRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += aiRecord
stdRecords += aoRecord
stdRecords += aSubRecord
biRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += biRecord
boRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += boRecord
stdRecords += calcRecord
calcoutRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += calcoutRecord
stdRecords += compressRecord
stdRecords += dfanoutRecord
stdRecords += eventRecord
stdRecords += fanoutRecord
stdRecords += histogramRecord
int64inRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += int64inRecord
int64outRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += int64outRecord
longinRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += longinRecord
longoutRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += longoutRecord
lsiRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += lsiRecord
lsoRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += lsoRecord
mbbiRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += mbbiRecord
mbbiDirectRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += mbbiDirectRecord
mbboRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += mbboRecord
mbboDirectRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += mbboDirectRecord
stdRecords += permissiveRecord
printfRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += printfRecord
stdRecords += selRecord
stdRecords += seqRecord
stdRecords += stateRecord
stdRecords += stringinRecord
stringoutRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += stringoutRecord
stdRecords += subRecord
subArrayRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += subArrayRecord
waveformRecord_CFLAGS += -DUSE_TYPED_DSET
stdRecords += waveformRecord

DBDINC += $(stdRecords)

# Generate stdRecords.dbd, not really by concatenation, see RULES
DBDCAT += stdRecords.dbd
stdRecords_DBD = $(patsubst %,%.dbd,$(stdRecords))

dbRecStd_SRCS += $(patsubst %,%.c,$(stdRecords))

HTMLS += $(patsubst %.dbd.pod,%.html,$(notdir $(wildcard ../rec/*Record.dbd.pod)))

vpath %.png $(SRC_DIRS)
HTMLS += image/compress-1.png
HTMLS += image/compress-2.png
