/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef GDD_APPLTYPE_TABLE_H
#define GDD_APPLTYPE_TABLE_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 */

#include "gdd.h"
#include "gddUtils.h"
#include <stdio.h>

// must be power of 2 for group size
#define APPLTABLE_GROUP_SIZE 64
#define APPLTABLE_GROUP_SIZE_POW 6

// default set of application type names
#define GDD_UNITS_SIZE 8
#define GDD_NAME_UNITS              "units"
#define GDD_NAME_MAX_ELEMENTS       "maxElements"
#define GDD_NAME_PRECISION          "precision"
#define GDD_NAME_GRAPH_HIGH         "graphicHigh"
#define GDD_NAME_GRAPH_LOW          "graphicLow"
#define GDD_NAME_CONTROL_HIGH       "controlHigh"
#define GDD_NAME_CONTROL_LOW        "controlLow"
#define GDD_NAME_ALARM_HIGH         "alarmHigh"
#define GDD_NAME_ALARM_LOW          "alarmLow"
#define GDD_NAME_ALARM_WARN_HIGH    "alarmHighWarning"
#define GDD_NAME_ALARM_WARN_LOW     "alarmLowWarning"
#define GDD_NAME_VALUE              "value"
#define GDD_NAME_ENUM               "enums"
#define GDD_NAME_MENUITEM           "menuitem"
#define GDD_NAME_STATUS             "status"
#define GDD_NAME_SEVERITY           "severity"
#define GDD_NAME_TIME_STAMP         "timeStamp"
#define GDD_NAME_ALL                "all"
#define GDD_NAME_ATTRIBUTES         "attributes"
#define GDD_NAME_PV_NAME            "name"
#define GDD_NAME_ACKT               "ackt"
#define GDD_NAME_ACKS               "acks"
#define GDD_NAME_CLASS              "class"

typedef enum
{
	gddApplicationTypeUndefined,
	gddApplicationTypeProto,
	gddApplicationTypeNormal
} gddApplicationTypeType;

class gddApplicationTypeTable;

class gddApplicationTypeDestructor : public gddDestructor
{
public:
	gddApplicationTypeDestructor(gddApplicationTypeTable* v) : gddDestructor(v) { }
	void run(void*);
};

class gddApplicationTypeElement
{
public:
	gddApplicationTypeElement(void);
	~gddApplicationTypeElement(void);

	char* app_name;
	size_t proto_size;
	aitIndex total_dds;
	gdd* proto;
	gdd* free_list;
	epicsMutex sem;
	gddApplicationTypeType type;
	aitUint32 user_value;
	aitUint16* map;
	aitUint16 map_size;
};

// The app table allows registering a prototype DD for app.  This class
// allows the user to ask for a DD given a app.  The class will manage
// a free list of DD buffers if the app has a prototype DD
// associated with it.  The gddApplicationTypeDestructor allows the DD to be
// returned to the free list for the DD app in the app table.

class epicsShareClass gddApplicationTypeTable
{
public:
	gddApplicationTypeTable(aitUint32 total_number_of_apps=(1<<9));
	~gddApplicationTypeTable(void);

	// standard method
	gddStatus registerApplicationType(const char* const name, aitUint32& app);
	gddStatus registerApplicationTypeWithProto(const char* const name,
		gdd* protoDD, aitUint32& app);

	// alternative method to register types
	aitUint32 registerApplicationType(const char* const name);
	aitUint32 registerApplicationTypeWithProto(const char* const name,
		gdd* protoDD);

	// hashing still not used for string names to app types
	aitUint32 getApplicationType(const char* const name) const;
	char* getName(aitUint32 app) const;
	gddStatus mapAppToIndex(aitUint32 container_app,
		aitUint32 app_to_map, aitUint32& index);

	// copy as best as possible from src to dest, one of the gdd must be
	// managed for this to succeed
	gddStatus smartCopy(gdd* dest, const gdd* src);
	gddStatus smartRef(gdd* dest, const gdd* src);

	// old style interface
	int tagC2I(const char* const ctag, int& tag);
	int tagC2I(const char* const ctag, int* const tag);
	int tagI2C(int tag, char* const ctag);
	int insertApplicationType(int tag, const char* ctag);

	gdd* getDD(aitUint32 app);

	// This should be a protected function, users should
	// always unreference a DD.  Probably can occurs if there is another
	// manager in the system.  This function cannot distinguish between
	// a DD managed by it or someone else.  The AppDestructor will call
	// this function indirectly by unreferencing the DD.
	gddStatus freeDD(gdd*);

	aitUint32 maxAttributes(void) const	  { return max_allowed; }
	aitUint32 totalregistered(void) const { return total_registered; }
	void describe(FILE*);
	gddStatus storeValue(aitUint32 app, aitUint32 user_value);
	aitUint32 getValue(aitUint32 app);

	static gddApplicationTypeTable& AppTable(void);
	static gddApplicationTypeTable app_table;

protected:
	void GenerateTypes(void);

	gddStatus copyDD_src(gdd& dest, const gdd& src);
	gddStatus copyDD_dest(gdd& dest, const gdd& src);
	gddStatus refDD_src(gdd& dest, const gdd& src);
	gddStatus refDD_dest(gdd& dest, const gdd& src);

private:
	gddStatus splitApplicationType(aitUint32 r,aitUint32& g,aitUint32& a) const;
	aitUint32 group(aitUint32 rapp) const;
	aitUint32 index(aitUint32 rapp) const;
	int describeDD(gddContainer* dd, FILE* fd, int level, char* tn);

	aitUint32 total_registered;
	aitUint32 max_allowed;
	aitUint32 max_groups;

	gddApplicationTypeElement** attr_table;
	epicsMutex sem;
};

inline aitUint32 gddApplicationTypeTable::group(aitUint32 rapp) const
	{ return (rapp&(~(APPLTABLE_GROUP_SIZE-1)))>>APPLTABLE_GROUP_SIZE_POW; }
inline aitUint32 gddApplicationTypeTable::index(aitUint32 rapp) const
	{ return rapp&(APPLTABLE_GROUP_SIZE-1); }

inline gddStatus gddApplicationTypeTable::splitApplicationType(aitUint32 rapp, 
	aitUint32& g, aitUint32& app) const
{
	gddStatus rc=0;
	g=group(rapp);
	app=index(rapp);
	if(rapp>=total_registered)
	{
		rc=gddErrorOutOfBounds;
		gddAutoPrint("gddAppTable::splitApplicationType()",rc);
	}
	return rc;
}

inline aitUint32 gddApplicationTypeTable::registerApplicationType(
	const char* const name)
{
	aitUint32 app;
	registerApplicationType(name,app);
	return app;
}

inline aitUint32 gddApplicationTypeTable::registerApplicationTypeWithProto(
	const char* const name, gdd* protoDD)
{
	aitUint32 app;
	registerApplicationTypeWithProto(name,protoDD,app);
	return app;
}

gddApplicationTypeTable* gddGenerateApplicationTypeTable(aitUint32 x=(1<<10));

#endif

