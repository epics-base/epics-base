/*************************************************************************\
* Copyright (c) 2018 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <testMain.h>
#include <epicsUnitTest.h>

#include <errSymTbl.h>
#include <epicsString.h>
#include <osiFileName.h>
#include <errlog.h>

#include <asLib.h>

// The maximum number of links in a chain of authority we will provide in test data.  Increase as needed
#define MAX_CERT_AUTH_CHAIN_LENGTH 10

// For tests these are the values of the client that are being tested against the given Access Security Group
static char *asUser,
            *asHost,
            *asMethod,
            *asAuthority;
static enum AsProtocol protocol=AS_PROTOCOL_TCP;
static int asAsl;

/**
 * @brief Test data with Host Access Groups (HAG)
 *
 * This includes a host access group (HAG) for localhost and a default Access Security Group (ASG)
 * with rules for read and write access to the HAG.
 */
static const char hostname_config[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n"

    "ASG(rw) {\n"
    "	RULE(1, WRITE) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * @brief Test data with METHOD, AUTHORITY, and PROTOCOL Access Security Group configurations
 *
 * This is a test case for `METHOD`, `AUTHORITY`, and `PROTOCOL` Access Security Group configurations.
 * It includes an Access Security Group (ASG) that is configured to allow access
 * to PV resources controlled by `METHOD` and `AUTHORITY` constraints.
 * It also showcases the use of `PROTOCOL` to specify constraints based on the transport layer security used.
 * It also includes an example of the `RPC` permission type for the `rwx` rule.
 */
static const char method_auth_config[] = ""
    "UAG(bar) {boss}\n"
    "UAG(foo) {testing}\n"
    "UAG(ops) {geek}\n"

    "AUTHORITY(AUTH_EPICS_ROOT, \"EPICS Org Root CA\") {\n"
    "	AUTHORITY(AUTH_INTERMEDIATE_CA, \"Intermediate CA\") {\n"
    "		AUTHORITY(AUTH_ORNL_CA, \"ORNL Org CA\")\n"
    "	}\n"
    "	AUTHORITY(AUTH_UNRELATED_CA, \"Unrelated CA\")\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		UAG(foo,ops)\n"
    "		METHOD(\"ca\")\n"
    "		PROTOCOL(\"TCP\")\n"
    "	}\n"
    "}\n"

    "ASG(rw) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, WRITE, TRAPWRITE) {\n"
    "		UAG(foo)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_UNRELATED_CA)\n"
    "	}\n"
    "}\n"

    "ASG(rwx) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, RPC) {\n"
    "		UAG(bar)\n"
    "		METHOD(\"x509\",\"ignored\",\"ignored_too\")\n"
    "		AUTHORITY(AUTH_UNRELATED_CA, AUTH_ORNL_CA)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n";

/**
 * Defines the expected output of asDumpFP() for the `method_auth_config` test case.
 *
 * This should be a direct copy of the `method_auth_config` string except that
 * all spaces in string lists are removed, and the default RULE flag (`NOTRAPWRITE`) is
 * added when it is not present in the original string.
 */
static const char *expected_method_auth_config =
    "UAG(bar) {boss}\n"
    "UAG(foo) {testing}\n"
    "UAG(ops) {geek}\n"

    "AUTHORITY(AUTH_EPICS_ROOT: EPICS Org Root CA)\n"
    "AUTHORITY(AUTH_INTERMEDIATE_CA: EPICS Org Root CA -> Intermediate CA)\n"
    "AUTHORITY(AUTH_ORNL_CA: EPICS Org Root CA -> Intermediate CA -> ORNL Org CA)\n"
    "AUTHORITY(AUTH_UNRELATED_CA: EPICS Org Root CA -> Unrelated CA)\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0,NONE,NOTRAPWRITE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0,NONE,NOTRAPWRITE)\n"
    "	RULE(1,READ,NOTRAPWRITE) {\n"
    "		UAG(foo,ops)\n"
    "		METHOD(\"ca\")\n"
    "		PROTOCOL(\"tcp\")\n"
    "	}\n"
    "}\n"

    "ASG(rw) {\n"
    "	RULE(0,NONE,NOTRAPWRITE)\n"
    "	RULE(1,WRITE,TRAPWRITE) {\n"
    "		UAG(foo)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_UNRELATED_CA)\n"
    "	}\n"
    "}\n"

    "ASG(rwx) {\n"
    "	RULE(0,NONE,NOTRAPWRITE)\n"
    "	RULE(1,RPC,NOTRAPWRITE) {\n"
    "		UAG(bar)\n"
    "		METHOD(\"x509\",\"ignored\",\"ignored_too\")\n"
    "		AUTHORITY(AUTH_UNRELATED_CA,AUTH_ORNL_CA)\n"
    "		PROTOCOL(\"tls\")\n"
    "	}\n"
    "}\n";

/**
 * Defines the expected output of asDumpRulesFP() for the `DEFAULT` Access Security Group.
 *
 * This should be a direct copy of the `method_auth_config` DEFAULT ASG string except that
 * all spaces in string lists are removed, and the default RULE flag (`NOTRAPWRITE`) is
 * added when it is not present in the original string.
 */
static const char *expected_DEFAULT_rules_config =
    "ASG(DEFAULT) {\n"
    "	RULE(0,NONE,NOTRAPWRITE)\n"
    "}\n";

/**
 * Defines the expected output of asDumpRulesFP() for the `ro` Access Security Group.
 *
 * This should be a direct copy of the `method_auth_config` ro ASG string except that
 * all spaces in string lists are removed, and the default RULE flag (`NOTRAPWRITE`) is
 * added when it is not present in the original string.
 */
static const char *expected_ro_rules_config =
    "ASG(ro) {\n"
    "	RULE(0,NONE,NOTRAPWRITE)\n"
    "	RULE(1,READ,NOTRAPWRITE) {\n"
    "		UAG(foo,ops)\n"
    "		METHOD(\"ca\")\n"
    "		PROTOCOL(\"tcp\")\n"
    "	}\n"
    "}\n";

/**
 * Defines the expected output of asDumpRulesFP() for the `rw` Access Security Group.
 *
 * This should be a direct copy of the `method_auth_config` rw ASG string except that
 * all spaces in string lists are removed, and the default RULE flag (`NOTRAPWRITE`) is
 * added when it is not present in the original string.
 */
static const char *expected_rw_rules_config =
    "ASG(rw) {\n"
    "	RULE(0,NONE,NOTRAPWRITE)\n"
    "	RULE(1,WRITE,TRAPWRITE) {\n"
    "		UAG(foo)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_UNRELATED_CA)\n"
    "	}\n"
    "}\n";

/**
 * Defines the expected output of asDumpRulesFP() for the `rwx` Access Security Group.
 *
 * This should be a direct copy of the `method_auth_config` rwx ASG string except that
 * all spaces in string lists are removed, and the default RULE flag (`NOTRAPWRITE`) is
 * added when it is not present in the original string.
 */
static const char *expected_rwx_rules_config =
    "ASG(rwx) {\n"
    "	RULE(0,NONE,NOTRAPWRITE)\n"
    "	RULE(1,RPC,NOTRAPWRITE) {\n"
    "		UAG(bar)\n"
    "		METHOD(\"x509\",\"ignored\",\"ignored_too\")\n"
    "		AUTHORITY(AUTH_UNRELATED_CA,AUTH_ORNL_CA)\n"
    "		PROTOCOL(\"tls\")\n"
    "	}\n"
    "}\n";

/**
 * @brief Test data for validating hierarchical certificate-based access control.
 *
 * @details
 * This test dataset validates the delegation of authority and inheritance in certificate chains by modeling
 * Oak Ridge National Laboratory's (ORNL) facilities and organizational structure. The test validates:
 *
 * 1. Hierarchical Certificate Authority chains:
 *    - Certificate Authorities can delegate trust by signing intermediate Certificate Authorities
 *    - Client certificates inherit trust from their entire signing chain
 *    - Multiple independent Certificate Authority hierarchies can coexist (e.g., ORNL Root vs ORNL IT Root)
 *
 * 2. Fine-grained access control:
 *    - Role-based access through User Access Groups (UAGs)
 *    - Facility-specific device grouping and permissions
 *    - Separation of admin, operations and user privileges
 *
 * The test extends EPICS Access Control File (ACF) syntax with PKI concepts:
 *
 * - @b METHOD: The authentication mechanism (e.g., "x509") required by a RULE
 * - @b AUTHORITY: The Certificate Authority(s) accepted by a RULE. Matches if client's chain contains any listed Certificate Authority
 * - @b PROTOCOL: Required transport security (e.g., "TLS") for the RULE
 *
 * Organization & Certificate Structure:
 *
 * Laboratory Level:
 *   ORNL Root CA (signs facility CAs)
 *   ORNL IT Root CA (signs user certificates)
 *   --> ORNL User CA (issues all user certificates)
 *
 * Spallation Neutron Source (SNS):
 *   SNS Intermediate CA
 *   --> SNS Control Systems CA (issues controls system device certs)
 *   --> SNS Beamline Operations CA (issues beamline equipment certs)
 *   Groups: Controls, Beamline Operations
 *   Roles per group: Admins, Operators, Users, Devices
 *
 * High Flux Isotope Reactor (HFIR):
 *   HFIR Intermediate CA
 *   --> HFIR Control Systems CA (issues reactor control system certs)
 *   --> HFIR Sample Environment CA (issues sample environment equipment certs)
 *   Groups: Controls, Sample Environment
 *   Roles per group: Admins, Operators, Users, Devices
 *
 * The test data includes:
 * - Complete Certificate Authority hierarchies for ORNL, SNS and HFIR
 * - User Access Groups for each facility/group/role combination
 * - Access Security Groups with rules validating:
 *   - Role-based access (admin vs operator vs user)
 *   - Certificate chain validation
 *   - Transport security requirements
 *   - Future-proofing support (GROUP keyword ignored)
 *
 * All human users have certificates issued by the ORNL User CA, while devices
 * have certificates from their respective facility CAs. This enforces proper
 * separation between user authentication and device authorization.
 */
static const char chained_auth_config[] = ""
    // Authority chain containing SNS and HIFR Certificate Authorities
    "AUTHORITY(AUTH_ORNL_ROOT, \"ORNL Root CA\") {\n"
    "	AUTHORITY(\"SNS Intermediate CA\") {\n"
    "		AUTHORITY(AUTH_SNS_CTRL, \"SNS Control Systems CA\")\n"
    "		AUTHORITY(AUTH_BEAMLINE, \"SNS Beamline Operations CA\")\n"
    "   }\n"
    "	AUTHORITY(\"HFIR Intermediate CA\") {\n"
    "		AUTHORITY(AUTH_HIFR_CTRL, \"HFIR Control Systems CA\")\n"
    "		AUTHORITY(AUTH_HIFR_SAMPLE, \"HFIR Sample Environment CA\")\n"
    "   }\n"
    "}\n"

    // Authority chain containing ORNL IT User Certificate Authorities
    "AUTHORITY(AUTH_ORNL_IT_ROOT, \"ORNL IT Root CA\") {\n"
    "	AUTHORITY(AUTH_ORNL_USERS, \"ORNL User Certificate Authority\")\n"
    "}\n"

    "UAG(ORNL:ADMINS) {s.streiffer}\n"

    "UAG(SNS:ADMINS) {s.streiffer}\n"
    "UAG(SNS:CTRL:ADMINS) {v.fanelli}\n"
    "UAG(SNS:CTRL:OPS) {v.fanelli, ann.op}\n"
    "UAG(SNS:CTRL:USERS) {v.fanelli, w.blower, x.windman, y.gale}\n"
    "UAG(SNS:CTRL:DEVICES) {SNS:CTRL:IOC:VAC01, SNS:CTRL:IOC:MOT02, SNS:CTRL:IOC:TEMP03, SNS:CTRL:IOC:PWR04}\n"
    "UAG(SNS:BEAM:ADMINS) {f.pilat}\n"
    "UAG(SNS:BEAM:OPS) {f.pilat, bee.op}\n"
    "UAG(SNS:BEAM:USERS) {f.pilat, g.squat, h.lunge, i.press}\n"
    "UAG(SNS:BEAM:DEVICES) {SNS:BEAM:IOC:DET01, SNS:BEAM:IOC:COLL02, SNS:BEAM:IOC:CHOP03, SNS:BEAM:IOC:MON04}\n"

    "UAG(HFIR:ADMINS) {s.streiffer}\n"
    "UAG(HFIR:CTRL:ADMINS) {b.weston}\n"
    "UAG(HFIR:CTRL:OPS) {b.weston, cee.op}\n"
    "UAG(HFIR:CTRL:USERS) {b.weston, c.north, d.southerly, e.eastman}\n"
    "UAG(HFIR:CTRL:DEVICES) {HFIR:CTRL:IOC:REACT01, HFIR:CTRL:IOC:COOL02, HFIR:CTRL:IOC:SHLD03}\n"
    "UAG(HFIR:ENV:ADMINS) {g.lynn}\n"
    "UAG(HFIR:ENV:OPS) {g.lynn, dee.op}\n"
    "UAG(HFIR:ENV:USERS) {g.lynn, h.overman, i.bachman}\n"
    "UAG(HFIR:ENV:DEVICES) {HFIR:ENV:IOC:TEMP01, HFIR:ENV:IOC:MAG02}\n"

    // Try out GROUP syntax: will be ignored by future proofing functionality
    "GROUP(PHYSICS_GROUP) {physics}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(PHYSICS) {\n"
    // ORNL Physics Users: To try out GROUPS syntax: ignored due to future proofing
    "	RULE(0, WRITE, TRAPWRITE) {\n"
    "		GROUP(PHYSICS_GROUP)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_IT_ROOT)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n"

    "ASG(ADMIN) {\n"
    // ORNL Admin Users
    "	RULE(0, WRITE, TRAPWRITE) {\n"
    "		UAG(ORNL:ADMINS)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_IT_ROOT)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n"

    "ASG(SNS:ADMIN) {\n"
    // SNS Admin Users
    "	RULE(0, WRITE, TRAPWRITE) {\n"
    "		UAG(SNS:ADMINS)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_IT_ROOT)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n"

    "ASG(SNS:CTRL:ADMIN) {\n"
    // SNS Controls Admin Users
    "	RULE(0, WRITE, TRAPWRITE) {\n"
    "		UAG(SNS:CTRL:ADMINS)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_USERS)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n"

    "ASG(SNS:CONTROLS) {\n"
    // SNS Controls Users
    "	RULE(0, READ) {\n"
    "		UAG(SNS:CTRL:USERS)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_USERS)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    // SNS Controls Operators and Devices
    "	RULE(1, WRITE, TRAPWRITE) {\n"
    "		UAG(SNS:CTRL:OPS, SNS:CTRL:DEVICES)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_USERS, AUTH_SNS_CTRL)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n"

    "ASG(SNS:BEAMLINE) {\n"
    // SNS Beamline Users
    "	RULE(0, READ) {\n"
    "		UAG(SNS:BEAM:USERS)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_USERS)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    // SNS Beamline Operators and Devices
    "	RULE(1, WRITE, TRAPWRITE) {\n"
    "		UAG(SNS:BEAM:OPS, SNS:BEAM:DEVICES)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_USERS, AUTH_BEAMLINE)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n"

    "ASG(HFIR:ADMIN) {\n"
    // HIFR Admin Users
    "	RULE(0, WRITE, TRAPWRITE) {\n"
    "		UAG(HFIR:ADMINS)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_IT_ROOT)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n"

    "ASG(HFIR:CTRL:ADMIN) {\n"
    // HIFR Controls Admin Users
    "	RULE(0, WRITE, TRAPWRITE) {\n"
    "		UAG(HFIR:CTRL:ADMINS)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_USERS)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n"

    "ASG(HFIR:CONTROLS) {\n"
    // HIFR Controls Users
    "	RULE(0, READ) {\n"
    "		UAG(HFIR:CTRL:USERS)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_USERS)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    // HIFR Controls Operators and Devices
    "	RULE(1, WRITE, TRAPWRITE) {\n"
    "		UAG(HFIR:CTRL:OPS, HFIR:CTRL:DEVICES)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_HIFR_CTRL,AUTH_ORNL_USERS)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n"

    "ASG(HFIR:ENV:ADMIN) {\n"
    // HIFR Sample Environment Admin Users
    "	RULE(0, WRITE, TRAPWRITE) {\n"
    "		UAG(HFIR:ENV:ADMINS)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_USERS)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n"

    "ASG(HFIR:ENVIRONMENT) {\n"
    "	RULE(0, READ) {\n"
    // HIFR Sample Environment Users
    "		UAG(HFIR:ENV:USERS)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_ORNL_USERS)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    // HIFR Sample Environment Operators and Devices
    "	RULE(1, WRITE, TRAPWRITE) {\n"
    "		UAG(HFIR:ENV:OPS, HFIR:ENV:DEVICES)\n"
    "		METHOD(\"x509\")\n"
    "		AUTHORITY(AUTH_HIFR_SAMPLE,AUTH_ORNL_USERS)\n"
    "		PROTOCOL(\"TLS\")\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(WELL,FORMED,LIST)
 * - valid top level keyword with well-formed arg list
 */
static const char supported_config_1[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC(WELL, FORMED, ARG, LIST)\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { WELL,FORMED,LIST }
 * - valid top level keyword with well-formed arg list and valid arg list body
 */
static const char supported_config_2[] = ""
    "HAG(foo) {localhost}\n"

    "SIMPLE(WELL, FORMED, ARG, LIST) {\n"
    "	WELL, FORMED, LIST\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { recursive-body-keyword(WELL,FORMED,LIST) }
 * - valid top level keyword with well-formed arg list and valid recursive body
 * - includes quoted strings, integers, and floating point numbers
 */
static const char supported_config_3[] = ""
    "HAG(foo) {localhost}\n"

    "COMPLEX_ARGUMENTS(1, WELL, \"FORMED\", ARG, LIST) {\n"
    "	ALSO_GENERIC(WELL, FORMED, ARG, LIST, 2.0) \n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { recursive-body-keyword(WELL,FORMED,LIST) { AND_BODY } }
 * - valid top level keyword with well-formed arg list and valid recursive body, with a nested body
 * - includes floating point numbers, and an empty arg list
 */
static const char supported_config_4[] = ""
    "HAG(foo) {localhost}\n"

    "SUB_BLOCKS(1.0, ARGS) {\n"
    "	ALSO_GENERIC() {\n"
    "		AND_LIST_BODY\n"
    "	}\n"
    "	ANOTHER_GENERIC() {\n"
    "		BIGGER, LIST, BODY\n"
    "	}\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { recursive-body-keyword(WELL,FORMED,LIST) { AND_RECURSIVE_BODY() {LIST, LIST } }
 * - valid top level keyword with well-formed arg list and valid recursive body, with a nested recursion
 * - includes floating point numbers, and an empty arg list
 */
static const char supported_config_5[] = ""
    "HAG(foo) {localhost}\n"

    "RECURSIVE_SUB_BLOCKS(1.0, -2.3, +4.5, ARGS, +2.71828E-23, -2.71828e+23, +12, -13, +-14) {\n"
    "	ALSO_GENERIC() {\n"
    "		AND_RECURSIVE(FOO) {\n"
    "			LIST, BODY\n"
    "		}\n"
    "	}\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(+1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(KEYWORD) { KEYWORD(KEYWORD) }
 * - valid top level keyword with keyword for args, recursive body name, and arg list
 * - top level generic items referenced in RULES, then RULES are ignored
 */
static const char supported_config_6[] = ""
    "HAG(foo) {localhost}\n"

    "WITH_KEYWORDS(UAG) {\n"
    "	ASG(HAL, IMP, CALC, RULE)\n"
    "	HAL(USG, METHOD) {\n"
    "		PROTOCOL(\"TLS\", AUTHORITY)\n"
    "	}\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ignored) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		WITH_KEYWORDS(UAG)\n"
    "	}\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "	RULE(2, WRITE) {\n"
    "		WITH_KEYWORDS(UAG)\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported elements should be silently ignored, and the rule will not match,
 * but the rest of the config is processed.
 *
 * - RULE contains unsupported elements
 */
static const char supported_config_7[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "		BAD_PREDICATE(\"x509\")\n"
    "		BAD_PREDICATE_AS_WELL(\"EPICS Certificate Authority\")\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported elements should be silently ignored, and the rule will not match,
 * but the rest of the config is processed.
 *
 * - unexpected permission name in arg list for RULE element ignored
 */
static const char supported_config_8[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, ADDITIONAL_PERMISSION) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n"
    ;

/**
 * Test data with unsupported elements.
 * The unsupported elements should be silently ignored, and the rule will not match,
 * but the rest of the config is processed.
 *
 * - RULE containing unexpected protocol name ignored
 */
static const char supported_config_9[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, WRITE) {\n"
    "		HAG(foo)\n"
    "		PROTOCOL(UNKNOWN_PROTOCOL)\n"
    "	}\n"
    "}\n"
    ;

/**
 * Test data with unsupported elements.
 * The unsupported element should cause an error as the format is invalid.
 *
 * top-unknown-keyword( a b )
 * - invalid arg list missing commas
 */
static const char unsupported_config_1[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC(not well-formed arg list)\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should cause an error as the format is invalid.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { a b }
 * - invalid string list
 */
static const char unsupported_config_2[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC(WELL, FORMED, ARG, LIST) {\n"
    "	NOT WELL-FORMED BODY\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should cause an error as the format is invalid.
 *
 * top-unknown-keyword { a, b }
 * - missing parameters (must have at least an empty arg list)
 */
static const char unsupported_config_3[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC {\n"
    "	WELL, FORMED, LIST, BODY\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should cause an error as the format is invalid.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { X, Y(a b c) }
 * - bad arg list for recursive body
 */
static const char unsupported_config_4[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC(WELL, FORMED, ARG, LIST) {\n"
    "	BODY(BAD ARG LIST)\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should cause an error as the format is invalid.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { X, Y(a b c) }
 * - mix of list and recursive type bodies
 */
static const char unsupported_config_5[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC(WELL, FORMED, ARG, LIST) {\n"
    "	LIST, BODY, MIXED, WITH,\n"
    "	RECURSIVE_BODY(ARG, LIST)\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - bad arg list for ASG element
 */
static const char unsupported_mod_1[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro BAD ARG LIST) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - bad arg list for HAG element
 */
static const char unsupported_mod_2[] = ""
    "HAG(BAD ARG LIST) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - bad arg list for RULE element
 */
static const char unsupported_mod_3[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0 BAD ARG LIST)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - bad arg count for ASG element
 */
static const char unsupported_mod_4[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro, UNKNOWN_PERMISSION) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - unexpected name in arg list for RULE element
 */
static const char unsupported_mod_5[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE, UNKNOWN_FLAG)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - unexpected recursive body mixed in with HAG string list body
 */
static const char unsupported_mod_6[] = ""
    "HAG(foo) {\n"
    "	localhost,\n"
    "	NETWORK(\"127.0.0.1\")\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - unexpected recursive body mixed in with UAG string list body
 */
static const char unsupported_mod_7[] = ""
    "UAG(foo) {\n"
    "	alice,\n"
    "	GROUP(admin)\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "	RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "	RULE(0, NONE)\n"
    "	RULE(1, READ) {\n"
    "		HAG(foo)\n"
    "	}\n"
    "}\n";

/**
 * Set the username for the authorization tests
 */
static void setUser(const char *name)
{
    free(asUser);
    asUser = epicsStrDup(name);
}

/**
 * Set the hostname for the authorization tests
 */
static void setHost(const char *name)
{
    free(asHost);
    asHost = epicsStrDup(name);
}

static void setMethod(const char *name)
{
    free(asMethod);
    asMethod = epicsStrDup(name);
}

static void setAuthority(const char *name)
{
    free(asAuthority);
    asAuthority = epicsStrDup(name);
}

static void setProtocol(enum AsProtocol the_protocol)
{
    protocol = the_protocol;
}

/**
 * @brief Converts a newline-delimited certificate authority chain into a printable string.
 *
 * @details
 * This function takes the global variable `asAuthority`, which contains a newline-delimited
 * list of certificate authorities ordered from signer to signee (i.e., root CA first,
 * issuer CA last), and converts it into a single-line, human-readable string
 *
 * The resulting format resembles:
 *   "Root Certificate Authority -> Intermediate CA -> Issuer CA"
 *
 * This makes the chain easier to read from trust anchor down to the end-entity.
 *
 * - If `asAuthority` is empty or NULL, the output buffer is set to an empty string.
 * - The result is written into `parsedCertAuthChainBuf` and truncated to `MAX_AUTH_CHAIN_STRING` bytes.
 * - Up to MAX_CERT_AUTH_CHAIN_LENGTH authority entries are supported in the chain.
 *
 * @param[out] parsedCertAuthChainBuf Buffer to receive the formatted authority chain string.
 */
static void parseCertAuthChain(char *parsedCertAuthChainBuf) {
    if (asAuthority) {
        parsedCertAuthChainBuf[0] = '\0';
        char *p = parsedCertAuthChainBuf;

        char unParsedAuthority[MAX_AUTH_CHAIN_STRING];
        strncpy(unParsedAuthority, asAuthority, sizeof(unParsedAuthority));
        unParsedAuthority[sizeof(unParsedAuthority) - 1] = '\0';

        const char *token = strtok(unParsedAuthority, "\n");
        if (token) {
            size_t len = 0;
            size_t remainingSpace = MAX_AUTH_CHAIN_STRING;
            len = strlen(token);
            if (len < remainingSpace) {
                strcpy(p, token);
                p += len;
                remainingSpace -= len;

                while (((token = strtok(NULL, "\n"))) && remainingSpace > 4) {
                    len = strlen(token);
                    if (len + 4 < remainingSpace) {
                        strcpy(p, " -> ");
                        p += 4;
                        strcpy(p, token);
                        p += len;
                        remainingSpace -= (len + 4);
                    } else {
                        break;
                    }
                }
            }
        }
    }
}

/**
 * Test the access control system with the given ASG, user, and hostname
 * This will test that the expected access given by mask is granted
 * when the Access Security Group (ASG) is interpreted in the
 * context of the configured user, host, and Level (asl).
 */
static void testAccess(const char *asg, unsigned mask)
{
    ASMEMBERPVT asp = 0; /* aka dbCommon::asp */
    ASCLIENTPVT client = 0;

    static __thread char formattedCertAuthChain[MAX_AUTH_CHAIN_STRING];
    parseCertAuthChain(&formattedCertAuthChain[0]);

    long ret = asAddMember(&asp, asg);
    if(ret) {
        testFail("testAccess(ASG:%s, ID:%s, METHOD:%s, AUTHORITY:%s, HOST:%s, PROTOCOL:%s, ASL:%d) -> asAddMember error: %s",
                 asg, asUser, asMethod?asMethod:"", asAuthority?formattedCertAuthChain:"", asHost, protocol ? "true":"false", asAsl, errSymMsg(ret));
    } else {
        ret = asAddClientIdentity(&client, asp, asAsl, (ASIDENTITY){ .user = asUser, .host = asHost, .method = asMethod, .authority = asAuthority, .protocol = protocol });
    }
    if(ret) {
        testFail("testAccess(ASG:%s, ID:%s, METHOD:%s, AUTHORITY:%s, HOST:%s, PROTOCOL:%s, ASL:%d) -> asAddClient error: %s",
                 asg, asUser, asMethod?asMethod:"", asAuthority?formattedCertAuthChain:"", asHost, protocol ? "true":"false", asAsl, errSymMsg(ret));
    } else {
        unsigned actual = 0;
        actual |= asCheckGet(client) ? 1 : 0;
        actual |= asCheckPut(client) ? 2 : 0;
        actual |= asCheckRPC(client) ? 4 : 0;
        testOk(actual==mask, "testAccess(ASG:%s, ID:%s, METHOD:%s, AUTHORITY:%s, HOST:%s, PROTOCOL:%s, ASL:%d) -> %x == %x",
               asg, asUser, asMethod?asMethod:"", asAuthority?formattedCertAuthChain:"", asHost, protocol ? "true":"false", asAsl, actual, mask);
    }
    if(client) asRemoveClient(&client);
    if(asp) asRemoveMember(&asp);
}

static void testSyntaxErrors(void)
{
    static const char empty[] = "\n#almost empty file\n\n";
    static const char duplicateMethod[] = "\nASG(foo) {RULE(0, NONE) {METHOD   (\"x509\"		)  METHOD   (\"x509\"		)}}\n\n";
    static const char duplicateAuthority[] = "\nASG(foo) {RULE(0, NONE) {AUTHORITY(\"Epics Org Root CA\")  AUTHORITY(\"Epics Org Root CA\")}}\n\n";
    static const char notDuplicateMethod[] = "\nASG(foo) {RULE(0, NONE) {METHOD   (\"x509\"		)} RULE(1, RPC			) {METHOD   (\"x509\"		)}}\n\n";
    static const char notDuplicateAuthority[] = "\nASG(foo) {RULE(0, NONE) {AUTHORITY(\"Epics Org Root CA\")} RULE(1, WRITE,TRAPWRITE) {AUTHORITY(\"Epics Org Root CA\")}}\n\n";
    static const char anotherNotDuplicatedMethod[] = "\nASG(foo) {RULE(0, NONE) {METHOD   (\"x509\"		)  METHOD   (\"ca\"		  )}}\n\n";
    static const char anotherNotDuplicatedAuthority[] = "\nASG(foo) {RULE(0, NONE) {AUTHORITY(\"Epics Org Root CA\")  AUTHORITY(\"ORNL CA\"	 )}}\n\n";
    long ret;

    testDiag("testSyntaxErrors()");
    asCheckClientIP = 0;

    eltc(0);
    ret = asInitMem(empty, NULL);
    testOk(ret==S_asLib_badConfig, "load \"empty\" config -> %s", errSymMsg(ret));

    ret = asInitMem(duplicateMethod, NULL);
    testOk(ret==S_asLib_badConfig, "load \"duplicate method rule\" config -> %s", errSymMsg(ret));

    ret = asInitMem(duplicateAuthority, NULL);
    testOk(ret==S_asLib_badConfig, "load \"duplicate authority rule\" config -> %s", errSymMsg(ret));

    ret = asInitMem(notDuplicateMethod, NULL);
    testOk(ret==0, "load non \"duplicate method rule\" config -> %s", errSymMsg(ret));

    ret = asInitMem(notDuplicateAuthority, NULL);
    testOk(ret==0, "load non \"duplicate authority rule\" config -> %s", errSymMsg(ret));

    ret = asInitMem(anotherNotDuplicatedMethod, NULL);
    testOk(ret==0, "load another non \"duplicate method rule\" config -> %s", errSymMsg(ret));

    ret = asInitMem(anotherNotDuplicatedAuthority, NULL);
    testOk(ret==0, "load another non \"duplicate authority rule\" config -> %s", errSymMsg(ret));

    eltc(1);
}

static void testHostNames(void)
{
    testDiag("testHostNames()");
    asCheckClientIP = 0;

    testOk1(asInitMem(hostname_config, NULL)==0);

    setUser("testing");
    setHost("localhost");
    asAsl = 0;

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 1);
    testAccess("rw", 3);

    setHost("127.0.0.1");

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 0);
    testAccess("rw", 0);

    setHost("guaranteed.invalid.");

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 0);
    testAccess("rw", 0);
}

static void testUseIP(void)
{
    testDiag("testUseIP()");
    asCheckClientIP = 1;

    /* still host names in .acf */
    testOk1(asInitMem(hostname_config, NULL)==0);
    /* now resolved to IPs */

    setUser("testing");
    setHost("localhost"); /* will not match against resolved IP */
    asAsl = 0;

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 0);
    testAccess("rw", 0);

    setHost("127.0.0.1");

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 1);
    testAccess("rw", 3);

    setHost("guaranteed.invalid.");

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 0);
    testAccess("rw", 0);
}

static void testFutureProofParser(void)
{
    long ret;

    testDiag("testFutureProofParser()");
    asCheckClientIP = 0;

    eltc(0);  /* Suppress error messages during test */

    /* Test parsing should reject unsupported elements badly placed or formed */
    ret = asInitMem(unsupported_config_1, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects invalid arg list missing commas -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_config_2, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects invalid string list -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_config_3, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects missing parameters (must have at least an empty arg list) -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_config_4, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects bad arg list for recursive body -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_config_5, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects mix of list and recursive type bodies -> %s", errSymMsg(ret));


    /* Test supported elements badly modified should be rejected */
    ret = asInitMem(unsupported_mod_1, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects bad arg list for ASG element -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_2, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects bad arg list for HAG element-> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_3, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects bad arg list for RULE element -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_4, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects bad arg count for ASG element -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_5, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects unexpected name in arg list for RULE element -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_6, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects unexpected recursive body in HAG element body -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_7, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects unexpected recursive body in UAG element body -> %s", errSymMsg(ret));


    eltc(1);

    /* Test supported for known elements containing unsupported elements, well-formed and ignored */
    setUser("testing");
    setHost("localhost");

    ret = asInitMem(supported_config_1, NULL);
    testOk(ret==0, "unknown elements ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_2, NULL);
    testOk(ret==0, "unknown elements with body ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_3, NULL);
    testOk(ret==0, "unknown elements with string and double args and a body, ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_4, NULL);
    testOk(ret==0, "unknown elements with recursive body ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_5, NULL);
    testOk(ret==0, "unknown elements with recursive body with recursion ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_6, NULL);
    testOk(ret==0, "unknown elements with keywords arguments and body names ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ignored", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_7, NULL);
    testOk(ret==0, "rules with unknown elements ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 0);
    }

    ret = asInitMem(supported_config_8, NULL);
    testOk(ret==0, "rules with unknown permission names ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 0);
    }

    ret = asInitMem(supported_config_9, NULL);
    testOk(ret==0, "rules with unknown protocol names ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 0);
    }
}

static void testMethodAndAuth(void)
{
    testDiag("testMethodAndAuth()");
    asCheckClientIP = 0;

    testOk1(asInitMem(method_auth_config, NULL)==0);

    asAsl = 0;
    testAccess("DEFAULT", 0);

    setHost("localhost"); // Not specified in test rules
    setUser("boss");
    setMethod("ca");

    testAccess("ro", 0);
    testAccess("rw", 0);
    testAccess("rwx", 0);

    setUser("testing");

    testAccess("ro", 1);
    testAccess("rw", 0);
    testAccess("rwx", 0);

    setMethod("x509");
    setAuthority(
        "EPICS Org Root CA"
        );

    testAccess("ro", 0);
    testAccess("rw", 0);
    testAccess("rwx", 0);

    setAuthority(
        "EPICS Org Root CA\n"
        "Unrelated CA"
        );
    setProtocol(AS_PROTOCOL_TLS);

    testAccess("ro", 0);
    testAccess("rw", 3);
    testAccess("rwx", 0);

    setAuthority(
        "EPICS Org Root CA\n"
        "Intermediate CA\n"
        "ORNL Org CA"
        );
    testAccess("ro", 0);
    testAccess("rw", 0);

    setUser("boss");
    testAccess("rwx", 7);
}

/**
 * @brief Tests Chains of Authority.
 *
 * @details
 * Validates hierarchical certificate chains passed to the authorization system as newline-separated entries.
 * The chain represents delegated authority, from the root CA down to the user's certificate.
 *
 * Inherited permission is supported: if a user holds a certificate from CA B, and CA B is signed by CA A,
 * then the user implicitly satisfies rules requiring CA A, even if CA B is not explicitly listed.
 *
 * This test data models a fictional lab (ORNL), its light sources, and associated operational groups.
 *
 * Organizational Structure:
 *   ORNL Lab - Stephen Streiffer: Laboratory Director
 *   --> Spallation Neutron Source (SNS)
 *       --> SNS Control Systems - Victor Fanelli: Group Leader
 *       --> SNS Beamline Operations - Fulvia Pilat: Director of Research
 *   --> High Flux Isotope Reactor (HFIR)
 *       --> HFIR Control Systems - Brian Weston: Chief Operating Officer
 *       --> HFIR Sample Environment - Gary Lynn: Section Head
 *
 * Certificate Authorities and certificates they manage:
 *   ORNL Root CA
 *   --> SNS Intermediate CA
 *       --> SNS Control Systems CA
 *           --> CERTIFICATE: Control System Devices
 *       --> SNS Beamline Operations CA
 *           --> CERTIFICATE: Beamline IOCs
 *   --> HFIR Intermediate CA
 *       --> HFIR Control Systems CA
 *           --> CERTIFICATE: Control System Devices
 *       --> HFIR Sample Environment CA
 *           --> CERTIFICATE: Sample Env. IOCs
 *   ORNL IT Root CA
 *   --> ORNL User Certificate Authority
 *       --> CERTIFICATE: ORNL Users
 */
static void testCertificateChains(void) {
    testDiag("testCertificateChains()");
    asCheckClientIP = 0;

    testOk1(asInitMem(chained_auth_config, NULL)==0);

    asAsl = 0;
    setHost("localhost"); // Not specified in test rules
    setMethod("x509");
    setProtocol(AS_PROTOCOL_TLS);

    // Laboratory Directorate and global admin
    setUser("s.streiffer");
    setAuthority(
        "ORNL IT Root CA\n"
        "ORNL User Certificate Authority"
        );
    testAccess("ADMIN", 3);

    // Spallation Neutron Source
    testAccess("SNS:ADMIN", 3);

    // Spallation Neutron Source Controls Group
    setUser("v.fanelli");
    testAccess("SNS:ADMIN", 0);
    testAccess("SNS:CTRL:ADMIN", 3);
    testAccess("SNS:CONTROLS", 3);
    setUser("ann.op");
    testAccess("SNS:CONTROLS", 3);

    setUser("w.blower");
    testAccess("SNS:CONTROLS", 1);
    setUser("x.windman");
    testAccess("SNS:CONTROLS", 1);
    setUser("y.gale");
    testAccess("SNS:CONTROLS", 1);
    setUser("g.squat");   // Wrong Group
    testAccess("SNS:CONTROLS", 0);
    setUser("h.lunge");   // Wrong Group
    testAccess("SNS:CONTROLS", 0);
    setUser("i.press");   // Wrong Group
    testAccess("SNS:CONTROLS", 0);

    // Spallation Neutron Source beamline operations
    setUser("f.pilat");
    testAccess("SNS:ADMIN", 0);
    testAccess("SNS:BEAM:ADMIN", 0);  // No such security group
    testAccess("SNS:BEAMLINE", 3);
    setUser("bee.op");
    testAccess("SNS:BEAMLINE", 3);

    setUser("g.squat");
    testAccess("SNS:BEAMLINE", 1);
    setUser("h.lunge");
    testAccess("SNS:BEAMLINE", 1);
    setUser("i.press");
    testAccess("SNS:BEAMLINE", 1);
    setUser("w.blower");  // Wrong Group
    testAccess("SNS:BEAMLINE", 0);
    setUser("x.windman"); // Wrong Group
    testAccess("SNS:BEAMLINE", 0);
    setUser("y.gale");    // Wrong Group
    testAccess("SNS:BEAMLINE", 0);

    // Spallation Neutron Source Devices
    setAuthority(
        "ORNL Root CA\n"
        "SNS Intermediate CA\n"
        "SNS Control Systems CA"
        );
    setUser("SNS:CTRL:IOC:VAC01");
    testAccess("SNS:CONTROLS", 3);
    setUser("SNS:CTRL:IOC:MOT02");
    testAccess("SNS:CONTROLS", 3);
    setUser("SNS:CTRL:IOC:TEMP03");
    testAccess("SNS:CONTROLS", 3);
    setUser("SNS:CTRL:IOC:PWR04");
    testAccess("SNS:CONTROLS", 3);

    setUser("SNS:BEAM:IOC:DET01");
    testAccess("SNS:BEAMLINE", 0); // Wrong CA chain
    setAuthority(
        "ORNL Root CA\n"
        "SNS Intermediate CA"
        );
    testAccess("SNS:BEAMLINE", 0); // Incomplete CA
    setAuthority( "" );
    testAccess("SNS:BEAMLINE", 0); // No CA chain
    setAuthority(
        "ORNL Root CA\n"
        "SNS Intermediate CA\n"
        "SNS Beamline Operations CA\n"
        "Sub CA"
        );
    testAccess("SNS:BEAMLINE", 3); // Unknown Leaf Certificate is ok
    setAuthority(
        "ORNL Root CA\n"
        "SNS Intermediate CA\n"
        "SNS Beamline Operations CA"
        );
    testAccess("SNS:BEAMLINE", 3);
    setUser("SNS:BEAM:IOC:COLL02");
    testAccess("SNS:BEAMLINE", 3);
    setUser("SNS:BEAM:IOC:CHOP03");
    testAccess("SNS:BEAMLINE", 3);
    setUser("SNS:BEAM:IOC:MON04");
    testAccess("SNS:BEAMLINE", 3);

    // High-Flux Isotope Reactor
    setUser("s.streiffer");
    setAuthority(
        "ORNL IT Root CA\n"
        "ORNL User Certificate Authority"
        );
    testAccess("HFIR:ADMIN", 3);

    // High-Flux Isotope Reactor Controls Group
    setUser("b.weston");
    testAccess("HFIR:ADMIN", 0);
    testAccess("HFIR:CTRL:ADMIN", 3);
    testAccess("HFIR:CONTROLS", 3);
    setUser("cee.op");
    testAccess("HFIR:CONTROLS", 3);

    setUser("c.north");
    testAccess("HFIR:CONTROLS", 1);
    setUser("d.southerly");
    testAccess("HFIR:CONTROLS", 1);
    setUser("e.eastman");
    testAccess("HFIR:CONTROLS", 1);
    setUser("g.lynn");   // Wrong Group
    testAccess("HFIR:CONTROLS", 0);
    setUser("h.overman");   // Wrong Group
    testAccess("HFIR:CONTROLS", 0);
    setUser("i.bachman");   // Wrong Group
    testAccess("HFIR:CONTROLS", 0);

    // High-Flux Isotope Reactor Sample Environment operations
    setUser("g.lynn");
    testAccess("HFIR:ADMIN", 0);
    testAccess("HFIR:ENV:ADMIN", 3);
    testAccess("HFIR:ENVIRONMENT", 3);
    setUser("dee.op");
    testAccess("HFIR:ENVIRONMENT", 3);

    setUser("h.overman");
    testAccess("HFIR:ENVIRONMENT", 1);
    setUser("i.bachman");
    testAccess("HFIR:ENVIRONMENT", 1);
    setUser("f.pilat");  // Wrong Group
    testAccess("HFIR:ENVIRONMENT", 0);
    setUser("g.squat");  // Wrong Group
    testAccess("HFIR:ENVIRONMENT", 0);
    setUser("h.lunge"); // Wrong Group
    testAccess("HFIR:ENVIRONMENT", 0);
    setUser("i.press");    // Wrong Group
    testAccess("HFIR:ENVIRONMENT", 0);

    // High-Flux Isotope Reactor Devices
    setAuthority(
        "ORNL Root CA\n"
        "HFIR Intermediate CA\n"
        "HFIR Control Systems CA"
        );
    setUser("HFIR:CTRL:IOC:REACT01");
    testAccess("HFIR:CONTROLS", 3);
    setUser("HFIR:CTRL:IOC:COOL02");
    testAccess("HFIR:CONTROLS", 3);
    setUser("HFIR:CTRL:IOC:SHLD03");
    testAccess("HFIR:CONTROLS", 3);

    setUser("HFIR:ENV:IOC:TEMP01");
    testAccess("HFIR:ENVIRONMENT", 0); // Wrong CA chain
    setAuthority(
        "ORNL Root CA\n"
        "HFIR Intermediate CA"
        );
    testAccess("HFIR:ENVIRONMENT", 0); // Incomplete CA chain
    setAuthority( "" );
    testAccess("HFIR:ENVIRONMENT", 0); // No CA chain
    setAuthority(
        "ORNL Root CA\n"
        "HFIR Intermediate CA\n"
        "HFIR Sample Environment CA\n"
        "Sub CA"
        );
    testAccess("HFIR:ENVIRONMENT", 3); // Extra Certificate Authority in Chain is ok
    setAuthority(
        "ORNL Root CA\n"
        "HFIR Intermediate CA\n"
        "HFIR Sample Environment CA"
        );
    testAccess("HFIR:ENVIRONMENT", 3);
    setUser("HFIR:ENV:IOC:MAG02");
    testAccess("HFIR:ENVIRONMENT", 3);
}

static char* readFile(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        return NULL;
    }

    // Seek to the end to determine file size.
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(file);
        return NULL;
    }
    long filesize = ftell(file);
    if (filesize < 0) {
        perror("ftell");
        fclose(file);
        return NULL;
    }
    rewind(file);

    // Allocate a buffer for the file contents plus a null terminator.
    char *buffer = malloc(filesize + 1);
    if (!buffer) {
        perror("malloc");
        fclose(file);
        return NULL;
    }

    // Read the file into the buffer.
    size_t read_size = fread(buffer, 1, filesize, file);
    if (read_size != (size_t)filesize) {
        perror("fread");
        free(buffer);
        fclose(file);
        return NULL;
    }
    buffer[filesize] = '\0';  // Null-terminate the string.

    fclose(file);
    return buffer;
}

static void testDumpOutput(void)
{
    testDiag("testDumpOutput()");
    asCheckClientIP = 0;

    testOk1(asInitMem(method_auth_config, NULL)==0);

    // Create temporary file in current directory
    char temp_filename[] = "aslib_test_XXXXXX";
#ifdef _WIN32
    _mktemp(temp_filename);
    FILE *fp = fopen(temp_filename, "w+");
#else
    int fd = mkstemp(temp_filename);
    testOk(fd != -1, "Created temporary file");
    if (fd == -1) return;
    FILE *fp = fdopen(fd, "w+");
#endif
    testOk(fp != NULL, "Opened temporary file stream");
    if (!fp) {
#ifndef _WIN32
        close(fd);
#endif
        unlink(temp_filename);
        return;
    }

    // Write dump to temporary file
    asDumpFP(fp, NULL, NULL, 0);
    fflush(fp);
    rewind(fp);

    // Read the entire dump into a buffer
    char *buf = readFile(temp_filename);

    testOk(buf != NULL && strcmp(expected_method_auth_config, buf) == 0,
           "asDumpFP output matches expected\nExpected:\n%s\nGot:\n%s",
           expected_method_auth_config, buf ? buf : "NULL");

    // Clean up
    free(buf);
    fclose(fp);
    unlink(temp_filename);  // Delete temporary file
}

static void runRestDumpRules(const char *rule, const char *expected_config)
{
    static char temp_filename[] = "aslib_test_XXXXXX";
#ifdef _WIN32
    _mktemp(temp_filename);
    FILE *fp = fopen(temp_filename, "w+");
#else
    int fd = mkstemp(temp_filename);
    FILE *fp = fdopen(fd, "w+");
#endif
    testOk(fp != NULL, "Opened temporary file for rule %s", rule);
    if (!fp) return;
    asDumpRulesFP(fp, rule);
    fclose(fp);
    char *buf = readFile(temp_filename);
    unlink(temp_filename);
    testOk(strcmp(expected_config, buf) == 0,
           "asDumpFP %s output matches expected\nExpected:\n%s\nGot:\n%s",
           rule, expected_config, buf);
    free(buf);
    strcpy(temp_filename, "aslib_test_XXXXXX");
}

static void testRulesDumpOutput(void)
{
    testDiag("testRulesDumpOutput()");
    asCheckClientIP = 0;

    testOk1(asInitMem(method_auth_config, NULL)==0);

    runRestDumpRules("DEFAULT",  expected_DEFAULT_rules_config);
    runRestDumpRules("ro",  expected_ro_rules_config);
    runRestDumpRules("rw",  expected_rw_rules_config);
    runRestDumpRules("rwx", expected_rwx_rules_config);
}

MAIN(aslibtest)
{
    testPlan(168);
    testSyntaxErrors();
    testHostNames();
    testDumpOutput();
    testRulesDumpOutput();
    testUseIP();
    testFutureProofParser();
    testMethodAndAuth();
    testCertificateChains();
    errlogFlush();
    return testDone();
}
