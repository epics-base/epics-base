/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Tests for dbCompleteRecord(), the IOC shell tab-completion helper
 * for record names and record.FIELD names.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <dbAccess.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>
#include <dbUnitTest.h>
#include <testMain.h>

static void freeMatches(char **m)
{
    char **p;
    if(!m)
        return;
    for(p = m; *p; p++)
        free(*p);
    free(m);
}

/* Check the result of completing 'word'.
 * 'subst' is the expected replacement text (ret[0]), or NULL to expect
 * no completion at all.  It is followed by the expected list of
 * alternatives, terminated by NULL.  An empty list means a single
 * unique match (ret[1]==NULL).
 */
static void testComplete(const char *word, const char *subst, ...)
{
    char **m = dbCompleteRecord(word);
    va_list ap;
    const char *exp;
    size_t i = 1u;
    int ok = 1;

    testDiag("complete '%s'", word);

    if(!subst) {
        testOk(m==NULL, "'%s' -> no completion", word);
        freeMatches(m);
        return;
    }

    if(!testOk(m!=NULL, "'%s' -> some completion", word)) {
        testSkip(2, "no result");
        return;
    }

    testOk(m[0] && strcmp(m[0], subst)==0,
           "'%s' substitutes '%s' (got '%s')", word, subst, m[0] ? m[0] : "(null)");

    va_start(ap, subst);
    while((exp = va_arg(ap, const char*)) != NULL) {
        if(!m[i] || strcmp(m[i], exp)!=0) {
            testDiag("  alternative %u expected '%s' got '%s'",
                     (unsigned)i, exp, m[i] ? m[i] : "(null)");
            ok = 0;
            break;
        }
        i++;
    }
    va_end(ap);
    if(ok && m[i]) {
        testDiag("  unexpected extra alternative '%s'", m[i]);
        ok = 0;
    }
    testOk(ok, "'%s' alternatives as expected", word);

    freeMatches(m);
}

/* Check every alternative offered for 'word' is a valid "rec.FIELD" */
static void testAllFields(const char *word, const char *recname)
{
    char **m = dbCompleteRecord(word);
    char **p;
    size_t n = 0u;
    int ok = 1;
    DBENTRY ent;

    if(!testOk(m && m[0] && m[1], "'%s' offers multiple fields", word)) {
        freeMatches(m);
        return;
    }

    dbInitEntry(pdbbase, &ent);
    for(p = m+1; *p; p++, n++) {
        size_t rlen = strlen(recname);
        if(strncmp(*p, recname, rlen)!=0 || (*p)[rlen]!='.' ||
           dbFindRecord(&ent, recname) || dbFindField(&ent, *p + rlen + 1)) {
            testDiag("  bad suggestion '%s'", *p);
            ok = 0;
        }
    }
    dbFinishEntry(&ent);
    testOk(ok, "all %u suggestions for '%s' are fields of %s",
           (unsigned)n, word, recname);
    freeMatches(m);
}

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(dbCompleteRecordTest)
{
    testPlan(34);
    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbCompleteRecordTest.db", NULL, NULL);

    testDiag("Record name completion (unchanged behaviour)");
    testComplete("CA", "CA_TESTS:",
                 "CA_TESTS:DONE", "CA_TESTS:LONG", "CA_TESTS:SEQ", NULL);
    testComplete("CA_TESTS:S", "CA_TESTS:SEQ", "CA_TESTS:SEQ", NULL);
    testComplete("CA_TESTS:", "CA_TESTS:",
                 "CA_TESTS:DONE", "CA_TESTS:LONG", "CA_TESTS:SEQ", NULL);
    testComplete("NOSUCH", NULL);

    testDiag("Field name completion");
    testAllFields("CA_TESTS:SEQ.", "CA_TESTS:SEQ");
    testComplete("CA_TESTS:SEQ.F", "CA_TESTS:SEQ.F",
                 "CA_TESTS:SEQ.F32", "CA_TESTS:SEQ.F64", "CA_TESTS:SEQ.FLNK", NULL);
    testComplete("CA_TESTS:SEQ.OT", "CA_TESTS:SEQ.OTST", NULL);
    testComplete("CA_TESTS:SEQ.OTST", "CA_TESTS:SEQ.OTST", NULL);
    testComplete("CA_TESTS:SEQ.DES", "CA_TESTS:SEQ.DESC", NULL);
    testComplete("CA_TESTS:SEQ.VA", "CA_TESTS:SEQ.VAL", NULL);

    testDiag("Field completion through an alias");
    testComplete("ALIAS:SEQ.OT", "ALIAS:SEQ.OTST", NULL);

    testDiag("No completion");
    testComplete("CA_TESTS:SEQ.ZZZ", NULL);
    testComplete("CA_TESTS:SE.V", NULL);
    testComplete("NOSUCH.V", NULL);
    testComplete(".V", NULL);

    testdbCleanup();

    return testDone();
}
