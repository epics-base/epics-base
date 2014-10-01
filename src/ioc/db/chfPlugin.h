/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     für Materialien und Energie GmbH.
* Copyright (c) 2014 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@gmx.de>
 */

/* Based on the linkoptions utility by Michael Davidsaver (BNL) */

#ifndef CHFPLUGIN_H
#define CHFPLUGIN_H

#include <shareLib.h>
#include <dbDefs.h>
#include <epicsTypes.h>
#include <dbChannel.h>

struct db_field_log;

/** @file chfPlugin.h
 * @brief Channel filter simplified plugins.
 *
 * Utility layer to allow an easier (reduced) interface for
 * channel filter plugins.
 *
 * Parsing the configuration arguments of a channel filter plugin
 * is done according to an argument description table provided by the plugin.
 * The parser stores the results directly into a user supplied structure
 * after appropriate type conversion.
 *
 * To specify the arguments, a chfPluginArgDef table must be defined
 * for the user structure. This table has to be specified when the plugin registers.
 *
 * The plugin is responsible to register an init function using
 * epicsExportRegistrar() and the accompanying registrar() directive in the dbd,
 * and call chfPluginRegister() from within the init function.
 *
 * For example:
 *
 * typedef struct myStruct {
 *   ... other stuff
 *   char       mode;
 *   epicsInt32 ival;
 *   double     dval;
 *   epicsInt32 ival2;
 *   int        enumval;
 *   char       strval[20];
 *   char       boolval;
 * } myStruct;
 *
 * static const
 * chfPluginEnumType colorEnum[] = { {"Red",1}, {"Green",2}, {"Blue",3}, {NULL,0} };
 *
 * static const
 * chfPluginDef myStructDef[] = {
 *   chfTagInt32(myStruct, ival,    "Integer" , ival2, 3, 0, 0),
 *   chfInt32   (myStruct, ival2,   "Second"  , 1, 0),
 *   chfDouble  (myStruct, dval,    "Double"  , 1, 0),
 *   chfString  (myStruct, strval , "String"  , 1, 0),
 *   chfEnum    (myStruct, enumval, "Color"   , 1, 0, colorEnum),
 *   chfBoolean (myStruct, boolval, "Bool"    , 1, 0),
 *   chfPluginEnd
 * };
 *
 * Note: The 4th argument specifies the parameter to be required (1) or optional (0),
 *       the 5th whether converting to the required type is allowed (1), or
 *       type mismatches are an error (0).
 * Note: The "Tag" version has two additional arguments. the 4th arg specifies the tag
 *       field (integer type) inside the structure to be set, the 5th arg specifies the
 *       value to set the tag field to. Arguments 6 and 7 specify "required" and
 *       "conversion" as described above.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Channel filter simplified plugin interface.
 *
 * The routines in this structure must be implemented by each filter plugin.
 */
typedef struct chfPluginIf {

    /* Memory management */
    /** @brief Allocate private resources.
     *
     * <em>Called before parsing starts.</em>
     * The plugin should allocate its per-instance structures,
     * returning a pointer to them or NULL requesting an abort of the operation.
     *
     * allocPvt may be set to NULL, if no resource allocation is needed.
     *
     * @return Pointer to private structure, NULL if operation is to be aborted.
     */
    void * (* allocPvt) (void);

    /** @brief Free private resources.
     *
     * <em>Called as part of abort or shutdown.</em>
     * The plugin should release any resources allocated for this filter;
     * no further calls through this interface will be made.
     *
     * freePvt may be set to NULL, if no resources need to be released.
     *
     * @param pvt Pointer to private structure.
     */
    void (* freePvt) (void *pvt);

    /* Parameter parsing results */
    /** @brief A parsing error occurred.
     *
     * <em>Called after parsing failed with an error.</em>
     *
     * @param pvt Pointer to private structure.
     */
    void (* parse_error) (void *pvt);

    /** @brief Configuration has been parsed successfully.
     *
     * <em>Called after parsing has finished ok.</em>
     * The plugin may check the validity of the parsed data,
     * returning -1 to request an abort of the operation.
     *
     * @param pvt Pointer to private structure.
     * @return 0 for success, -1 if operation is to be aborted.
     */
    int (* parse_ok) (void *pvt);

    /* Channel operations */
    /** @brief Open channel.
     *
     * <em>Called as part of the channel connection setup.</em>
     *
     * @param chan dbChannel for which the connection is being made.
     * @param pvt Pointer to private structure.
     * @return 0 for success, -1 if operation is to be aborted.
     */
    long (* channel_open) (dbChannel *chan, void *pvt);

    /** @brief Register callbacks for pre-event-queue operation.
     *
     * <em>Called as part of the channel connection setup.</em>
     *
     * This function is called to establish the stack of plugins that an event
     * is passed through between the database and the event queue.
     *
     * The plugin must set pe_out to point to its own post-event callback in order
     * to be called when a data update is sent from the database towards the
     * event queue.
     *
     * The plugin may find out the type of data it will receive by looking at 'probe'.
     * If the plugin will change the data type and/or size, it must update 'probe'
     * accordingly.
     *
     * @param chan dbChannel for which the connection is being made.
     * @param pvt Pointer to private structure.
     * @param cb_out Pointer to this plugin's post-event callback (NULL to bypass
     *        this plugin).
     * @param arg_out Argument that must be supplied when calling
     *        this plugin's post-event callback.
     */
    void (* channelRegisterPre) (dbChannel *chan, void *pvt,
                                 chPostEventFunc **cb_out, void **arg_out,
                                 db_field_log *probe);

    /** @brief Register callbacks for post-event-queue operation.
     *
     * <em>Called as part of the channel connection setup.</em>
     *
     * This function is called to establish the stack of plugins that an event
     * is passed through between the event queue and the final user (CA server or
     * database access).
     *
     * The plugin must set pe_out to point to its own post-event callback in order
     * to be called when a data update is sent from the event queue towards the
     * final user.
     *
     * The plugin may find out the type of data it will receive by looking at 'probe'.
     * If the plugin will change the data type and/or size, it must update 'probe'
     * accordingly.
     *
     * @param chan dbChannel for which the connection is being made.
     * @param pvt Pointer to private structure.
     * @param cb_out Pointer to this plugin's post-event callback (NULL to bypass
     *        this plugin).
     * @param arg_out Argument that must be supplied when calling
     *        this plugin's post-event callback.
     */
    void (* channelRegisterPost) (dbChannel *chan, void *pvt,
                                  chPostEventFunc **cb_out, void **arg_out,
                                  db_field_log *probe);

    /** @brief Channel report request.
     *
     * <em>Called as part of show... routines.</em>
     *
     * @param chan dbChannel for which the report is requested.
     * @param pvt Pointer to private structure.
     * @param level Interest level.
     * @param indent Number of spaces to print before each output line.
     */
    void (* channel_report) (dbChannel *chan, void *pvt, int level, const unsigned short indent);

    /** @brief Channel close request.
     *
     * <em>Called as part of connection shutdown.</em>
     * @param chan dbChannel for which the connection is being shut down.
     * @param pvt Pointer to private structure.
     */
    void (* channel_close) (dbChannel *chan, void *pvt);

} chfPluginIf;

typedef enum chfPluginArg {
    chfPluginArgInvalid=0,
    chfPluginArgBoolean,
    chfPluginArgInt32,
    chfPluginArgDouble,
    chfPluginArgString,
    chfPluginArgEnum
} chfPluginArg;

typedef struct chfPluginEnumType {
    const char *name;
    const int value;
} chfPluginEnumType;

typedef struct chfPluginArgDef {
    const char * name;
    chfPluginArg optType;
    unsigned int required:1;
    unsigned int convert:1;
    unsigned int tagged:1;
    epicsUInt32  tagOffset;
    epicsUInt32  choice;
    epicsUInt32  dataOffset;
    epicsUInt32  size;
    const chfPluginEnumType *enums;
} chfPluginArgDef;

/* Simple arguments */

#define chfInt32(Struct, Member, Name, Req, Conv) \
    {Name, chfPluginArgInt32, Req, Conv, 0, 0, 0, \
    OFFSET(Struct, Member), sizeof( ((Struct*)0)->Member ), NULL}

#define chfBoolean(Struct, Member, Name, Req, Conv) \
    {Name, chfPluginArgBoolean, Req, Conv, 0, 0, 0, \
    OFFSET(Struct, Member), sizeof( ((Struct*)0)->Member ), NULL}

#define chfDouble(Struct, Member, Name, Req, Conv) \
    {Name, chfPluginArgDouble, Req, Conv, 0, 0, 0, \
    OFFSET(Struct, Member), sizeof( ((Struct*)0)->Member ), NULL}

#define chfString(Struct, Member, Name, Req, Conv) \
    {Name, chfPluginArgString, Req, Conv, 0, 0, 0, \
    OFFSET(Struct, Member), sizeof( ((Struct*)0)->Member ), NULL}

#define chfEnum(Struct, Member, Name, Req, Conv, Enums) \
    {Name, chfPluginArgEnum, Req, Conv, 0, 0, 0, \
    OFFSET(Struct, Member), sizeof( ((Struct*)0)->Member ), Enums}

/* Tagged arguments */

#define chfTagInt32(Struct, Member, Name, Tag, Choice, Req, Conv) \
    {Name, chfPluginArgInt32, Req, Conv, 1, OFFSET(Struct, Tag), Choice, \
    OFFSET(Struct, Member), sizeof( ((Struct*)0)->Member ), NULL}

#define chfTagBoolean(Struct, Member, Name, Tag, Choice, Req, Conv) \
    {Name, chfPluginArgBoolean, Req, Conv, 1, OFFSET(Struct, Tag), Choice, \
    OFFSET(Struct, Member), sizeof( ((Struct*)0)->Member ), NULL}

#define chfTagDouble(Struct, Member, Name, Tag, Choice, Req, Conv) \
    {Name, chfPluginArgDouble, Req, Conv, 1, OFFSET(Struct, Tag), Choice, \
    OFFSET(Struct, Member), sizeof( ((Struct*)0)->Member ), NULL}

#define chfTagString(Struct, Member, Name, Tag, Choice, Req, Conv) \
    {Name, chfPluginArgString, Req, Conv, 1, OFFSET(Struct, Tag), Choice, \
    OFFSET(Struct, Member), sizeof( ((Struct*)0)->Member ), NULL}

#define chfTagEnum(Struct, Member, Name, Tag, Choice, Req, Conv, Enums) \
    {Name, chfPluginArgEnum, Req, Conv, 1, OFFSET(Struct, Tag), Choice, \
    OFFSET(Struct, Member), sizeof( ((Struct*)0)->Member ), Enums}

#define chfPluginArgEnd {0}

/* Extra output when parsing and converting */
#define CHFPLUGINDEBUG 1

/** @brief Return the string associated with Enum index 'i'.
 *
 * @param Enums A null-terminated array of string/integer pairs.
 * @param i An Enum index.
 * @param def String to be returned when 'i' isn't a valid Enum index.
 * @return The string associated with 'i'.
 */
epicsShareFunc const char* chfPluginEnumString(const chfPluginEnumType *Enums, int i, const char* def);

/** @brief Register a plugin.
 *
 * @param key The plugin name key that clients will use.
 * @param pif Pointer to the plugin's interface.
 * @param opts Pointer to the configuration argument description table.
 */
epicsShareFunc int chfPluginRegister(const char* key, const chfPluginIf *pif, const chfPluginArgDef* opts);

#ifdef __cplusplus
}
#endif

#endif // CHFPLUGIN_H
