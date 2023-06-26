/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/** @file menu.h
 * @brief Adds db menu capabilities
 * @see @ref menu
 */

#ifndef INCmenuH
#define INCmenuH

#include "menuFtype.h"
#include "epicsTypes.h"

/*Returns the variable size of the menuFtype*/
epicsEnum16 menuEpicsTypeSizes(menuFtype type)
{
	switch (type)
	{
	case menuFtypeSTRING:
		return sizeof(epicsString);
	case menuFtypeCHAR:
		return sizeof(epicsInt8);
	case menuFtypeUCHAR:
		return sizeof(epicsUInt8);
	case menuFtypeSHORT:
		return sizeof(epicsInt16);
	case menuFtypeUSHORT:
		return sizeof(epicsUInt16);
	case menuFtypeLONG:
		return sizeof(epicsInt32);
	case menuFtypeULONG:
		return sizeof(epicsUInt32);
	case menuFtypeINT64:
		return sizeof(epicsInt64);
	case menuFtypeUINT64:
		return sizeof(epicsUInt64);
	case menuFtypeFLOAT:
		return sizeof(epicsFloat32);
	case menuFtypeDOUBLE:
		return sizeof(epicsFloat64);
	case menuFtypeENUM:
		return sizeof(epicsEnum16);
	default:
		return 0;
	}
}

#endif
