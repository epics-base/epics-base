/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2003 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to the Software License Agreement
* found in the file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author: Andrew Johnson   Date: 2003-04-08 */

#include <iostream>
#include <string>
#include <list>
#include <stdexcept>

#include <epicsGetopt.h>
#include "registryFunction.h"
#include "epicsThread.h"
#include "epicsExit.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "dbStaticLib.h"
#include "subRecord.h"
#include "dbAccess.h"
#include "asDbLib.h"
#include "iocInit.h"
#include "iocsh.h"
#include "osiFileName.h"
#include "epicsInstallDir.h"
#include "dbStaticPvt.h"

extern "C" int softIoc_registerRecordDeviceDriver(struct dbBase *pdbbase);

#ifndef EPICS_BASE
// so IDEs knows EPICS_BASE is a string constant
#  define EPICS_BASE "/"
#  error -DEPICS_BASE required
#endif



#define DBD_BASE "dbd" OSI_PATH_SEPARATOR "softIoc.dbd"
#define EXIT_BASE "db" OSI_PATH_SEPARATOR "softIocExit.db"
#define DBD_FILE_REL ".." OSI_PATH_SEPARATOR ".." OSI_PATH_SEPARATOR DBD_BASE
#define EXIT_FILE_REL ".." OSI_PATH_SEPARATOR ".." OSI_PATH_SEPARATOR EXIT_BASE
#define DBD_FILE EPICS_BASE OSI_PATH_SEPARATOR DBD_BASE
#define EXIT_FILE EPICS_BASE OSI_PATH_SEPARATOR EXIT_BASE

namespace {

bool verbose = false;

static void exitSubroutine(subRecord *precord) {
    epicsExitLater((precord->a == 0.0) ? EXIT_SUCCESS : EXIT_FAILURE);
}

void usage(const char *arg0, const std::string& base_dbd, const std::string& prefix) {
    std::cout<<"Usage: "<<arg0<<
               " [-D softIoc.dbd] [-h] [-S] [-s] [-v] [-a ascf]\n"
               "[-m macro=value,macro2=value2] [-d file.db]\n"
               "[-x prefix] [st.cmd]\n"
               "\n"
               "    -D <dbd>  If used, must come first. Specify the path to the softIoc.dbdfile.\n"
               "        The compile-time install location is saved in the binary as a default.\n"
               "        If the compile-time default is not found, EPICS_BASE/dbd will also be tried.\n"
               "\n"
               "    -h  Print this mesage and exit.\n"
               "\n"
               "    -S  Prevents an interactive shell being started.\n"
               "\n"
               "    -s  Previously caused a shell to be started.  Now accepted and ignored.\n"
               "\n"
               "    -v  Verbose, display steps taken during startup.\n"
               "\n"
               "    -a <acf>  Access Security configuration file.  Macro substitution is\n"
               "        performed.\n"
               "\n"
               "    -m <MAC>=<value>,... Set/replace macro definitions used by subsequent -d and\n"
               "        -a.\n"
               "\n"
               "    -d <db>  Load records from file (dbLoadRecords).  Macro substitution is\n"
               "        performed.\n"
               "\n"
               "    -x <prefix>  Load softIocExit.db.  Provides a record \"<prefix>:exit\".\n"
               "        Put 0 to exit with success, or non-zero to exit with an error.\n"
               "\n"
               "Any number of -m and -d arguments can be interspersed; the macros are applied\n"
               "to the following .db files.  Each later -m option causes earlier macros to be\n"
               "discarded.\n"
               "\n"
               "A st.cmd file is optional.  If any databases were loaded the st.cmd file will\n"
               "be run *after* iocInit.  To perform iocsh commands before iocInit, all database\n"
               "loading must be performed by the script itself, or by the user from the\n"
               "interactive IOC shell.\n"
               "\n"
               "Compiled-in path to softIoc.dbd is:\n"
               "\t"<<prefix+DBD_FILE_REL<<std::endl;
               if(base_dbd == prefix+DBD_FILE_REL){
                   std::cout<<"If it can't be located here, EPICS_BASE/dbd/softIoc.dbd will also be tried."<<std::endl;
               }
}

void errIf(int ret, const std::string& msg)
{
    if(ret)
        throw std::runtime_error(msg);
}

bool lazy_dbd_loaded;

void lazy_dbd(const std::string& dbd_file, const std::string& prefix) {
    if(lazy_dbd_loaded) return;
    lazy_dbd_loaded = true;
    std::string loaded_file;
    FILE *fp = NULL;
    std::string dbd_relative_to_epics_base(DBD_FILE);
    loaded_file = dbd_file;
    dbOpenFile(&pdbbase, dbd_file.c_str(), &fp);
    if(verbose)
        std::cout<<"dbOpenFile(&pdbbase"<<dbd_file<<",&fp)"<<std::endl;
    if((!fp) && dbd_file==prefix+DBD_FILE_REL){
        loaded_file=dbd_relative_to_epics_base;
        if(verbose)
            std::cout<<"dbOpenFile(&pdbbase"<<dbd_relative_to_epics_base<<",&fp)"<<std::endl;
        dbOpenFile(&pdbbase,dbd_relative_to_epics_base.c_str(),&fp);
    }
    if(fp && dbReadDatabaseFP(&pdbbase, fp, NULL, NULL)){
        std::cerr<<"Failed to read "<<loaded_file<<", but it was found and loaded."<<std::endl;
    }
    if (verbose)
        std::cout<<"softIoc_registerRecordDeviceDriver(pdbbase)"<<std::endl;
    errIf(softIoc_registerRecordDeviceDriver(pdbbase),
          "Failed to initialize database");
    registryFunctionAdd("exit", (REGISTRYFUNCTION) exitSubroutine);
}

} // namespace

int main(int argc, char *argv[])
{
    try {
        std::string dbd_file(DBD_FILE),
                    exit_file(EXIT_FILE),
                    macros, // scratch space for macros (may be given more than once)
                    xmacro;
        bool interactive = true;
        bool loadedDb = false;
        bool ranScript = false;

        // attempt to compute relative paths
        std::string prefix;
        char *cprefix = epicsGetExecDir();
        if(cprefix) {
            try {
                prefix = cprefix;
                free(cprefix);
            } catch(...) {
                free(cprefix);
                throw;
            }
        }

        dbd_file = prefix + DBD_FILE_REL;
        exit_file = prefix + EXIT_FILE_REL;

        int opt;

        while ((opt = getopt(argc, argv, "ha:D:d:m:Ssx:v")) != -1) {
            switch (opt) {
            case 'h':               /* Print usage */
                usage(argv[0], dbd_file, prefix);
                epicsExit(0);
                return 0;
            default:
                usage(argv[0], dbd_file, prefix);
                std::cerr<<"Unknown argument: -"<<char(opt)<<"\n";
                epicsExit(2);
                return 2;
            case 'a':
                lazy_dbd(dbd_file, prefix);
                if (!macros.empty()) {
                    if (verbose)
                        std::cout<<"asSetSubstitutions(\""<<macros<<"\")\n";
                    if(asSetSubstitutions(macros.c_str()))
                        throw std::bad_alloc();
                }
                if (verbose)
                    std::cout<<"asSetFilename(\""<<optarg<<"\")\n";
                if(asSetFilename(optarg))
                    throw std::bad_alloc();
                break;
            case 'D':
                if(lazy_dbd_loaded) {
                    throw std::runtime_error("-D specified too late.  softIoc.dbd already loaded.\n");
                }
                dbd_file = optarg;
                break;
            case 'd':
                lazy_dbd(dbd_file, prefix);
                if (verbose) {
                    std::cout<<"dbLoadRecords(\""<<optarg<<"\"";
                    if(!macros.empty())
                        std::cout<<", \""<<macros<<"\"";
                    std::cout<<")\n";
                }
                errIf(dbLoadRecords(optarg, macros.c_str()),
                      std::string("Failed to load: ")+optarg);
                loadedDb = true;
                break;
            case 'm':
                macros = optarg;
                break;
            case 'S':
                interactive = false;
                break;
            case 's':
                break; // historical
            case 'v':
                verbose = true;
                break;
            case 'x':
                lazy_dbd(dbd_file, prefix);
                xmacro  = "IOC=";
                xmacro += optarg;
                FILE *fp;
                if(verbose)
                    std::cout<<"dbOpenFile(\""<<exit_file<<"\", \""<<xmacro<<"\")\n";
                dbOpenFile(&pdbbase, exit_file.c_str(), &fp);
                if(!fp){
                    dbOpenFile(&pdbbase, EXIT_FILE, &fp);
                    if(verbose)
                        std::cout<<"dbOpenFile(\""<<EXIT_FILE<<"\", \""<<xmacro<<"\")\n";
                }
                errIf(fp && dbLoadRecords(exit_file.c_str(), xmacro.c_str()),
                      std::string("Failed to read: ")+exit_file+", but it was found and loaded.\n");
                loadedDb = true;
                break;
            }
        }

        lazy_dbd(dbd_file, prefix);

        if(optind<argc)  {
            // run script
            // ignore any extra positional args (historical)

            if (verbose)
                std::cout<<"# Begin "<<argv[optind]<<"\n";
            errIf(iocsh(argv[optind]),
                        std::string("Error in ")+argv[optind]);
            if (verbose)
                std::cout<<"# End "<<argv[optind]<<"\n";

            epicsThreadSleep(0.2);
            ranScript = true;    /* Assume the script has done any necessary initialization */
        }

        if (loadedDb) {
            if (verbose)
                std::cout<<"iocInit()\n";
            iocInit();
            epicsThreadSleep(0.2);
        }

        if(interactive) {
            std::cout.flush();
            std::cerr.flush();
            if(iocsh(NULL)) {
                epicsExit(1);
                return 1;
            }

        } else {
            if (loadedDb || ranScript) {
                // non-interactive IOC.  spin forever
                while(true) {
                    epicsThreadSleep(1000.0);
                }

            } else {
                usage(argv[0], dbd_file, prefix);
                std::cerr<<"Nothing to do!\n";
                epicsExit(1);
                return 1;
            }
        }

        epicsExit(0);
        return 0;

    }catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        epicsExit(2);
        return 2;
    }
}
