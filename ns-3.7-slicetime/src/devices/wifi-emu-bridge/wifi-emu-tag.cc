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
#include "wifi-emu-tag.h"

namespace ns3 {

TypeId 
WifiEmuTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WifiEmuTag")
    .SetParent<Tag> ()
    .AddConstructor<WifiEmuTag> ()
    ;
  return tid;
}

TypeId
WifiEmuTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t 
WifiEmuTag::GetSerializedSize (void) const
{
  return 4;
}

void 
WifiEmuTag::Serialize (TagBuffer buf) const
{
  buf.WriteU32 (m_size);
}

void 
WifiEmuTag::Deserialize (TagBuffer buf)
{
  m_size = buf.ReadU32 ();
}

void 
WifiEmuTag::Print (std::ostream &os) const
{
  os << "WifiEmu size=" << m_size;
}

WifiEmuTag::WifiEmuTag ()
  : Tag ()
{
}

WifiEmuTag::WifiEmuTag (uint32_t size)
  : Tag (),
    m_size (size)
{
}

uint32_t
WifiEmuTag::GetSize (void) const
{
  return m_size;
}

} // namespace ns3

