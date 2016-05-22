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

#define epicsExportSharedSymbols
#include "gddAppTable.h"

// useless utility function
gddApplicationTypeTable* gddGenerateApplicationTypeTable(long x/*=(1<<13)*/)
{
	gddApplicationTypeTable* tt = new gddApplicationTypeTable((unsigned int)x);
	return tt;
}

void gddApplicationTypeTable::GenerateTypes(void)
{
	gddScalar* add_units = new gddScalar(0,aitEnumString);

	// One attempt at building a menu using aitString, which is not so
	// good.
	// gddAtomic* add_enum = new gddAtomic(0,aitEnumString,1,16);
	// aitString add_enum_buf[16];
	// add_enum->putRef(add_enum_buf);

	// Just describe the menu - allow the block of choiced to be
	// referenced in.
	// gddAtomic* add_enum = new gddAtomic(0,aitEnumFixedString,1,16);

	// ----------------------------------------------------------------
	// register simple types

	// should I allow a dd retrieved through the tag table to be adjusted
	// even though it is flattened?  What if an array needs to change size

	// or type in a flattened dd?

	// value - filled by user no dd to register
	// units - variable length of array of chars

	// not really required attributes - they are contained in DDs
	registerApplicationType(GDD_NAME_STATUS);
	registerApplicationType(GDD_NAME_SEVERITY);
	registerApplicationType(GDD_NAME_TIME_STAMP);
	registerApplicationType(GDD_NAME_PV_NAME);
    registerApplicationType(GDD_NAME_CLASS);

	// required attributes
	int type_prec=registerApplicationType(GDD_NAME_PRECISION);
	int type_ghigh=registerApplicationType(GDD_NAME_GRAPH_HIGH);
	int type_glow=registerApplicationType(GDD_NAME_GRAPH_LOW);
	int type_chigh=registerApplicationType(GDD_NAME_CONTROL_HIGH);
	int type_clow=registerApplicationType(GDD_NAME_CONTROL_LOW);
	int type_ahigh=registerApplicationType(GDD_NAME_ALARM_HIGH);
	int type_alow=registerApplicationType(GDD_NAME_ALARM_LOW);
	int type_awhigh=registerApplicationType(GDD_NAME_ALARM_WARN_HIGH);
	int type_awlow=registerApplicationType(GDD_NAME_ALARM_WARN_LOW);
	int type_maxele=registerApplicationType(GDD_NAME_MAX_ELEMENTS);
	int type_value=registerApplicationType(GDD_NAME_VALUE);
	int type_menu=registerApplicationType(GDD_NAME_ENUM);
	int type_units=registerApplicationTypeWithProto(GDD_NAME_UNITS,add_units);
    int type_ackt=registerApplicationType(GDD_NAME_ACKT);
    int type_acks=registerApplicationType(GDD_NAME_ACKS);

	// old menu method
	// int type_menu=registerApplicationType(GDD_NAME_ENUM);

	// ----------------------------------------------------------------
	// register container types - not as easy

	// container of all PV attributes
	gddContainer* cdd_attr=new gddContainer(1);
	cdd_attr->insert(getDD(type_prec)); 
	cdd_attr->insert(getDD(type_ghigh)); 
	cdd_attr->insert(getDD(type_glow)); 
	cdd_attr->insert(getDD(type_chigh)); 
	cdd_attr->insert(getDD(type_clow)); 
	cdd_attr->insert(getDD(type_ahigh)); 
	cdd_attr->insert(getDD(type_alow)); 
	cdd_attr->insert(getDD(type_awhigh)); 
	cdd_attr->insert(getDD(type_awlow)); 
	cdd_attr->insert(getDD(type_units)); 
	cdd_attr->insert(getDD(type_maxele)); 
	registerApplicationTypeWithProto(GDD_NAME_ATTRIBUTES,cdd_attr);

	// container of everything about a PV
	gddContainer* cdd_all=new gddContainer(1);
	cdd_all->insert(getDD(type_prec)); 
	cdd_all->insert(getDD(type_ghigh)); 
	cdd_all->insert(getDD(type_glow)); 
	cdd_all->insert(getDD(type_chigh)); 
	cdd_all->insert(getDD(type_clow)); 
	cdd_all->insert(getDD(type_ahigh)); 
	cdd_all->insert(getDD(type_alow)); 
	cdd_all->insert(getDD(type_awhigh)); 
	cdd_all->insert(getDD(type_awlow)); 
	cdd_all->insert(getDD(type_units)); 
	cdd_all->insert(getDD(type_value)); 
	registerApplicationTypeWithProto(GDD_NAME_ALL,cdd_all);

	// ------------- generate required dbr containers -----------------

	// DBR_GR_SHORT
	gddContainer* cdd_gr_short=new gddContainer(0);
	cdd_gr_short->insert(new gddScalar(type_value,aitEnumInt16)); 
	cdd_gr_short->insert(new gddScalar(type_ghigh,aitEnumInt16)); 
	cdd_gr_short->insert(new gddScalar(type_glow,aitEnumInt16)); 
	cdd_gr_short->insert(new gddScalar(type_ahigh,aitEnumInt16)); 
	cdd_gr_short->insert(new gddScalar(type_alow,aitEnumInt16)); 
	cdd_gr_short->insert(new gddScalar(type_awhigh,aitEnumInt16)); 
	cdd_gr_short->insert(new gddScalar(type_awlow,aitEnumInt16)); 
	cdd_gr_short->insert(getDD(type_units)); 
	registerApplicationTypeWithProto("dbr_gr_short",cdd_gr_short);

	// DBR_GR_FLOAT
	gddContainer* cdd_gr_float=new gddContainer(0);
	cdd_gr_float->insert(new gddScalar(type_value,aitEnumFloat32)); 
	cdd_gr_float->insert(new gddScalar(type_prec,aitEnumInt16)); 
	cdd_gr_float->insert(new gddScalar(type_ghigh,aitEnumFloat32)); 
	cdd_gr_float->insert(new gddScalar(type_glow,aitEnumFloat32)); 
	cdd_gr_float->insert(new gddScalar(type_ahigh,aitEnumFloat32)); 
	cdd_gr_float->insert(new gddScalar(type_alow,aitEnumFloat32)); 
	cdd_gr_float->insert(new gddScalar(type_awhigh,aitEnumFloat32)); 
	cdd_gr_float->insert(new gddScalar(type_awlow,aitEnumFloat32)); 
	cdd_gr_float->insert(getDD(type_units)); 
	registerApplicationTypeWithProto("dbr_gr_float",cdd_gr_float);

	// DBR_GR_ENUM
	gddContainer* cdd_gr_enum=new gddContainer(0);
	// old: cdd_gr_enum->insert(new gddAtomic(type_menu,aitEnumFixedString,1)); 
	cdd_gr_enum->insert(getDD(type_menu)); 
	cdd_gr_enum->insert(new gddScalar(type_value,aitEnumEnum16)); 
	registerApplicationTypeWithProto("dbr_gr_enum",cdd_gr_enum);

	// DBR_GR_CHAR
	gddContainer* cdd_gr_char=new gddContainer(0);
	cdd_gr_char->insert(new gddScalar(type_value,aitEnumInt8)); 
	cdd_gr_char->insert(new gddScalar(type_ghigh,aitEnumInt8)); 
	cdd_gr_char->insert(new gddScalar(type_glow,aitEnumInt8)); 
	cdd_gr_char->insert(new gddScalar(type_ahigh,aitEnumInt8)); 
	cdd_gr_char->insert(new gddScalar(type_alow,aitEnumInt8)); 
	cdd_gr_char->insert(new gddScalar(type_awhigh,aitEnumInt8)); 
	cdd_gr_char->insert(new gddScalar(type_awlow,aitEnumInt8)); 
	cdd_gr_char->insert(getDD(type_units)); 
	registerApplicationTypeWithProto("dbr_gr_char",cdd_gr_char);

	// DBR_GR_LONG
	gddContainer* cdd_gr_long=new gddContainer(0);
	cdd_gr_long->insert(new gddScalar(type_value,aitEnumInt32)); 
	cdd_gr_long->insert(new gddScalar(type_ghigh,aitEnumInt32)); 
	cdd_gr_long->insert(new gddScalar(type_glow,aitEnumInt32)); 
	cdd_gr_long->insert(new gddScalar(type_ahigh,aitEnumInt32)); 
	cdd_gr_long->insert(new gddScalar(type_alow,aitEnumInt32)); 
	cdd_gr_long->insert(new gddScalar(type_awhigh,aitEnumInt32)); 
	cdd_gr_long->insert(new gddScalar(type_awlow,aitEnumInt32)); 
	cdd_gr_long->insert(getDD(type_units)); 
	registerApplicationTypeWithProto("dbr_gr_long",cdd_gr_long);

	// DBR_GR_DOUBLE
	gddContainer* cdd_gr_double=new gddContainer(0);
	cdd_gr_double->insert(new gddScalar(type_value,aitEnumFloat64)); 
	cdd_gr_double->insert(new gddScalar(type_prec,aitEnumInt16)); 
	cdd_gr_double->insert(new gddScalar(type_ghigh,aitEnumFloat64)); 
	cdd_gr_double->insert(new gddScalar(type_glow,aitEnumFloat64)); 
	cdd_gr_double->insert(new gddScalar(type_ahigh,aitEnumFloat64)); 
	cdd_gr_double->insert(new gddScalar(type_alow,aitEnumFloat64)); 
	cdd_gr_double->insert(new gddScalar(type_awhigh,aitEnumFloat64)); 
	cdd_gr_double->insert(new gddScalar(type_awlow,aitEnumFloat64)); 
	cdd_gr_double->insert(getDD(type_units)); 
	registerApplicationTypeWithProto("dbr_gr_double",cdd_gr_double);

	// DBR_CTRL_SHORT
	gddContainer* cdd_ctrl_short=new gddContainer(0);
	cdd_ctrl_short->insert(new gddScalar(type_value,aitEnumInt16)); 
	cdd_ctrl_short->insert(new gddScalar(type_ghigh,aitEnumInt16)); 
	cdd_ctrl_short->insert(new gddScalar(type_glow,aitEnumInt16)); 
	cdd_ctrl_short->insert(new gddScalar(type_chigh,aitEnumInt16)); 
	cdd_ctrl_short->insert(new gddScalar(type_clow,aitEnumInt16)); 
	cdd_ctrl_short->insert(new gddScalar(type_ahigh,aitEnumInt16)); 
	cdd_ctrl_short->insert(new gddScalar(type_alow,aitEnumInt16)); 
	cdd_ctrl_short->insert(new gddScalar(type_awhigh,aitEnumInt16)); 
	cdd_ctrl_short->insert(new gddScalar(type_awlow,aitEnumInt16)); 
	cdd_ctrl_short->insert(getDD(type_units)); 
	registerApplicationTypeWithProto("dbr_ctrl_short",cdd_ctrl_short);

	// DBR_CTRL_FLOAT
	gddContainer* cdd_ctrl_float=new gddContainer(0);
	cdd_ctrl_float->insert(new gddScalar(type_value,aitEnumFloat32)); 
	cdd_ctrl_float->insert(new gddScalar(type_prec,aitEnumInt16)); 
	cdd_ctrl_float->insert(new gddScalar(type_ghigh,aitEnumFloat32)); 
	cdd_ctrl_float->insert(new gddScalar(type_glow,aitEnumFloat32)); 
	cdd_ctrl_float->insert(new gddScalar(type_chigh,aitEnumFloat32)); 
	cdd_ctrl_float->insert(new gddScalar(type_clow,aitEnumFloat32)); 
	cdd_ctrl_float->insert(new gddScalar(type_ahigh,aitEnumFloat32)); 
	cdd_ctrl_float->insert(new gddScalar(type_alow,aitEnumFloat32)); 
	cdd_ctrl_float->insert(new gddScalar(type_awhigh,aitEnumFloat32)); 
	cdd_ctrl_float->insert(new gddScalar(type_awlow,aitEnumFloat32)); 
	cdd_ctrl_float->insert(getDD(type_units)); 
	registerApplicationTypeWithProto("dbr_ctrl_float",cdd_ctrl_float);

	// DBR_CTRL_ENUM
	gddContainer* cdd_ctrl_enum=new gddContainer(0);
	//old:cdd_ctrl_enum->insert(new gddAtomic(type_menu,aitEnumFixedString,1)); 
	cdd_ctrl_enum->insert(getDD(type_menu)); 
	cdd_ctrl_enum->insert(new gddScalar(type_value,aitEnumEnum16)); 
	registerApplicationTypeWithProto("dbr_ctrl_enum",cdd_ctrl_enum);

	// DBR_CTRL_CHAR
	gddContainer* cdd_ctrl_char=new gddContainer(0);
	cdd_ctrl_char->insert(new gddScalar(type_value,aitEnumInt8)); 
	cdd_ctrl_char->insert(new gddScalar(type_ghigh,aitEnumInt8)); 
	cdd_ctrl_char->insert(new gddScalar(type_glow,aitEnumInt8)); 
	cdd_ctrl_char->insert(new gddScalar(type_chigh,aitEnumInt8)); 
	cdd_ctrl_char->insert(new gddScalar(type_clow,aitEnumInt8)); 
	cdd_ctrl_char->insert(new gddScalar(type_ahigh,aitEnumInt8)); 
	cdd_ctrl_char->insert(new gddScalar(type_alow,aitEnumInt8)); 
	cdd_ctrl_char->insert(new gddScalar(type_awhigh,aitEnumInt8)); 
	cdd_ctrl_char->insert(new gddScalar(type_awlow,aitEnumInt8)); 
	cdd_ctrl_char->insert(getDD(type_units)); 
	registerApplicationTypeWithProto("dbr_ctrl_char",cdd_ctrl_char);

	// DBR_CTRL_LONG
	gddContainer* cdd_ctrl_long=new gddContainer(0);
	cdd_ctrl_long->insert(new gddScalar(type_value,aitEnumInt32)); 
	cdd_ctrl_long->insert(new gddScalar(type_ghigh,aitEnumInt32)); 
	cdd_ctrl_long->insert(new gddScalar(type_glow,aitEnumInt32)); 
	cdd_ctrl_long->insert(new gddScalar(type_chigh,aitEnumInt32)); 
	cdd_ctrl_long->insert(new gddScalar(type_clow,aitEnumInt32)); 
	cdd_ctrl_long->insert(new gddScalar(type_ahigh,aitEnumInt32)); 
	cdd_ctrl_long->insert(new gddScalar(type_alow,aitEnumInt32)); 
	cdd_ctrl_long->insert(new gddScalar(type_awhigh,aitEnumInt32)); 
	cdd_ctrl_long->insert(new gddScalar(type_awlow,aitEnumInt32)); 
	cdd_ctrl_long->insert(getDD(type_units)); 
	registerApplicationTypeWithProto("dbr_ctrl_long",cdd_ctrl_long);

	// DBR_CTRL_DOUBLE
	gddContainer* cdd_ctrl_double=new gddContainer(0);
	cdd_ctrl_double->insert(new gddScalar(type_value,aitEnumFloat64)); 
	cdd_ctrl_double->insert(new gddScalar(type_prec,aitEnumInt16)); 
	cdd_ctrl_double->insert(new gddScalar(type_ghigh,aitEnumFloat64)); 
	cdd_ctrl_double->insert(new gddScalar(type_glow,aitEnumFloat64)); 
	cdd_ctrl_double->insert(new gddScalar(type_chigh,aitEnumFloat64)); 
	cdd_ctrl_double->insert(new gddScalar(type_clow,aitEnumFloat64)); 
	cdd_ctrl_double->insert(new gddScalar(type_ahigh,aitEnumFloat64)); 
	cdd_ctrl_double->insert(new gddScalar(type_alow,aitEnumFloat64)); 
	cdd_ctrl_double->insert(new gddScalar(type_awhigh,aitEnumFloat64)); 
	cdd_ctrl_double->insert(new gddScalar(type_awlow,aitEnumFloat64)); 
	cdd_ctrl_double->insert(getDD(type_units)); 
	registerApplicationTypeWithProto("dbr_ctrl_double",cdd_ctrl_double);

    // DBR_STSACK_STRING
    gddContainer* cdd_stsack_string=new gddContainer(0);
    cdd_stsack_string->insert(new gddScalar(type_value,aitEnumString)); 
    cdd_stsack_string->insert(new gddScalar(type_acks,aitEnumUint16)); 
    cdd_stsack_string->insert(new gddScalar(type_ackt,aitEnumUint16)); 
    registerApplicationTypeWithProto("dbr_stsack_string",cdd_stsack_string);
}

