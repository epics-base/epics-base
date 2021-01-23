/*************************************************************************\
* Copyright (c) 2021 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string>
#include <vector>

#include <string.h>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <osiUnistd.h>
#include <osiFileName.h>
#include <dbDefs.h>

#define SEP OSI_PATH_SEPARATOR

namespace {

void testDF(const char* inp, const char* expectdir, const char* expectfile)
{
    std::vector<char> dbuf(strlen(inp)+1u);
    memcpy(&dbuf[0], inp, dbuf.size()-1u);

    std::vector<char> fbuf(dbuf); // copy

    size_t len = epicsPathDir(&dbuf[0], dbuf.size()-1u);
    dbuf[dbuf.size()-1u] = '\0';
    testOk(len==strlen(expectdir) && strcmp(expectdir, &dbuf[0])==0,
            "\"%s\" directory (%u) \"%s\" == \"%s\"", inp, unsigned(len), &dbuf[0], expectdir);

    len = epicsPathFile(&fbuf[0], fbuf.size()-1u);
    fbuf[fbuf.size()-1u] = '\0';
    testOk(len==strlen(expectfile) && strcmp(expectfile, &fbuf[0])==0,
            "\"%s\" file (%u) \"%s\" == \"%s\"", inp, unsigned(len), &fbuf[0], expectfile);
}

#define testIsAbs(EXPECT, PATH) testOk((EXPECT)==epicsPathIsAbs(PATH, strlen(PATH)), "is %s \"%s\"", (EXPECT)?"abs":"rel", PATH)


#define testJoin(EXPECT, ...) \
do { \
    const char* parts[] = {__VA_ARGS__}; \
    char *actual = epicsPathJoin(parts, NELEMENTS(parts)); \
    testOk(strcmp(actual, EXPECT)==0, "expect \"%s\" == \"%s\"", EXPECT, actual); \
}while(0)

} // namespace

MAIN(epicsPathTest)
{
#if defined(_WIN32) || defined(__CYGWIN__)
    testPlan(58);
#else
    testPlan(44);
#endif

    char *cwd = epicsPathAllocCWD();
    testOk(cwd!=NULL, "cur dir \"%s\"", cwd);
    free(cwd);

    testDiag("Test path split");
    testDF("", "", "");
    testDF("\0notdir/junk", "", "");
    testDF("a", "", "a");
    testDF("/a", "/", "a");
    testDF("a/", "a/", "");
    testDF("test.txt", "", "test.txt");
    testDF("test.txt\0junk", "", "test.txt");
    testDF("test.txt\0notdir/junk", "", "test.txt");
    testDF("dir/file", "dir/", "file");
    testDF("some/dir/file", "some/dir/", "file");
    testDF("some/dir/file\0junk", "some/dir/", "file");
    testDF("/abs/file", "/abs/", "file");
    testDF("./abs/file", "./abs/", "file");

#if defined(_WIN32) || defined(__CYGWIN__)
    testDiag("Test path split windows");
    testDF("dir\\file", "dir\\", "file");
    testDF("dir\\file\0some\\junk/notdir", "dir\\", "file");
    testDF("\\\\system07\\C$\\", "\\\\system07\\C$\\", "");
    testDF("\\\\system07\\C$\\bar", "\\\\system07\\C$\\", "bar");
#endif

    testDiag("Test absolute/relative path classification");
    testIsAbs(0, "");
    testIsAbs(0, "foo");
    testIsAbs(0, "foo/bar");
    testIsAbs(0, "./foo/bar");
    testIsAbs(0, "../foo/bar");
    testIsAbs(1, "/foo");
    testIsAbs(1, "/foo/bar");

#if defined(_WIN32) || defined(__CYGWIN__)
    testDiag("Test absolute/relative path classification windows");
    testIsAbs(0, "C:foo");
    testIsAbs(0, "c:foo");
    testIsAbs(1, "C:/foo");
    testIsAbs(1, "c:/foo");
    testIsAbs(1, "\\\\system07\\C$\\");
    testIsAbs(1, "\\\\system07\\C$\\bar");
#endif

    testDiag("Test path join");
    testJoin("", NULL);
    testJoin("", "", NULL);
    testJoin("", "", "", NULL);
    testJoin("foo1", "foo1", NULL);
    testJoin("foo2", "", "foo2", "", NULL);
    testJoin("/foo3", "/foo3", NULL);
    testJoin("foo" SEP "bar1", "foo", "bar1", NULL);
    testJoin("/foo" SEP "bar2", "baz", "/foo", "bar2", NULL);
    testJoin("/foo" SEP "bar3", "/baz", "/foo", "bar3", NULL);

    {
        std::vector<char> buf(1024);
        if(getcwd(&buf[0], buf.size())==NULL)
            testAbort("PWD is too large");

        std::string expect(&buf[0]);
        expect += SEP;
        expect += "foo";
        expect += SEP;
        expect += "bar";

        testJoin(expect.c_str(), "/ignore", epicsPathJoinCurDir, "foo", "bar", NULL);
    }

    return testDone();
}
