#include <iomanip>
#include <iostream>
#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MTSQ");

std::ofstream cwndStreamT1;
std::ofstream cwndStreamT2;

std::ofstream instantThroughputT1;
std::ofstream instantThroughputT2;
std::ofstream queueLength;



static void
CwndTracerT1 (uint32_t oldval, uint32_t newval)
{
  cwndStreamT1 << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << std::setw (12) << newval << std::endl;
}

static void
CwndTracerT2 (uint32_t oldval, uint32_t newval)
{
  cwndStreamT2 << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << std::setw (12) << newval << std::endl;
}


void
ConnectSocketTraces (void)
{
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracerT1));
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracerT2));
}

void
CheckQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = queue->GetNPackets ();
  uint32_t qSizeBytes = queue->GetNBytes ();
  queueLength << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << " " << qSize << " " << qSizeBytes << std::endl;
  // check queue size every 1/1000 of a second
  Simulator::Schedule (MilliSeconds (1), &CheckQueueSize, queue);
}


void ThroughputMonitor (FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon)
	{
		std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
		Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());
		for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats)
		{
      Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow (stats->first);
      if (stats->first == 1)
      {
        instantThroughputT1 << Simulator::Now ().GetSeconds () << " " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())/1000/1000 << std::endl;
      }
      else if (stats->first == 3)
      {
        instantThroughputT2 << Simulator::Now ().GetSeconds () << " " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())/1000/1000 << std::endl;
      }
		}
		Simulator::Schedule(MicroSeconds(100),&ThroughputMonitor, fmhelper, flowMon);
	}


int
main (int argc, char *argv[])
{

  int PacketSize = 1000; //bytes
  std::string T1SwitchBW = "30Gbps";
  std::string T1SwitchDelay = "1us";
  std::string T2SwitchBW = "100Gbps";
  std::string T2SwitchDelay = "1us";
  std::string switchReceiverBW = "100Gbps";
  std::string switchReceiverDelay = "1us";
  double minTh = 20;
  double maxTh = 60;
  std::string bufferSize = "2666p";


  bool tracing = false;
  uint32_t maxBytes = 0; // value of zero corresponds to unlimited send
  std::string transportProtocol = "ns3::TcpDctcp";
  bool useEcn = true;
  bool useQueueDisc = true;

  Time simulationEndTime = Seconds (0.05);

  // Configure defaults that are not based on explicit command-line arguments
  // They may be overridden by general attribute configuration of command line
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (transportProtocol)));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));

  CommandLine cmd (__FILE__);
  cmd.AddValue("T1SwitchBW", "tenant 1 to switch bandwidth", T1SwitchBW);
  cmd.AddValue("T1SwitchDelay", "tenant 1 to switch delay", T1SwitchDelay);
  cmd.AddValue("T2SwitchBW", "tenant 2 to switch bandwidth", T2SwitchBW);
  cmd.AddValue("T2SwitchDelay", "tenant 2 to switch delay", T2SwitchDelay);
  cmd.AddValue("switchReceiverBW", "switch to receiver second path bandwidth", switchReceiverBW);
  cmd.AddValue("switchReceiverDelay", "switch to second path delay", switchReceiverDelay);
  cmd.AddValue("minTh", "Red Min Threshold", minTh);
  cmd.AddValue("maxTh", "Red Max Threshold", maxTh);
  cmd.AddValue("bufferSize", "switch buffer size", bufferSize);

  cmd.AddValue ("tracing", "Flag to enable/disable Ascii and Pcap tracing", tracing);
  cmd.AddValue ("maxBytes", "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("useEcn", "Flag to enable/disable ECN", useEcn);
  cmd.AddValue ("useQueueDisc", "Flag to enable/disable queue disc on bottleneck", useQueueDisc);
  cmd.AddValue ("simulationEndTime", "Simulation end time", simulationEndTime);
  cmd.Parse (argc, argv);

  // Configure defaults based on command-line arguments
  Config::SetDefault ("ns3::TcpSocketBase::UseEcn", (useEcn ? EnumValue (TcpSocketState::On) : EnumValue (TcpSocketState::Off)));
  Config::SetDefaultFailSafe ("ns3::Ipv4L3Protocol::ECN", BooleanValue (true));

  // Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
  // Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (2));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

  // Set default parameters for RED queue disc
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (useEcn));
  Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (false));
  // Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1500));
  Config::SetDefault ("ns3::RedQueueDisc::MaxSize", QueueSizeValue (QueueSize (bufferSize)));
  // DCTCP tracks instantaneous queue length only; so set QW = 1
  Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue(minTh));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue(maxTh));

  NS_LOG_INFO ("T1SwitchBW:                    " << T1SwitchBW);
  NS_LOG_INFO ("T1SwitchDelay:                 " << T1SwitchDelay);
  NS_LOG_INFO ("T2SwitchBW:                    " << T2SwitchBW);
  NS_LOG_INFO ("T2SwitchDelay:                 " << T2SwitchDelay);
  NS_LOG_INFO ("switchReceiverBW:              " << switchReceiverBW);
  NS_LOG_INFO ("switchReceiverDelay:           " << switchReceiverDelay);
  NS_LOG_INFO ("minTh:                         " << minTh);
  NS_LOG_INFO ("maxTh:                         " << maxTh);
  NS_LOG_INFO ("bufferSize:                    " << bufferSize);
  NS_LOG_INFO ("simulationEndTime:             " << simulationEndTime);


//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (4);

  NS_LOG_INFO ("Create channels.");

//
// Explicitly create the point-to-point link required by the topology (shown above).
//
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

//
// Install the internet stack on the nodes
//
  InternetStackHelper internet;
  internet.Install (nodes);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  NS_LOG_INFO ("Create Applications.");

//
// Create a BulkSendApplication and install it on node 0
//
  uint16_t port = 9;  // well-known echo port number


  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (i.GetAddress (1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (0.0));
  sourceApps.Stop (Seconds (10.0));

//
// Create a PacketSinkApplication and install it on node 1
//
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

//
// Set up tracing if enabled
//
  if (tracing)
    {
      AsciiTraceHelper ascii;
      pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-bulk-send.tr"));
      pointToPoint.EnablePcapAll ("tcp-bulk-send", false);
    }

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
  std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;
}
