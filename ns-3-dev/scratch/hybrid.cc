//
// Network topology
//
//           100Gb/s, 1us       50Gb/s, 1.5us
//       n0-----------------n1-----------------n2
//                          |                   |
//                          |___________________|
//                              50Gb/s, 1us
// - Tracing of queues and packet receptions to file
//   "dctcp-routernet.tr"
// - pcap traces also generated in the following files
//   "dctcp-routernet-$n-$i.pcap" where n and i represent node and interface
// numbers respectively
//  Usage (e.g.): ./waf --run "dctcp-routernet
//                      --timePeriod=10
//                      --totalTxBytes=100000000
//                      --senderToSwitchBW=10Gbps
//                      --senderToSwitchDelay=10us
//                      --switchFirstPathBW=1Gbps
//                      --switchFirstPathDelay=10us
//                      --switchSecondPathBW=1Gbps
//                      --switchSecondPathDelay=15us
//                      --minTh=4
//                      --maxTh=4
//                      --bufferSize=25p
//                      --tracing=true"

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

NS_LOG_COMPONENT_DEFINE ("Hybrid");

std::ofstream cwndStream_first;
std::ofstream cwndStream_second;
std::ofstream packetTraceStream;
std::ofstream txPreciseTraceStream;
std::ofstream instantThroughputFirstFlow;
std::ofstream instantThroughputSecondFlow;

std::ofstream firstQueueLength;
std::ofstream secondQueueLength;

static void
CwndTracer_first (uint32_t oldval, uint32_t newval)
{
  cwndStream_first << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << std::setw (12) << newval << std::endl;
}

static void
CwndTracer_second (uint32_t oldval, uint32_t newval)
{
  cwndStream_second << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << std::setw (12) << newval << std::endl;
}

static void
TxTracer (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface)
{
  packetTraceStream << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << " tx " << p->GetSize () << std::endl;
}

static void
RxTracer (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface)
{
  packetTraceStream << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << " rx " << p->GetSize () << std::endl;
}

static void
PreciseTxTracer (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface)
{
  txPreciseTraceStream << Simulator::Now ().GetSeconds () << " " << p->GetSize () << std::endl;
}

void
ConnectSocketTraces (void)
{
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer_first));
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/1/CongestionWindow", MakeCallback (&CwndTracer_second));
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::Ipv4L3Protocol/Tx", MakeCallback (&TxTracer));
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::Ipv4L3Protocol/Rx", MakeCallback (&RxTracer));
  Config::ConnectWithoutContext ("/NodeList/2/$ns3::Ipv4L3Protocol/Rx", MakeCallback (&PreciseTxTracer));
}

void
CheckFirstQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = queue->GetNPackets ();
  uint32_t qSizeBytes = queue->GetNBytes ();
  firstQueueLength << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << " " << qSize << " " << qSizeBytes << std::endl;
  // check queue size every 1/1000 of a second
  Simulator::Schedule (MicroSeconds (100), &CheckFirstQueueSize, queue);
}

void
CheckSecondQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = queue->GetNPackets ();
  uint32_t qSizeBytes = queue->GetNBytes ();
  secondQueueLength << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << " " << qSize << " " << qSizeBytes << std::endl;
  // check queue size every 1/1000 of a second
  Simulator::Schedule (MicroSeconds (100), &CheckSecondQueueSize, queue);
}

void
PauseFlow (Ptr<BulkSendApplication> bsApp)
{
  Ptr<Socket> localSocket = bsApp->GetSocket();
  Ptr<TcpSocketBase> localSocketBase = DynamicCast<TcpSocketBase> (localSocket);
  //localSocketBase->SetMinRto(Time::Max ());
  localSocketBase->SetPaused(true);
}

void
UnPauseFlow (Ptr<BulkSendApplication> bsApp)
{
  Ptr<Socket> localSocket = bsApp->GetSocket();
  Ptr<TcpSocketBase> localSocketBase = DynamicCast<TcpSocketBase> (localSocket);
  //localSocketBase->SetMinRto(Time::Max ());
  localSocketBase->SetPaused(false);
}
void
PauseUnPause (int timePeriod, Ptr<BulkSendApplication> bsApp1, Ptr<BulkSendApplication> bsApp2)
{
  UnPauseFlow(bsApp1);
  PauseFlow(bsApp2);
  //NS_LOG_INFO (bsApp1 <<" " << bsApp2);
  Simulator::Schedule (MicroSeconds (timePeriod), &PauseUnPause, timePeriod, bsApp2, bsApp1);
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
        instantThroughputFirstFlow << Simulator::Now ().GetSeconds () << " " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())/1000/1000 << std::endl;
      }
      if (stats->first == 3)
      {
        instantThroughputSecondFlow << Simulator::Now ().GetSeconds () << " " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())/1000/1000 << std::endl;
      }
		}
		Simulator::Schedule(MicroSeconds(100),&ThroughputMonitor, fmhelper, flowMon);
	}

uint32_t sendSize[2];
void CheckThroughput (Ptr<PacketSink> sink, uint32_t senderID) {
    uint32_t totalRecvBytes = sink->GetTotalRx ();
    uint32_t currentPeriodRecvBytes = totalRecvBytes - sendSize[senderID];
    sendSize[senderID] = totalRecvBytes;
    Simulator::Schedule (MicroSeconds (32), &CheckThroughput, sink, senderID);
    if (senderID == 0)
    {
        NS_LOG_UNCOND ("Flow: " << senderID << ", throughput (Gbps): " << currentPeriodRecvBytes * 8 / 0.000032 / 1000000000);
        instantThroughputFirstFlow << Simulator::Now ().GetSeconds () << " " << currentPeriodRecvBytes * 8 / 0.000032 / 1000000000 << std::endl;
    }
    else
    {
        NS_LOG_UNCOND ("Flow: " << senderID << ", throughput (Gbps): " << currentPeriodRecvBytes * 8 / 0.000032 / 1000000000);
        instantThroughputSecondFlow << Simulator::Now ().GetSeconds () << " " << currentPeriodRecvBytes * 8 / 0.000032 / 1000000000 << std::endl;
    }
}

int
main (int argc, char *argv[])
{
  int timePeriod = 1;
  int PacketSize = 1400; //bytes
  std::string senderToSwitchBW = "100Gbps";
  std::string senderToSwitchDelay = "1us";
  std::string switchFirstPathBW = "100Gbps";
  std::string switchFirstPathDelay = "1us";
  std::string switchSecondPathBW = "10Gbps";
  std::string switchSecondPathDelay = "1.5us";
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
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));

  CommandLine cmd (__FILE__);
  cmd.AddValue("timePeriod", "time period before a path switch", timePeriod);
  cmd.AddValue("senderToSwitchBW", "sender to switch bandwidth", senderToSwitchBW);
  cmd.AddValue("senderToSwitchDelay", "sender to switch delay", senderToSwitchDelay);
  cmd.AddValue("switchFirstPathBW", "switch to receiver first path bandwidth", switchFirstPathBW);
  cmd.AddValue("switchFirstPathDelay", "switch to receiver first path delay", switchFirstPathDelay);
  cmd.AddValue("switchSecondPathBW", "switch to receiver second path bandwidth", switchSecondPathBW);
  cmd.AddValue("switchSecondPathDelay", "switch to second path delay", switchSecondPathDelay);
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

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (PacketSize));
  Config::SetDefault("ns3::Ipv4GlobalRouting::SourceRouting",BooleanValue(true));

  NS_LOG_INFO ("timePeriod:                    " << timePeriod);
  NS_LOG_INFO ("senderToSwitchBW:              " << senderToSwitchBW);
  NS_LOG_INFO ("senderToSwitchDelay:           " << senderToSwitchDelay);
  NS_LOG_INFO ("switchFirstPathBW:             " << switchFirstPathBW);
  NS_LOG_INFO ("switchFirstPathDelay:          " << switchFirstPathDelay);
  NS_LOG_INFO ("switchSecondPathBW:            " << switchSecondPathBW);
  NS_LOG_INFO ("switchSecondPathDelay:         " << switchSecondPathDelay);
  NS_LOG_INFO ("minTh:                         " << minTh);
  NS_LOG_INFO ("maxTh:                         " << maxTh);
  NS_LOG_INFO ("bufferSize:                    " << bufferSize);
  NS_LOG_INFO ("simulationEndTime:             " << simulationEndTime);


  NS_LOG_INFO ("Create nodes.");
  NodeContainer c;
  c.Create (4);

  NS_LOG_INFO ("Create channels.");

  NodeContainer n0n1 = NodeContainer (c.Get (0), c.Get (1));
  NodeContainer n1n2_first = NodeContainer (c.Get (1), c.Get (2));
  NodeContainer n1n2_second = NodeContainer (c.Get (1), c.Get (2));;
  NodeContainer n2n3 = NodeContainer (c.Get (2), c.Get (3));

  //Define Node link properties
  PointToPointHelper senderToSwitch;
  senderToSwitch.SetDeviceAttribute ("DataRate", StringValue(senderToSwitchBW));
  senderToSwitch.SetChannelAttribute ("Delay", StringValue(senderToSwitchDelay));

  PointToPointHelper switchToReceiver_first;
  switchToReceiver_first.SetDeviceAttribute ("DataRate", StringValue(switchFirstPathBW));
  switchToReceiver_first.SetChannelAttribute ("Delay", StringValue(switchFirstPathDelay));

  PointToPointHelper switchToReceiver_second;
  switchToReceiver_second.SetDeviceAttribute ("DataRate", StringValue(switchSecondPathBW));
  switchToReceiver_second.SetChannelAttribute ("Delay", StringValue(switchSecondPathDelay));

  PointToPointHelper switchToReceiver;
  switchToReceiver.SetDeviceAttribute ("DataRate", StringValue(senderToSwitchBW));
  switchToReceiver.SetChannelAttribute ("Delay", StringValue(senderToSwitchDelay));

  NetDeviceContainer dev0 = senderToSwitch.Install (n0n1);
  NetDeviceContainer dev1 = switchToReceiver_first.Install (n1n2_first);
  NetDeviceContainer dev2 = switchToReceiver_second.Install (n1n2_second);
  NetDeviceContainer dev3 = switchToReceiver.Install (n2n3);

  //Install Internet stack
  InternetStackHelper stack;
  stack.Install (c);

  // Install traffic control

  TrafficControlHelper tchRed;
  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue(senderToSwitchBW),
                             "LinkDelay", StringValue(senderToSwitchDelay),
                             "MinTh", DoubleValue(minTh),
                             "MaxTh", DoubleValue(maxTh));
  QueueDiscContainer queueDisc = tchRed.Install (dev0.Get (1));

  TrafficControlHelper tchRed_first;
  tchRed_first.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue(switchFirstPathBW),
                             "LinkDelay", StringValue(switchFirstPathDelay),
                             "MinTh", DoubleValue(minTh),
                             "MaxTh", DoubleValue(maxTh));
  QueueDiscContainer queueDisc_first = tchRed_first.Install (dev1);

  TrafficControlHelper tchRed_second;
  tchRed_second.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue(switchSecondPathBW),
                             "LinkDelay", StringValue(switchSecondPathDelay),
                             "MinTh", DoubleValue(minTh),
                             "MaxTh", DoubleValue(maxTh));
  QueueDiscContainer queueDisc_second = tchRed_second.Install (dev2);

  TrafficControlHelper tchRedDest;
  tchRedDest.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue(senderToSwitchBW),
                             "LinkDelay", StringValue(senderToSwitchDelay),
                             "MinTh", DoubleValue(minTh),
                             "MaxTh", DoubleValue(maxTh));
  QueueDiscContainer queueDisc_dest = tchRedDest.Install (dev3.Get (0));


  NS_LOG_INFO ("Assign IP Addresses.");

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4.Assign (dev0);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ipInterfs_first = ipv4.Assign (dev1);
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ipInterfs_second = ipv4.Assign (dev2);
  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer ipInterfs_dest = ipv4.Assign (dev3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");
  // Two Sink Applications
  uint16_t servPort_first = 50001;
  uint16_t servPort_second = 50002;

  Address sinkAddress_first (InetSocketAddress (ipInterfs_dest.GetAddress (1), servPort_first));
  Address sinkAddress_second (InetSocketAddress (ipInterfs_dest.GetAddress (1), servPort_second));
  PacketSinkHelper packetSinkHelper_first ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), servPort_first));
  PacketSinkHelper packetSinkHelper_second ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), servPort_second));
  ApplicationContainer sinkApp_first = packetSinkHelper_first.Install (c.Get (3));
  ApplicationContainer sinkApp_second = packetSinkHelper_second.Install (c.Get (3));

  sinkApp_first.Start (Seconds (0));
  sinkApp_first.Stop (simulationEndTime);
  sinkApp_second.Start (Seconds (0));
  sinkApp_second.Stop (simulationEndTime);

  // Randomize the start time between 0 and 100 useconds
  Ptr<UniformRandomVariable> uniformRv = CreateObject<UniformRandomVariable> ();
  uniformRv->SetStream (0);

  Ptr<ConstantRandomVariable> ConstRv = CreateObject<ConstantRandomVariable> ();
  ConstRv->SetAttribute ("Constant", DoubleValue ((double)timePeriod / 1000 / 1000));

  // Two Source Applications
  //BulkSendHelper source0 ("ns3::TcpSocketFactory", sinkAddress_first);
  //BulkSendHelper source1 ("ns3::TcpSocketFactory", sinkAddress_second);
  OnOffHelper source0 ("ns3::TcpSocketFactory", sinkAddress_first);
  source0.SetAttribute ("OnTime",  PointerValue (ConstRv));
  source0.SetAttribute ("OffTime",  PointerValue (ConstRv));
  source0.SetAttribute ("DataRate", DataRateValue (senderToSwitchBW));
  source0.SetAttribute ("PacketSize",  UintegerValue (PacketSize));

  OnOffHelper source1 ("ns3::TcpSocketFactory", sinkAddress_second);
  source1.SetAttribute ("OnTime",  PointerValue (ConstRv));
  source1.SetAttribute ("OffTime",  PointerValue (ConstRv));
  source1.SetAttribute ("DataRate", DataRateValue (senderToSwitchBW));
  source1.SetAttribute ("PacketSize",  UintegerValue (PacketSize));


  // Set the amount of data to send in bytes.  Zero is unlimited.
  source0.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  source1.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApps0 = source0.Install (c.Get (0));
  ApplicationContainer sourceApps1 = source1.Install (c.Get (0));

  int AppStartTime = uniformRv->GetInteger (0, 100);
  //sourceApps0.Start (MicroSeconds (AppStartTime));
  sourceApps0.Start (MicroSeconds (0));
  sourceApps0.Stop (simulationEndTime);

  //sourceApps1.Start (MicroSeconds (AppStartTime + timePeriod));
  sourceApps1.Start (MicroSeconds (timePeriod));
  sourceApps1.Stop (simulationEndTime);

  //Ptr<BulkSendApplication> bsApp_first = DynamicCast<BulkSendApplication>(sourceApps0.Get(0));
  //Ptr<BulkSendApplication> bsApp_second = DynamicCast<BulkSendApplication>(sourceApps1.Get(0));

  //Simulator::Schedule (MicroSeconds (200), &PauseFlow, bsApp_first);
  //Simulator::Schedule (MicroSeconds (210), &UnPauseFlow, bsApp_first);
  //Simulator::Schedule (MicroSeconds (750), &PauseFlow, bsApp_first);

  //Simulator::Schedule (MicroSeconds (AppStartTime + 101), &PauseUnPause, timePeriod, bsApp_first, bsApp_second);

  if (tracing)
    {
      NS_LOG_INFO ("Tracing.");
      AsciiTraceHelper ascii;
      std::stringstream traceFileName;
      traceFileName << "Hybrid-" << simulationEndTime << "-" << timePeriod << ".tr";
      senderToSwitch.EnableAsciiAll (ascii.CreateFileStream (traceFileName.str()));
      senderToSwitch.EnablePcapAll ("Hybrid", false);
    }

  std::stringstream cwndFileName_first;
  cwndFileName_first << "Hybrid-cwnd-first-" << simulationEndTime << "-" << timePeriod << ".dat";
  cwndStream_first.open (cwndFileName_first.str(), std::ios::out);
  cwndStream_first << "#Time(s) Congestion Window (B)" << std::endl;

  std::stringstream cwndFileName_second;
  cwndFileName_second << "Hybrid-cwnd-second-" << simulationEndTime << "-" << timePeriod << ".dat";
  cwndStream_second.open (cwndFileName_second.str(), std::ios::out);
  cwndStream_second << "#Time(s) Congestion Window (B)" << std::endl;

  std::stringstream packetTraceStreamFileName;
  packetTraceStreamFileName << "Hybrid-packetTraceStream-" << simulationEndTime << "-" << timePeriod << ".dat";
  packetTraceStream.open (packetTraceStreamFileName.str(), std::ios::out);
  packetTraceStream << "#Time(s) tx/rx size (B)" << std::endl;

  std::stringstream txPreciseTraceStreamFileName;
  txPreciseTraceStreamFileName << "Hybrid-txTraceStream-" << simulationEndTime << "-" << timePeriod << ".dat";
  txPreciseTraceStream.open (txPreciseTraceStreamFileName.str(), std::ios::out);
  //packetTraceStream << "#Time(s) tx/rx size (B)" << std::endl;

  std::stringstream firstQueueLengthFileName;
  firstQueueLengthFileName << "Hybrid-firstQueueLength-" << simulationEndTime << "-" << timePeriod << ".dat";
  firstQueueLength.open (firstQueueLengthFileName.str(), std::ios::out);
  firstQueueLength << "#Time(s) QLen (Packets) QLen (Bytes)" << std::endl;

  std::stringstream secondQueueLengthFileName;
  secondQueueLengthFileName << "Hybrid-secondQueueLength-" << simulationEndTime << "-" << timePeriod << ".dat";
  secondQueueLength.open (secondQueueLengthFileName.str(), std::ios::out);
  secondQueueLength << "#Time(s) QLen (Packets) QLen (Bytes)" << std::endl;

  Simulator::Schedule (MicroSeconds (timePeriod+1), &ConnectSocketTraces);
  Simulator::Schedule (MicroSeconds (1), &CheckFirstQueueSize, queueDisc_first.Get (0));
  Simulator::Schedule (MicroSeconds (1), &CheckSecondQueueSize, queueDisc_second.Get (0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  std::stringstream instantThroughputFirstFlowFileName;
  instantThroughputFirstFlowFileName << "Hybrid-instantThroughputFirstFlow-" << simulationEndTime << "-" << timePeriod << ".dat";
  instantThroughputFirstFlow.open (instantThroughputFirstFlowFileName.str(), std::ios::out);

  std::stringstream instantThroughputSecondFlowFileName;
  instantThroughputSecondFlowFileName << "Hybrid-instantThroughputSecondFlow-" << simulationEndTime << "-" << timePeriod << ".dat";
  instantThroughputSecondFlow.open (instantThroughputSecondFlowFileName.str(), std::ios::out);

  Ptr<PacketSink> pktSink_first = sinkApp_first.Get (0)->GetObject<PacketSink> ();
  Simulator::Schedule (MicroSeconds(1), &CheckThroughput, pktSink_first, 0);

  Ptr<PacketSink> pktSink_second = sinkApp_second.Get (0)->GetObject<PacketSink> ();
  Simulator::Schedule (MicroSeconds(1), &CheckThroughput, pktSink_second, 1);

  //Simulator::Schedule(MicroSeconds(101),&ThroughputMonitor,&flowmon, monitor);
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (simulationEndTime);
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets:   " << i->second.txPackets << "\n";
      std::cout << "  Tx Bytes:     " << i->second.txBytes << "\n";
      //std::cout << "  TxOffered:    " << i->second.txBytes * 8.0 / simulationEndTime.GetSeconds () / 1000 / 1000  << " Mbps\n";
      std::cout << "  TxOffered:    " << i->second.txBytes * 8.0 / (i->second.timeLastTxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000  << " Mbps\n";
      std::cout << "  Rx Packets:   " << i->second.rxPackets << "\n";
      std::cout << "  Rx Bytes:     " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.txPackets - i->second.rxPackets << "\n";
      //std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simulationEndTime.GetSeconds () / 1000 / 1000  << " Mbps\n";
      std::cout << "  Throughput:   " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds() )/ 1000 / 1000  << " Mbps\n";
    }

  std::stringstream XmlFileName;
  XmlFileName << "Hybrid-" << simulationEndTime << "-" << timePeriod << ".xml";
  monitor->SerializeToXmlFile( XmlFileName.str() , true, true);

  firstQueueLength.close ();
  secondQueueLength.close ();
  packetTraceStream.close ();
  txPreciseTraceStream.close();
  cwndStream_first.close ();
  cwndStream_second.close();

  Simulator::Destroy ();
}
