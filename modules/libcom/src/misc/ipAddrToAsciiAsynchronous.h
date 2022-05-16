/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file ipAddrToAsciiAsynchronous.h
 * \brief Convert ip addresses to ASCII asynchronously
 *
 * ipAddrToAsciiEngine is the first class needed to convert an ipAddr to an
 * ASCII address.  From the engine, you can use createTransaction() to create a
 * transaction object.  The transaction object lets you attach callbacks and
 * convert an address to ASCII.
 *
 * Example
 * -------------
 *
 * \code{.cpp}
 * class ConvertIPAddr: ipAddrToAsciiCallBack
 * {
 *      ipAddrToAsciiTransaction & trans;
 *
 * public:
 *      epicsEvent complete;
 *
 *      ConvertIPAddr(ipAddrToAsciiEngine & engine, osiSockAddr & addr):
 *          trans(engine.createTransaction())
 *      {
 *          trans.ipAddrToAscii(addr, *this);
 *      }
 *
 *      virtual void transactionComplete (char const * node) override
 *      {
 *          printf("Address is %s\n", node);
 *          complete.signal();
 *      }
 *
 *      virtual void show(unsigned level) override const
 *      {
 *          printf("This is a ConvertIPAddr class object.");
 *      }
 *
 *      virtual ~ConvertIPAddr()
 *      {
 *          trans.release();
 *      }
 * };
 *
 * ipAddrToAsciiEngine & engine(ipAddrToAsciiEngine::allocate());
 *
 * osiSockAddr addr;
 * addr.ia.sin_family = AF_INET;
 * addr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
 * addr.ia.sin_port = htons(8080);
 *
 * ConvertIPAddr result(engine, addr);
 *
 * // Do other work here before waiting on the result
 *
 * result.complete.wait(2.0);
 * engine.release();
 * \endcode
 */

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef ipAddrToAsciiAsynchronous_h
#define ipAddrToAsciiAsynchronous_h

#include "osiSock.h"
#include "libComAPI.h"

/** \brief Users implement this virtual class to use ipAddrToAsciiTransaction
 *
 * In order to use ipAddrToAsciiTranaction, users should implement this virtual
 * class to handle events that occur asynchronously while converting the IP
 * address.
 *
 */
class LIBCOM_API ipAddrToAsciiCallBack {
public:
    /// Called once the ip address is converted to ASCII
    /*
     * \param pHostName The converted ASCII name */
    virtual void transactionComplete ( const char * pHostName ) = 0;

    /// Called by the show() method of ipAddrToAsciiTransaction or
    /// ipAddrToAsciiEngine when passed a level >= 1.
    virtual void show ( unsigned level ) const;

    virtual ~ipAddrToAsciiCallBack () = 0;
};

/// Class which convert an ipAddr to ascii and call a user-supplied callback
/// when finished.
class LIBCOM_API ipAddrToAsciiTransaction {
public:
    /// Destroy this transaction object and remove from the parent engine object
    virtual void release () = 0;

    /** \brief Convert an IP address to ascii, asynchronously
     *
     * \note The ipAddrToAsciiCallBack referenced must remain valid until release() is called on this transaction.
     * \param addrIn Reference to the address to convert
     * \param cbIn Reference to the user supplied callbacks to call when the result is available
     */
    virtual void ipAddrToAscii ( const osiSockAddr & addrIn, ipAddrToAsciiCallBack & cbIn ) = 0;

    /** \brief Get the conversion address currently set
     * \return Get the last (or current) address converted to ascii
     */
    virtual osiSockAddr address () const  = 0;

    /**
     * \brief Prints the converted IP address
     *
     * Prints to stdout
     *
     * \param level 0 prints basic info, greater than 0 prints information from the callback's show() method too
     */
    virtual void show ( unsigned level ) const = 0;
protected:
    virtual ~ipAddrToAsciiTransaction () = 0;
};

/// Class which manages creating transactions for converting ipAddr's to ASCII
class LIBCOM_API ipAddrToAsciiEngine {
public:
    /// Cancel any pending transactions and destroy this ipAddrToAsciiEngine object.
    virtual void release () = 0;

    /**
     * \brief Create a new transaction object used to do IP address conversions
     * \note Caller must release() the returned transaction
     * \return The newly created transaction object
     */
    virtual ipAddrToAsciiTransaction & createTransaction () = 0;

    /**
     * \brief Print information about this engine object and how many requests its processing
     *
     * Prints to stdout
     *
     * \param level 0 for basic information, 1 for extra info
     */
    virtual void show ( unsigned level ) const = 0;

    /**
     * \brief Creates a new ipAddrToAsciiEngine to convert IP addresses
     * \note Caller must release() this engine.
     * \return Newly created engine object
     */
    static ipAddrToAsciiEngine & allocate ();
protected:
    virtual ~ipAddrToAsciiEngine () = 0;
public:
#ifdef EPICS_PRIVATE_API
    static void cleanup();
#endif
};

#endif // ifdef ipAddrToAsciiAsynchronous_h
