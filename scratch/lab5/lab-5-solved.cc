/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
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
 */



/*
	LAB Assignment #5
    1. Setup a 5x5 wireless adhoc network with a grid. You may use
    examples/wireless/wifi-simple-adhoc-grid.cc as a base.

    2. Install the OLSR routing protocol.

    3. Setup three UDP traffic flows, one along each diagonal and one
    along the middle (at high rates of transmission).

    4. Setup the ns-3 flow monitor for each of these flows.

    5. Now schedule each of the flows at times 1s, 1.5s, and 2s.

    6. Now using the flow monitor, observe the throughput of each of the
    UDP flows. Furthermore, use the tracing mechanism to monitor the number of
    packet collisions/drops at intermediary nodes. Around which nodes are most
    of the collisions/drops happening?

    7. Now repeat the experiment with RTS/CTS enabled on the wifi devices.

    8. Show the difference in throughput and packet drops if any.


	Solution by: Konstantinos Katsaros (K.Katsaros@surrey.ac.uk)
	based on wifi-simple-adhoc-grid.cc
*/

// The default layout is like this, on a 2-D grid.
//
// n20  n21  n22  n23  n24
// n15  n16  n17  n18  n19
// n10  n11  n12  n13  n14
// n5   n6   n7   n8   n9
// n0   n1   n2   n3   n4
//
// the layout is affected by the parameters given to GridPositionAllocator;
// by default, GridWidth is 5 and numNodes is 25..
//
// Flow 1: 0->24
// Flow 2: 20->4
// Flow 3: 10->4

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/flow-monitor-module.h"
#include "myapp.h"

#include "ns3/stats-module.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("Lab5");

using namespace ns3;

class Experiment
{
public:
  Experiment()
  {
    m_bytesTotal = 0;
    isFromAddressInitialized = false;
    flowsCount = 0;
    useErrorModel = false;
  }

  Experiment(std::string name) : m_output(name)
  {
    m_bytesTotal = 0;
    isFromAddressInitialized = false;
    flowsCount = 0;
    useErrorModel = false;
  }

  Gnuplot2dDataset Run(std::string wifiManager);

  void SetUseErrorModel(bool val)
  {
    useErrorModel = true;
  }

private:
  uint32_t MacTxDropCount;
  uint32_t MacRxDropCount;
  uint32_t PhyTxDropCount;
  uint32_t PhyRxDropCount;

  // speed tester
  uint32_t m_bytesTotal;
  Gnuplot2dDataset m_output;
  Ptr<PacketSink> packetSinkApp1;
  Address fromAddress;
  bool isFromAddressInitialized;
  int flowsCount;

  bool useErrorModel;

  void
  MacTxDrop(Ptr<const Packet> p);

  void
  MacRxDrop(Ptr<const Packet> p);

  void
  PrintDrop();

  void
  PhyTxDrop(Ptr<const Packet> p);

  void
  PhyRxDrop(Ptr<const Packet> p);


  void 
  MeasureSpeed();

  void
  ReceivePacket (Ptr<Socket> socket);

  void 
  SetRecvCallback();

  void
  SetupFlow(int from, int to, Time start, Time stop, NodeContainer c, Ipv4InterfaceContainer ifcont, int packetSize, int numPackets, bool trace);

  void IncrementFlowCounter()
  {
    flowsCount++;
  }

  void DecrementFlowCounter()
  {
    flowsCount--;
  }

};

  void
  Experiment::MacTxDrop(Ptr<const Packet> p)
  {
    NS_LOG_INFO("Packet Drop");
    MacTxDropCount++;
  }

  void
  Experiment::MacRxDrop(Ptr<const Packet> p)
  {
    NS_LOG_INFO("Packet Drop");
    MacRxDropCount++;
  }

  void
  Experiment::PrintDrop()
  {
    //std::cout << Simulator::Now().GetSeconds() << "\t" << MacTxDropCount << "\t"<< PhyTxDropCount << "\t" << PhyRxDropCount << "\t" << MacRxDropCount << "\n";
    Simulator::Schedule(Seconds(5.0), &Experiment::PrintDrop, this);
  }

  void
  Experiment::PhyTxDrop(Ptr<const Packet> p)
  {
    NS_LOG_INFO("Packet Drop");
    PhyTxDropCount++;
  }
  void
  Experiment::PhyRxDrop(Ptr<const Packet> p)
  {
    NS_LOG_INFO("Packet Drop");
    PhyRxDropCount++;
  }


  void 
  Experiment::MeasureSpeed()
  {
    double mbs = (m_bytesTotal * 8.0) / 1000000;
    //std::cout << Simulator::Now().GetSeconds() << " mbs = " << mbs << std::endl;
    m_bytesTotal = 0;
    m_output.Add (Simulator::Now().GetSeconds(), mbs);

    Simulator::Schedule (Seconds (1.0), &Experiment::MeasureSpeed, this);
  }

  void
  Experiment::ReceivePacket (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address recievedFrom;

    while ((packet = socket->RecvFrom (recievedFrom)))
      {
         //Ipv6Address experimentAddress = Ipv6Address::ConvertFrom(recievedFrom);

        //std::cout << ((Address)fromAddress).GetMType() << " == " << recievedFrom.GetMType() << "?" << std::endl;

        /*

        if (!isFromAddressInitialized)
        {
          fromAddress = recievedFrom;
          isFromAddressInitialized = true;
        }

        */

          m_bytesTotal += packet->GetSize ();
      }

    //std::cout << "ReceivePacket()" << m_bytesTotal << std::endl;
  }

  void 
  Experiment::SetRecvCallback()
  {
    Ptr<Socket> sinkSocket1 = packetSinkApp1->GetListeningSocket();
    Callback<void, Ptr<Socket> > receivePacketCallback = MakeCallback (&Experiment::ReceivePacket, this);
    sinkSocket1->SetRecvCallback (receivePacketCallback);
  }


void 
Experiment::SetupFlow(int from, int to, Time start, Time stop, NodeContainer c, Ipv4InterfaceContainer ifcont, int packetSize, int numPackets, bool trace)
{
  uint16_t sinkPort = 6; // use the same for all apps

  // UDP connection from N0 to N24
  Address sinkAddress1 (InetSocketAddress (ifcont.GetAddress (to), sinkPort)); // interface of n24

  PacketSinkHelper packetSinkHelper1 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps1 = packetSinkHelper1.Install (c.Get (to)); //n2 as sink
  sinkApps1.Start (Seconds (0.));
  sinkApps1.Stop (Seconds (150.));

  Ptr<Socket> ns3UdpSocket1 = Socket::CreateSocket (c.Get (from), UdpSocketFactory::GetTypeId ()); //source at n0

  if (trace) 
  {
    // setup callback
    Ptr<Application> sinkApp1 = sinkApps1.Get(0);
    packetSinkApp1 = DynamicCast<PacketSink>(sinkApp1);
    Simulator::Schedule(Seconds(1.0), &Experiment::SetRecvCallback, this);
    Simulator::Schedule(Seconds(1.0), &Experiment::MeasureSpeed, this);
  }

  // Create UDP application at n0
  Ptr<MyApp> app1 = CreateObject<MyApp> ();
  app1->Setup (ns3UdpSocket1, sinkAddress1, packetSize, numPackets, DataRate (60000000));
  c.Get(from)->AddApplication (app1);
  app1->SetStartTime (start);
  app1->SetStopTime (stop);

  Simulator::Schedule(start, &Experiment::IncrementFlowCounter, this);
  Simulator::Schedule(stop, &Experiment::DecrementFlowCounter, this);
}


Gnuplot2dDataset 
Experiment::Run(std::string wifiManager)
{
  //std::string phyMode ("DsssRate1Mbps");
  double distance = 50;  // m
  uint32_t numNodes = 6;  // by default, 5x5
  double interval = 0.001; // seconds
  uint32_t packetSize = 600; // bytes
  uint32_t numPackets = 10000000;
  std::string rtslimit = "1500";
  CommandLine cmd;

  //cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("packetSize", "distance (m)", packetSize);
  // cmd.AddValue ("rtslimit", "RTS/CTS Threshold (bytes)", rtslimit);
  //cmd.Parse (argc, argv);
  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue (rtslimit));
  // Fix non-unicast data rate to be the same as that of unicast
  //Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));

  NodeContainer c;
  c.Create (numNodes);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (-10) ); 
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 

  if (useErrorModel)
  {
    wifiPhy.SetErrorRateModel("ns3::NistErrorRateModel");
  }

  YansWifiChannelHelper wifiChannel;
  
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  /*
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  */
  wifi.SetRemoteStationManager (wifiManager); 
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);



  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 8.6603, 0.0));
  positionAlloc->Add (Vector (-5.0, 8.6603, 0.0));
  positionAlloc->Add (Vector (-5.0, -8.6603, 0.0));
  positionAlloc->Add (Vector (5.0, -8.6603, 0.0));

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);

  // Enable OLSR
  OlsrHelper olsr;

  Ipv4ListRoutingHelper list;
  list.Add (olsr, 10);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (c);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifcont = ipv4.Assign (devices);

  // Create Apps

  SetupFlow(1, 0, Seconds(0), Seconds(100), c, ifcont, 550, numPackets, true);
  SetupFlow(2, 0, Seconds(20), Seconds(100), c, ifcont, 600, numPackets, false);
  SetupFlow(3, 0, Seconds(40), Seconds(100), c, ifcont, 650, numPackets, false);
  SetupFlow(4, 0, Seconds(60), Seconds(100), c, ifcont, 700, numPackets, false);
  SetupFlow(5, 0, Seconds(80), Seconds(100), c, ifcont, 500, numPackets, false);


  // Install FlowMonitor on all nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


  // Trace Collisions
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&Experiment::MacTxDrop, this));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRxDrop", MakeCallback(&Experiment::MacRxDrop, this));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback(&Experiment::PhyRxDrop, this));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxDrop", MakeCallback(&Experiment::PhyTxDrop, this));

  Simulator::Schedule(Seconds(5.0), &Experiment::PrintDrop, this);

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();

  PrintDrop();

  // Print per flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  NS_LOG_UNCOND("----------------------------------");

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
    
          NS_LOG_UNCOND("Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
          //NS_LOG_UNCOND("Tx Packets = " << iter->second.txPackets);
          //NS_LOG_UNCOND("Rx Packets = " << iter->second.rxPackets);
          //NS_LOG_UNCOND("Lost Packets = " << iter->second.lostPackets);
          NS_LOG_UNCOND("Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1024  << " Kbps");
    }
  monitor->SerializeToXmlFile("lab-5.flowmon", true, true);

  Simulator::Destroy ();

  return m_output;
}

int main (int argc, char *argv[])       
{
  Gnuplot gnuplot = Gnuplot ("avatar-van.png");

  Experiment idealExperiment("Ideal");
  Experiment caraExperiment("Cara");
  Experiment aarfExperiment("Aarf");
  //Experiment arfExperiment("Arf");
  Experiment vanExperiment("Hybrid");

  //gnuplot.AddDataset (caraCcaExperiment.Run("ns3::RraaWifiManager"));
  //gnuplot.AddDataset (vanExperiment.Run("ns3::HybridWifiManager"));
  //gnuplot.AddDataset (idealExperiment.Run("ns3::IdealWifiManager"));
  //gnuplot.AddDataset (caraExperiment.Run("ns3::CaraWifiManager"));
  gnuplot.AddDataset (aarfExperiment.Run("ns3::AarfWifiManager"));
  //gnuplot.AddDataset (vanExperiment.Run("ns3::HybridWifiManager"));

  gnuplot.GenerateOutput (std::cout);
}

