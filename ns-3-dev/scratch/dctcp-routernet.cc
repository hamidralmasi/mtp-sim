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
 */

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
//  Usage (e.g.): ./waf --run dctcp-routernet

#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("dctcp-routernet");

// The number of bytes to send in this simulation.
int totalTxBytes = 20000000;

std::string senderToSwitchBW = "100Gbps";
std::string senderToSwitchDelay = "1us";

std::string switchFirstPathBW = "50Gbps";
std::string switchFirstPathDelay = "1.5us";

std::string switchSecondPathBW = "50Gbps";
std::string switchSecondPathDelay = "1us";

double minTh = 20;
double maxTh = 60;

Time measurementWindow = Seconds (1);


std::string bufferSize = "2666p";

static uint32_t currentTxBytes = 0;
// Perform series of 1040 byte writes (this is a multiple of 26 since
// we want to detect data splicing in the output stream)
static const uint32_t writeSize = 1040;
uint8_t data[writeSize];

std::ofstream firstQueueLength;
std::ofstream secondQueueLength;
std::ofstream firstSinkThroughput;
std::ofstream secondSinkThroughput;

uint64_t rxBytesFirst = 0;
uint64_t rxBytesSecond = 0;

int timePeriod = 10;

// These are for starting the writing process, and handling the sending
// socket's notification upcalls (events).  These two together more or less
// implement a sending "Application", although not a proper ns3::Application
// subclass.

void StartFlow (Ptr<Socket>, Ipv4Address, uint16_t);
void WriteUntilBufferFull (Ptr<Socket>, uint32_t);

static void
CwndTracer (uint32_t oldval, uint32_t newval)
{
  NS_LOG_INFO ("Moving cwnd from " << oldval << " to " << newval);
}

void
TraceFirstSink (Ptr<const Packet> p)
{
  rxBytesFirst += p->GetSize ();
}

void
TraceSecondSink (Ptr<const Packet> p)
{
  rxBytesSecond += p->GetSize ();
}

void
PrintThroughput (Time measurementWindow)
{
  firstSinkThroughput << Simulator::Now ().GetSeconds () << "s " << " " << (rxBytesFirst * 8) / (measurementWindow.GetSeconds ()) / 1e6 << std::endl;
  secondSinkThroughput << Simulator::Now ().GetSeconds () << "s " << " " << (rxBytesSecond * 8) / (measurementWindow.GetSeconds ()) / 1e6 << std::endl;
}

void
CheckFirstQueueSize (Ptr<QueueDisc> queue)
{
  // 1500 byte packets
  uint32_t qSize = queue->GetNPackets ();
  // Time backlog = Seconds (static_cast<double> (qSize * 1500 * 8) / 1e10); // 10 Gb/s
  // report size in units of packets and ms
  // firstQueueLength << std::fixed << std::setprecision (2) << Simulator::Now ().GetSeconds () << " " << qSize << " " << backlog.GetMicroSeconds () << std::endl;
  firstQueueLength << std::fixed << std::setprecision (2) << Simulator::Now ().GetSeconds () << " " << qSize << " " << std::endl;
  // check queue size every 1/100 of a second
  Simulator::Schedule (MilliSeconds (10), &CheckFirstQueueSize, queue);
}

void
CheckSecondQueueSize (Ptr<QueueDisc> queue)
{
  // 1500 byte packets
  uint32_t qSize = queue->GetNPackets ();
  // Time backlog = Seconds (static_cast<double> (qSize * 1500 * 8) / 1e10); // 10 Gb/s
  // report size in units of packets and ms
  // secondQueueLength << std::fixed << std::setprecision (2) << Simulator::Now ().GetSeconds () << " " << qSize << " " << backlog.GetMicroSeconds () << std::endl;
  secondQueueLength << std::fixed << std::setprecision (2) << Simulator::Now ().GetSeconds () << " " << qSize << " " << std::endl;
  // check queue size every 1/100 of a second
  Simulator::Schedule (MilliSeconds (10), &CheckSecondQueueSize, queue);
}

int main (int argc, char *argv[])
{
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  //  LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
  //  LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
  //  LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
  //  LogComponentEnable("TcpLargeTransfer", LOG_LEVEL_ALL);
  std::string tcpTypeId = "TcpDctcp";
  bool enableSwitchEcn = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("tcpTypeId", "ns-3 TCP TypeId", tcpTypeId);
  cmd.AddValue ("enableSwitchEcn", "enable ECN at switches", enableSwitchEcn);
  cmd.AddValue("timePeriod", "time period before a path switch", timePeriod);

  cmd.AddValue("totalTxBytes", "total number of bytes to transmit", totalTxBytes);

  cmd.AddValue("senderToSwitchBW", "sender to switch bandwidth", senderToSwitchBW);
  cmd.AddValue("senderToSwitchDelay", "sender to switch delay", senderToSwitchDelay);

  cmd.AddValue("switchFirstPathBW", "switch to receiver first path bandwidth", switchFirstPathBW);
  cmd.AddValue("switchFirstPathDelay", "switch to receiver first path delay", switchFirstPathDelay);

  cmd.AddValue("switchSecondPathBW", "switch to receiver second path bandwidth", switchSecondPathBW);
  cmd.AddValue("switchSecondPathDelay", "switch to second path delay", switchSecondPathDelay);

  cmd.AddValue("minTh", "Red Min Threshold", minTh);
  cmd.AddValue("maxTh", "Red Max Threshold", maxTh);

  cmd.AddValue("bufferSize", "switch buffer size", bufferSize);
  cmd.AddValue("measurementWindow", "throughput measurement time window", measurementWindow);


  cmd.Parse (argc, argv);
  Config::SetDefaultFailSafe ("ns3::Ipv4L3Protocol::ECN", BooleanValue (true));
  // Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + tcpTypeId));
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpDctcp::GetTypeId ()));
  //NS_LOG_INFO ("DCTCP Socket Type:  " << TcpDctcp::GetTypeId ());
  // initialize the tx buffer.
  for(uint32_t i = 0; i < writeSize; ++i)
    {
      char m = toascii (97 + i % 26);
      data[i] = m;
    }

  // Here, we will explicitly create three nodes.  The first container contains
  // nodes 0 and 1 from the diagram above, and the second one contains nodes
  // 1 and 2.  This reflects the channel connectivity, and will be used to
  // install the network interfaces and connect them with a channel.
  NodeContainer n0n1;
  n0n1.Create (2);

  NodeContainer n1n2_first;
  n1n2_first.Add (n0n1.Get (1));
  n1n2_first.Create (1);

  NodeContainer n1n2_second;
  n1n2_second.Add (n1n2_first.Get (0));
  n1n2_second.Add (n1n2_first.Get (1));


  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (2));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

  // Set default parameters for RED queue disc
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (enableSwitchEcn));
  Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (false));
  // Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1500));
  // Triumph and Scorpion switches used in DCTCP Paper have 4 MB of buffer
  // If every packet is 1500 bytes, 2666 packets can be stored in 4 MB
  Config::SetDefault ("ns3::RedQueueDisc::MaxSize", QueueSizeValue (QueueSize (bufferSize)));
  // DCTCP tracks instantaneous queue length only; so set QW = 1
  Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue(minTh));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue(maxTh));
  Config::SetDefault ("ns3::TcpSocketBase::UseEcn", StringValue ("On"));

  // We create the channels first without any IP addressing information
  // First make and configure the helper, so that it will put the appropriate
  // attributes on the network interfaces and channels we are about to install.
  PointToPointHelper senderToSwitch;
  senderToSwitch.SetDeviceAttribute ("DataRate", StringValue(senderToSwitchBW));
  senderToSwitch.SetChannelAttribute ("Delay", StringValue(senderToSwitchDelay));

  PointToPointHelper switchToReceiver_first;
  switchToReceiver_first.SetDeviceAttribute ("DataRate", StringValue(switchFirstPathBW));
  switchToReceiver_first.SetChannelAttribute ("Delay", StringValue(switchFirstPathDelay));

  PointToPointHelper switchToReceiver_second;
  switchToReceiver_second.SetDeviceAttribute ("DataRate", StringValue(switchSecondPathBW));
  switchToReceiver_second.SetChannelAttribute ("Delay", StringValue(switchSecondPathDelay));

  NS_LOG_INFO ("measurementWindow:             " << measurementWindow);
  NS_LOG_INFO ("timePeriod:                    " << timePeriod);
  NS_LOG_INFO ("totalTxBytes:                  " << totalTxBytes);
  NS_LOG_INFO ("senderToSwitchBW:              " << senderToSwitchBW);
  NS_LOG_INFO ("senderToSwitchDelay:           " << senderToSwitchDelay);
  NS_LOG_INFO ("switchFirstPathBW:             " << switchFirstPathBW);
  NS_LOG_INFO ("switchFirstPathDelay:          " << switchFirstPathDelay);
  NS_LOG_INFO ("switchSecondPathBW:            " << switchSecondPathBW);
  NS_LOG_INFO ("switchSecondPathDelay:         " << switchSecondPathDelay);
  NS_LOG_INFO ("bufferSize:                    " << bufferSize);
  NS_LOG_INFO ("minTh:                         " << minTh);
  NS_LOG_INFO ("maxTh:                         " << maxTh);
  //NS_LOG_INFO ("TCP Socket Type:" << TcpSocketFactory::GetTypeId ());

  // And then install devices and channels connecting our topology.
  NetDeviceContainer dev0 = senderToSwitch.Install (n0n1);
  NetDeviceContainer dev1 = switchToReceiver_first.Install (n1n2_first);
  NetDeviceContainer dev2 = switchToReceiver_second.Install (n1n2_second);

  // Now add ip/tcp stack to all nodes.
  InternetStackHelper internet;
  internet.InstallAll ();

  TrafficControlHelper tchRed;
  // MinTh = 50, MaxTh = 150 recommended in ACM SIGCOMM 2010 DCTCP Paper
  // This yields a target (MinTh) queue depth of 60us at 10 Gb/s
  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue(senderToSwitchBW),
                             "LinkDelay", StringValue(senderToSwitchDelay),
                             "MinTh", DoubleValue(minTh),
                             "MaxTh", DoubleValue(maxTh));
  QueueDiscContainer queueDisc = tchRed.Install (dev0.Get (1));

  TrafficControlHelper tchRed_first;
  // MinTh = 50, MaxTh = 150 recommended in ACM SIGCOMM 2010 DCTCP Paper
  // This yields a target (MinTh) queue depth of 60us at 10 Gb/s
  tchRed_first.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue(switchFirstPathBW),
                             "LinkDelay", StringValue(switchFirstPathDelay),
                             "MinTh", DoubleValue(minTh),
                             "MaxTh", DoubleValue(maxTh));
  QueueDiscContainer queueDisc_first = tchRed_first.Install (dev1.Get (0));

  TrafficControlHelper tchRed_second;
  // MinTh = 50, MaxTh = 150 recommended in ACM SIGCOMM 2010 DCTCP Paper
  // This yields a target (MinTh) queue depth of 60us at 10 Gb/s
  tchRed_second.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue(switchSecondPathBW),
                             "LinkDelay", StringValue(switchSecondPathDelay),
                             "MinTh", DoubleValue(minTh),
                             "MaxTh", DoubleValue(maxTh));
  QueueDiscContainer queueDisc_second = tchRed_second.Install (dev2.Get (0));


  // Later, we add IP addresses.
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  ipv4.Assign (dev0);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ipInterfs_first = ipv4.Assign (dev1);
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ipInterfs_second = ipv4.Assign (dev2);
  // and setup ip routing tables to get total ip-level connectivity.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  ///////////////////////////////////////////////////////////////////////////
  // Simulation 1
  //
  // Send 2000000 bytes over a connection to server port 50001 at time 0
  // Should observe SYN exchange, a lot of data segments and ACKS, and FIN
  // exchange.  FIN exchange isn't quite compliant with TCP spec (see release
  // notes for more info)
  //
  ///////////////////////////////////////////////////////////////////////////

  uint16_t servPort_first = 50001;
  uint16_t servPort_second = 50002;

  // Create a packet sink to receive these packets on n2...
  PacketSinkHelper sink_first ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), servPort_first));
  PacketSinkHelper sink_second ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), servPort_second));


  ApplicationContainer apps_first = sink_first.Install (n1n2_first.Get (1));
  apps_first.Start (Seconds (0.0));
  apps_first.Stop (Seconds (3.0));

  ApplicationContainer apps_second = sink_second.Install (n1n2_second.Get (1));
  apps_second.Start (Seconds (0.0));
  apps_second.Stop (Seconds (3.0));
  // Create a source to send packets from n0.  Instead of a full Application
  // and the helper APIs you might see in other example files, this example
  // will use sockets directly and register some socket callbacks as a sending
  // "Application".
  // Create and bind the socket...

  Ptr<Socket> localSocket_first = Socket::CreateSocket (n0n1.Get (0), TcpSocketFactory::GetTypeId ());
  localSocket_first->Bind ();

  Ptr<Socket> localSocket_second = Socket::CreateSocket (n0n1.Get (0), TcpSocketFactory::GetTypeId ());
  localSocket_second->Bind ();

  // Trace changes to the congestion window
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));

  firstQueueLength.open ("first-path-queueLength.dat", std::ios::out);
  firstQueueLength << "#Time(s) qlen(pkts) qlen(us)" << std::endl;

  secondQueueLength.open ("second-path-queueLength.dat", std::ios::out);
  secondQueueLength << "#Time(s) qlen(pkts) qlen(us)" << std::endl;

  firstSinkThroughput.open ("dctcp-throughput_first.dat", std::ios::out);
  firstSinkThroughput << "#Time(s) flow throughput(Mb/s)" << std::endl;

  secondSinkThroughput.open ("dctcp-throughput_second.dat", std::ios::out);
  secondSinkThroughput << "#Time(s) flow throughput(Mb/s)" << std::endl;

  //Ptr<PacketSink> packetSinkFirst = apps_first.Get (0)->GetObject<PacketSink> ();
  //packetSinkFirst->TraceConnectWithoutContext ("Rx", MakeBoundCallback(&TraceFirstSink));

  //Ptr<PacketSink> packetSinkSecond = apps_second.Get (0)->GetObject<PacketSink> ();
  //packetSinkSecond->TraceConnectWithoutContext ("Rx", MakeBoundCallback(&TraceSecondSink));

  Simulator::ScheduleNow (&CheckFirstQueueSize, queueDisc_first.Get (0));
  // Simulator::ScheduleNow (&CheckSecondQueueSize, queueDisc_second.Get (0));
  //Simulator::ScheduleNow (&PrintThroughput, measurementWindow);

  // ...and schedule the sending "Application"; This is similar to what an
  // ns3::Application subclass would do internally.
  Simulator::ScheduleNow (&StartFlow, localSocket_first, ipInterfs_first.GetAddress (1), servPort_first);

  //Simulator::ScheduleNow (&StartFlow, localSocket_second, ipInterfs_second.GetAddress (1), servPort_second);

  // One can toggle the comment for the following line on or off to see the
  // effects of finite send buffer modelling.  One can also change the size of
  // said buffer.

  localSocket_first->SetAttribute("SndBufSize", UintegerValue(1e8));

  //Ask for ASCII and pcap traces of network traffic
  AsciiTraceHelper ascii;
  senderToSwitch.EnableAsciiAll (ascii.CreateFileStream ("dctcp-routernet.tr"));
  senderToSwitch.EnablePcapAll ("dctcp-routernet");

  // Finally, set up the simulator to run.  The 1000 second hard limit is a
  // failsafe in case some change above causes the simulation to never end
  Simulator::Stop (Seconds (1000));
  Simulator::Run ();

  firstQueueLength.close ();
  secondQueueLength.close ();

  Simulator::Destroy ();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//begin implementation of sending "Application"
void StartFlow (Ptr<Socket> localSocket,
                Ipv4Address servAddress,
                uint16_t servPort)
{
  NS_LOG_LOGIC ("Starting flow at time " <<  Simulator::Now ().GetSeconds ());
  localSocket->Connect (InetSocketAddress (servAddress, servPort)); //connect

  // tell the tcp implementation to call WriteUntilBufferFull again
  // if we blocked and new tx buffer space becomes available
  localSocket->SetSendCallback (MakeCallback (&WriteUntilBufferFull));
  WriteUntilBufferFull (localSocket, localSocket->GetTxAvailable ());
}

void WriteUntilBufferFull (Ptr<Socket> localSocket, uint32_t txSpace)
{
  while (currentTxBytes < totalTxBytes && localSocket->GetTxAvailable () > 0)
    {
      uint32_t left = totalTxBytes - currentTxBytes;
      uint32_t dataOffset = currentTxBytes % writeSize;
      uint32_t toWrite = writeSize - dataOffset;
      toWrite = std::min (toWrite, left);
      toWrite = std::min (toWrite, localSocket->GetTxAvailable ());
      int amountSent = localSocket->Send (&data[dataOffset], toWrite, 0);
      if(amountSent < 0)
        {
          // we will be called again when new tx space becomes available.
          return;
        }
      currentTxBytes += amountSent;
    }
  if (currentTxBytes >= totalTxBytes)
    {
      localSocket->Close ();
    }
}
