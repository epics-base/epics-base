/*************************************************************************\
* Copyright (c) 2022 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <vector>
#include <string>

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define EPICS_PRIVATE_API

#include <testMain.h>
#include <epicsUnitTest.h>

#include <dbDefs.h>
#include <epicsStdio.h>
#include <epicsString.h>

namespace {

std::string escape(const char* buf, size_t len)
{
    size_t elen = epicsStrnEscapedFromRawSize(buf, len);

    std::vector<char> ebuf(elen+1u);

    epicsStrnEscapedFromRaw(&ebuf[0], ebuf.size(),
                            buf, len);

    return std::string(ebuf.begin(), ebuf.end());
}

#define STR(S) S, sizeof(S)-1
#define STR2(S) S, sizeof(S)-1, sizeof(S)-1
#define CSTR(A, C) C, sizeof(C)-1, sizeof(A)-1

const epicsIMF testFiles[] = {
    {
        "/db/blah.txt",
        STR2("this is a test\n"),
        epicsIMFRaw,
        1,
    },
    {
        "/some.bin",
        STR2("\xde\xad\0\xbe\xef"),
        epicsIMFRaw,
        1,
    },
    {
        "/test.txt.z",
        CSTR("this is a test!",
             "x\xda+\xc9\xc8,V\x00\xa2" "D\x85\x92\xd4\xe2\x12" "E\x00+j\x05" "7"),
        /* an example of why compression is not always a benefit... */
        epicsIMFZ,
        1,
    },
    {NULL, NULL, 0, 0, epicsIMFRaw, 0}
};

void testMount()
{
    testDiag("testMount()");

    int ret = epicsMemMount(testFiles,
                            EPICS_MEM_MOUNT_VERBOSE);

    testOk(!ret, "testMount %d", ret);
}

void testDynMount()
{
    epicsIMF hello[] = {
        {
        "/hello.txt",
        NULL, 0, 0,
        epicsIMFRaw,
        0,
        },
        {NULL, NULL, 0, 0, epicsIMFRaw, 0}
    };
    char* str = epicsStrDup("hello world");

    hello[0].content = str;
    hello[0].contentLen = hello[0].actualLen = strlen(str);

    testDiag("testDynMount()");

    int ret = epicsMemMount(hello,
                            EPICS_MEM_MOUNT_VERBOSE);

    testOk(!ret, "testDynMount %d", ret);

    str[0] = 'X'; /* spoil */

    free(str);
}

void showFile(void*, const epicsIMF *mf)
{
    testDiag("F: '%s'", mf->path);
}

void testRead(const char* path, const char* mode,
              const char* content, size_t contentLen)
{
    testDiag("testRead(\"%s\", \"%s\")", path, mode);

    std::string econtent(escape(content, contentLen));

    FILE *fp = epicsFOpen(path, mode);
    if(!fp) {
        testFail("Failed to open \"%s\" -> %d", path, errno);

    } else {
        int err;
        std::vector<char> buf(2*contentLen);

        int n = fread(&buf[0], 1u, buf.size()-1u, fp);
        err = errno;
        fclose(fp);
        if(n<0) {
            testFail("IO Error \"%s\" : %d", path, err);

        } else {
            buf.resize(n);
            std::string ebuf(escape(&buf[0], buf.size()));

            testOk(ebuf==econtent, "\"%s\" == \"%s\"", econtent.c_str(), ebuf.c_str());
        }
    }
}

void testCantOpen(int err, const char* path, const char* mode, const char* msg)
{
    FILE *fp = epicsFOpen(path, mode);
    int actual = errno;
    testOk(!fp && actual==err,
                   "Expect failure epicsFOpen(\"%s\", \"%s\") -> %d == %d : %s",
                  path, mode, err, actual, msg);
    if(fp)
        fclose(fp);
}

void testCompressZ()
{
#ifdef IMF_HAS_ZLIB
    testRead("app:///test.txt.z", "r", STR("this is a test!"));
#else
    testCantOpen(EIO, "app:///test.txt.z", "r", "no zlib");
#endif
}

} // namespace

MAIN(epicsMemFileTest)
{
    testPlan(12);
    testMount();
    testDynMount();
    epicsMemFileForEach(&showFile, NULL);
    testRead("app:///hello.txt", "r", STR("hello world"));
    testRead("app:///db/blah.txt", "r", STR("this is a test\n"));
    testRead("app:///some.bin", "rb", STR("\xde\xad\0\xbe\xef"));
    testOk(!epicsMemUnmount("/some.bin", 0), "Unmount /some.bin");
    testCantOpen(ENOENT, "app:///some.bin", "r", "no such file");
    testCantOpen(ENOENT, "app:///blah.txt", "r", "no such file");
    testCantOpen(ENOENT, "app://blah.txt", "r", "missing '/' prefix");
    testCantOpen(ENOENT, "app:///blah.txt/", "r", "missing filename");
    testCantOpen(EINVAL, "app:///blah.txt", "rx", "bad mode");
    testCompressZ();
    return testDone();
}
