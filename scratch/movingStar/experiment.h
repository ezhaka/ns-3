#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/flow-monitor-module.h"
#include "myapp.h"
#include "math.h"

#include "ns3/stats-module.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("Lab5");

using namespace ns3;

const double PI = 3.141592653589793238463;

class Experiment
{
public:
  Experiment(std::string name) : m_output(name)
  {
    m_bytesTotal = 0;
    isFromAddressInitialized = false;
    flowsCount = 0;
    useErrorModel = false;
  }

  Gnuplot2dDataset Run(std::string wifiManager);

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

  YansWifiChannelHelper GetChannel();

  void InstallPosition(NodeContainer c);

  WifiHelper GetWifiHelper(std::string wifiManager);

  NqosWifiMacHelper GetWifiMacHelper();

  YansWifiPhyHelper GetWifiPhy(YansWifiChannelHelper wifiChannel);

  Ptr<Socket>
  SetupPacketReceive (Ptr<Node> node);
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
  std::cout << Simulator::Now().GetSeconds() << "\t" << MacTxDropCount << "\t"<< PhyTxDropCount << "\t" << PhyRxDropCount << "\t" << MacRxDropCount << "\n";
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
  double mbs = flowsCount == 0 ? 0 : ((m_bytesTotal * 8.0) / 1000000) / flowsCount;
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

YansWifiChannelHelper Experiment::GetChannel()
{
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

  return wifiChannel;
}

void Experiment::InstallPosition(NodeContainer c)
{
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0 * cos(PI / 5.0), 5.0 * sin(PI / 5.0), 0.0));
  positionAlloc->Add (Vector (5.0 * cos(2.0 * PI / 5.0), 5.0 * sin(2.0 * PI / 5.0), 0.0));
  positionAlloc->Add (Vector (5.0 * cos(2.0 * PI / 5.0), 5.0 * sin(2.0 * PI / 5.0), 0.0));
  positionAlloc->Add (Vector (5.0 * cos(3.0 * PI / 5.0), 5.0 * sin(3.0 * PI / 5.0), 0.0));
  positionAlloc->Add (Vector (5.0 * cos(4.0 * PI / 5.0), 5.0 * sin(4.0 * PI / 5.0), 0.0));

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);
}

WifiHelper Experiment::GetWifiHelper(std::string wifiManager)
{
  WifiHelper wifi = WifiHelper::Default();
  wifi.SetStandard (WIFI_PHY_STANDARD_holland);
  wifi.SetRemoteStationManager (wifiManager); 

  return wifi;
}

YansWifiPhyHelper Experiment::GetWifiPhy(YansWifiChannelHelper wifiChannel)
{
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel(wifiChannel.Create());

  return wifiPhy;
}

NqosWifiMacHelper Experiment::GetWifiMacHelper()
{
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  return wifiMac; 
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

Gnuplot2dDataset 
Experiment::Run(std::string wifiManager)
{
  uint32_t numNodes = 6;

  NodeContainer c;
  c.Create (numNodes);

  YansWifiChannelHelper wifiChannel = GetChannel();
  WifiHelper wifi = GetWifiHelper(wifiManager);
  YansWifiPhyHelper wifiPhy = GetWifiPhy(wifiChannel);
  NqosWifiMacHelper wifiMac = GetWifiMacHelper();
  
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);

  InstallPosition(c);  

  PacketSocketAddress socket;
  socket.SetSingleDevice (devices.Get (1)->GetIfIndex ());
  socket.SetPhysicalAddress (devices.Get (0)->GetAddress ());
  socket.SetProtocol (1);

  OnOffHelper onoff ("ns3::PacketSocketFactory", Address (socket));
  onoff.SetConstantRate (DataRate (60000000));
  onoff.SetAttribute ("PacketSize", UintegerValue (2000));

  ApplicationContainer apps = onoff.Install (c.Get (1));
  apps.Start (Seconds (0.5));
  apps.Stop (Seconds (250.0));

  Ptr<Socket> recvSink = SetupPacketReceive (c.Get (0));


/*
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
*/

  // Install FlowMonitor on all nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


  // Trace Collisions
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&Experiment::MacTxDrop, this));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRxDrop", MakeCallback(&Experiment::MacRxDrop, this));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback(&Experiment::PhyRxDrop, this));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxDrop", MakeCallback(&Experiment::PhyTxDrop, this));

  //Simulator::Schedule(Seconds(5.0), &Experiment::PrintDrop, this);

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();

  //PrintDrop();

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

  Simulator::Destroy ();

  return m_output;
}
