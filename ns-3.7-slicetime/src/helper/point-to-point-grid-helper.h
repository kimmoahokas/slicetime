/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Josh Pelkey <jpelkey@gatech.edu>
 */

#ifndef POINT_TO_POINT_GRID_HELPER_H
#define POINT_TO_POINT_GRID_HELPER_H

#include <vector>

#include "internet-stack-helper.h"
#include "point-to-point-helper.h"
#include "ipv4-address-helper.h"
#include "ipv4-interface-container.h"
#include "net-device-container.h"

namespace ns3 {
  
/**
 * \brief A helper to make it easier to create a grid topology
 * with p2p links
 */
class PointToPointGridHelper 
{
public: 
  /**
   * Create a PointToPointGridHelper in order to easily create
   * grid topologies using p2p links
   *
   * \param nRows total number of rows in the grid
   *
   * \param nCols total number of colums in the grid
   *
   * \param pointToPoint the PointToPointHelper which is used 
   *                     to connect all of the nodes together 
   *                     in the grid
   */
  PointToPointGridHelper (uint32_t nRows, 
                          uint32_t nCols, 
                          PointToPointHelper pointToPoint);

  ~PointToPointGridHelper ();

  /**
   * \param row the row address of the node desired
   *
   * \param col the column address of the node desired
   *
   * \returns a pointer to the node specified by the 
   *          (row, col) address
   */
  Ptr<Node> GetNode (uint32_t row, uint32_t col);

  /**
   * This returns an Ipv4 address at the node specified by 
   * the (row, col) address.  Technically, a node will have 
   * multiple interfaces in the grid; therefore, it also has 
   * multiple Ipv4 addresses.  This method only returns one of 
   * the addresses. If you picture the grid, the address returned 
   * is the left row device of all the nodes, except the left-most 
   * grid nodes, which returns the right row device.
   *
   * \param row the row address of the node desired
   *
   * \param col the column address of the node desired
   *
   * \returns Ipv4Address of one of the intefaces of the node 
   *          specified by the (row, col) address
   */
  Ipv4Address GetIpv4Address (uint32_t row, uint32_t col);

  /**
   * \param stack an InternetStackHelper which is used to install 
   *              on every node in the grid
   */
  void InstallStack (InternetStackHelper stack);

  /**
   * Assigns Ipv4 addresses to all the row and column interfaces
   *
   * \param rowIp the Ipv4AddressHelper used to assign Ipv4 addresses 
   *              to all of the row interfaces in the grid
   *
   * \param colIp the Ipv4AddressHelper used to assign Ipv4 addresses 
   *              to all of the row interfaces in the grid
   */
  void AssignIpv4Addresses (Ipv4AddressHelper rowIp, Ipv4AddressHelper colIp);

  /**
   * Sets up the node canvas locations for every node in the grid.
   * This is needed for use with the animation interface
   *
   * \param ulx upper left x value
   * \param uly upper left y value
   * \param lrx lower right x value
   * \param lry lower right y value
   */
  void BoundingBox (double ulx, double uly, double lrx, double lry);

private:
  uint32_t m_xSize;
  uint32_t m_ySize;
  std::vector<NetDeviceContainer> m_rowDevices;
  std::vector<NetDeviceContainer> m_colDevices;
  std::vector<Ipv4InterfaceContainer> m_rowInterfaces;
  std::vector<Ipv4InterfaceContainer> m_colInterfaces;
  std::vector<NodeContainer> m_nodes;
};

} // namespace ns3
      
#endif /* POINT_TO_POINT_GRID_HELPER_H */
