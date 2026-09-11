/*************************************************************************\
* (C) 2014 David Lettier.
* http://www.lettier.com/
* SPDX-License-Identifier: BSD-3-Clause
\*************************************************************************/
#ifndef EPICSSIMPLENTP_H
#define EPICSSIMPLENTP_H

#ifdef __cplusplus
extern "C" {
#endif

#define NTP_TIMESTAMP_DELTA 2208988800ull

int epicsSimpleNtpGetTime(char *ntpIp, struct timespec *now);

typedef struct
{

  uint8_t li_vn_mode __attribute__((unused));      // Eight bits. li, vn, and mode.
                                                   // li.   Two bits.   Leap indicator.
                                                   // vn.   Three bits. Version number of the protocol.
                                                   // mode. Three bits. Client will pick mode 3 for client.

  uint8_t stratum __attribute__((unused));         // Eight bits. Stratum level of the local clock.
  uint8_t poll __attribute__((unused));            // Eight bits. Maximum interval between successive messages.
  uint8_t precision __attribute__((unused));       // Eight bits. Precision of the local clock.

  uint32_t rootDelay __attribute__((unused));      // 32 bits. Total round trip delay time.
  uint32_t rootDispersion __attribute__((unused)); // 32 bits. Max error aloud from primary clock source.
  uint32_t refId __attribute__((unused));          // 32 bits. Reference clock identifier.

  uint32_t refTm_s __attribute__((unused));        // 32 bits. Reference time-stamp seconds.
  uint32_t refTm_f __attribute__((unused));        // 32 bits. Reference time-stamp fraction of a second.

  uint32_t origTm_s __attribute__((unused));       // 32 bits. Originate time-stamp seconds.
  uint32_t origTm_f __attribute__((unused));       // 32 bits. Originate time-stamp fraction of a second.

  uint32_t rxTm_s __attribute__((unused));         // 32 bits. Received time-stamp seconds.
  uint32_t rxTm_f __attribute__((unused));         // 32 bits. Received time-stamp fraction of a second.

  uint32_t txTm_s __attribute__((unused));         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
  uint32_t txTm_f __attribute__((unused));         // 32 bits. Transmit time-stamp fraction of a second.

} ntp_packet;              // Total: 384 bits or 48 bytes.

#ifdef __cplusplus
}
#endif
#endif // EPICSSIMPLENTP_H
