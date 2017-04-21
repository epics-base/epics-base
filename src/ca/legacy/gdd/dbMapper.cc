/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
// Author: Jim Kowalkowski
// Date: 2/96

#define DB_MAPPER_SOURCE 1

#include <stdio.h>
#include <epicsAlgorithm.h>

#define epicsExportSharedSymbols
#include "gddApps.h"
#include "gddAppTable.h"
#include "dbMapper.h"
// #include "templates/dbMapperTempl.h"

// hardcoded in same order as aitConvert.h
// no way to detect a string type!!!!!!!

extern epicsShareDef const int gddAitToDbr[aitConvertLast+1] = { 
	DBR_STRING,
	DBR_CHAR,
	DBR_CHAR,
	DBR_SHORT,
	DBR_SHORT,
	DBR_ENUM,
	DBR_LONG,
	DBR_LONG,
	DBR_FLOAT,
	DBR_DOUBLE,
	DBR_STRING,
	DBR_STRING
};

extern epicsShareDef const unsigned gddDbrToAitNElem = 
        sizeof(gddAitToDbr)/sizeof(gddAitToDbr[0]);

/*
 * application type ("app" field) is initialized
 * at run tim using the "app_name" field.
 */
epicsShareDef gddDbrToAitTable gddDbrToAit[DBM_N_DBR_TYPES] = {
    // Atomic
    { aitEnumFixedString,	0,	"value" },              // DBR_STRING
    { aitEnumInt16,		    0,	"value" },              // DBR_SHORT
    { aitEnumFloat32,	    0,	"value" },              // DBR_FLOAT
    { aitEnumEnum16,	    0,	"value" },              // DBR_ENUM
    { aitEnumInt8,		    0,	"value" },              // DBR_CHAR
    { aitEnumInt32,		    0,	"value" },              // DBR_LONG
    { aitEnumFloat64,	    0,	"value" },              // DBR_DOUBLE
    // Status
    { aitEnumFixedString,	0,	"value" },              // DBR_STS_STRING
    { aitEnumInt16,		    0,	"value" },              // DBR_STS_SHORT
    { aitEnumFloat32,	    0,	"value" },              // DBR_STS_FLOAT
    { aitEnumEnum16,	    0,	"value" },              // DBR_STS_ENUM
    { aitEnumInt8,		    0,	"value" },              // DBR_STS_CHAR
    { aitEnumInt32,		    0,	"value" },              // DBR_STS_LONG
    { aitEnumFloat64,	    0,	"value" },              // DBR_STS_DOUBLE
    // Time
    { aitEnumFixedString,	0,	"value" },              // DBR_TIME_STRING
    { aitEnumInt16,		    0,	"value" },              // DBR_TIME_SHORT
    { aitEnumFloat32,	    0,	"value" },              // DBR_TIME_FLOAT
    { aitEnumEnum16,	    0,	"value" },              // DBR_TIME_ENUM
    { aitEnumInt8,		    0,	"value" },              // DBR_TIME_CHAR
    { aitEnumInt32,		    0,	"value" },              // DBR_TIME_LONG
    { aitEnumFloat64,	    0,	"value" },              // DBR_TIME_DOUBLE
    // Graphic
    { aitEnumFixedString,	0,	"value" },              // DBR_GR_STRING
    { aitEnumInt16,		    0,	"dbr_gr_short" },       // DBR_GR_SHORT
    { aitEnumFloat32,	    0,	"dbr_gr_float" },       // DBR_GR_FLOAT
    { aitEnumEnum16,	    0,	"dbr_gr_enum" },        // DBR_GR_ENUM
    { aitEnumInt8,		    0,	"dbr_gr_char" },        // DBR_GR_CHAR
    { aitEnumInt32,		    0,	"dbr_gr_long" },        // DBR_GR_LONG
    { aitEnumFloat64,	    0,	"dbr_gr_double" },      // DBR_GR_DOUBLE
    // Control
    { aitEnumFixedString,   0,  "value" },              // DBR_CTRL_STRING
    { aitEnumInt16,		    0,	"dbr_ctrl_short" },     // DBR_CTRL_SHORT
    { aitEnumFloat32,	    0,	"dbr_ctrl_float" },     // DBR_CTRL_FLOAT
    { aitEnumEnum16,	    0,	"dbr_ctrl_enum" },      // DBR_CTRL_ENUM
    { aitEnumInt8,		    0,	"dbr_ctrl_char" },      // DBR_CTRL_CHAR
    { aitEnumInt32,		    0,	"dbr_ctrl_long" },      // DBR_CTRL_LONG
    { aitEnumFloat64,	    0,	"dbr_ctrl_double" },     // DBR_CTRL_DOUBLE
    // Ack
    { aitEnumUint16,        0,  "ackt" },               // DBR_PUT_ACKT
    { aitEnumUint16,        0,  "acks" },               // DBR_PUT_ACKS
    { aitEnumFixedString,   0,  "dbr_stsack_string" },  // DBR_STSACK_STRING
    // Class
    { aitEnumFixedString,   0,  "class" }               // DBR_CLASS_NAME
};

#if DBM_N_DBR_TYPES != (LAST_BUFFER_TYPE+1)
#error db mapper is out of sync with db_access.h
#endif

static gddApplicationTypeTable* type_table = NULL;
static aitDataFormat local_data_format=aitLocalDataFormat;

extern epicsShareDef const unsigned gddAitToDbrNElem = 
        sizeof(gddDbrToAit)/sizeof(gddDbrToAit[0]);

//
// special gddDestructor guarantees same form of new and delete
//
class dbMapperFixedStringDestructor: public gddDestructor {
	virtual void run (void *);
};


// I generated a container for each of the important DBR types.  This
// includes all the control and graphic structures.  The others are
// not needed become you can get time stamp and status in each gdd.
// Currently the containers are built here with c++ code.

// string type needs to be written special and not use template
// maybe create a template for the string type
// the problem is that the value of a string structure is an array,
// not just a single element

static smartGDDPointer mapStringToGdd(void* v,aitIndex count) {
	aitFixedString* db = (aitFixedString*)v;
	aitEnum to_type = gddDbrToAit[DBR_STRING].type;
	aitUint16 to_app  = gddDbrToAit[DBR_STRING].app;

	if(count<=1)
	{
		smartGDDPointer dd = new gddScalar(to_app,to_type);
        dd->unreference();
		dd->put(*db);
	    return dd;
	}
	else
	{
		smartGDDPointer dd=new gddAtomic(to_app,to_type,1,count);
        dd->unreference();
        aitFixedString* pCopy = new aitFixedString [count];
        memcpy (pCopy,db,sizeof(aitFixedString)*count);
		dd->putRef(db,new dbMapperFixedStringDestructor);
	    return dd;
	}
}

static int mapGddToString(void* vd, aitIndex count, 
    const gdd & dd, const gddEnumStringTable &enumStringTable) {
	aitFixedString* db = (aitFixedString*)vd;
	aitIndex sz = dd.getDataSizeElements();
	const void* v = dd.dataVoid();
	int status;

    if (count>sz) {
        memset (db+sz, '\0', sizeof(*db)*(count-sz));
        count = sz;
    }

	if(local_data_format==aitLocalDataFormat) {
		if((aitFixedString*)v!=db) {
			status = aitConvert(aitEnumFixedString,db,
                dd.primitiveType(),v,count, &enumStringTable);
		}
		else {
			status = sz*sizeof(aitFixedString);
		}
	}
	else {
		status = aitConvertToNet(aitEnumFixedString,
            db,dd.primitiveType(),v,count, &enumStringTable);
	}
	
	return status;
}

static smartGDDPointer mapShortToGdd(void* v,aitIndex count) {
	dbr_short_t* sv = (dbr_short_t*)v;

	if(count>1) {
		smartGDDPointer dd=new gddAtomic(gddDbrToAit[DBR_SHORT].app,
			gddDbrToAit[DBR_SHORT].type,1,count);
        dd->unreference();
        dbr_short_t* pCopy = (dbr_short_t*) new char [sizeof(dbr_short_t)*count];
        memcpy (pCopy,sv,sizeof(dbr_short_t)*count);
		dd->putRef(pCopy, new gddDestructor);
	    return dd;
	} else {
		smartGDDPointer dd=new gddScalar(gddDbrToAit[DBR_SHORT].app);
        dd->unreference();
		*dd=*sv;
	    return dd;
	}
}

static int mapGddToShort(void* vd, aitIndex count, const gdd &dd, 
                         const gddEnumStringTable &enumStringTable) {
	dbr_short_t* sv = (dbr_short_t*)vd;
    aitIndex sz = dd.getDataSizeElements();
	const void * v=dd.dataVoid();
	int status;

    if (count>sz) {
        memset (sv+sz, '\0', sizeof(*sv)*(count-sz));
        count = sz;
    }

	if (local_data_format==aitLocalDataFormat) {
		if((dbr_short_t*)v!=sv) {
			status = aitConvert(aitEnumInt16,sv,
                dd.primitiveType(),v,count, &enumStringTable);
		}
		else {
			status = count*sizeof(dbr_short_t);
		}
	}
	else {
		status = aitConvertToNet(aitEnumInt16,sv,
            dd.primitiveType(),v,count, &enumStringTable);
	}

	return status;
}

static smartGDDPointer mapFloatToGdd(void* v,aitIndex count) {
	dbr_float_t* sv = (dbr_float_t*)v;

	if(count>1) {
		smartGDDPointer dd=new gddAtomic(gddDbrToAit[DBR_FLOAT].app,
			gddDbrToAit[DBR_FLOAT].type,1,count);
        dd->unreference();
        dbr_float_t* pCopy = (dbr_float_t*) new char [sizeof(dbr_float_t)*count];
        memcpy (pCopy,sv,sizeof(dbr_float_t)*count);
		dd->putRef(pCopy, new gddDestructor);
	    return dd;
	} else {
		smartGDDPointer dd=new gddScalar(gddDbrToAit[DBR_FLOAT].app);
        dd->unreference();
		*dd=*sv;
	    return dd;
	}
}

static int mapGddToFloat(void* vd, aitIndex count, 
     const gdd & dd, const gddEnumStringTable &enumStringTable) {
	dbr_float_t* sv = (dbr_float_t*)vd;
    aitIndex sz=dd.getDataSizeElements();
	const void * v = dd.dataVoid();
	int status;

    if (count>sz) {
        memset (sv+sz, '\0', sizeof(*sv)*(count-sz));
        count = sz;
    }

	if(local_data_format==aitLocalDataFormat) {
		if((dbr_float_t*)v!=sv) {
			status = aitConvert(aitEnumFloat32,sv,
                dd.primitiveType(),v,count, &enumStringTable);
		}
		else {
			status = sz*sizeof(dbr_float_t);
		}
	}
	else {
		status = aitConvertToNet(aitEnumFloat32,sv,
            dd.primitiveType(),v,count, &enumStringTable);
	}

	return status;
}

static smartGDDPointer mapEnumToGdd(void* v,aitIndex count) {
	dbr_enum_t* sv = (dbr_enum_t*)v;
	smartGDDPointer dd;

	if(count>1) {
		dd=new gddAtomic(gddDbrToAit[DBR_ENUM].app,
			gddDbrToAit[DBR_ENUM].type,1,count);
        dd->unreference();
        dbr_enum_t* pCopy = (dbr_enum_t *) new char [sizeof(dbr_enum_t)*count];
        memcpy (pCopy,sv,sizeof(dbr_enum_t)*count);
		dd->putRef(pCopy, new gddDestructor);
	} else {
		dd=new gddScalar(gddDbrToAit[DBR_ENUM].app);
        dd->unreference();
		*dd=*sv;
	}
	return dd;
}

static int mapGddToEnum(void* vd, aitIndex count, const gdd & dd, 
                        const gddEnumStringTable &enumStringTable) {
	dbr_enum_t* sv = (dbr_enum_t*)vd;
    aitIndex sz=dd.getDataSizeElements();
	const void* v = dd.dataVoid();
	int status;

    if (count>sz) {
        memset (sv+sz, '\0', sizeof(*sv)*(count-sz));
        count = sz;
    }

	if(local_data_format==aitLocalDataFormat) {
		if((dbr_enum_t*)v!=sv) {
			status = aitConvert(aitEnumEnum16,sv,
                dd.primitiveType(),v,count, &enumStringTable);
		}
		else {
			status = sizeof(dbr_enum_t)*count;
		}
	}
	else {
		status = aitConvertToNet(aitEnumEnum16,sv,
            dd.primitiveType(),v,count, &enumStringTable);
	}

	return status;
}

static smartGDDPointer mapCharToGdd(void* v,aitIndex count) {
	dbr_char_t* sv = (dbr_char_t*)v;
	smartGDDPointer dd;

	if(count>1) {
		dd=new gddAtomic(gddDbrToAit[DBR_CHAR].app,
			gddDbrToAit[DBR_CHAR].type,1,count);
        dd->unreference();
        dbr_char_t* pCopy = (dbr_char_t *) new char [sizeof(dbr_char_t)*count];
        memcpy (pCopy,sv,sizeof(dbr_char_t)*count);
		dd->putRef(pCopy, new gddDestructor);
	} else {
		dd=new gddScalar(gddDbrToAit[DBR_CHAR].app);
        dd->unreference();
		*dd=*sv;
	}
	return dd;
}

static int mapGddToChar(void* vd, aitIndex count, 
     const gdd & dd, const gddEnumStringTable &enumStringTable) {
	dbr_char_t* sv = (dbr_char_t*)vd;
    aitIndex sz=dd.getDataSizeElements();
	const void* v = dd.dataVoid();
	int status;

   if (count>sz) {
        memset (sv+sz, '\0', sizeof(*sv)*(count-sz));
        count = sz;
    }

	if (local_data_format==aitLocalDataFormat) {
		if((dbr_char_t*)v!=sv) {
			status = aitConvert(aitEnumInt8,sv,
                dd.primitiveType(),v,count, &enumStringTable);
		}
		else {
			status = sz*sizeof(dbr_char_t);
		}
	}
	else {
		status = aitConvertToNet(aitEnumInt8,sv,
            dd.primitiveType(),v,count, &enumStringTable);
	}

	return status;
}

static smartGDDPointer mapLongToGdd(void* v,aitIndex count) {
	dbr_long_t* sv = (dbr_long_t*)v;
	smartGDDPointer dd;

	if(count>1) {
		dd=new gddAtomic(gddDbrToAit[DBR_LONG].app,
			gddDbrToAit[DBR_LONG].type,1,count);
        dd->unreference();
        dbr_long_t* pCopy = (dbr_long_t*) new char [sizeof(dbr_long_t)*count];
        memcpy (pCopy,sv,sizeof(dbr_long_t)*count);
		dd->putRef(pCopy, new gddDestructor);
	} else {
		dd=new gddScalar(gddDbrToAit[DBR_LONG].app);
        dd->unreference();
		*dd=*sv;
	}
	return dd;
}

static int mapGddToLong(void* vd, aitIndex count, const gdd & dd, 
                      const gddEnumStringTable &enumStringTable) {
	dbr_long_t* sv = (dbr_long_t*)vd;
    aitIndex sz=dd.getDataSizeElements();
	const void* v = dd.dataVoid();
	int status;

    if (count>sz) {
        memset (sv+sz, '\0', sizeof(*sv)*(count-sz));
        count = sz;
    }

	if (local_data_format==aitLocalDataFormat) {
		if ((dbr_long_t*)v!=sv) {
			status = aitConvert(aitEnumInt32,sv,
                dd.primitiveType(),v,count, &enumStringTable);
		}
		else {
			status = count*sizeof(dbr_long_t);
		}
	}
	else {
		status = aitConvertToNet(aitEnumInt32,sv,
            dd.primitiveType(),v,count, &enumStringTable);
	}

	return status;
}

static smartGDDPointer mapDoubleToGdd(void* v,aitIndex count) {
	dbr_double_t* sv = (dbr_double_t*)v;
    smartGDDPointer dd;

	if(count>1) {
		dd=new gddAtomic(gddDbrToAit[DBR_DOUBLE].app,
			gddDbrToAit[DBR_DOUBLE].type,1,count);
        dd->unreference();
        dbr_double_t* pCopy = (dbr_double_t *) new char [sizeof(dbr_double_t)*count];
        memcpy (pCopy,sv,sizeof(dbr_double_t)*count);
		dd->putRef(pCopy, new gddDestructor);
	} else {
		dd=new gddScalar(gddDbrToAit[DBR_DOUBLE].app);
        dd->unreference();
		*dd=*sv;
	}
	return dd;
}

static int mapGddToDouble(void* vd, aitIndex count, const gdd & dd, 
                          const gddEnumStringTable &enumStringTable) {
	dbr_double_t* sv = (dbr_double_t*)vd;
    aitIndex sz=dd.getDataSizeElements();
	const void* v = dd.dataVoid();
	int status;

    if (count>sz) {
        memset (sv+sz, '\0', sizeof(*sv)*(count-sz));
        count = sz;
    }

	if (local_data_format==aitLocalDataFormat) {
		if ((dbr_double_t*)v!=sv) {
			status = aitConvert(aitEnumFloat64,sv,
                dd.primitiveType(),v,count, &enumStringTable);
		}
		else {
			status = count*sizeof(dbr_double_t);
		}
	}
	else {
		status = aitConvertToNet(aitEnumFloat64,sv,
            dd.primitiveType(),v,count, &enumStringTable);
	}

	return status;
}

// ********************************************************************
//                      sts structure mappings
// ********************************************************************

static smartGDDPointer mapStsStringToGdd(void* v,aitIndex count)
{
	dbr_sts_string* db = (dbr_sts_string*)v;
	aitFixedString* dbv = (aitFixedString*)db->value;
	aitEnum to_type = gddDbrToAit[DBR_STS_STRING].type;
	aitUint16 to_app  = gddDbrToAit[DBR_STS_STRING].app;
	smartGDDPointer dd;

	if(count<=1)
	{
		dd=new gddScalar(to_app,to_type);
        dd->unreference();
		dd->put(*dbv);
	}
	else
	{
		dd=new gddAtomic(to_app,to_type,1,count);
        dd->unreference();
        aitFixedString* pCopy = (aitFixedString*)  new char [sizeof(aitFixedString)*count];
        memcpy (pCopy,dbv,sizeof(aitFixedString)*count);
		dd->putRef(pCopy, new gddDestructor);
	}

	dd->setStatSevr(db->status,db->severity);
	return dd;
}

static int mapStsGddToString(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_sts_string* db = (dbr_sts_string*)v;
	aitFixedString* dbv = (aitFixedString*)db->value;

	dd.getStatSevr(db->status,db->severity);
	return mapGddToString(dbv, count, dd, enumStringTable);
}

static smartGDDPointer mapStsShortToGdd(void* v,aitIndex count)
{
	dbr_sts_short* dbv = (dbr_sts_short*)v;
	smartGDDPointer dd=mapShortToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToShort(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_sts_short* dbv = (dbr_sts_short*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	return mapGddToShort(&dbv->value, count, dd, enumStringTable);
}

static smartGDDPointer mapStsFloatToGdd(void* v,aitIndex count)
{
	dbr_sts_float* dbv = (dbr_sts_float*)v;
	smartGDDPointer dd=mapFloatToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToFloat(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_sts_float* dbv = (dbr_sts_float*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	return mapGddToFloat(&dbv->value,count,dd, enumStringTable);
}

static smartGDDPointer mapStsEnumToGdd(void* v,aitIndex count)
{
	dbr_sts_enum* dbv = (dbr_sts_enum*)v;
	smartGDDPointer dd=mapEnumToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToEnum(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_sts_enum* dbv = (dbr_sts_enum*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	return mapGddToEnum(&dbv->value,count,dd, enumStringTable);
}

static smartGDDPointer mapStsCharToGdd(void* v,aitIndex count)
{
	dbr_sts_char* dbv = (dbr_sts_char*)v;
	smartGDDPointer dd=mapCharToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToChar(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_sts_char* dbv = (dbr_sts_char*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	dbv->RISC_pad = '\0'; // shut up purify
	return mapGddToChar(&dbv->value,count,dd, enumStringTable);
}

static smartGDDPointer mapStsLongToGdd(void* v,aitIndex count)
{
	dbr_sts_long* dbv = (dbr_sts_long*)v;
	smartGDDPointer dd=mapLongToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToLong(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_sts_long* dbv = (dbr_sts_long*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	return mapGddToLong(&dbv->value,count,dd, enumStringTable);
}

static smartGDDPointer mapStsDoubleToGdd(void* v,aitIndex count)
{
	dbr_sts_double* dbv = (dbr_sts_double*)v;
	smartGDDPointer dd=mapDoubleToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToDouble(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_sts_double* dbv = (dbr_sts_double*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	dbv->RISC_pad = 0; // shut up purify
	return mapGddToDouble(&dbv->value,count,dd, enumStringTable);
}

// ********************************************************************
//                      time structure mappings
// ********************************************************************

static smartGDDPointer mapTimeStringToGdd(void* v,aitIndex count)
{
	dbr_time_string* db = (dbr_time_string*)v;
	aitFixedString* dbv = (aitFixedString*)db->value;
	aitEnum to_type = gddDbrToAit[DBR_TIME_STRING].type;
	aitUint16 to_app  = gddDbrToAit[DBR_TIME_STRING].app;
	smartGDDPointer dd;

	if(count<=1)
	{
		dd=new gddScalar(to_app,to_type);
        dd->unreference();
		dd->put(*dbv);
	}
	else
	{
		dd=new gddAtomic(to_app,to_type,1,count);
        dd->unreference();
        aitFixedString* pCopy = (aitFixedString*) new char [sizeof(aitFixedString)*count];
        memcpy (pCopy,dbv,sizeof(aitFixedString)*count);
		dd->putRef(pCopy, new gddDestructor);
	}

	dd->setStatSevr(db->status,db->severity);
	dd->setTimeStamp(&db->stamp);
	return dd;
}

static int mapTimeGddToString(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_time_string* db = (dbr_time_string*)v;
	aitFixedString* dbv = (aitFixedString*)db->value;

	dd.getStatSevr(db->status,db->severity);
	dd.getTimeStamp(&db->stamp);
	return mapGddToString(dbv, count, dd, enumStringTable);
}

static smartGDDPointer mapTimeShortToGdd(void* v,aitIndex count)
{
	dbr_time_short* dbv = (dbr_time_short*)v;
	smartGDDPointer dd=mapShortToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp(&dbv->stamp);
	return dd;
}

static int mapTimeGddToShort(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_time_short* dbv = (dbr_time_short*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	dd.getTimeStamp(&dbv->stamp);
	dbv->RISC_pad = 0; // shut up purify
	return mapGddToShort(&dbv->value,count,dd, enumStringTable);
}

static smartGDDPointer mapTimeFloatToGdd(void* v,aitIndex count)
{
	dbr_time_float* dbv = (dbr_time_float*)v;
	smartGDDPointer dd=mapFloatToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp(&dbv->stamp);
	return dd;
}

static int mapTimeGddToFloat(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_time_float* dbv = (dbr_time_float*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	dd.getTimeStamp(&dbv->stamp);
	return mapGddToFloat(&dbv->value,count,dd, enumStringTable);
}

static smartGDDPointer mapTimeEnumToGdd(void* v,aitIndex count)
{
	dbr_time_enum* dbv = (dbr_time_enum*)v;
	smartGDDPointer dd=mapEnumToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp(&dbv->stamp);
	return dd;
}

static int mapTimeGddToEnum(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_time_enum* dbv = (dbr_time_enum*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	dd.getTimeStamp(&dbv->stamp);
	dbv->RISC_pad = 0; // shut up purify
	return mapGddToEnum(&dbv->value,count,dd, enumStringTable);
}

static smartGDDPointer mapTimeCharToGdd(void* v,aitIndex count)
{
	dbr_time_char* dbv = (dbr_time_char*)v;
	smartGDDPointer dd=mapCharToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp(&dbv->stamp);
	return dd;
}

static int mapTimeGddToChar(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_time_char* dbv = (dbr_time_char*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	dd.getTimeStamp(&dbv->stamp);
	dbv->RISC_pad0 = 0; // shut up purify
	dbv->RISC_pad1 = '\0'; // shut up purify
	return mapGddToChar(&dbv->value,count,dd, enumStringTable);
}

static smartGDDPointer mapTimeLongToGdd(void* v,aitIndex count)
{
	dbr_time_long* dbv = (dbr_time_long*)v;
	smartGDDPointer dd=mapLongToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp(&dbv->stamp);
	return dd;
}

static int mapTimeGddToLong(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_time_long* dbv = (dbr_time_long*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	dd.getTimeStamp(&dbv->stamp);
	return mapGddToLong(&dbv->value,count,dd, enumStringTable);
}

static smartGDDPointer mapTimeDoubleToGdd(void* v,aitIndex count)
{
	dbr_time_double* dbv = (dbr_time_double*)v;
	smartGDDPointer dd=mapDoubleToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp(&dbv->stamp);
	return dd;
}

static int mapTimeGddToDouble(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	dbr_time_double* dbv = (dbr_time_double*)v;
	dd.getStatSevr(dbv->status,dbv->severity);
	dd.getTimeStamp(&dbv->stamp);
	dbv->RISC_pad = 0; // shut up purify
	return mapGddToDouble(&dbv->value,count,dd, enumStringTable);
}

// ********************************************************************
//                      graphic structure mappings
// ********************************************************************

// -------------map the short structures----------------
static smartGDDPointer mapGraphicShortToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_gr_short* db = (dbr_gr_short*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_GR_SHORT].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_gr_short_value];

	aitString* str=NULL;
	(*dd)[gddAppTypeIndex_dbr_gr_short_units].getRef(str);
	str->copy(db->units);

	(*dd)[gddAppTypeIndex_dbr_gr_short_graphicLow]=db->lower_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_short_graphicHigh]=db->upper_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_short_alarmLow]=db->lower_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_short_alarmHigh]=db->upper_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_short_alarmLowWarning]=db->lower_warning_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_short_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);

        dbr_short_t* pCopy = (dbr_short_t*) new char [sizeof(dbr_short_t)*count];
        memcpy (pCopy,&db->value,sizeof(dbr_short_t)*count);
		vdd.putRef(pCopy, new gddDestructor);
	}
	return dd;
}

static smartGDDPointer mapControlShortToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_ctrl_short* db = (dbr_ctrl_short*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_CTRL_SHORT].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_ctrl_short_value];

	aitString* str = NULL;
	(*dd)[gddAppTypeIndex_dbr_ctrl_short_units].getRef(str);
	str->copy(db->units);

	(*dd)[gddAppTypeIndex_dbr_ctrl_short_graphicLow]=db->lower_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_short_graphicHigh]=db->upper_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_short_controlLow]=db->lower_ctrl_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_short_controlHigh]=db->upper_ctrl_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_short_alarmLow]=db->lower_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_short_alarmHigh]=db->upper_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_short_alarmLowWarning]=db->lower_warning_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_short_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);

        dbr_short_t* pCopy = (dbr_short_t*) new char [sizeof(dbr_short_t)*count];
        memcpy (pCopy,&db->value,sizeof(dbr_short_t)*count);
		vdd.putRef(pCopy, new gddDestructor);
	}
	return dd;
}

static int mapGraphicGddToShort(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	const aitString* str;
	dbr_gr_short* db = (dbr_gr_short*)v;
	const gdd& vdd = dd[gddAppTypeIndex_dbr_gr_short_value];

	dd[gddAppTypeIndex_dbr_gr_short_units].getRef(str);
	if(str->string()) {
		strncpy(db->units,str->string(), sizeof(db->units));
		db->units[sizeof(db->units)-1u] = '\0';	
	}

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_gr_short_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_gr_short_graphicHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_gr_short_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_gr_short_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_gr_short_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_gr_short_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);
	return mapGddToShort(&db->value,count,vdd, enumStringTable);
}

static int mapControlGddToShort(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	const aitString* str;
	dbr_ctrl_short* db = (dbr_ctrl_short*)v;
	const gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_short_value];

	dd[gddAppTypeIndex_dbr_ctrl_short_units].getRef(str);
	if(str->string()) {
		strncpy(db->units,str->string(), sizeof(db->units));
		db->units[sizeof(db->units)-1u] = '\0';	
	}

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_short_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_short_graphicHigh];
	db->lower_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_short_controlLow];
	db->upper_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_short_controlHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_short_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_short_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_short_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_short_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);
	return mapGddToShort(&db->value,count,vdd, enumStringTable);
}

// -------------map the float structures----------------
static smartGDDPointer mapGraphicFloatToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_gr_float* db = (dbr_gr_float*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_GR_FLOAT].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_gr_float_value];

	aitString* str = NULL;
	(*dd)[gddAppTypeIndex_dbr_gr_float_units].getRef(str);
	str->copy(db->units);

	(*dd)[gddAppTypeIndex_dbr_gr_float_precision]=db->precision;
	(*dd)[gddAppTypeIndex_dbr_gr_float_graphicLow]=db->lower_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_float_graphicHigh]=db->upper_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_float_alarmLow]=db->lower_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_float_alarmHigh]=db->upper_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_float_alarmLowWarning]=db->lower_warning_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_float_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumFloat32,1,&count);
		else vdd.setPrimType(aitEnumFloat32);
		vdd.setBound(0,0,count);

        dbr_float_t* pCopy = (dbr_float_t*) new char [sizeof(dbr_float_t)*count];
        memcpy (pCopy,&db->value,sizeof(dbr_float_t)*count);
		vdd.putRef(pCopy, new gddDestructor);
	}
	return dd;
}

static smartGDDPointer mapControlFloatToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_ctrl_float* db = (dbr_ctrl_float*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_CTRL_FLOAT].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_ctrl_float_value];

	aitString* str = NULL;
	(*dd)[gddAppTypeIndex_dbr_ctrl_float_units].getRef(str);
	str->copy(db->units);

	(*dd)[gddAppTypeIndex_dbr_ctrl_float_precision]=db->precision;
	(*dd)[gddAppTypeIndex_dbr_ctrl_float_graphicLow]=db->lower_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_float_graphicHigh]=db->upper_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_float_controlLow]=db->lower_ctrl_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_float_controlHigh]=db->upper_ctrl_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_float_alarmLow]=db->lower_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_float_alarmHigh]=db->upper_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_float_alarmLowWarning]=db->lower_warning_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_float_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumFloat32,1,&count);
		else vdd.setPrimType(aitEnumFloat32);
		vdd.setBound(0,0,count);

        dbr_float_t* pCopy = (dbr_float_t*) new char [sizeof(dbr_float_t)*count];
        memcpy (pCopy,&db->value,sizeof(dbr_float_t)*count);
		vdd.putRef(pCopy, new gddDestructor);
	}
	return dd;
}

static int mapGraphicGddToFloat(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	const aitString* str;
	dbr_gr_float* db = (dbr_gr_float*)v;
	const gdd& vdd = dd[gddAppTypeIndex_dbr_gr_float_value];

	dd[gddAppTypeIndex_dbr_gr_float_units].getRef(str);
	if(str->string()) {
		strncpy(db->units,str->string(), sizeof(db->units));
		db->units[sizeof(db->units)-1u] = '\0';	
	}

	db->precision=dd[gddAppTypeIndex_dbr_gr_float_precision];
	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_gr_float_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_gr_float_graphicHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_gr_float_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_gr_float_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_gr_float_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_gr_float_alarmHighWarning];
	db->RISC_pad0 = 0; // shut up purify

	vdd.getStatSevr(db->status,db->severity);
	return mapGddToFloat(&db->value,count,vdd, enumStringTable);
}

static int mapControlGddToFloat(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	const aitString* str;
	dbr_ctrl_float* db = (dbr_ctrl_float*)v;
	const gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_float_value];

	dd[gddAppTypeIndex_dbr_ctrl_float_units].getRef(str);
	if(str->string()) {
		strncpy(db->units,str->string(), sizeof(db->units));
		db->units[sizeof(db->units)-1u] = '\0';	
	}

	db->precision=dd[gddAppTypeIndex_dbr_ctrl_float_precision];
	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_float_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_float_graphicHigh];
	db->lower_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_float_controlLow];
	db->upper_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_float_controlHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_float_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_float_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_float_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_float_alarmHighWarning];
	db->RISC_pad = 0; // shut up purify

	vdd.getStatSevr(db->status,db->severity);
	return mapGddToFloat(&db->value,count,vdd, enumStringTable);
}

// -------------map the enum structures----------------
static smartGDDPointer mapGraphicEnumToGdd(void* v, aitIndex /*count*/)
{
	dbr_gr_enum* db = (dbr_gr_enum*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_GR_ENUM].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_gr_enum_value];
	gdd& menu = (*dd)[gddAppTypeIndex_dbr_gr_enum_enums];
	aitFixedString* str = menu;
	//aitFixedString* f = (aitFixedString*)db->strs;
	aitIndex sz,i;

	// int i;
	// old way using aitString menu
	// for(i=0;i<db->no_str;i++) str[i]=((const char*)&(db->strs[i][0]));

	if(menu.dataPointer()==NULL || !menu.isAtomic())
	{
		// need to copy the menu chunk from dbr to aitFixedString chunk
		menu.setDimension(1);
		sz=db->no_str;
		str=new aitFixedString[db->no_str];
		menu.putRef(str,new dbMapperFixedStringDestructor);
	}
	else
	{
		if((sz=menu.getDataSizeElements())>(aitIndex)db->no_str)
			sz=db->no_str;
	}

    unsigned minl = epicsMin(sizeof(aitFixedString),sizeof(str[0].fixed_string)) - 1;
	for (i=0;i<sz;i++) {
		strncpy (str[i].fixed_string, &(db->strs[i][0]), minl);
		memset(&str[i].fixed_string[minl], '\0', sizeof(aitFixedString)-minl);
	}
	menu.setBound(0,0,sz);

	// should always be a scaler
	if(vdd.dimension()) vdd.clear();
	vdd=db->value;
	vdd.setStatSevr(db->status,db->severity);
	return dd;
}

static smartGDDPointer mapControlEnumToGdd(void* v, aitIndex /*count*/)
{
	dbr_ctrl_enum* db = (dbr_ctrl_enum*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_CTRL_ENUM].app);
	gdd& menu = (*dd)[gddAppTypeIndex_dbr_ctrl_enum_enums];
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_ctrl_enum_value];
	aitFixedString* str = menu;
	//aitFixedString* f = (aitFixedString*)db->strs;
	aitIndex sz,i;

	// int i;
	// old way using aitString menu
	// for(i=0;i<db->no_str;i++) str[i]=((const char*)&(db->strs[i][0]));

	if(menu.dataPointer()==NULL || !menu.isAtomic())
	{
		// need to copy the menu chunk from dbr to aitFixedString chunk
		menu.setDimension(1);
		sz=db->no_str;
		str=new aitFixedString[db->no_str];
		menu.putRef(str,new dbMapperFixedStringDestructor);
	}
	else
	{
		if((sz=menu.getDataSizeElements())>(aitIndex)db->no_str)
			sz=db->no_str;
	}

    unsigned minl = epicsMin(sizeof(aitFixedString),(size_t)MAX_ENUM_STRING_SIZE) - 1;
	for (i=0;i<sz;i++) {
		strncpy(str[i].fixed_string,&(db->strs[i][0]), minl);
		memset(&str[i].fixed_string[minl], '\0', sizeof(aitFixedString)-minl);
	}
	menu.setBound(0,0,sz);

	// should always be a scaler
	if(vdd.dimension()) vdd.clear();
	vdd=db->value;
	vdd.setStatSevr(db->status,db->severity);
	return dd;
}

static int mapGraphicGddToEnum (
    void * v, aitIndex count, const gdd & dd, 
    const gddEnumStringTable & enumStringTable )
{
	dbr_gr_enum * db = ( dbr_gr_enum * ) v;
	const gdd & vdd = dd[gddAppTypeIndex_dbr_gr_enum_value];

	vdd.getStatSevr ( db->status, db->severity );
    unsigned noStr = enumStringTable.numberOfStrings ();
    if ( noStr < MAX_ENUM_STATES ) {
        db->no_str = static_cast < dbr_short_t > ( noStr );
    }
    else {
        db->no_str = MAX_ENUM_STATES;
    }
	for ( int i=0; i < db->no_str; i++ ) {
        enumStringTable.getString ( i, 
            &(db->strs[i][0]), MAX_ENUM_STRING_SIZE );
	}
	for ( int j = db->no_str; j < MAX_ENUM_STATES; j++ ) {
		db->strs[j][0] = '\0';
    }
	return mapGddToEnum ( &db->value, 
        count, vdd, enumStringTable );
}

static int mapControlGddToEnum ( 
    void * v, aitIndex count, const gdd & dd, 
    const gddEnumStringTable & enumStringTable )
{
	dbr_ctrl_enum* db = ( dbr_ctrl_enum * ) v;
	const gdd & vdd = dd[gddAppTypeIndex_dbr_ctrl_enum_value];

	vdd.getStatSevr ( db->status, db->severity );
    unsigned noStr = enumStringTable.numberOfStrings ();
    if ( noStr < MAX_ENUM_STATES ) {
        db->no_str = static_cast < dbr_short_t > ( noStr );
    }
    else {
        db->no_str = MAX_ENUM_STATES;
    }
	for ( int i=0; i < db->no_str; i++ ) {
        enumStringTable.getString ( i, 
            &(db->strs[i][0]), MAX_ENUM_STRING_SIZE );
	}
	for ( int j = db->no_str; j < MAX_ENUM_STATES; j++ ) {
		db->strs[j][0] = '\0';
    }
	return mapGddToEnum ( &db->value, 
        count, vdd, enumStringTable );
}

// -------------map the char structures----------------
static smartGDDPointer mapGraphicCharToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_gr_char* db = (dbr_gr_char*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_GR_CHAR].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_gr_char_value];

	aitString* str = NULL;
	(*dd)[gddAppTypeIndex_dbr_gr_char_units].getRef(str);
	str->copy(db->units);

	(*dd)[gddAppTypeIndex_dbr_gr_char_graphicLow]=db->lower_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_char_graphicHigh]=db->upper_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_char_alarmLow]=db->lower_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_char_alarmHigh]=db->upper_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_char_alarmLowWarning]=db->lower_warning_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_char_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt8,1,&count);
		else vdd.setPrimType(aitEnumInt8);
		vdd.setBound(0,0,count);

        dbr_char_t* pCopy = (dbr_char_t *) new char [sizeof(dbr_char_t)*count];
        memcpy (pCopy,&db->value,sizeof(dbr_char_t)*count);
		vdd.putRef(pCopy, new gddDestructor);
	}
	return dd;
}

static smartGDDPointer mapControlCharToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_ctrl_char* db = (dbr_ctrl_char*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_CTRL_CHAR].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_ctrl_char_value];

	aitString* str = NULL;
	(*dd)[gddAppTypeIndex_dbr_ctrl_char_units].getRef(str);
	str->copy(db->units);

	(*dd)[gddAppTypeIndex_dbr_ctrl_char_graphicLow]=db->lower_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_char_graphicHigh]=db->upper_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_char_controlLow]=db->lower_ctrl_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_char_controlHigh]=db->upper_ctrl_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_char_alarmLow]=db->lower_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_char_alarmHigh]=db->upper_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_char_alarmLowWarning]=db->lower_warning_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_char_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt8,1,&count);
		else vdd.setPrimType(aitEnumInt8);
		vdd.setBound(0,0,count);

        dbr_char_t* pCopy = (dbr_char_t*) new char [sizeof(dbr_char_t)*count];
        memcpy (pCopy,&db->value,sizeof(dbr_char_t)*count);
		vdd.putRef(pCopy, new gddDestructor);
 	}
	return dd;
}

static int mapGraphicGddToChar(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	const aitString* str;
	dbr_gr_char* db = (dbr_gr_char*)v;
	const gdd& vdd = dd[gddAppTypeIndex_dbr_gr_char_value];

	dd[gddAppTypeIndex_dbr_gr_char_units].getRef(str);
	if(str->string()) {
		strncpy(db->units,str->string(), sizeof(db->units));
		db->units[sizeof(db->units)-1u] = '\0';	
	}

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_gr_char_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_gr_char_graphicHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_gr_char_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_gr_char_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_gr_char_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_gr_char_alarmHighWarning];
	db->RISC_pad = 0;

	vdd.getStatSevr(db->status,db->severity);
	return mapGddToChar(&db->value,count,vdd, enumStringTable);
}

static int mapControlGddToChar(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	const aitString* str;
	dbr_ctrl_char* db = (dbr_ctrl_char*)v;
	const gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_char_value];

	dd[gddAppTypeIndex_dbr_ctrl_char_units].getRef(str);
	if(str->string()) {
		strncpy(db->units,str->string(), sizeof(db->units));
		db->units[sizeof(db->units)-1u] = '\0';	
	}

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_char_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_char_graphicHigh];
	db->lower_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_char_controlLow];
	db->upper_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_char_controlHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_char_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_char_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_char_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_char_alarmHighWarning];
	db->RISC_pad = '\0'; // shut up purify

	vdd.getStatSevr(db->status,db->severity);
	return mapGddToChar(&db->value,count,vdd, enumStringTable);
}

// -------------map the long structures----------------
static smartGDDPointer mapGraphicLongToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_gr_long* db = (dbr_gr_long*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_GR_LONG].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_gr_long_value];

	aitString* str = NULL;
	(*dd)[gddAppTypeIndex_dbr_gr_long_units].getRef(str);
	str->copy(db->units);

	(*dd)[gddAppTypeIndex_dbr_gr_long_graphicLow]=db->lower_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_long_graphicHigh]=db->upper_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_long_alarmLow]=db->lower_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_long_alarmHigh]=db->upper_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_long_alarmLowWarning]=db->lower_warning_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_long_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt32,1,&count);
		else vdd.setPrimType(aitEnumInt32);
		vdd.setBound(0,0,count);

        dbr_long_t* pCopy = (dbr_long_t*) new char [sizeof(dbr_long_t)*count];
        memcpy (pCopy,&db->value,sizeof(dbr_long_t)*count);
		vdd.putRef(pCopy, new gddDestructor);
	}
	return dd;
}

static smartGDDPointer mapControlLongToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_ctrl_long* db = (dbr_ctrl_long*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_CTRL_LONG].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_ctrl_long_value];

	aitString* str = NULL;
	(*dd)[gddAppTypeIndex_dbr_ctrl_long_units].getRef(str);
	str->copy(db->units);

	(*dd)[gddAppTypeIndex_dbr_ctrl_long_graphicLow]=db->lower_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_long_graphicHigh]=db->upper_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_long_controlLow]=db->lower_ctrl_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_long_controlHigh]=db->upper_ctrl_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_long_alarmLow]=db->lower_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_long_alarmHigh]=db->upper_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_long_alarmLowWarning]=db->lower_warning_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_long_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt32,1,&count);
		else vdd.setPrimType(aitEnumInt32);
		vdd.setBound(0,0,count);

        dbr_long_t* pCopy = (dbr_long_t *)new char [sizeof(dbr_long_t)*count];
        memcpy (pCopy,&db->value,sizeof(dbr_long_t)*count);
		vdd.putRef(pCopy, new gddDestructor);
	}
	return dd;
}

static int mapGraphicGddToLong(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	const aitString* str;
	dbr_gr_long* db = (dbr_gr_long*)v;
	const gdd& vdd = dd[gddAppTypeIndex_dbr_gr_long_value];

	dd[gddAppTypeIndex_dbr_gr_long_units].getRef(str);
	if(str->string()) {
		strncpy(db->units,str->string(), sizeof(db->units));
		db->units[sizeof(db->units)-1u] = '\0';	
	}

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_gr_long_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_gr_long_graphicHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_gr_long_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_gr_long_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_gr_long_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_gr_long_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);
	return mapGddToLong(&db->value,count,vdd, enumStringTable);
}

static int mapControlGddToLong(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	const aitString* str;
	dbr_ctrl_long* db = (dbr_ctrl_long*)v;
	const gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_long_value];

	dd[gddAppTypeIndex_dbr_ctrl_long_units].getRef(str);
	if(str->string()) {
		strncpy(db->units,str->string(), sizeof(db->units));
		db->units[sizeof(db->units)-1u] = '\0';	
	}

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_long_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_long_graphicHigh];
	db->lower_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_long_controlLow];
	db->upper_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_long_controlHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_long_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_long_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_long_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_long_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);
	return mapGddToLong(&db->value,count,vdd, enumStringTable);
}

// -------------map the double structures----------------
static smartGDDPointer mapGraphicDoubleToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_gr_double* db = (dbr_gr_double*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_GR_DOUBLE].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_gr_double_value];

	aitString* str = NULL;
	(*dd)[gddAppTypeIndex_dbr_gr_double_units].getRef(str);
	str->copy(db->units);

	(*dd)[gddAppTypeIndex_dbr_gr_double_precision]=db->precision;
	(*dd)[gddAppTypeIndex_dbr_gr_double_graphicLow]=db->lower_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_double_graphicHigh]=db->upper_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_double_alarmLow]=db->lower_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_double_alarmHigh]=db->upper_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_double_alarmLowWarning]=db->lower_warning_limit;
	(*dd)[gddAppTypeIndex_dbr_gr_double_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumFloat64,1,&count);
		else vdd.setPrimType(aitEnumFloat64);
		vdd.setBound(0,0,count);

        dbr_double_t* pCopy = (dbr_double_t *) new char [sizeof(dbr_double_t)*count];
        memcpy (pCopy,&db->value,sizeof(dbr_double_t)*count);
		vdd.putRef(pCopy, new gddDestructor);
	}
	return dd;
}

static smartGDDPointer mapControlDoubleToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_ctrl_double* db = (dbr_ctrl_double*)v;
	smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_CTRL_DOUBLE].app);
	gdd& vdd = (*dd)[gddAppTypeIndex_dbr_ctrl_double_value];

	aitString* str = NULL;
	(*dd)[gddAppTypeIndex_dbr_ctrl_double_units].getRef(str);
	str->copy(db->units);

	(*dd)[gddAppTypeIndex_dbr_ctrl_double_precision]=db->precision;
	(*dd)[gddAppTypeIndex_dbr_ctrl_double_graphicLow]=db->lower_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_double_graphicHigh]=db->upper_disp_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_double_controlLow]=db->lower_ctrl_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_double_controlHigh]=db->upper_ctrl_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_double_alarmLow]=db->lower_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_double_alarmHigh]=db->upper_alarm_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_double_alarmLowWarning]=db->lower_warning_limit;
	(*dd)[gddAppTypeIndex_dbr_ctrl_double_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumFloat64,1,&count);
		else vdd.setPrimType(aitEnumFloat64);
		vdd.setBound(0,0,count);

        dbr_double_t* pCopy = (dbr_double_t *) new char [sizeof(dbr_double_t)*count];
        memcpy (pCopy,&db->value,sizeof(dbr_double_t)*count);
		vdd.putRef(pCopy, new gddDestructor);
	}
	return dd;
}

static int mapGraphicGddToDouble(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	const aitString* str;
	dbr_gr_double* db = (dbr_gr_double*)v;
	const gdd& vdd = dd[gddAppTypeIndex_dbr_gr_double_value];

	dd[gddAppTypeIndex_dbr_gr_double_units].getRef(str);
	if(str->string()) {
		strncpy(db->units,str->string(), sizeof(db->units));
		db->units[sizeof(db->units)-1u] = '\0';	
	}

	db->precision=dd[gddAppTypeIndex_dbr_gr_double_precision];
	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_gr_double_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_gr_double_graphicHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_gr_double_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_gr_double_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_gr_double_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_gr_double_alarmHighWarning];
	db->RISC_pad0 = 0; // shut up purify

	vdd.getStatSevr(db->status,db->severity);
	return mapGddToDouble(&db->value,count,vdd, enumStringTable);
}

static int mapControlGddToDouble(void* v, aitIndex count, const gdd & dd, const gddEnumStringTable &enumStringTable)
{
	const aitString* str;
	dbr_ctrl_double* db = (dbr_ctrl_double*)v;
	const gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_double_value];

	dd[gddAppTypeIndex_dbr_ctrl_double_units].getRef(str);
	if(str->string()) {
		strncpy(db->units,str->string(), sizeof(db->units));
		db->units[sizeof(db->units)-1u] = '\0';	
	}

	db->precision=dd[gddAppTypeIndex_dbr_ctrl_double_precision];
	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_double_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_double_graphicHigh];
	db->lower_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_double_controlLow];
	db->upper_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_double_controlHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_double_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_double_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_double_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_double_alarmHighWarning];
	db->RISC_pad0 = '\0'; // shut up purify

	vdd.getStatSevr(db->status,db->severity);
	return mapGddToDouble(&db->value,count,vdd, enumStringTable);
}

static smartGDDPointer mapStsAckStringToGdd(void* v, aitIndex count)
{
    // must be a container
    dbr_stsack_string* db = (dbr_stsack_string*)v;
    aitFixedString* dbv = (aitFixedString*)db->value;
    smartGDDPointer dd = type_table->getDD(gddDbrToAit[DBR_STSACK_STRING].app);
    gdd& vdd = (*dd)[gddAppTypeIndex_dbr_stsack_string_value];
    
    (*dd)[gddAppTypeIndex_dbr_stsack_string_ackt]=db->ackt;
    (*dd)[gddAppTypeIndex_dbr_stsack_string_acks]=db->acks;
    
    vdd.setStatSevr(db->status,db->severity);
    
    if(count==1) {
        if(vdd.dimension()) vdd.clear();
        vdd.put(*dbv);
    } else {
        if(vdd.dimension()!=1) 
            vdd.reset(aitEnumFixedString,1,&count);
        else vdd.setPrimType(aitEnumFixedString);
        vdd.setBound(0,0,count);

        aitFixedString* pCopy = (aitFixedString*) new char [sizeof(aitFixedString)*count];
        memcpy (pCopy,&db->value,sizeof(aitFixedString)*count);
		vdd.putRef(pCopy, new gddDestructor);
    }
    return dd;
}

static int mapStsAckGddToString(void* v, aitIndex count, const gdd &dd, 
                                const gddEnumStringTable &enumStringTable)
{
    dbr_stsack_string* db = (dbr_stsack_string*)v;
    const gdd& vdd = dd[gddAppTypeIndex_dbr_stsack_string_value];
    
    db->ackt=dd[gddAppTypeIndex_dbr_stsack_string_ackt];
    db->acks=dd[gddAppTypeIndex_dbr_stsack_string_acks];
    
    // Unlike all the others, which are dbr_short_t, status and
    // severity for a dbr_stsack_string are dbr_ushort_t.  Have to
    // convert.
    dbr_short_t st,sv;
    vdd.getStatSevr(st,sv);
    db->status = (dbr_ushort_t)st;
    db->severity = (dbr_ushort_t)sv;
    return mapGddToString (&db->value, count, vdd, enumStringTable);
}

// -------------map the ack elements --------------------
static smartGDDPointer mapAcktToGdd(void* v,aitIndex count) {
    dbr_put_ackt_t* sv = (dbr_put_ackt_t*)v;
    smartGDDPointer dd;
    
    // Count should be 1, but leave it general
    if(count>1) {
        dd=new gddAtomic(gddDbrToAit[DBR_PUT_ACKT].app,
            gddDbrToAit[DBR_PUT_ACKT].type,1,count);
        dd->unreference();
        dbr_put_ackt_t* pCopy = (dbr_put_ackt_t*) new char [sizeof(dbr_put_ackt_t)*count];
        memcpy (pCopy,sv,sizeof(dbr_put_ackt_t)*count);
		dd->putRef(pCopy, new gddDestructor);
    } else {
        dd=new gddScalar(gddDbrToAit[DBR_PUT_ACKT].app);
        dd->unreference();
        *dd=*sv;
    }
    return dd;
}

static int mapGddToAckt(void* vd, aitIndex count, const gdd & dd, 
                        const gddEnumStringTable &enumStringTable) {
    dbr_put_ackt_t* sv = (dbr_put_ackt_t*)vd;
    aitIndex sz = dd.getDataSizeElements();
    const void* v=dd.dataVoid();
    int status;
    
    // Count should be 1, but leave it general
    if (count==sz) {
        if (local_data_format==aitLocalDataFormat) {
            if((dbr_put_ackt_t*)v!=sv) {
                status = 
                    aitConvert(aitEnumUint16,sv,
                    dd.primitiveType(),v,sz,&enumStringTable);
            }
            else {
                status = sz*sizeof(dbr_put_ackt_t);
            }
        }
        else {
            status = 
                aitConvertToNet(aitEnumUint16,sv,
                dd.primitiveType(),v,sz, &enumStringTable);
        }
    }
    else {
        status = -1;
    }
    
    return status;
}

static smartGDDPointer mapAcksToGdd(void* v,aitIndex count) {
    dbr_put_acks_t* sv = (dbr_put_acks_t*)v;
    smartGDDPointer dd;
    
    // Count should be 1, but leave it general
    if(count>1) {
        dd=new gddAtomic(gddDbrToAit[DBR_PUT_ACKS].app,
            gddDbrToAit[DBR_PUT_ACKS].type,1,count);
        dd->unreference();
        dbr_put_acks_t* pCopy = (dbr_put_acks_t*) new char [sizeof(dbr_put_acks_t)*count];
        memcpy (pCopy,sv,sizeof(dbr_put_acks_t)*count);
		dd->putRef(pCopy, new gddDestructor);
    } else {
        dd=new gddScalar(gddDbrToAit[DBR_PUT_ACKS].app);
        dd->unreference();
        *dd=*sv;
    }
    return dd;
}

static int mapGddToAcks(void* vd, aitIndex count, const gdd &dd, 
                        const gddEnumStringTable &enumStringTable) {
    dbr_put_acks_t* sv = (dbr_put_acks_t*)vd;
    aitIndex sz = dd.getDataSizeElements();
    const void* v=dd.dataVoid();
    int status;
    
    // Count should be 1, but leave it general
    if (count==sz) {
        if (local_data_format==aitLocalDataFormat) {
            if((dbr_put_acks_t*)v!=sv) {
                status = 
                    aitConvert(aitEnumUint16,sv,
                    dd.primitiveType(),v,sz, &enumStringTable);
            }
            else {
                status = sz*sizeof(dbr_put_acks_t);
            }
        }
        else {
            status = 
                aitConvertToNet(aitEnumUint16,sv,
                dd.primitiveType(),v,sz, &enumStringTable);
        }
    }
    else {
        status = -1;
    }
    
    return status;
}

// -------------map the class name element --------------

static smartGDDPointer mapClassNameToGdd(void* v,aitIndex count) {
    aitFixedString* db = (aitFixedString*)v;
    aitEnum to_type = gddDbrToAit[DBR_CLASS_NAME].type;
    aitUint16 to_app  = gddDbrToAit[DBR_CLASS_NAME].app;
    smartGDDPointer dd;
    
    // Count should be 1, but leave it general
    if(count<=1)
    {
        dd=new gddScalar(to_app,to_type);
        dd->unreference();
        dd->put(*db);
    }
    else
    {
        dd=new gddAtomic(to_app,to_type,1,count);
        dd->unreference();
        aitFixedString* pCopy = (aitFixedString*) new char [sizeof(aitFixedString)*count];
        memcpy (pCopy,db,sizeof(aitFixedString)*count);
		dd->putRef(pCopy, new gddDestructor);
    }
    return dd;
}

static int mapGddToClassName(void* vd, aitIndex count, const gdd & dd, 
                             const gddEnumStringTable &enumStringTable) {
    aitFixedString* db = (aitFixedString*)vd;
    aitIndex sz = dd.getDataSizeElements();
    const void* v = dd.dataVoid();
    int status;
    
    // Count should be 1, but leave it general
    if (count<=sz) {
        if(local_data_format==aitLocalDataFormat) {
            if((aitFixedString*)v!=db) {
                status = 
                    aitConvert(aitEnumFixedString,db,
                    dd.primitiveType(),v,count, &enumStringTable);
            }
            else {
                status = sz*sizeof(aitFixedString);
            }
        }
        else {
            status = 
                aitConvertToNet(aitEnumFixedString,db,
                dd.primitiveType(),v,count, &enumStringTable);
        }
    }
    else {
        status = -1;
    }
    
    return status;
}


// ----------------must run this to use mapping functions--------------
epicsShareFunc void gddMakeMapDBR(gddApplicationTypeTable& tt) { gddMakeMapDBR(&tt); }
epicsShareFunc void gddMakeMapDBR(gddApplicationTypeTable* tt)
{
	type_table=tt;
	size_t i;

	// Storing the DBRxxx type code in the app table will not work
	// for most of the types.  This is because many share the same
	// app type "value", this includes the normal, sts, and time structures

	for(i=0;i<sizeof(gddDbrToAit)/sizeof(gddDbrToAitTable);i++)
	{
		gddDbrToAit[i].app=tt->getApplicationType(gddDbrToAit[i].app_name);
		tt->storeValue(gddDbrToAit[i].app,i);
	}
}

//
// dbMapperFixedStringDestructor::run()
//
// special gddDestructor guarantees same form of new and delete
//
void dbMapperFixedStringDestructor::run (void *pUntyped)
{
	aitFixedString *ps = (aitFixedString *) pUntyped;
	delete [] ps;
}

// An array of one function per DBR structure is provided here so conversions
// can take place quickly by knowing the DBR enumerated type.
//
// C++ will not make a const decl external linkage unless
// we explicitly specify extern
//
extern epicsShareDef const gddDbrMapFuncTable gddMapDbr[DBM_N_DBR_TYPES] = {
    { mapStringToGdd,       mapGddToString },           // DBR_STRING
    { mapShortToGdd,        mapGddToShort },            // DBR_SHORT
    { mapFloatToGdd,        mapGddToFloat },            // DBR_FLOAT
    { mapEnumToGdd,         mapGddToEnum },             // DBR_ENUM
    { mapCharToGdd,         mapGddToChar },             // DBR_CHAR
    { mapLongToGdd,         mapGddToLong },             // DBR_LONG
    { mapDoubleToGdd,       mapGddToDouble },           // DBR_DOUBLE
    { mapStsStringToGdd,    mapStsGddToString },        // DBR_STS_STRING
    { mapStsShortToGdd,     mapStsGddToShort },         // DBR_STS_SHORT
    { mapStsFloatToGdd,     mapStsGddToFloat },         // DBR_STS_FLOAT
    { mapStsEnumToGdd,      mapStsGddToEnum },          // DBR_STS_ENUM
    { mapStsCharToGdd,      mapStsGddToChar },          // DBR_STS_CHAR
    { mapStsLongToGdd,      mapStsGddToLong },          // DBR_STS_LONG
    { mapStsDoubleToGdd,    mapStsGddToDouble },        // DBR_STS_DOUBLE
    { mapTimeStringToGdd,   mapTimeGddToString },       // DBR_TIME_STRING
    { mapTimeShortToGdd,    mapTimeGddToShort },        // DBR_TIME_SHORT
    { mapTimeFloatToGdd,    mapTimeGddToFloat },        // DBR_TIME_FLOAT
    { mapTimeEnumToGdd,     mapTimeGddToEnum },         // DBR_TIME_ENUM
    { mapTimeCharToGdd,     mapTimeGddToChar },         // DBR_TIME_CHAR
    { mapTimeLongToGdd,     mapTimeGddToLong },         // DBR_TIME_LONG
    { mapTimeDoubleToGdd,   mapTimeGddToDouble },       // DBR_TIME_DOUBLE
    { mapStsStringToGdd,    mapStsGddToString },        // DBR_GR_STRING
    { mapGraphicShortToGdd, mapGraphicGddToShort },     // DBR_GR_SHORT
    { mapGraphicFloatToGdd, mapGraphicGddToFloat },     // DBR_GR_FLOAT
    { mapGraphicEnumToGdd,  mapGraphicGddToEnum },      // DBR_GR_ENUM
    { mapGraphicCharToGdd,  mapGraphicGddToChar },      // DBR_GR_CHAR
    { mapGraphicLongToGdd,  mapGraphicGddToLong },      // DBR_GR_LONG
    { mapGraphicDoubleToGdd,mapGraphicGddToDouble },    // DBR_GR_DOUBLE
    { mapStsStringToGdd,    mapStsGddToString },        // DBR_CTRL_STRING
    { mapControlShortToGdd, mapControlGddToShort },     // DBR_CTRL_SHORT
    { mapControlFloatToGdd, mapControlGddToFloat },     // DBR_CTRL_FLOAT
    { mapControlEnumToGdd,  mapControlGddToEnum },      // DBR_CTRL_ENUM
    { mapControlCharToGdd,  mapControlGddToChar },      // DBR_CTRL_CHAR
    { mapControlLongToGdd,  mapControlGddToLong },      // DBR_CTRL_LONG
    { mapControlDoubleToGdd,mapControlGddToDouble },     // DBR_CTRL_DOUBLE
    { mapAcktToGdd,         mapGddToAckt },             // DBR_PUT_ACKT
    { mapAcksToGdd,         mapGddToAcks },             // DBR_PUT_ACKS
    { mapStsAckStringToGdd, mapStsAckGddToString },     // DBR_STSACK_STRING
    { mapClassNameToGdd,    mapGddToClassName }         // DBR_CLASS_NAME
};

#if DBM_N_DBR_TYPES != (LAST_BUFFER_TYPE+1)
#error db mapper is out of sync with db_access.h
#endif

extern epicsShareDef const unsigned gddMapDbrNElem = 
        sizeof(gddMapDbr)/sizeof(gddDbrToAit[0]);

