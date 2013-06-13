/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA *
 * Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */

#ifndef WIFI_EMU_COMM_UDP_H
#define WIFI_EMU_COMM_UDP_H

#include "wifi-emu-comm.h"
#include "wifi-emu-msg.h"

#include <map>
#include <sys/socket.h>
#include <arpa/inet.h>

namespace ns3 {
/**
 * \ingroup WifiEmuBridgeModule
 *
 * \brief Helper class for WifiEmuBridge which handles the message exchange using UDP for communication.
 *
 * The configuration takes place through the global variables WifiEmuCommReceiveAddress and WifiEmuCommReceivePort.
 * Global variables have been chosen since there is only one instance which is created through static methods.
 *
 * @see WifiEmuBridge
 */

class WifiEmuCommUdp : public WifiEmuComm
{
// WifiEmuComm has access to our private stuff (kind of 'reverse protected')
friend class WifiEmuComm;

public:
  static TypeId GetTypeId (void);

private:
  /**
   * \internal
   *
   * Creates a new instance of the udp specific communication class.
   * The constructor is private, because it is only used through WifiEmuComm's static methods.
   */
  WifiEmuCommUdp();
  ~WifiEmuCommUdp();

  /**
   * \internal
   *
   * Tries to send a message to client_id
   *
   * \param client_id the client to send the message to (address and port from the register message are used)
   * \param msg the message
   * \param len length of the message
   */
  void CommSend(uint16_t client_id, uint8_t* msg, int len);

  /**
   * \internal
   *
   * Receives a message
   *
   * \param client_id the client_id specified in the message is stored here
   * \param msg the received message is stored here
   * \returns the length of the received message
   */
  int CommRecv(uint16_t &client_id, uint8_t &type, uint8_t* msg);

  // socket to receive with
  int32_t m_sock;

  // map to translate client_id <-> UDP endpoint
  std::map<uint16_t, struct sockaddr_in> m_allAddresses;

};

} // namespace ns3

#endif /* WIFI_EMU_COMM_UDP_H */


