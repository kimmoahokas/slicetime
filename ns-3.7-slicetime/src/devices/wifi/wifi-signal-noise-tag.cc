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
#include "wifi-signal-noise-tag.h"

namespace ns3 {

TypeId 
WifiSignalNoiseTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WifiSignalNoiseTag")
    .SetParent<Tag> ()
    .AddConstructor<WifiSignalNoiseTag> ()
    ;
  return tid;
}

TypeId
WifiSignalNoiseTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t 
WifiSignalNoiseTag::GetSerializedSize (void) const
{
  return 2*sizeof(double);
}

void 
WifiSignalNoiseTag::Serialize (TagBuffer buf) const
{
  buf.WriteDouble (m_signal);
  buf.WriteDouble (m_noise);
}

void 
WifiSignalNoiseTag::Deserialize (TagBuffer buf)
{
  m_signal = buf.ReadDouble ();
  m_noise = buf.ReadDouble ();
}

void 
WifiSignalNoiseTag::Print (std::ostream &os) const
{
  os << "WifiSignalNoise signal=" << m_signal << " noise=" << m_noise;
}

WifiSignalNoiseTag::WifiSignalNoiseTag ()
  : Tag ()
{
}

WifiSignalNoiseTag::WifiSignalNoiseTag (double signal, double noise)
  : Tag (),
    m_signal (signal),
    m_noise (noise)
{
}

double
WifiSignalNoiseTag::GetSignal (void) const
{
  return m_signal;
}

double
WifiSignalNoiseTag::GetNoise (void) const
{
  return m_noise;
}

} // namespace ns3

