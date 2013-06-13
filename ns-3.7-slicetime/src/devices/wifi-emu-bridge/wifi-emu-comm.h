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

#ifndef WIFI_EMU_COMM_H
#define WIFI_EMU_COMM_H

#include "wifi-emu-bridge.h"
#include "ns3/system-thread.h"
#include "ns3/uinteger.h"

#include <map>

namespace ns3 {

/**
 * \ingroup WifiEmuBridgeModule
 *
 * \brief Helper class for WifiEmuBridge which handles the message exchange.
 *
 * This class only contains the framework needed for communication. It contains
 * virtual methods for the communication specific tasks, which derived 
 * subclasses (called WifiEmuCommX) have to implement.
 * The global value WifiEmuCommType specifies which subclass is instantiated.
 *
 * The access to this class is done via the the static methods, which if needed
 * instantiate an instance of the corresponding subclass.
 *
 * The idea is to have only one object which performs the communication and dispatches 
 * received messages to the corresponding WifiEmuBridges.
 *
 * @see WifiEmuBridge
 */
class WifiEmuComm : public Object
{
public:
  static TypeId GetTypeId (void);

  /**
   * Enumeration of communication types
   */
  enum CommType {
    CommTypeUDP,
  };

  /**
   * Registers a WifiEmuBridge and instantiates a WifiEmuComm object if needed.
   *
   * \param clientid the clientid used for communication
   * \param bridge the WifiEmuBridge for which messages shall be received
   */
  static void Register(uint16_t clientid, WifiEmuBridge* bridge);

  /**
   * Unregisters a WifiEmuBridge and deletes the WifiEmuComm instance if not
   * longer needed.
   *
   * \param clientid the clientid used for communication
   */
  static void Unregister(uint16_t clientid);


  /**
   * Sends a message
   *
   * \param the clientid of the driver to send the message to
   * \param msg the data to send
   * \param len length of the data to send
   */
  static void Send(uint16_t clientid, uint8_t* msg, int len);

protected:
  /**
   * Constructor
   * (Just to have a non-public constructor - the static methods create an instance (through the subclass))
   */
  WifiEmuComm ();
  ~WifiEmuComm ();

  /**
   *  is called by subclass's constructor
   */
  void CommStart ();

private:
  /**
   * \internal
   *
   * Tries to send a message to client_id
   *
   * \param client_id the client to send the message to
   * \param msg the message
   * \param len length of the message
   */
  virtual void CommSend(uint16_t client_id, uint8_t* msg, int len) = 0;

  /**
   * \internal
   *
   * Tries to receive a message and loops so long until a correct message was received
   *
   * \param client_id the client_id specified in the message is stored here
   * \param msg the received message is stored here
   * \returns the length of the received message
   */
  virtual int CommRecv(uint16_t &client_id, uint8_t &type, uint8_t* msg) = 0;


  // pointer to the instance of this class
  static WifiEmuComm *m_comm;

  // receives messages (extra thread)
  void ReadThread();

  // handle for the thread used for receiving
  Ptr<SystemThread> m_readThread;

  // map to store currently registered bridges
  std::map<uint16_t, WifiEmuBridge*> m_allBridges;


};

} // namespace ns3

#endif /* WIFI_EMU_COMM_H */


