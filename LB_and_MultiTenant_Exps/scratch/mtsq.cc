#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/link-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MTSQ");


std::ofstream cwndStreamT1;
std::ofstream cwndStreamT2;

std::ofstream instantThroughputT1;
std::ofstream instantThroughputT2;
std::ofstream queueLength;
std::ofstream queueComposition;


static void
CwndTracerT1 (uint32_t oldval, uint32_t newval)
{
  cwndStreamT1 << std::fixed << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
}

static void
CwndTracerT2 (uint32_t oldval, uint32_t newval)
{
  cwndStreamT2 << std::fixed << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
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
  queueLength << std::fixed << Simulator::Now ().GetSeconds () << " " << qSize << " " << qSizeBytes << std::endl;
  // check queue size every 1/1000 of a second
  Simulator::Schedule (MicroSeconds (1), &CheckQueueSize, queue);
}

void
CheckQueueComposition (Ptr<TCNQueueDisc> queue, uint32_t tw)
{
    uint32_t n_packets_0 = queue->GetNPackets0 ();
    uint32_t n_packets_1 = queue->GetNPackets1 ();
    queueComposition << std::fixed << Simulator::Now ().GetSeconds () << " " << n_packets_0 << " " << n_packets_1 << std::endl;
    Simulator::Schedule (MicroSeconds (tw), &CheckQueueComposition, queue, tw);

}

enum AQM {
    RED
};


uint32_t sendSize[90];
void CheckThroughput (Ptr<PacketSink> sink, uint32_t senderID) {
    uint32_t totalRecvBytes = sink->GetTotalRx ();
    uint32_t currentPeriodRecvBytes = totalRecvBytes - sendSize[senderID];
    sendSize[senderID] = totalRecvBytes;
    Simulator::Schedule (MicroSeconds (32), &CheckThroughput, sink, senderID);
    double throughput = currentPeriodRecvBytes * 8 / 0.000032 / 1000000000;
    if (senderID == 0)
    {
        instantThroughputT1 << Simulator::Now ().GetSeconds () << " " << throughput << std::endl;
    }
    else
    {
        instantThroughputT2 << Simulator::Now ().GetSeconds () << " " << throughput << std::endl;
    }
    // if (0 <= senderID && senderID < numFlowsT1)
    // {
    //     //NS_LOG_UNCOND ("Flow: " << senderID << ", throughput (Gbps): " << throughput);
    //     instantThroughputT1 << senderID << " " << Simulator::Now ().GetSeconds () << " " << throughput << std::endl;
    // }
    // else
    // {
    //     //NS_LOG_UNCOND ("Flow: " << senderID << ", throughput (Gbps): " << throughput);
    //     instantThroughputT2 << senderID << " " << Simulator::Now ().GetSeconds () << " " << throughput << std::endl;
    // }
}

template<typename T> T
rand_range (T min, T max)
{
    return min + ((double)max - min) * rand () / RAND_MAX;
}

std::string
GetFormatedStr (std::string id, std::string str, std::string terminal, AQM aqm, double endTime)
{
    std::stringstream ss;
    if (aqm == RED)
    {
        //ss << "MTSQ_RED_" << id << "_" << str << "." << terminal;
        ss << "MTSQ_RED_" << id << "_" << str << "-" << endTime << "." << terminal;
    }
    return ss.str ();
}

int main (int argc, char *argv[])
{
    std::string T1SwitchBW = "100Gbps";
    std::string T1SwitchDelay = "10us";
    std::string T2SwitchBW = "100Gbps";
    std::string T2SwitchDelay = "10us";
    std::string switchReceiverBW = "100Gbps";
    std::string switchReceiverDelay = "10us";
    double minTh = 5;
    double maxTh = 15;
    uint32_t bufferSize = 50;
    int numFlowsT1=10;
    int numFlowsT2=80;
    bool tracing = false;

#if 1
    LogComponentEnable ("MTSQ", LOG_LEVEL_INFO);
#endif

    uint32_t numOfSenders = 2;

    std::string id = "undefined";

    std::string transportProt = "DcTcp";
    std::string aqmStr = "RED";

    AQM aqm;
    double endTime = 0.1;

    unsigned randomSeed = 0;


    CommandLine cmd;
    cmd.AddValue ("id", "The running ID", id);
    cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, DcTcp", transportProt);
    cmd.AddValue ("AQM", "AQM to use: RED", aqmStr);

    cmd.AddValue ("endTime", "Simulation end time", endTime);
    cmd.AddValue ("randomSeed", "Random seed, 0 for random generated", randomSeed);

    cmd.AddValue("T1SwitchBW", "tenant 1 to switch bandwidth", T1SwitchBW);
    cmd.AddValue("T1SwitchDelay", "tenant 1 to switch delay", T1SwitchDelay);
    cmd.AddValue("T2SwitchBW", "tenant 2 to switch bandwidth", T2SwitchBW);
    cmd.AddValue("T2SwitchDelay", "tenant 2 to switch delay", T2SwitchDelay);
    cmd.AddValue("switchReceiverBW", "switch to receiver second path bandwidth", switchReceiverBW);
    cmd.AddValue("switchReceiverDelay", "switch to second path delay", switchReceiverDelay);
    cmd.AddValue("bufferSize", "switch buffer size", bufferSize);
    cmd.AddValue("minTh", "Red Min Threshold", minTh);
    cmd.AddValue("maxTh", "Red Max Threshold", maxTh);
    cmd.AddValue("numFlowsT1", "Number of flows generated from Tenant 1", numFlowsT1);
    cmd.AddValue("numFlowsT2", "Number of flows generated from Tenant 2", numFlowsT2);

    cmd.AddValue ("tracing", "Flag to enable/disable Ascii and Pcap tracing", tracing);

    cmd.Parse (argc, argv);

    if (transportProt.compare ("Tcp") == 0)
    {
        Config::SetDefault ("ns3::TcpSocketBase::Target", BooleanValue (false));
    }
    else if (transportProt.compare ("DcTcp") == 0)
    {
        NS_LOG_INFO ("Enabling DcTcp");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpDCTCP::GetTypeId ()));
    }
    else
    {
        return 0;
    }

    if (aqmStr.compare ("RED") == 0)
    {
        aqm = RED;
    }
    else
    {
        return 0;
    }
    uint32_t TCNThreshold = (minTh + maxTh) / 2;

    // TCP Configuration
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(1400));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0));
    Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (5)));
    Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
    Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (5)));
    Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));
    Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (40)));
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (160000000));
    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (160000000));

    // RED Configuration
    Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue(bufferSize));
    Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));
    Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue(minTh));
    Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue(maxTh));

    // TCN Configuration
    Config::SetDefault ("ns3::TCNQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::TCNQueueDisc::MaxPackets", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::TCNQueueDisc::Threshold", TimeValue (MicroSeconds (TCNThreshold)));
    Config::SetDefault ("ns3::TCNQueueDisc::CountThreshold", UintegerValue (TCNThreshold));
    Config::SetDefault ("ns3::TCNQueueDisc::SeparateCounters", BooleanValue (true));
    Config::SetDefault ("ns3::TCNQueueDisc::UseEcn", BooleanValue (true));



    NS_LOG_INFO ("T1SwitchBW:                    " << T1SwitchBW);
    NS_LOG_INFO ("T1SwitchDelay:                 " << T1SwitchDelay);
    NS_LOG_INFO ("T2SwitchBW:                    " << T2SwitchBW);
    NS_LOG_INFO ("T2SwitchDelay:                 " << T2SwitchDelay);
    NS_LOG_INFO ("switchReceiverBW:              " << switchReceiverBW);
    NS_LOG_INFO ("switchReceiverDelay:           " << switchReceiverDelay);
    NS_LOG_INFO ("minTh:                         " << minTh);
    NS_LOG_INFO ("maxTh:                         " << maxTh);
    NS_LOG_INFO ("numFlowsT1:                    " << numFlowsT1);
    NS_LOG_INFO ("numFlowsT2:                    " << numFlowsT2);
    NS_LOG_INFO ("bufferSize:                    " << bufferSize);
    NS_LOG_INFO ("simulationEndTime:             " << endTime);

    NS_LOG_INFO ("Setting up nodes.");
    NodeContainer senders;
    senders.Create (numOfSenders);

    NodeContainer receivers;
    receivers.Create (1);

    NodeContainer switches;
    switches.Create (1);

    InternetStackHelper internet;
    internet.Install (senders);
    internet.Install (switches);
    internet.Install (receivers);

    PointToPointHelper p2p;

    TrafficControlHelper tc;

    NS_LOG_INFO ("Assign IP address");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");

    std::string DataRate = "";
    std::string linkLatency = "10us";

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        if ( i == 0 )
        {
            DataRate = T1SwitchBW;
            linkLatency = T1SwitchDelay;
        }
        else if ( i == 1 )
        {
            DataRate = T2SwitchBW;
            linkLatency = T2SwitchDelay;
        }
        p2p.SetChannelAttribute ("Delay", StringValue(linkLatency));
        p2p.SetDeviceAttribute ("DataRate", StringValue (DataRate));
        p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (bufferSize));

        NodeContainer nodeContainer = NodeContainer (senders.Get (i), switches.Get (0));
        NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
        Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
        ipv4.NewNetwork ();
        tc.Uninstall (netDeviceContainer);
    }

    p2p.SetChannelAttribute ("Delay", StringValue (switchReceiverDelay));
    p2p.SetDeviceAttribute ("DataRate", StringValue (switchReceiverBW));
    p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (5));

    NodeContainer switchToRecvNodeContainer = NodeContainer (switches.Get (0), receivers.Get (0));
    NetDeviceContainer switchToRecvNetDeviceContainer = p2p.Install (switchToRecvNodeContainer);


    Ptr<DWRRQueueDisc> dwrrQdisc = CreateObject<DWRRQueueDisc> ();
    //Ptr<WFQQueueDisc> wfqQdisc = CreateObject<WFQQueueDisc> ();

    Ptr<Ipv4SimplePacketFilter> filter = CreateObject<Ipv4SimplePacketFilter> ();

    dwrrQdisc->AddPacketFilter (filter);
    //wfqQdisc->AddPacketFilter (filter);

    ObjectFactory innerQueueFactory;
    if (aqm == RED)
    {
        innerQueueFactory.SetTypeId ("ns3::TCNQueueDisc");
    }

    Ptr<QueueDisc> queueDisc1 = innerQueueFactory.Create<QueueDisc> ();
    //Ptr<QueueDisc> queueDisc2 = innerQueueFactory.Create<QueueDisc> ();

    dwrrQdisc->AddDWRRClass (queueDisc1, 0, 1450);
    dwrrQdisc->AddDWRRClass (queueDisc1, 1, 1450);

    // wfqQdisc->AddWFQClass (queueDisc1, 0, 1);
    // wfqQdisc->AddWFQClass (queueDisc2, 1, 1);


    Ptr<NetDevice> device = switchToRecvNetDeviceContainer.Get (0);
    Ptr<TrafficControlLayer> tcl = device->GetNode ()->GetObject<TrafficControlLayer> ();

    dwrrQdisc->SetNetDevice (device);
    //wfqQdisc->SetNetDevice (device);
    tcl->SetRootQueueDiscOnDevice (device, dwrrQdisc);
    //tcl->SetRootQueueDiscOnDevice (device, wfqQdisc);

    Ipv4InterfaceContainer switchToRecvIpv4Container = ipv4.Assign (switchToRecvNetDeviceContainer);

    NS_LOG_INFO ("Setting up routing table");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


    NS_LOG_INFO ("Initialize random seed: " << randomSeed);
    if (randomSeed == 0)
    {
        srand ((unsigned)time (NULL));
    }
    else
    {
        srand (randomSeed);
    }

    uint16_t basePort = 8080;

    Ptr<UniformRandomVariable> uniformRv = CreateObject<UniformRandomVariable> ();
    uniformRv->SetStream (0);

    NS_LOG_INFO ("Install flows for tenants");
    for (uint32_t i = 0; i < 2; ++i)
    {
        if (i == 0) 
        {
            PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort));
            ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
            sinkApp.Start (Seconds (0.0));
            sinkApp.Stop (Seconds (endTime));
            Ptr<PacketSink> pktSink = sinkApp.Get (0)->GetObject<PacketSink> ();
            Simulator::ScheduleNow (&CheckThroughput, pktSink, i);

            for (uint32_t j = 0; j < numFlowsT1; ++j)
            {
                BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
                source.SetAttribute ("MaxBytes", UintegerValue (0)); // 150kb
                source.SetAttribute ("SendSize", UintegerValue (1400));
                source.SetAttribute ("SimpleTOS", UintegerValue (0));
                ApplicationContainer sourceApps = source.Install (senders.Get (i));

                int AppStartTime = uniformRv->GetInteger (0, 100);
                sourceApps.Start (MicroSeconds (AppStartTime));
                sourceApps.Stop (Seconds (endTime));
            }
        }
        else
        {
            PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort));
            ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
            sinkApp.Start (Seconds (0.0));
            sinkApp.Stop (Seconds (endTime));
            Ptr<PacketSink> pktSink = sinkApp.Get (0)->GetObject<PacketSink> ();
            Simulator::ScheduleNow (&CheckThroughput, pktSink, i);

            for (uint32_t j = numFlowsT1; j < (numFlowsT1+numFlowsT2); ++j)
            {
                BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
                source.SetAttribute ("MaxBytes", UintegerValue (0)); // 150kb
                source.SetAttribute ("SendSize", UintegerValue (1400));
                source.SetAttribute ("SimpleTOS", UintegerValue (1));
                ApplicationContainer sourceApps = source.Install (senders.Get (i));

                int AppStartTime = uniformRv->GetInteger (0, 100);
                sourceApps.Start (MicroSeconds (AppStartTime));
                sourceApps.Stop (Seconds (endTime));


            }
         
        }
        basePort++;
    }


    // NS_LOG_INFO ("Install 100 short TCP flows");
    // for (uint32_t i = 0; i < 100; ++i)
    // {
    //     double startTime = rand_range (0.0, 0.4);
    //     uint32_t tos = rand_range (0, 3);
    //     BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
    //     source.SetAttribute ("MaxBytes", UintegerValue (28000)); // 14kb
    //     source.SetAttribute ("SendSize", UintegerValue (1400));
    //     source.SetAttribute ("SimpleTOS", UintegerValue (tos));
    //     ApplicationContainer sourceApps = source.Install (senders.Get (2 + i % 2));
    //     sourceApps.Start (Seconds (startTime));
    //     sourceApps.Stop (Seconds (endTime));

    //     PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort++));
    //     ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
    //     sinkApp.Start (Seconds (0.0));
    //     sinkApp.Stop (Seconds (endTime));
    // }

    NS_LOG_INFO ("Enabling Flow Monitor");
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    if (tracing)
    {
        NS_LOG_INFO ("Tracing.");
        AsciiTraceHelper ascii;
        std::stringstream traceFileName;
        traceFileName << "MTSQ-" << endTime << ".tr";
        //p2p.EnableAsciiAll (ascii.CreateFileStream (traceFileName.str()));
        //p2p.EnablePcapAll ("MTSQ", false);
        p2p.EnablePcap("MTSQ", switchToRecvNetDeviceContainer.Get (0));
        NS_LOG_INFO ("POST TRACING ENABLED");
    }

    std::stringstream cwndT1FileName;
    cwndT1FileName << id << "-MTSQ-cwnd-T1-" << TCNThreshold << "-" << bufferSize << "-" << numFlowsT1 << "-" << numFlowsT2 << "-" << endTime <<".dat";
    cwndStreamT1.open (cwndT1FileName.str(), std::ios::out);
    cwndStreamT1 << "#Time(s) Congestion Window (B)" << std::endl;

    std::stringstream cwndT2FileName;
    cwndT2FileName << id << "-MTSQ-cwnd-T2-" << TCNThreshold << "-" << bufferSize << "-" << numFlowsT1 << "-" << numFlowsT2 << "-" << endTime <<".dat";
    cwndStreamT2.open (cwndT2FileName.str(), std::ios::out);
    cwndStreamT2 << "#Time(s) Congestion Window (B)" << std::endl;

    std::stringstream queueLengthFileName;
    queueLengthFileName << id << "-MTSQ-QueueLength-" << TCNThreshold << "-" << bufferSize << "-" << numFlowsT1 << "-" << numFlowsT2 << "-" << endTime << ".dat";
    queueLength.open (queueLengthFileName.str(), std::ios::out);
    queueLength << "#Time(s) QLen (Packets) QLen (Bytes)" << std::endl;

    // std::stringstream queueLengthT2FileName;
    // queueLengthT2FileName << id << "MTSQ-QueueLength-T2-" << TCNThreshold << "-" << bufferSize << "-" << numFlowsT1 << "-" << numFlowsT2 << "-"<< endTime << ".dat";
    // queueLengthT2.open (queueLengthT2FileName.str(), std::ios::out);
    // queueLengthT2 << "#Time(s) QLen (Packets) QLen (Bytes)" << std::endl;

    Simulator::Schedule (MicroSeconds (101), &ConnectSocketTraces);
    Simulator::Schedule (MicroSeconds (101), &CheckQueueSize, queueDisc1);
    // double timeWindow = 11616 * bufferSize / 100e3;
    double timeWindow = 1;
    Ptr<TCNQueueDisc> TCNqueueDisc = DynamicCast<TCNQueueDisc> (queueDisc1);
    Simulator::Schedule (MicroSeconds (101), &CheckQueueComposition, TCNqueueDisc, timeWindow);

    //Simulator::Schedule (MicroSeconds (101), &CheckQueueSizeT2, queueDisc2);

    std::stringstream instantThroughputT1FileName;
    instantThroughputT1FileName << id << "-MTSQ-instantThroughput-T1-" << TCNThreshold << "-" << bufferSize << "-" << numFlowsT1 << "-" << numFlowsT2 << "-" << endTime << ".dat";
    instantThroughputT1.open (instantThroughputT1FileName.str(), std::ios::out);

    std::stringstream instantThroughputT2FileName;
    instantThroughputT2FileName << id << "-MTSQ-instantThroughput-T2-" << TCNThreshold << "-" << bufferSize << "-" << numFlowsT1 << "-" << numFlowsT2 << "-" << endTime << ".dat";
    instantThroughputT2.open (instantThroughputT2FileName.str(), std::ios::out);

    std::stringstream queueCompositionFileName;
    queueCompositionFileName << id << "-MTSQ-QueueComposition-" << TCNThreshold << "-" << bufferSize << "-" << numFlowsT1 << "-" << numFlowsT2 << "-" << endTime << ".dat";
    queueComposition.open (queueCompositionFileName.str(), std::ios::out);
    //queueComposition << "#Time(s) T1Qlen (Packets) T2Qlen (Packets)" << std::endl;


    NS_LOG_INFO ("Run Simulations");

    Simulator::Stop (Seconds (endTime));
    Simulator::Run ();


    flowMonitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();
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

    flowMonitor->SerializeToXmlFile(GetFormatedStr (id, "Flow_Monitor", "xml", aqm, endTime), true, true);

    Simulator::Destroy ();

    return 0;
}
