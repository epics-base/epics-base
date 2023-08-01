/*************************************************************************\
* (C) 2014 David Lettier.
* http://www.lettier.com/
* SPDX-License-Identifier: BSD-3-Clause
\*************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <rtems.h>
#include "epicsNtp.h"

int epicsNtpGetTime(char *ntpIp, struct timespec *now)
{

  int sockfd, n; // Socket file descriptor and the n return result from writing/reading from the socket.
  int portno = 123; // NTP UDP port number.

  // Create and zero out the packet. All 48 bytes worth.
  ntp_packet packet = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  memset( &packet, 0, sizeof( ntp_packet ) );
  // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.
  *( ( char * ) &packet + 0 ) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

  // Create a UDP socket, convert the host-name to an IP address, set the port number,
  // connect to the server, send the packet, and then read in the return packet.
  struct sockaddr_in serv_addr; // Server address data structure.

  sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ); // Create a UDP socket.

  if ( sockfd < 0 ) {
    perror( "epicsNtpGetTime creating socket" );
    return -1;
  }

  // Zero out the server address structure.
  memset( ( char* ) &serv_addr, 0, sizeof( serv_addr ) );

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ntpIp);

  serv_addr.sin_port = htons( portno );

  // Call up the server using its IP address and port number.
  if ( connect( sockfd, ( struct sockaddr * ) &serv_addr, sizeof( serv_addr) ) < 0 ) {
    perror( "epicsNtpGetTime connecting socket" );
    close( sockfd );
    return -1;
  }

  // Send it the NTP packet it wants. If n == -1, it failed.
  n = write( sockfd, ( char* ) &packet, sizeof( ntp_packet ) );
  if ( n < 0 ) {
    perror( "epicsNtpGetTime sending NTP request" );
    close( sockfd );
    return -1;
  }

  // Wait and receive the packet back from the server. If n == -1, it failed.
  n = read( sockfd, ( char* ) &packet, sizeof( ntp_packet ) );
  if ( n < 0 ) {
    perror( "epicsNtpGetTime reading NTP reply" );
    close( sockfd );
    return -1;
  }

  close( sockfd );

  // These two fields contain the time-stamp seconds as the packet left the NTP server.
  // The number of seconds correspond to the seconds passed since 1900.
  // ntohl() converts the bit/byte order from the network's to host's "endianness".
  packet.txTm_s = ntohl( packet.txTm_s ); // Time-stamp seconds.
  packet.txTm_f = ntohl( packet.txTm_f ); // Time-stamp fraction of a second.

  // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
  // Subtract 70 years worth of seconds from the seconds since 1900.
  // This leaves the seconds since the UNIX epoch of 1970.
  // (1900)------------------(1970)**************************************(Time Packet Left the Server)
  time_t txTm = ( time_t ) ( packet.txTm_s - NTP_TIMESTAMP_DELTA );
  now->tv_sec = txTm;
  return 0;
}
