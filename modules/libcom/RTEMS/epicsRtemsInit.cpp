/*************************************************************************\
* Copyright (c) 2026 Chris Johns
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <epicsThread.h>

#include <rtems.h>
#include <rtems/shell.h>

#include <epicsRtemsInit.h>

extern "C" { int main(int argc, const char** argv); }

using main_args = std::vector<const char*>;
using args_type = std::vector<std::string>;

struct initHandler {
    std::string domain;
    std::string name;
    size_t order;
    bool enabled;
    epicsRtemsInitHandler handler;
    initHandler(
        const char* domain, const char* name, const size_t order, bool enabled,
        const epicsRtemsInitHandler& handler);
    initHandler(const initHandler& from);
    std::string str() const;
};

using initHandlers = std::vector<initHandler>;
using initHandlers_ptr = std::unique_ptr<initHandlers>;
using initLockType = std::mutex;
using initLockGuard = std::lock_guard<initLockType>;

static initHandlers_ptr handlers;
static initLockType initLock;
static std::once_flag handlers_make;
static std::atomic_bool handlers_init;

static constexpr bool rtems_shell_force = false;
static constexpr bool rtems_shell_ioc_error = true;

static void make_handlers() {
    handlers = std::make_unique<initHandlers>();
}

static bool check_fields(const std::string& label, const char* cstr) {
    std::string str(cstr);
    std::string::size_type pos = 0;
    while (pos != std::string::npos) {
        auto comma = str.find(',', pos);
        auto field = str.substr(pos, comma - pos);
        if (label == field) {
            return true;
        }
        pos = comma;
        if (pos != std::string::npos) {
            ++pos;
        }
    }
    return false;
}

initHandler::initHandler(
    const char* domain_, const char* name_, const size_t order_,
    const bool enabled_, const epicsRtemsInitHandler& handler_)
    : domain(domain_), name(name_), order(order_), enabled(enabled_),
      handler(handler_) {
}

initHandler::initHandler(const initHandler& from)
    : domain(from.domain), name(from.name), order(from.order),
      enabled(from.enabled), handler(from.handler) {
}

std::string initHandler::str() const {
    return domain + '.' + name;
}

epicsRtemsInitRegister::epicsRtemsInitRegister(
    const char* domain, const char* name, const size_t order,
    const bool enabled, const epicsRtemsInitHandler& handler) {
    epicsRtemsInitRegisterHandler(domain, name, order, enabled, handler);
}

/*
 * Register an initialize handler called in order.
 */
int epicsRtemsInitRegisterHandler(
    const char* domain, const char* name, const size_t order,
    const bool enabled, const epicsRtemsInitHandler& handler) {
    std::call_once(handlers_make, make_handlers);
    if (handlers_init.load()) {
        std::cout << "warning: register while init running: "
                  << domain << '.' << name << std::endl;
        return -1;
    }
    initLockGuard guard(initLock);
    auto matchi = std::find_if(
        handlers->begin(), handlers->end(), [name](auto& hnd) {
            return hnd.name == name;
        });
    /* A unique name is a unique handler */
    if (matchi != handlers->end()) {
        auto& match = *matchi;
        /* Abort if there is a duplicate module */
        assert(match.domain != domain || match.order != order);
        /* Override any system domain modules */
        if (match.domain == "system") {
            match.domain = domain;
            match.order = order;
            match.enabled = enabled;
            match.handler = handler;
            return 0;
        }
        return -1;
    }
    handlers->emplace_back(domain, name, order, enabled, handler);
    return 0;
}

static void initConsole(void)
{
    struct termios t;
    if (tcgetattr(fileno (stdin), &t) < 0) {
        fprintf(stderr, "tcgetattr failed: %s\n", strerror (errno));
        return;
    }
    t.c_iflag &= ~(IXOFF | IXON | IXANY);
    if (tcsetattr(fileno (stdin), TCSANOW, &t) < 0) {
        fprintf(stderr, "tcsetattr failed: %s\n", strerror (errno));
        return;
    }
}

static void initChangePriority(unsigned int priority) {
    struct epicsThreadOSD info;
    info.osiPriority = priority;
    int posix_priority = epicsThreadGetPosixPriority(&info);
    int r = pthread_setschedprio(pthread_self(), posix_priority);
    if (r != 0) {
        printf("error: initChangePriority: cannot set priority: %s\n",
               std::strerror(r));
    }
}

static void initMakeArguments(args_type& args, main_args& margs) {
    auto bootfilep = getenv("RTEMS_NET_BOOT_FILE");
    std::string bootfile;
    if (bootfilep == nullptr) {
        bootfile = "ioc";
    } else {
        bootfile = bootfilep;
    }
    args.emplace_back(bootfile);
    margs.emplace_back(args.back().c_str());
    auto cmdlinep = getenv("RTEMS_BOOT_CMD_LINE");
    if (cmdlinep != nullptr) {
        std::string cmdline = cmdlinep;
        while (!cmdline.empty()) {
            auto first_space = cmdline.find_first_of(' ');
            if (first_space == 0) {
                auto last_space = cmdline.find_first_not_of(' ');
                cmdline.erase(first_space, last_space);
                continue;
            }
            args.emplace_back(cmdline.substr(0, first_space));
            margs.emplace_back(args.back().c_str());
            if (first_space == std::string::npos) {
                cmdline.clear();
            } else {
                cmdline.erase(0, first_space);
            }
        }
    }
    margs.push_back(nullptr);
}

static void epicsRtemsInit_register() {
    epicRtemsInit_cmds();
    epicRtemsInit_debugger();
    epicRtemsInit_filesys();
    epicRtemsInit_ioc();
    epicRtemsInit_log();
    epicRtemsInit_net();
    epicRtemsInit_nfs();
    epicRtemsInit_ntp();
}

static void update_env(const char* env, const char* domain, const char* name) {
    std::string label;
    label += domain;
    label += '.';
    label += name;
    std::string estr;
    char* enable = getenv(env);
    if (enable != nullptr) {
        estr = enable;
    }
    if (!estr.empty()) {
        estr += ',';
    }
    estr += label;
    auto r = setenv(env, estr.c_str(), 1);
    if (r != 0) {
        std::cout << "error: setenv: " << env
                  << ": " << std::strerror(errno) << std::endl;
    }
}

void epicsRtemsInit_enable(const char* domain, const char* name) {
    update_env("RTEMS_INIT_ENABLE", domain, name);
}

void epicsRtemsInit_disable(const char* domain, const char* name) {
    update_env("RTEMS_INIT_DISABLE", domain, name);
}

static bool epicsRtemsInit_enabled(const initHandler& handler) {
    const char* enable = getenv("RTEMS_INIT_ENABLE");
    const char* disable = getenv("RTEMS_INIT_DISABLE");
    std::string label = handler.str();
    bool enabled = handler.enabled;
    if (!enabled && enable != nullptr) {
        enabled = check_fields(label, enable);
    }
    if (enabled && disable != nullptr) {
        enabled = !check_fields(label, disable);
    }
    return enabled;
}

/*
 * Use the default POSIX entry point. It can be overrided with:
 *  CONFIGURE_POSIX_INIT_THREAD_ENTRY_POINT
 */
extern "C" { void *POSIX_Init(void *argument); }
void *POSIX_Init(void *) {
    /*
     * Initialize the console to settings for EPICS
     */
    initConsole();

    /*
     * Make the handlers if not made and call module intialise
     * register functions to registers system handlers.
     */
    std::call_once(handlers_make, make_handlers);
    epicsRtemsInit_register();

    /*
     * RTEMS does not provide a way to set the default prioirty of the
     * POSIX entry point as doing so adds a lot of unnecessary
     * configuration clutter. Change the default POSIX priority of 2
     * here early.
     *
     * We cannot use epicsThreadSetPriority because this thread is not
     * known to EPICS and it righty rejects changing the priority as
     * the priority scale could be differernt. Force the change onto
     * this thread.
     */
    initChangePriority(epicsThreadPriorityIocsh);

    /*
     * Say hello
     */
    std::cout << std::endl
              << "EPICS RTEMS " << rtems_get_version_string() << std::endl
              << "  Tools: " << __VERSION__ << std::endl
              << "  Cores: "
#if RTEMS_SMP
              << rtems_scheduler_get_processor_maximum()
#else
              << '1'
#endif
              << std::endl << std::endl;

    /*
     * No more registrations
     */
    handlers_init = true;

    /*
     * Sort the registered init handlers then run them
     */
    std::sort(
        handlers->begin(), handlers->end(), [](auto& a, auto& b) {
            return a.order < b.order;
        });
    size_t step = 0;
    int first_error = 0;
    std::for_each(
        handlers->begin(), handlers->end(),
        [&step, &first_error](auto& handler) {
            ++step;
            bool enabled = epicsRtemsInit_enabled(handler);
            std::cout << "] Init (" << handler.order
                      << ',' << step << '/' << handlers->size()
                      << ',' << static_cast<const char*>(enabled ? "enabled" : "disabled")
                      << "): " << handler.str()
                      << std::endl << std::flush;
            if (enabled) {
                int error;
                try {
                    error = handler.handler();
                } catch (std::exception& e) {
                    std::cout << "error: " << handler.name
                              << ": exception=" << e.what()
                              << std::endl;
                    error = 1000;
                }
                if (error != 0) {
                    if (first_error == 0) {
                        first_error = error;
                    }
                    std::cout << "error: " << handler.name << ": code=" << error
                              << std::endl;
                }
            }
            std::cout << std::flush;
        });

    /*
     * Args before the pointers so the pointers are destructed first
     */
    args_type args;
    main_args margs;
    initMakeArguments(args, margs);
    std::cout << std::endl << "IOC Command line:";
    for (auto arg : args) {
        std::cout << ' '<< arg;
    }
    std::cout << std::endl << std::endl << std::flush;

    int ec = 0;

    /*
     * Run EPICS if the RTEMS shell is not forced and there were no
     * initialize error
     */
    if (!rtems_shell_force && first_error == 0) {
        ec = main(args.size(), margs.data());
        std::cout << std::endl << "EPICS IOC exiting: code=" << ec << std::endl;
    }

    if (rtems_shell_force || first_error != 0 ||
        (rtems_shell_ioc_error && ec != 0)) {
        if (ec != 0) {
            std::cout << "IOC error, starting RTEMS shell ..." << std::endl;
        } else if (first_error != 0) {
            std::cout << "Init error, starting RTEMS shell ..." << std::endl;
        } else if (rtems_shell_force) {
            std::cout << "Init forced to RTEMS shell ..." << std::endl;
        }
        rtems_shell_init(
            "SHLL", RTEMS_MINIMUM_STACK_SIZE * 4, 100, "/dev/console",
            false, true, NULL);
    }

    exit(ec);

    return NULL;
}
