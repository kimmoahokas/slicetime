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
#ifndef WIFI_SIGNAL_NOISE_TAG_H
#define WIFI_SIGNAL_NOISE_TAG_H

#include "ns3/tag.h"

namespace ns3 {

/**
 * \brief Tag which stores signal and noise of a Wifi packet
 *
 * (is used to determine the signal quality in WiFi scans)
 */
class WifiSignalNoiseTag : public Tag
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer buf) const;
  virtual void Deserialize (TagBuffer buf);
  virtual void Print (std::ostream &os) const;
  WifiSignalNoiseTag ();
  WifiSignalNoiseTag (double signal, double noise);
  double GetSignal(void) const;
  double GetNoise(void) const;
private:
  double m_signal;
  double m_noise;
};

} // namespace ns3

#endif /* WIFI_SIGNAL_NOISE_TAG_H */
