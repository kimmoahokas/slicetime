/*
 * Copyright (c) 2010 RWTH Aachen University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */

// This file defines the structs used for message exchange between the driver module and the network simulation
// All structs are packed to avoid that the compiler inserts spaces for aligning reasons.
//  (This would lead to problems if it is done differently on different hosts)

#ifndef _WIFI_EMU_MSG_H
#define _WIFI_EMU_MSG_H

#ifdef __KERNEL__
 #include <linux/types.h>
#else
 #include <stdint.h>
#endif

#ifndef __KERNEL__
 namespace ns3
 {
#endif

#define WIFI_EMU_ESSID_LEN 32
#define WIFI_EMU_MAC_LEN 6

#define WIFI_EMU_MSG_LEN 4096

// message types
#define WIFI_EMU_MSG_REQ_REGISTER 1
#define WIFI_EMU_MSG_RESP_REGISTER 2
#define WIFI_EMU_MSG_UNREGISTER 3
#define WIFI_EMU_MSG_FRAME 4
#define WIFI_EMU_MSG_STATS 5
#define WIFI_EMU_MSG_RESP_SETTER 6
#define WIFI_EMU_MSG_REQ_SETCHANNEL 7
#define WIFI_EMU_MSG_REQ_SETMODE 8
#define WIFI_EMU_MSG_REQ_SETWAP 9
#define WIFI_EMU_MSG_REQ_SETESSID 10
#define WIFI_EMU_MSG_REQ_SETRATE 11
#define WIFI_EMU_MSG_REQ_SETIFUP 12
#define WIFI_EMU_MSG_REQ_SCAN 13
#define WIFI_EMU_MSG_RESP_SCAN 14
#define WIFI_EMU_MSG_REQ_SETSPY 15
#define WIFI_EMU_MSG_SPY_UPDATE 16
#define WIFI_EMU_MSG_FRAME_SENT 17
#define WIFI_EMU_MSG_REQ_FER 18
#define WIFI_EMU_MSG_RESP_FER 19

// WiFi standards
#define WIFI_EMU_STANDARD_UNASSOCIATED 0
#define WIFI_EMU_STANDARD_80211A 1
#define WIFI_EMU_STANDARD_80211B 2

// WiFi modes
#define WIFI_EMU_MODE_UNASSOCIATED 0
#define WIFI_EMU_MODE_MASTER 1
#define WIFI_EMU_MODE_ADHOC 2
#define WIFI_EMU_MODE_MONITOR 3

// general message header (first field of all messages)
struct wifi_emu_msg_header
{
  uint8_t type;
  uint16_t client_id;
} __attribute__ ((__packed__));

// quality of a link
struct wifi_emu_qual
{
  uint8_t qual; // (0..100)
  int8_t level; // signal (dBm)
  int8_t noise; // noise (dBm)
} __attribute__ ((__packed__));

// statistics (included in stats and register messages)
struct wifi_emu_stats
{
  uint8_t standard;
  uint16_t channel; // channel number
  uint8_t mode;
  uint8_t remote_mac[WIFI_EMU_MAC_LEN];
  uint8_t essid[WIFI_EMU_ESSID_LEN];
  uint8_t essid_len;
  uint32_t bitrate; // bit/s
  struct wifi_emu_qual qual;
} __attribute__ ((__packed__));

// scan entry (for scan result)
struct wifi_emu_scan_entry
{
  uint16_t channel;
  uint8_t mode;
  uint8_t remote_mac[WIFI_EMU_MAC_LEN];
  uint8_t essid[WIFI_EMU_ESSID_LEN];
  uint8_t essid_len;
  struct wifi_emu_qual qual;
} __attribute__ ((__packed__));


// register message (driver->simulation)
struct wifi_emu_msg_req_register
{
  struct wifi_emu_msg_header h;
  uint8_t virtual_buf;
} __attribute__ ((__packed__));

// register response (simulation->driver)
struct wifi_emu_msg_resp_register
{
  struct wifi_emu_msg_header h;
  uint8_t ok;
  uint8_t mac_address[WIFI_EMU_MAC_LEN];
  uint16_t mtu;
  struct wifi_emu_stats wstats;
} __attribute__ ((__packed__));


// unregister message (driver->simulation)
struct wifi_emu_msg_unregister
{
  struct wifi_emu_msg_header h;
} __attribute__ ((__packed__));


// network data frame (both directions)
struct wifi_emu_msg_frame
{
  struct wifi_emu_msg_header h;
  uint16_t data_len;
  uint8_t data[];
} __attribute__ ((__packed__));


// stats message (simulation->driver)
struct wifi_emu_msg_stats
{
  struct wifi_emu_msg_header h;
  struct wifi_emu_stats wstats;
} __attribute__ ((__packed__));

// frame sent message (simulation->driver)
struct wifi_emu_msg_frame_sent
{
  struct wifi_emu_msg_header h;
  uint16_t len;
} __attribute__ ((__packed__));

// response to setter messages (simulation->driver)
struct wifi_emu_msg_resp_setter
{
  struct wifi_emu_msg_header h;
  uint8_t ok;
} __attribute__ ((__packed__));

// setter messages (driver->simulation)
struct wifi_emu_msg_req_setchannel
{
  struct wifi_emu_msg_header h;
  uint16_t channel;
} __attribute__ ((__packed__));

struct wifi_emu_msg_req_setmode
{
  struct wifi_emu_msg_header h;
  uint8_t mode;
} __attribute__ ((__packed__));

struct wifi_emu_msg_req_setwap
{
  struct wifi_emu_msg_header h;
  uint8_t remote_mac[WIFI_EMU_MAC_LEN];
} __attribute__ ((__packed__));

struct wifi_emu_msg_req_setessid
{
  struct wifi_emu_msg_header h;
  uint8_t essid[WIFI_EMU_ESSID_LEN];
  uint8_t essid_len;
} __attribute__ ((__packed__));

struct wifi_emu_msg_req_setrate
{
  struct wifi_emu_msg_header h;
  uint32_t bitrate;
} __attribute__ ((__packed__));

struct wifi_emu_msg_req_setifup
{
  struct wifi_emu_msg_header h;
  uint8_t up;
} __attribute__ ((__packed__));

struct wifi_emu_msg_req_setspy
{
  struct wifi_emu_msg_header h;
  uint8_t num_addr;
  uint8_t addr[][WIFI_EMU_MAC_LEN];
} __attribute__ ((__packed__));

// spy data update (simulation->driver)
struct wifi_emu_msg_spy_update
{
  struct wifi_emu_msg_header h;
  uint8_t addr[WIFI_EMU_MAC_LEN];
  struct wifi_emu_qual qual;
} __attribute__ ((__packed__));

// network scan request (driver->simulation)
struct wifi_emu_msg_req_scan
{
  struct wifi_emu_msg_header h;
  uint8_t active;
  uint8_t essid[WIFI_EMU_ESSID_LEN];
  uint8_t essid_len;
} __attribute__ ((__packed__));

// network scan response (simulation->driver)
struct wifi_emu_msg_resp_scan
{
  struct wifi_emu_msg_header h;
  uint8_t num_entries;
  struct wifi_emu_scan_entry data[];
} __attribute__ ((__packed__));

// FER request (driver->simulation)
struct wifi_emu_msg_req_fer
{
  struct wifi_emu_msg_header h;
} __attribute__ ((__packed__));

// FER response (simulation->driver)
struct wifi_emu_msg_resp_fer
{
  struct wifi_emu_msg_header h;
  uint8_t fer;
} __attribute__ ((__packed__));

#ifndef __KERNEL__
 } // namespace ns3
#endif


#endif // _WIFI_EMU_MSG_H


