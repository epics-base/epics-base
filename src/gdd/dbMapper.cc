// Author: Jim Kowalkowski
// Date: 2/96
// 
// $Id$
// 
// $Log$
// Revision 1.13  1996/08/23 20:29:35  jbk
// completed fixes for the aitString and fixed string management
//
// Revision 1.12  1996/08/22 21:05:43  jbk
// More fixes to make strings and fixed string work better.
//
// Revision 1.11  1996/08/14 12:30:12  jbk
// fixes for converting aitString to aitInt8* and back
// fixes for managing the units field for the dbr types
//
// Revision 1.10  1996/08/13 15:07:45  jbk
// changes for better string manipulation and fixes for the units field
//
// Revision 1.9  1996/08/06 19:14:10  jbk
// Fixes to the string class.
// Changes units field to a aitString instead of aitInt8.
//
// Revision 1.8  1996/07/26 02:23:15  jbk
// Fixed the spelling error with Scalar.
//
// Revision 1.7  1996/07/24 22:48:06  jhill
// fixed gnu warning int=>size_t
//
// Revision 1.6  1996/07/23 17:13:30  jbk
// various fixes - dbmapper incorrectly worked with enum types
//
// Revision 1.5  1996/07/01 19:59:12  jhill
// fixed case where gdd was mapped to a string without cvrt
//
// Revision 1.4  1996/06/26 21:00:06  jbk
// Fixed up code in aitHelpers, removed unused variables in others
// Fixed potential problem in gddAppTable.cc with the map functions
//
// Revision 1.3  1996/06/26 02:42:05  jbk
// more correction to the aitString processing - testing menus
//
// Revision 1.2  1996/06/25 19:18:12  jbk
// moved from extensions to base - fixed trouble in dbMapper.cc
//
// Revision 1.1  1996/06/25 19:11:34  jbk
// new in EPICS base
//
//

// *Revision 1.5  1996/06/25 18:59:00  jbk
// *more fixes for the aitString management functions and mapping menus
// *Revision 1.4  1996/06/24 03:15:30  jbk
// *name changes and fixes for aitString and fixed string functions
// *Revision 1.3  1996/06/17 15:24:08  jbk
// *many mods, string class corrections.
// *gdd operator= protection.
// *dbMapper uses aitString array for menus now
// *Revision 1.2  1996/06/13 21:31:54  jbk
// *Various fixes and correction - including ref_cnt change to unsigned short
// *Revision 1.1  1996/05/31 13:15:23  jbk
// *add new stuff

#define DB_MAPPER_SOURCE 1
#include <stdio.h>

#include "gddApps.h"
#include "gddAppTable.h"
#include "dbMapper.h"
// #include "templates/dbMapperTempl.h"

// hardcoded in same order as aitConvert.h
// no way to detect a string type!!!!!!!

static gddApplicationTypeTable* type_table = NULL;

const chtype gddAitToDbr[] = {
	0,
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
	DBR_STRING,
	999
};

gddDbrToAitTable gddDbrToAit[] = {
	// normal
	{ aitEnumFixedString,	0,	"value" },
	{ aitEnumInt16,		0,	"value" },
	{ aitEnumFloat32,	0,	"value" },
	{ aitEnumEnum16,	0,	"value" },
	{ aitEnumInt8,		0,	"value" },
	{ aitEnumInt32,		0,	"value" },
	{ aitEnumFloat64,	0,	"value" },
	// STS
	{ aitEnumFixedString,	0,	"value" },
	{ aitEnumInt16,		0,	"value" },
	{ aitEnumFloat32,	0,	"value" },
	{ aitEnumEnum16,	0,	"value" },
	{ aitEnumInt8,		0,	"value" },
	{ aitEnumInt32,		0,	"value" },
	{ aitEnumFloat64,	0,	"value" },
	// TIME
	{ aitEnumFixedString,	0,	"value" },
	{ aitEnumInt16,		0,	"value" },
	{ aitEnumFloat32,	0,	"value" },
	{ aitEnumEnum16,	0,	"value" },
	{ aitEnumInt8,		0,	"value" },
	{ aitEnumInt32,		0,	"value" },
	{ aitEnumFloat64,	0,	"value" },
	// Graphic
	{ aitEnumFixedString,	0,	"value" },
	{ aitEnumInt16,		0,	"dbr_gr_short" },
	{ aitEnumFloat32,	0,	"dbr_gr_float" },
	{ aitEnumEnum16,	0,	"dbr_gr_enum" },
	{ aitEnumInt8,		0,	"dbr_gr_char" },
	{ aitEnumInt32,		0,	"dbr_gr_long" },
	{ aitEnumFloat64,	0,	"dbr_gr_double" },
	// control
	{ aitEnumFixedString,	0,	"value" },
	{ aitEnumInt16,		0,	"dbr_ctrl_short" },
	{ aitEnumFloat32,	0,	"dbr_ctrl_float" },
	{ aitEnumEnum16,	0,	"dbr_ctrl_enum" },
	{ aitEnumInt8,		0,	"dbr_ctrl_char" },
	{ aitEnumInt32,		0,	"dbr_ctrl_long" },
	{ aitEnumFloat64,	0,	"dbr_ctrl_double" }
};

// I generated a container for each of the important DBR types.  This
// includes all the control and graphic structures.  The others are
// not needed become you can get time stamp and status in each gdd.
// Currently the containers are build here with c++ code.

// string type needs to be written special and not use template
// maybe create a template for the string type
// the problem is that the value of a string structure is an array,
// not just a single element

static gdd* mapStringToGdd(void* v,aitIndex count) {
	aitFixedString* db = (aitFixedString*)v;
	aitEnum to_type = gddDbrToAit[DBR_STRING].type;
	aitUint16 to_app  = gddDbrToAit[DBR_STRING].app;
	gdd* dd;

	if(count<=1)
	{
		dd=new gddScalar(to_app,to_type);
		dd->put(*db);
	}
	else
	{
		dd=new gddAtomic(to_app,to_type,1,count);
		dd->putRef(db);
	}
	return dd;
}

static int mapGddToString(void* v, gdd* dd) {
	aitFixedString* db = (aitFixedString*)v;
	aitFixedString* dbx;

	if(dd->isScalar())
		dbx=(aitFixedString*)dd->dataAddress();
	else
		dbx=(aitFixedString*)dd->dataPointer();

	int len = dd->getDataSizeElements();
	if(dbx!=db) dd->get(*db);
	return len;
}

static gdd* mapShortToGdd(void* v,aitIndex count) {
	dbr_short_t* sv = (dbr_short_t*)v;
	gdd* dd;

	if(count>1) {
		dd=new gddAtomic(gddDbrToAit[DBR_SHORT].app,
			gddDbrToAit[DBR_SHORT].type,1,count);
		dd->putRef(sv);
	} else {
		dd=new gddScalar(gddDbrToAit[DBR_SHORT].app);
		*dd=*sv;
	}
	return dd;
}

static int mapGddToShort(void* v, gdd* dd) {
	dbr_short_t* sv = (dbr_short_t*)v;
    int sz=1;

	if(dd->getBounds()) {
		sz=dd->getDataSizeElements();
		if(dd->dataPointer()!=sv)
			memcpy(sv,dd->dataPointer(),dd->getDataSizeBytes());
	} else
		*sv=*dd;
	return sz;
}

static gdd* mapFloatToGdd(void* v,aitIndex count) {
	dbr_float_t* sv = (dbr_float_t*)v;
	gdd* dd;

	if(count>1) {
		dd=new gddAtomic(gddDbrToAit[DBR_FLOAT].app,
			gddDbrToAit[DBR_FLOAT].type,1,count);
		dd->putRef(sv);
	} else {
		dd=new gddScalar(gddDbrToAit[DBR_FLOAT].app);
		*dd=*sv;
	}
	return dd;
}

static int mapGddToFloat(void* v, gdd* dd) {
	dbr_float_t* sv = (dbr_float_t*)v;
    int sz=1;

	if(dd->getBounds()) {
		sz=dd->getDataSizeElements();
		if(dd->dataPointer()!=sv)
			memcpy(sv,dd->dataPointer(),dd->getDataSizeBytes());
	} else
		*sv=*dd;
	return sz;
}

static gdd* mapEnumToGdd(void* v,aitIndex count) {
	dbr_enum_t* sv = (dbr_enum_t*)v;
	gdd* dd;

	if(count>1) {
		dd=new gddAtomic(gddDbrToAit[DBR_ENUM].app,
			gddDbrToAit[DBR_ENUM].type,1,count);
		dd->putRef(sv);
	} else {
		dd=new gddScalar(gddDbrToAit[DBR_ENUM].app);
		*dd=*sv;
	}
	return dd;
}

static int mapGddToEnum(void* v, gdd* dd) {
	dbr_enum_t* sv = (dbr_enum_t*)v;
	aitEnum16* e;
    int sz=1;

	if(dd->dimension()) {
		dd->getRef(e);
		*sv=*e;
	} else
		*sv=*dd;
	return sz;
}

static gdd* mapCharToGdd(void* v,aitIndex count) {
	dbr_char_t* sv = (dbr_char_t*)v;
	gdd* dd;

	if(count>1) {
		dd=new gddAtomic(gddDbrToAit[DBR_CHAR].app,
			gddDbrToAit[DBR_CHAR].type,1,count);
		dd->putRef(sv);
	} else {
		dd=new gddScalar(gddDbrToAit[DBR_CHAR].app);
		*dd=*sv;
	}
	return dd;
}

static int mapGddToChar(void* v, gdd* dd) {
	dbr_char_t* sv = (dbr_char_t*)v;
    int sz=1;

	if(dd->dimension()) {
		sz=dd->getDataSizeElements();
		if(dd->dataPointer()!=sv)
			memcpy(sv,dd->dataPointer(),dd->getDataSizeBytes());
	} else
		*sv=*dd;
	return sz;
}

static gdd* mapLongToGdd(void* v,aitIndex count) {
	dbr_long_t* sv = (dbr_long_t*)v;
	gdd* dd;

	if(count>1) {
		dd=new gddAtomic(gddDbrToAit[DBR_LONG].app,
			gddDbrToAit[DBR_LONG].type,1,count);
		dd->putRef(sv);
	} else {
		dd=new gddScalar(gddDbrToAit[DBR_LONG].app);
		*dd=*sv;
	}
	return dd;
}

static int mapGddToLong(void* v, gdd* dd) {
	dbr_long_t* sv = (dbr_long_t*)v;
    int sz=1;

	if(dd->dimension()) {
		sz=dd->getDataSizeElements();
		if(dd->dataPointer()!=sv)
			memcpy(sv,dd->dataPointer(),dd->getDataSizeBytes());
	} else
		*sv=*dd;
	return sz;
}

static gdd* mapDoubleToGdd(void* v,aitIndex count) {
	dbr_double_t* sv = (dbr_double_t*)v;
	gdd* dd;

	if(count>1) {
		dd=new gddAtomic(gddDbrToAit[DBR_DOUBLE].app,
			gddDbrToAit[DBR_DOUBLE].type,1,count);
		dd->putRef(sv);
	} else {
		dd=new gddScalar(gddDbrToAit[DBR_DOUBLE].app);
		*dd=*sv;
	}
	return dd;
}

static int mapGddToDouble(void* v, gdd* dd) {
	dbr_double_t* sv = (dbr_double_t*)v;
    int sz=1;

	if(dd->dimension()) {
		sz=dd->getDataSizeElements();
		if(dd->dataPointer()!=sv)
			memcpy(sv,dd->dataPointer(),dd->getDataSizeBytes());
	} else
		*sv=*dd;
	return sz;
}

// ********************************************************************
//                      sts structure mappings
// ********************************************************************

static gdd* mapStsStringToGdd(void* v,aitIndex count)
{
	dbr_sts_string* db = (dbr_sts_string*)v;
	aitFixedString* dbv = (aitFixedString*)db->value;
	aitEnum to_type = gddDbrToAit[DBR_STS_STRING].type;
	aitUint16 to_app  = gddDbrToAit[DBR_STS_STRING].app;
	gdd* dd;

	if(count<=1)
	{
		dd=new gddScalar(to_app,to_type);
		dd->put(*dbv);
	}
	else
	{
		dd=new gddAtomic(to_app,to_type,1,count);
		dd->putRef(dbv);
	}

	dd->setStatSevr(db->status,db->severity);
	return dd;
}

static int mapStsGddToString(void* v, gdd* dd)
{
	dbr_sts_string* db = (dbr_sts_string*)v;
	aitFixedString* dbv = (aitFixedString*)db->value;
	aitFixedString* dbx;
	
	if(dd->isScalar())
		dbx=(aitFixedString*)dd->dataAddress();
	else
		dbx=(aitFixedString*)dd->dataPointer();

	// copy string into user buffer for now if not the same as one in gdd
	if(dbx!=dbv) dd->get(*dbv);
	dd->getStatSevr(db->status,db->severity);
	return dd->getDataSizeElements();
}

static gdd* mapStsShortToGdd(void* v,aitIndex count)
{
	dbr_sts_short* dbv = (dbr_sts_short*)v;
	gdd* dd=mapShortToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToShort(void* v, gdd* dd)
{
	dbr_sts_short* dbv = (dbr_sts_short*)v;
	int sz=mapGddToShort(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	return sz;
}

static gdd* mapStsFloatToGdd(void* v,aitIndex count)
{
	dbr_sts_float* dbv = (dbr_sts_float*)v;
	gdd* dd=mapFloatToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToFloat(void* v, gdd* dd)
{
	dbr_sts_float* dbv = (dbr_sts_float*)v;
	int sz=mapGddToFloat(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	return sz;
}

static gdd* mapStsEnumToGdd(void* v,aitIndex count)
{
	dbr_sts_enum* dbv = (dbr_sts_enum*)v;
	gdd* dd=mapEnumToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToEnum(void* v, gdd* dd)
{
	dbr_sts_enum* dbv = (dbr_sts_enum*)v;
	int sz=mapGddToEnum(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	return sz;
}

static gdd* mapStsCharToGdd(void* v,aitIndex count)
{
	dbr_sts_char* dbv = (dbr_sts_char*)v;
	gdd* dd=mapCharToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToChar(void* v, gdd* dd)
{
	dbr_sts_char* dbv = (dbr_sts_char*)v;
	int sz=mapGddToChar(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	return sz;
}

static gdd* mapStsLongToGdd(void* v,aitIndex count)
{
	dbr_sts_long* dbv = (dbr_sts_long*)v;
	gdd* dd=mapLongToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToLong(void* v, gdd* dd)
{
	dbr_sts_long* dbv = (dbr_sts_long*)v;
	int sz=mapGddToLong(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	return sz;
}

static gdd* mapStsDoubleToGdd(void* v,aitIndex count)
{
	dbr_sts_double* dbv = (dbr_sts_double*)v;
	gdd* dd=mapDoubleToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	return dd;
}

static int mapStsGddToDouble(void* v, gdd* dd)
{
	dbr_sts_double* dbv = (dbr_sts_double*)v;
	int sz=mapGddToDouble(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	return sz;
}

// ********************************************************************
//                      time structure mappings
// ********************************************************************

static gdd* mapTimeStringToGdd(void* v,aitIndex count)
{
	dbr_time_string* db = (dbr_time_string*)v;
	aitFixedString* dbv = (aitFixedString*)db->value;
	aitEnum to_type = gddDbrToAit[DBR_TIME_STRING].type;
	aitUint16 to_app  = gddDbrToAit[DBR_TIME_STRING].app;
	gdd* dd;

	if(count<=1)
	{
		dd=new gddScalar(to_app,to_type);
		dd->put(*dbv);
	}
	else
	{
		dd=new gddAtomic(to_app,to_type,1,count);
		dd->putRef(dbv);
	}

	dd->setStatSevr(db->status,db->severity);
	dd->setTimeStamp((aitTimeStamp*)&db->stamp);
	return dd;
}

static int mapTimeGddToString(void* v, gdd* dd)
{
	dbr_time_string* db = (dbr_time_string*)v;
	aitFixedString* dbv = (aitFixedString*)db->value;
	aitFixedString* dbx;

	// copy string into user buffer for now if not the same as one in gdd
	if(dd->isScalar())
		dbx=(aitFixedString*)dd->dataAddress();
	else
		dbx=(aitFixedString*)dd->dataPointer();

	if(dbv!=dbx) dd->get(*dbv);

	dd->getStatSevr(db->status,db->severity);
	dd->getTimeStamp((aitTimeStamp*)&db->stamp);
	return dd->getDataSizeBytes();
}

static gdd* mapTimeShortToGdd(void* v,aitIndex count)
{
	dbr_time_short* dbv = (dbr_time_short*)v;
	gdd* dd=mapShortToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp((aitTimeStamp*)&dbv->stamp);
	return dd;
}

static int mapTimeGddToShort(void* v, gdd* dd)
{
	dbr_time_short* dbv = (dbr_time_short*)v;
	int sz=mapGddToShort(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	dd->getTimeStamp((aitTimeStamp*)&dbv->stamp);
	return sz;
}

static gdd* mapTimeFloatToGdd(void* v,aitIndex count)
{
	dbr_time_float* dbv = (dbr_time_float*)v;
	gdd* dd=mapFloatToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp((aitTimeStamp*)&dbv->stamp);
	return dd;
}

static int mapTimeGddToFloat(void* v, gdd* dd)
{
	dbr_time_float* dbv = (dbr_time_float*)v;
	int sz=mapGddToFloat(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	dd->getTimeStamp((aitTimeStamp*)&dbv->stamp);
	return sz;
}

static gdd* mapTimeEnumToGdd(void* v,aitIndex count)
{
	dbr_time_enum* dbv = (dbr_time_enum*)v;
	gdd* dd=mapEnumToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp((aitTimeStamp*)&dbv->stamp);
	return dd;
}

static int mapTimeGddToEnum(void* v, gdd* dd)
{
	dbr_time_enum* dbv = (dbr_time_enum*)v;
	int sz=mapGddToEnum(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	dd->getTimeStamp((aitTimeStamp*)&dbv->stamp);
	return sz;
}

static gdd* mapTimeCharToGdd(void* v,aitIndex count)
{
	dbr_time_char* dbv = (dbr_time_char*)v;
	gdd* dd=mapCharToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp((aitTimeStamp*)&dbv->stamp);
	return dd;
}

static int mapTimeGddToChar(void* v, gdd* dd)
{
	dbr_time_char* dbv = (dbr_time_char*)v;
	int sz=mapGddToChar(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	dd->getTimeStamp((aitTimeStamp*)&dbv->stamp);
	return sz;
}

static gdd* mapTimeLongToGdd(void* v,aitIndex count)
{
	dbr_time_long* dbv = (dbr_time_long*)v;
	gdd* dd=mapLongToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp((aitTimeStamp*)&dbv->stamp);
	return dd;
}

static int mapTimeGddToLong(void* v, gdd* dd)
{
	dbr_time_long* dbv = (dbr_time_long*)v;
	int sz=mapGddToLong(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	dd->getTimeStamp((aitTimeStamp*)&dbv->stamp);
	return sz;
}

static gdd* mapTimeDoubleToGdd(void* v,aitIndex count)
{
	dbr_time_double* dbv = (dbr_time_double*)v;
	gdd* dd=mapDoubleToGdd(&dbv->value,count);
	dd->setStatSevr(dbv->status,dbv->severity);
	dd->setTimeStamp((aitTimeStamp*)&dbv->stamp);
	return dd;
}

static int mapTimeGddToDouble(void* v, gdd* dd)
{
	dbr_time_double* dbv = (dbr_time_double*)v;
	int sz=mapGddToDouble(&dbv->value,dd);
	dd->getStatSevr(dbv->status,dbv->severity);
	dd->getTimeStamp((aitTimeStamp*)&dbv->stamp);
	return sz;
}

// ********************************************************************
//                      graphic structure mappings
// ********************************************************************

// -------------map the short structures----------------
static gdd* mapGraphicShortToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_gr_short* db = (dbr_gr_short*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_GR_SHORT].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_short_value];

	aitString* str=NULL;
	dd[gddAppTypeIndex_dbr_gr_short_units].getRef(str);
	str->installString(db->units);

	dd[gddAppTypeIndex_dbr_gr_short_graphicLow]=db->lower_disp_limit;
	dd[gddAppTypeIndex_dbr_gr_short_graphicHigh]=db->upper_disp_limit;
	dd[gddAppTypeIndex_dbr_gr_short_alarmLow]=db->lower_alarm_limit;
	dd[gddAppTypeIndex_dbr_gr_short_alarmHigh]=db->upper_alarm_limit;
	dd[gddAppTypeIndex_dbr_gr_short_alarmLowWarning]=db->lower_warning_limit;
	dd[gddAppTypeIndex_dbr_gr_short_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);
		vdd.putRef(&db->value);
	}
	return dd;
}

static gdd* mapControlShortToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_ctrl_short* db = (dbr_ctrl_short*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_CTRL_SHORT].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_short_value];

	aitString* str = NULL;
	dd[gddAppTypeIndex_dbr_ctrl_short_units].getRef(str);
	str->installString(db->units);

	dd[gddAppTypeIndex_dbr_ctrl_short_graphicLow]=db->lower_disp_limit;
	dd[gddAppTypeIndex_dbr_ctrl_short_graphicHigh]=db->upper_disp_limit;
	dd[gddAppTypeIndex_dbr_ctrl_short_controlLow]=db->lower_ctrl_limit;
	dd[gddAppTypeIndex_dbr_ctrl_short_controlHigh]=db->upper_ctrl_limit;
	dd[gddAppTypeIndex_dbr_ctrl_short_alarmLow]=db->lower_alarm_limit;
	dd[gddAppTypeIndex_dbr_ctrl_short_alarmHigh]=db->upper_alarm_limit;
	dd[gddAppTypeIndex_dbr_ctrl_short_alarmLowWarning]=db->lower_warning_limit;
	dd[gddAppTypeIndex_dbr_ctrl_short_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);
		vdd.putRef(&db->value);
	}
	return dd;
}

static int mapGraphicGddToShort(void* v, gdd* dd)
{
	dbr_gr_short* db = (dbr_gr_short*)v;
	int sz=1;
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_short_value];

	aitString* str;
	dd[gddAppTypeIndex_dbr_gr_short_units].getRef(str);
	if(str->string()) strcpy(db->units,str->string());

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_gr_short_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_gr_short_graphicHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_gr_short_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_gr_short_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_gr_short_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_gr_short_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);

	if(vdd.dimension()) {
		sz=vdd.getDataSizeElements();
		if(vdd.dataPointer()!=v)
			memcpy(&db->value, vdd.dataPointer(), vdd.getDataSizeBytes());
	} else
		db->value=vdd;
	return sz;
}

static int mapControlGddToShort(void* v, gdd* dd)
{
	dbr_ctrl_short* db = (dbr_ctrl_short*)v;
	int sz=1;
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_short_value];

	aitString* str;
	dd[gddAppTypeIndex_dbr_ctrl_short_units].getRef(str);
	if(str->string()) strcpy(db->units,str->string());

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_short_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_short_graphicHigh];
	db->lower_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_short_controlLow];
	db->upper_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_short_controlHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_short_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_short_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_short_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_short_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);

	if(vdd.dimension()) {
		sz=vdd.getDataSizeElements();
		if(vdd.dataPointer()!=v)
			memcpy(&db->value, vdd.dataPointer(), vdd.getDataSizeBytes());
	} else
		db->value=vdd;
	return sz;
}

// -------------map the float structures----------------
static gdd* mapGraphicFloatToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_gr_float* db = (dbr_gr_float*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_GR_FLOAT].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_float_value];

	aitString* str = NULL;
	dd[gddAppTypeIndex_dbr_gr_float_units].getRef(str);
	str->installString(db->units);

	dd[gddAppTypeIndex_dbr_gr_float_precision]=db->precision;
	dd[gddAppTypeIndex_dbr_gr_float_graphicLow]=db->lower_disp_limit;
	dd[gddAppTypeIndex_dbr_gr_float_graphicHigh]=db->upper_disp_limit;
	dd[gddAppTypeIndex_dbr_gr_float_alarmLow]=db->lower_alarm_limit;
	dd[gddAppTypeIndex_dbr_gr_float_alarmHigh]=db->upper_alarm_limit;
	dd[gddAppTypeIndex_dbr_gr_float_alarmLowWarning]=db->lower_warning_limit;
	dd[gddAppTypeIndex_dbr_gr_float_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);
		vdd.putRef(&db->value);
	}
	return dd;
}

static gdd* mapControlFloatToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_ctrl_float* db = (dbr_ctrl_float*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_CTRL_FLOAT].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_float_value];

	aitString* str = NULL;
	dd[gddAppTypeIndex_dbr_ctrl_float_units].getRef(str);
	str->installString(db->units);

	dd[gddAppTypeIndex_dbr_ctrl_float_precision]=db->precision;
	dd[gddAppTypeIndex_dbr_ctrl_float_graphicLow]=db->lower_disp_limit;
	dd[gddAppTypeIndex_dbr_ctrl_float_graphicHigh]=db->upper_disp_limit;
	dd[gddAppTypeIndex_dbr_ctrl_float_controlLow]=db->lower_ctrl_limit;
	dd[gddAppTypeIndex_dbr_ctrl_float_controlHigh]=db->upper_ctrl_limit;
	dd[gddAppTypeIndex_dbr_ctrl_float_alarmLow]=db->lower_alarm_limit;
	dd[gddAppTypeIndex_dbr_ctrl_float_alarmHigh]=db->upper_alarm_limit;
	dd[gddAppTypeIndex_dbr_ctrl_float_alarmLowWarning]=db->lower_warning_limit;
	dd[gddAppTypeIndex_dbr_ctrl_float_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);
		vdd.putRef(&db->value);
	}
	return dd;
}

static int mapGraphicGddToFloat(void* v, gdd* dd)
{
	dbr_gr_float* db = (dbr_gr_float*)v;
	int sz=1;
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_float_value];

	aitString* str;
	dd[gddAppTypeIndex_dbr_gr_float_units].getRef(str);
	if(str->string()) strcpy(db->units,str->string());

	db->precision=dd[gddAppTypeIndex_dbr_gr_float_precision];
	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_gr_float_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_gr_float_graphicHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_gr_float_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_gr_float_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_gr_float_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_gr_float_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);

	if(vdd.dimension()) {
		sz=vdd.getDataSizeElements();
		if(vdd.dataPointer()!=v)
			memcpy(&db->value, vdd.dataPointer(), vdd.getDataSizeBytes());
	} else
		db->value=vdd;
	return sz;
}

static int mapControlGddToFloat(void* v, gdd* dd)
{
	dbr_ctrl_float* db = (dbr_ctrl_float*)v;
	int sz=1;
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_float_value];

	aitString* str;
	dd[gddAppTypeIndex_dbr_ctrl_float_units].getRef(str);
	if(str->string()) strcpy(db->units,str->string());

	db->precision=dd[gddAppTypeIndex_dbr_ctrl_float_precision];
	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_float_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_float_graphicHigh];
	db->lower_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_float_controlLow];
	db->upper_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_float_controlHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_float_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_float_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_float_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_float_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);

	if(vdd.dimension()) {
		sz=vdd.getDataSizeElements();
		if(vdd.dataPointer()!=v)
			memcpy(&db->value, vdd.dataPointer(), vdd.getDataSizeBytes());
	} else
		db->value=vdd;
	return sz;
}

// -------------map the enum structures----------------
static gdd* mapGraphicEnumToGdd(void* v, aitIndex /*count*/)
{
	dbr_gr_enum* db = (dbr_gr_enum*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_GR_ENUM].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_enum_value];
	gdd& menu = dd[gddAppTypeIndex_dbr_gr_enum_enums];
	aitFixedString* str = menu;
	aitFixedString* f = (aitFixedString*)db->strs;
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
		menu.putRef(str,new gddDestructor);
	}
	else
	{
		if((sz=menu.getDataSizeElements())>db->no_str)
			sz=db->no_str;
	}

	for(i=0;i<sz;i++) strcpy(str[i].fixed_string,&(db->strs[i][0]));
	menu.setBound(0,0,sz);

	// should always be a scaler
	if(vdd.dimension()) vdd.clear();
	vdd=db->value;
	vdd.setStatSevr(db->status,db->severity);
	return dd;
}

static gdd* mapControlEnumToGdd(void* v, aitIndex /*count*/)
{
	dbr_ctrl_enum* db = (dbr_ctrl_enum*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_CTRL_ENUM].app);
	gdd& menu = dd[gddAppTypeIndex_dbr_ctrl_enum_enums];
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_enum_value];
	aitFixedString* str = menu;
	aitFixedString* f = (aitFixedString*)db->strs;
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
		menu.putRef(str,new gddDestructor);
	}
	else
	{
		if((sz=menu.getDataSizeElements())>db->no_str)
			sz=db->no_str;
	}

	for(i=0;i<sz;i++) strcpy(str[i].fixed_string,&(db->strs[i][0]));
	menu.setBound(0,0,sz);

	// should always be a scaler
	if(vdd.dimension()) vdd.clear();
	vdd=db->value;
	vdd.setStatSevr(db->status,db->severity);
	return dd;
}

static int mapGraphicGddToEnum(void* v, gdd* dd)
{
	dbr_gr_enum* db = (dbr_gr_enum*)v;
	gdd& menu = dd[gddAppTypeIndex_dbr_gr_enum_enums];
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_enum_value];
	aitFixedString* str = menu;
	aitFixedString* f = (aitFixedString*)db->strs;
	int i;

	vdd.getStatSevr(db->status,db->severity);
	db->value=vdd; // always scaler
	db->no_str=menu.getDataSizeElements();

	if(str && str!=f)
	{
		for(i=0;i<db->no_str;i++)
			strcpy(&(db->strs[i][0]),str[i].fixed_string);
	}
	return 1;
}

static int mapControlGddToEnum(void* v, gdd* dd)
{
	dbr_ctrl_enum* db = (dbr_ctrl_enum*)v;
	gdd& menu = dd[gddAppTypeIndex_dbr_ctrl_enum_enums];
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_enum_value];
	aitFixedString* str = menu;
	aitFixedString* f = (aitFixedString*)db->strs;
	int i;

	vdd.getStatSevr(db->status,db->severity);
	db->value=vdd; // always scaler
	db->no_str=menu.getDataSizeElements();

	if(str && str!=f)
	{
		for(i=0;i<db->no_str;i++)
			strcpy(&(db->strs[i][0]),str[i].fixed_string);
	}
	return 1;
}

// -------------map the char structures----------------
static gdd* mapGraphicCharToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_gr_char* db = (dbr_gr_char*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_GR_CHAR].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_char_value];

	aitString* str = NULL;
	dd[gddAppTypeIndex_dbr_gr_char_units].getRef(str);
	str->installString(db->units);

	dd[gddAppTypeIndex_dbr_gr_char_graphicLow]=db->lower_disp_limit;
	dd[gddAppTypeIndex_dbr_gr_char_graphicHigh]=db->upper_disp_limit;
	dd[gddAppTypeIndex_dbr_gr_char_alarmLow]=db->lower_alarm_limit;
	dd[gddAppTypeIndex_dbr_gr_char_alarmHigh]=db->upper_alarm_limit;
	dd[gddAppTypeIndex_dbr_gr_char_alarmLowWarning]=db->lower_warning_limit;
	dd[gddAppTypeIndex_dbr_gr_char_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);
		vdd.putRef(&db->value);
	}
	return dd;
}

static gdd* mapControlCharToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_ctrl_char* db = (dbr_ctrl_char*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_CTRL_CHAR].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_char_value];

	aitString* str = NULL;
	dd[gddAppTypeIndex_dbr_ctrl_char_units].getRef(str);
	str->installString(db->units);

	dd[gddAppTypeIndex_dbr_ctrl_char_graphicLow]=db->lower_disp_limit;
	dd[gddAppTypeIndex_dbr_ctrl_char_graphicHigh]=db->upper_disp_limit;
	dd[gddAppTypeIndex_dbr_ctrl_char_controlLow]=db->lower_ctrl_limit;
	dd[gddAppTypeIndex_dbr_ctrl_char_controlHigh]=db->upper_ctrl_limit;
	dd[gddAppTypeIndex_dbr_ctrl_char_alarmLow]=db->lower_alarm_limit;
	dd[gddAppTypeIndex_dbr_ctrl_char_alarmHigh]=db->upper_alarm_limit;
	dd[gddAppTypeIndex_dbr_ctrl_char_alarmLowWarning]=db->lower_warning_limit;
	dd[gddAppTypeIndex_dbr_ctrl_char_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);
		vdd.putRef(&db->value);
	}
	return dd;
}

static int mapGraphicGddToChar(void* v, gdd* dd)
{
	dbr_gr_char* db = (dbr_gr_char*)v;
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_char_value];
	int sz=1;

	aitString* str;
	dd[gddAppTypeIndex_dbr_gr_char_units].getRef(str);
	if(str->string()) strcpy(db->units,str->string());

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_gr_char_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_gr_char_graphicHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_gr_char_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_gr_char_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_gr_char_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_gr_char_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);

	if(vdd.dimension()) {
		sz=vdd.getDataSizeElements();
		if(vdd.dataPointer()!=v)
			memcpy(&db->value, vdd.dataPointer(), vdd.getDataSizeBytes());
	} else
		db->value=vdd;
	return sz;
}

static int mapControlGddToChar(void* v, gdd* dd)
{
	dbr_ctrl_char* db = (dbr_ctrl_char*)v;
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_char_value];
	int sz=1;

	aitString* str;
	dd[gddAppTypeIndex_dbr_ctrl_char_units].getRef(str);
	if(str->string()) strcpy(db->units,str->string());

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_char_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_char_graphicHigh];
	db->lower_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_char_controlLow];
	db->upper_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_char_controlHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_char_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_char_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_char_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_char_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);

	if(vdd.dimension()) {
		sz=vdd.getDataSizeElements();
		if(vdd.dataPointer()!=v)
			memcpy(&db->value, vdd.dataPointer(), vdd.getDataSizeBytes());
	} else
		db->value=vdd;
	return sz;
}

// -------------map the long structures----------------
static gdd* mapGraphicLongToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_gr_long* db = (dbr_gr_long*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_GR_LONG].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_long_value];

	aitString* str = NULL;
	dd[gddAppTypeIndex_dbr_gr_long_units].getRef(str);
	str->installString(db->units);

	dd[gddAppTypeIndex_dbr_gr_long_graphicLow]=db->lower_disp_limit;
	dd[gddAppTypeIndex_dbr_gr_long_graphicHigh]=db->upper_disp_limit;
	dd[gddAppTypeIndex_dbr_gr_long_alarmLow]=db->lower_alarm_limit;
	dd[gddAppTypeIndex_dbr_gr_long_alarmHigh]=db->upper_alarm_limit;
	dd[gddAppTypeIndex_dbr_gr_long_alarmLowWarning]=db->lower_warning_limit;
	dd[gddAppTypeIndex_dbr_gr_long_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);
		vdd.putRef(&db->value);
	}
	return dd;
}

static gdd* mapControlLongToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_ctrl_long* db = (dbr_ctrl_long*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_CTRL_LONG].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_long_value];

	aitString* str = NULL;
	dd[gddAppTypeIndex_dbr_ctrl_long_units].getRef(str);
	str->installString(db->units);

	dd[gddAppTypeIndex_dbr_ctrl_long_graphicLow]=db->lower_disp_limit;
	dd[gddAppTypeIndex_dbr_ctrl_long_graphicHigh]=db->upper_disp_limit;
	dd[gddAppTypeIndex_dbr_ctrl_long_controlLow]=db->lower_ctrl_limit;
	dd[gddAppTypeIndex_dbr_ctrl_long_controlHigh]=db->upper_ctrl_limit;
	dd[gddAppTypeIndex_dbr_ctrl_long_alarmLow]=db->lower_alarm_limit;
	dd[gddAppTypeIndex_dbr_ctrl_long_alarmHigh]=db->upper_alarm_limit;
	dd[gddAppTypeIndex_dbr_ctrl_long_alarmLowWarning]=db->lower_warning_limit;
	dd[gddAppTypeIndex_dbr_ctrl_long_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);
		vdd.putRef(&db->value);
	}
	return dd;
}

static int mapGraphicGddToLong(void* v, gdd* dd)
{
	dbr_gr_long* db = (dbr_gr_long*)v;
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_long_value];
	int sz=1;

	aitString* str;
	dd[gddAppTypeIndex_dbr_gr_long_units].getRef(str);
	if(str->string()) strcpy(db->units,str->string());

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_gr_long_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_gr_long_graphicHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_gr_long_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_gr_long_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_gr_long_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_gr_long_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);

	if(vdd.dimension()) {
		sz=vdd.getDataSizeElements();
		if(vdd.dataPointer()!=v)
			memcpy(&db->value, vdd.dataPointer(), vdd.getDataSizeBytes());
	} else
		db->value=vdd;
	return sz;
}

static int mapControlGddToLong(void* v, gdd* dd)
{
	dbr_ctrl_long* db = (dbr_ctrl_long*)v;
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_long_value];
	int sz=1;

	aitString* str;
	dd[gddAppTypeIndex_dbr_ctrl_long_units].getRef(str);
	if(str->string()) strcpy(db->units,str->string());

	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_long_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_long_graphicHigh];
	db->lower_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_long_controlLow];
	db->upper_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_long_controlHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_long_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_long_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_long_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_long_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);

	if(vdd.dimension()) {
		sz=vdd.getDataSizeElements();
		if(vdd.dataPointer()!=v)
			memcpy(&db->value, vdd.dataPointer(), vdd.getDataSizeBytes());
	} else
		db->value=vdd;
	return sz;
}

// -------------map the double structures----------------
static gdd* mapGraphicDoubleToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_gr_double* db = (dbr_gr_double*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_GR_DOUBLE].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_double_value];

	aitString* str = NULL;
	dd[gddAppTypeIndex_dbr_gr_double_units].getRef(str);
	str->installString(db->units);

	dd[gddAppTypeIndex_dbr_gr_double_precision]=db->precision;
	dd[gddAppTypeIndex_dbr_gr_double_graphicLow]=db->lower_disp_limit;
	dd[gddAppTypeIndex_dbr_gr_double_graphicHigh]=db->upper_disp_limit;
	dd[gddAppTypeIndex_dbr_gr_double_alarmLow]=db->lower_alarm_limit;
	dd[gddAppTypeIndex_dbr_gr_double_alarmHigh]=db->upper_alarm_limit;
	dd[gddAppTypeIndex_dbr_gr_double_alarmLowWarning]=db->lower_warning_limit;
	dd[gddAppTypeIndex_dbr_gr_double_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);
		vdd.putRef(&db->value);
	}
	return dd;
}

static gdd* mapControlDoubleToGdd(void* v, aitIndex count)
{
	// must be a container
	dbr_ctrl_double* db = (dbr_ctrl_double*)v;
	gdd* dd = type_table->getDD(gddDbrToAit[DBR_CTRL_DOUBLE].app);
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_double_value];

	aitString* str = NULL;
	dd[gddAppTypeIndex_dbr_ctrl_double_units].getRef(str);
	str->installString(db->units);

	dd[gddAppTypeIndex_dbr_ctrl_double_precision]=db->precision;
	dd[gddAppTypeIndex_dbr_ctrl_double_graphicLow]=db->lower_disp_limit;
	dd[gddAppTypeIndex_dbr_ctrl_double_graphicHigh]=db->upper_disp_limit;
	dd[gddAppTypeIndex_dbr_ctrl_double_controlLow]=db->lower_ctrl_limit;
	dd[gddAppTypeIndex_dbr_ctrl_double_controlHigh]=db->upper_ctrl_limit;
	dd[gddAppTypeIndex_dbr_ctrl_double_alarmLow]=db->lower_alarm_limit;
	dd[gddAppTypeIndex_dbr_ctrl_double_alarmHigh]=db->upper_alarm_limit;
	dd[gddAppTypeIndex_dbr_ctrl_double_alarmLowWarning]=db->lower_warning_limit;
	dd[gddAppTypeIndex_dbr_ctrl_double_alarmHighWarning]=db->upper_warning_limit;

	vdd.setStatSevr(db->status,db->severity);

	if(count==1) {
		if(vdd.dimension()) vdd.clear();
		vdd=db->value;
	} else {
		if(vdd.dimension()!=1) vdd.reset(aitEnumInt16,1,&count);
		else vdd.setPrimType(aitEnumInt16);
		vdd.setBound(0,0,count);
		vdd.putRef(&db->value);
	}
	return dd;
}

static int mapGraphicGddToDouble(void* v, gdd* dd)
{
	dbr_gr_double* db = (dbr_gr_double*)v;
	gdd& vdd = dd[gddAppTypeIndex_dbr_gr_double_value];
	int sz=1;

	aitString* str;
	dd[gddAppTypeIndex_dbr_gr_double_units].getRef(str);
	if(str->string()) strcpy(db->units,str->string());

	db->precision=dd[gddAppTypeIndex_dbr_gr_double_precision];
	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_gr_double_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_gr_double_graphicHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_gr_double_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_gr_double_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_gr_double_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_gr_double_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);

	if(vdd.dimension()) {
		sz=vdd.getDataSizeElements();
		if(vdd.dataPointer()!=v)
			memcpy(&db->value, vdd.dataPointer(), vdd.getDataSizeBytes());
	} else
		db->value=vdd;
	return sz;
}

static int mapControlGddToDouble(void* v, gdd* dd)
{
	dbr_ctrl_double* db = (dbr_ctrl_double*)v;
	gdd& vdd = dd[gddAppTypeIndex_dbr_ctrl_double_value];
	int sz=1;

	aitString* str;
	dd[gddAppTypeIndex_dbr_ctrl_double_units].getRef(str);
	if(str->string()) strcpy(db->units,str->string());

	db->precision=dd[gddAppTypeIndex_dbr_ctrl_double_precision];
	db->lower_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_double_graphicLow];
	db->upper_disp_limit=dd[gddAppTypeIndex_dbr_ctrl_double_graphicHigh];
	db->lower_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_double_controlLow];
	db->upper_ctrl_limit=dd[gddAppTypeIndex_dbr_ctrl_double_controlHigh];
	db->lower_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_double_alarmLow];
	db->upper_alarm_limit=dd[gddAppTypeIndex_dbr_ctrl_double_alarmHigh];
	db->lower_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_double_alarmLowWarning];
	db->upper_warning_limit=dd[gddAppTypeIndex_dbr_ctrl_double_alarmHighWarning];

	vdd.getStatSevr(db->status,db->severity);

	if(vdd.dimension()) {
		sz=vdd.getDataSizeElements();
		if(vdd.dataPointer()!=v)
			memcpy(&db->value, vdd.dataPointer(), vdd.getDataSizeBytes());
	} else
		db->value=vdd;
	return sz;
}

// ----------------must run this to use mapping functions--------------
void gddMakeMapDBR(gddApplicationTypeTable* tt);
void gddMakeMapDBR(gddApplicationTypeTable& tt) { gddMakeMapDBR(&tt); }
void gddMakeMapDBR(gddApplicationTypeTable* tt)
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

// An array of one function per DBR structure is provided here so conversions
// can take place quickly by knowing the DBR enumerated type.

gddDbrMapFuncTable gddMapDbr[] = {
	{ mapStringToGdd,		mapGddToString },			// DBR_STRING
	{ mapShortToGdd,		mapGddToShort },			// DBR_SHORT
	{ mapFloatToGdd,		mapGddToFloat },			// DBR_FLOAT
	{ mapEnumToGdd,			mapGddToEnum },				// DBR_ENUM
	{ mapCharToGdd,			mapGddToChar },				// DBR_CHAR
	{ mapLongToGdd,			mapGddToLong },				// DBR_LONG
	{ mapDoubleToGdd,		mapGddToDouble },			// DBR_DOUBLE
	{ mapStsStringToGdd,	mapStsGddToString },		// DBR_STS_STRING
	{ mapStsShortToGdd,		mapStsGddToShort },			// DBR_STS_SHORT
	{ mapStsFloatToGdd,		mapStsGddToFloat },			// DBR_STS_FLOAT
	{ mapStsEnumToGdd,		mapStsGddToEnum },			// DBR_STS_ENUM
	{ mapStsCharToGdd,		mapStsGddToChar },			// DBR_STS_CHAR
	{ mapStsLongToGdd,		mapStsGddToLong },			// DBR_STS_LONG
	{ mapStsDoubleToGdd,	mapStsGddToDouble },		// DBR_STS_DOUBLE
	{ mapTimeStringToGdd,	mapTimeGddToString },		// DBR_TIME_STRING
	{ mapTimeShortToGdd,	mapTimeGddToShort },		// DBR_TIME_SHORT
	{ mapTimeFloatToGdd,	mapTimeGddToFloat },		// DBR_TIME_FLOAT
	{ mapTimeEnumToGdd,		mapTimeGddToEnum },			// DBR_TIME_ENUM
	{ mapTimeCharToGdd,		mapTimeGddToChar },			// DBR_TIME_CHAR
	{ mapTimeLongToGdd,		mapTimeGddToLong },			// DBR_TIME_LONG
	{ mapTimeDoubleToGdd,	mapTimeGddToDouble },		// DBR_TIME_DOUBLE
	{ mapStsStringToGdd,	mapStsGddToString },		// DBR_GR_STRING
	{ mapGraphicShortToGdd,	mapGraphicGddToShort },		// DBR_GR_SHORT
	{ mapGraphicFloatToGdd,	mapGraphicGddToFloat },		// DBR_GR_FLOAT
	{ mapGraphicEnumToGdd,	mapGraphicGddToEnum },		// DBR_GR_ENUM
	{ mapGraphicCharToGdd,	mapGraphicGddToChar },		// DBR_GR_CHAR
	{ mapGraphicLongToGdd,	mapGraphicGddToLong },		// DBR_GR_LONG
	{ mapGraphicDoubleToGdd,mapGraphicGddToDouble },	// DBR_GR_DOUBLE
	{ mapStsStringToGdd,	mapStsGddToString },		// DBR_CTRL_STRING
	{ mapControlShortToGdd,	mapControlGddToShort },		// DBR_CTRL_SHORT
	{ mapControlFloatToGdd,	mapControlGddToFloat },		// DBR_CTRL_FLOAT
	{ mapControlEnumToGdd,	mapControlGddToEnum },		// DBR_CTRL_ENUM
	{ mapControlCharToGdd,	mapControlGddToChar },		// DBR_CTRL_CHAR
	{ mapControlLongToGdd,	mapControlGddToLong },		// DBR_CTRL_LONG
	{ mapControlDoubleToGdd,mapControlGddToDouble }		// DBR_CTRL_DOUBLE
};

