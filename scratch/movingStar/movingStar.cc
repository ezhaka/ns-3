/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006,2007 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/stats-module.h"
#include "ns3/wifi-module.h"
#include "math.h"
#include <map>
#include <sstream>
#include <vector>
#include <iostream>

NS_LOG_COMPONENT_DEFINE ("Main");

const double PI = 3.141592653589793238463;

using namespace ns3;

class Experiment
{
public:
  Experiment (std::string name);
  Gnuplot2dDataset Run (const WifiHelper &wifi, const YansWifiPhyHelper &wifiPhy,
                        const NqosWifiMacHelper &wifiMac, const YansWifiChannelHelper &wifiChannel);
private:
  void ReceivePacket (Ptr<Socket> socket);
  void SetPosition (Ptr<Node> node, Vector position);
  Vector GetPosition (Ptr<Node> node);
  void AdvancePosition (Ptr<Node> node);
  Ptr<Socket> SetupPacketReceive (Ptr<Node> node);
  void ChangeSpeed(OnOffHelper onOffHelper);

  void SetupApp(
    int from, 
    Time start, 
    Time stop, 
    NetDeviceContainer devices,
    NodeContainer c,
    bool setupPacketReceive);

  std::string AddressToString(Address address);
  Gnuplot2dDataset dataset;
  std::map<Address, uint32_t> mbytesTotal;
};

Experiment::Experiment (std::string name) : dataset(name)
{
  dataset.SetStyle (Gnuplot2dDataset::LINES);
}

void
Experiment::SetPosition (Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  mobility->SetPosition (position);
}

Vector
Experiment::GetPosition (Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  return mobility->GetPosition ();
}

std::string 
Experiment::AddressToString(Address address)
{
  std::stringstream ss;
  ss << address;
  return ss.str();
}

void 
Experiment::AdvancePosition (Ptr<Node> node) 
{
  typedef std::map<Address, uint32_t>::iterator it_type;
  double mbs = 0.0;

  for (it_type iter = mbytesTotal.begin(); iter != mbytesTotal.end(); iter++)
  {
    mbs += ((iter->second * 8.0) / 1000000);
    iter->second = 0;
  }

  dataset.Add(Simulator::Now().GetSeconds(), mbs / mbytesTotal.size());

  NS_LOG_UNCOND(Simulator::Now().GetSeconds());
  Simulator::Schedule (Seconds (1.0), &Experiment::AdvancePosition, this, node);
}

void
Experiment::ReceivePacket (Ptr<Socket> socket)
{
  Address address;
  Ptr<Packet> packet;

  while ((packet = socket->RecvFrom (address)))
  {
    if (mbytesTotal.count(address))
    {
      mbytesTotal[address] += packet->GetSize();
    }
    else
    {
      mbytesTotal[address] = packet->GetSize();
    }
  }
}

Ptr<Socket>
Experiment::SetupPacketReceive (Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName ("ns3::PacketSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid);
  sink->Bind ();
  sink->SetRecvCallback (MakeCallback (&Experiment::ReceivePacket, this));
  return sink;
}

void
Experiment::SetupApp(
  int from, 
  Time start, 
  Time stop, 
  NetDeviceContainer devices,
  NodeContainer c,
  bool setupPacketReceive)
{
  PacketSocketAddress socket;
  socket.SetSingleDevice (devices.Get (from)->GetIfIndex ());
  socket.SetPhysicalAddress (devices.Get (0)->GetAddress ());
  socket.SetProtocol (1);

  OnOffHelper onoff ("ns3::PacketSocketFactory", Address (socket));
  onoff.SetConstantRate (DataRate (60000000));
  onoff.SetAttribute ("PacketSize", UintegerValue (2000));

  ApplicationContainer apps = onoff.Install (c.Get (from));
  apps.Start (start);
  apps.Stop (stop);
  
  if (setupPacketReceive)
  {
  }
}

Gnuplot2dDataset
Experiment::Run (const WifiHelper &wifi, const YansWifiPhyHelper &wifiPhy,
                 const NqosWifiMacHelper &wifiMac, const YansWifiChannelHelper &wifiChannel)
{
  NodeContainer c;
  int nodesCount = 7;
  c.Create (nodesCount);

  PacketSocketHelper packetSocket;
  packetSocket.Install (c);

  YansWifiPhyHelper phy = wifiPhy;
  phy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper mac = wifiMac;
  NetDeviceContainer devices = wifi.Install (phy, mac, c);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0 * cos(PI / (nodesCount - 1)), 5.0 * sin(PI / (nodesCount - 1)), 0.0));
  positionAlloc->Add (Vector (5.0 * cos(2.0 * PI / (nodesCount - 1)), 5.0 * sin(2.0 * PI / (nodesCount - 1)), 0.0));
  positionAlloc->Add (Vector (5.0 * cos(2.0 * PI / (nodesCount - 1)), 5.0 * sin(2.0 * PI / (nodesCount - 1)), 0.0));
  positionAlloc->Add (Vector (5.0 * cos(3.0 * PI / (nodesCount - 1)), 5.0 * sin(3.0 * PI / (nodesCount - 1)), 0.0));
  positionAlloc->Add (Vector (5.0 * cos(4.0 * PI / (nodesCount - 1)), 5.0 * sin(4.0 * PI / (nodesCount - 1)), 0.0));
  positionAlloc->Add (Vector (5.0 * cos(5.0 * PI / (nodesCount - 1)), 5.0 * sin(5.0 * PI / (nodesCount - 1)), 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (c);

  SetupApp(1, Seconds(0.5), Seconds(300.0), devices, c, true);
  SetupApp(2, Seconds(50.0), Seconds(300.0), devices, c, false);
  SetupApp(3, Seconds(100.0), Seconds(300.0), devices, c, false);
  SetupApp(4, Seconds(150.0), Seconds(300.0), devices, c, false);
  SetupApp(5, Seconds(200.0), Seconds(300.0), devices, c, false);
  SetupApp(6, Seconds(250.0), Seconds(300.0), devices, c, false);

  Simulator::Schedule (Seconds (1.5), &Experiment::AdvancePosition, this, c.Get (0));
  Ptr<Socket> recvSink = SetupPacketReceive (c.Get (0));

  Simulator::Stop (Seconds (250.0));
  Simulator::Run ();

  Simulator::Destroy ();

  return dataset;
}

int main (int argc, char *argv[])
{
  // disable fragmentation
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  Gnuplot gnuplot = Gnuplot ("reference-rates.png");

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();


  gnuplot = Gnuplot ("rate-control.png");
  wifi.SetStandard (WIFI_PHY_STANDARD_holland);

  Gnuplot2dDataset dataset;

  // NS_LOG_DEBUG ("aarf");
  // Experiment experiment = Experiment ("aarf");
  // wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  // dataset = experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  // gnuplot.AddDataset (dataset);

  // NS_LOG_DEBUG ("cara");
  // experiment = Experiment ("cara");
  // wifi.SetRemoteStationManager ("ns3::CaraWifiManager");
  // dataset = experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  // gnuplot.AddDataset (dataset);

  NS_LOG_DEBUG ("hybrid");
  Experiment experiment = Experiment ("hybrid");
  wifi.SetRemoteStationManager ("ns3::HybridWifiManager");
  dataset = experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  gnuplot.AddDataset (dataset);

  // NS_LOG_DEBUG ("ideal");
  // experiment = Experiment ("ideal");
  // wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
  // dataset = experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  // gnuplot.AddDataset (dataset);
  
  gnuplot.GenerateOutput (std::cout);

  return 0;
}
