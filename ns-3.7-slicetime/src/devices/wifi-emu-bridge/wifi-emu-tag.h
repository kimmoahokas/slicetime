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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */
#ifndef WIFI_EMU_TAG_H
#define WIFI_EMU_TAG_H

#include "ns3/tag.h"

namespace ns3 {

/**
 * \ingroup WifiEmuBridgeModule
 *
 * \brief Packet Tag which is used by WifiEmuComm
 *
 * This tag is used by WifiEmuComm in order to recognize packets it has sent
 * down the wireless network stack. The size-attribute is needed to store their
 * original size.
 *
 * @see WifiEmuBridge
 */
class WifiEmuTag : public Tag
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer buf) const;
  virtual void Deserialize (TagBuffer buf);
  virtual void Print (std::ostream &os) const;
  WifiEmuTag ();
  WifiEmuTag (uint32_t size);
  uint32_t GetSize(void) const;
private:
  uint32_t m_size;
};

} // namespace ns3

#endif /* WIFI_EMU_TAG_H */
