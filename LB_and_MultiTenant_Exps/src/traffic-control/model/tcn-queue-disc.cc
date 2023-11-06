#include "tcn-queue-disc.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/drop-tail-queue.h"
#include <fstream>
#define DEFAULT_TCN_LIMIT 4000
//std::ofstream queueLength;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TCNQueueDisc");

class TCNTimestampTag : public Tag
{
public:
    TCNTimestampTag ();

    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;

    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;

  /**
   * Gets the Tag creation time
   * @return the time object stored in the tag
   */
  Time GetTxTime (void) const;

private:
  uint64_t m_creationTime; //!< Tag creation time

};

TCNTimestampTag::TCNTimestampTag ()
  : m_creationTime (Simulator::Now ().GetTimeStep ())
{
}

TypeId
TCNTimestampTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TCNTimestampTag")
    .SetParent<Tag> ()
    .AddConstructor<TCNTimestampTag> ()
    .AddAttribute ("CreationTime",
                   "The time at which the timestamp was created",
                   StringValue ("0.0s"),
                   MakeTimeAccessor (&TCNTimestampTag::GetTxTime),
                   MakeTimeChecker ())
  ;
  return tid;
}

TypeId
TCNTimestampTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
TCNTimestampTag::GetSerializedSize (void) const
{
  return 8;
}

void
TCNTimestampTag::Serialize (TagBuffer i) const
{
  i.WriteU64 (m_creationTime);
}

void
TCNTimestampTag::Deserialize (TagBuffer i)
{
  m_creationTime = i.ReadU64 ();
}

void
TCNTimestampTag::Print (std::ostream &os) const
{
  os << "CreationTime=" << m_creationTime;
}

Time
TCNTimestampTag::GetTxTime (void) const
{
  return TimeStep (m_creationTime);
}

NS_OBJECT_ENSURE_REGISTERED (TCNQueueDisc);

TypeId
TCNQueueDisc::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::TCNQueueDisc")
        .SetParent<QueueDisc> ()
        .SetGroupName ("TrafficControl")
        .AddConstructor<TCNQueueDisc> ()
        .AddAttribute ("Mode", "Whether to use Bytes (see MaxBytes) or Packets (see MaxPackets) as the maximum queue size metric.",
                        EnumValue (Queue::QUEUE_MODE_BYTES),
                        MakeEnumAccessor (&TCNQueueDisc::m_mode),
                        MakeEnumChecker (Queue::QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                         Queue::QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
        .AddAttribute ("MaxPackets", "The maximum number of packets accepted by this TCNQueueDisc.",
                        UintegerValue (DEFAULT_TCN_LIMIT),
                        MakeUintegerAccessor (&TCNQueueDisc::m_maxPackets),
                        MakeUintegerChecker<uint32_t> ())
        .AddAttribute ("MaxBytes", "The maximum number of bytes accepted by this TCNQueueDisc.",
                        UintegerValue (1500 * DEFAULT_TCN_LIMIT),
                        MakeUintegerAccessor (&TCNQueueDisc::m_maxBytes),
                        MakeUintegerChecker<uint32_t> ())
        .AddAttribute ("Threshold",
                       "Instantaneous sojourn time threshold",
                        StringValue ("10us"),
                        MakeTimeAccessor (&TCNQueueDisc::m_threshold),
                        MakeTimeChecker ())
        .AddAttribute ("CountThreshold",
                       "Instantaneous count threshold",
                        UintegerValue (10),
                        MakeUintegerAccessor (&TCNQueueDisc::n_threshold),
                        MakeUintegerChecker<uint32_t> ())
        .AddAttribute ("SeparateCounters",
                       "Whether or not to use separate counters for tenants when the queue is shared between them",
                        BooleanValue (false),
                        MakeBooleanAccessor (&TCNQueueDisc::m_separateCounters),
                        MakeBooleanChecker ())
        .AddAttribute ("UseEcn",
                       "Whether or not to ECN mark",
                        BooleanValue (true),
                        MakeBooleanAccessor (&TCNQueueDisc::m_useEcn),
                        MakeBooleanChecker ())
    ;
    return tid;
}

TCNQueueDisc::TCNQueueDisc ()
    : QueueDisc (),
      m_threshold (0),
      n_packets_0 (0),
      n_packets_1 (0),
      n_threshold (0)
{
    NS_LOG_FUNCTION (this);
    // std::stringstream queueLengthFileName;
    // queueLengthFileName << "MTSQ-QueueLengthComposition.dat";
    // queueLength.open (queueLengthFileName.str(), std::ios::out);
}

TCNQueueDisc::~TCNQueueDisc ()
{
    //queueLength.close();
    NS_LOG_FUNCTION (this);
}

bool
TCNQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION (this << item);

    Ptr<Packet> p = item->GetPacket ();
    if (m_mode == Queue::QUEUE_MODE_PACKETS && (GetInternalQueue (0)->GetNPackets () + 1 > m_maxPackets))
    {
        Drop (item);
        return false;
    }

    if (m_mode == Queue::QUEUE_MODE_BYTES && (GetInternalQueue (0)->GetNBytes () + item->GetPacketSize () > m_maxBytes))
    {
        Drop (item);
        return false;
    }

    TCNTimestampTag tag;
    p->AddPacketTag (tag);

    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem> (item);
    Ipv4Header header = ipv4Item -> GetHeader ();
    GetInternalQueue (0)->Enqueue (item);
    if (m_separateCounters)
    {
        if (header.GetSource() == Ipv4Address("10.1.1.1"))
        {
            n_packets_0++;
        }
        else
        {
            n_packets_1++;
        }
    }
    else
    {
        n_packets_0++;
    }

    //GetInternalQueue (0)->EnqueueWithToS (item, 0);
    return true;

}

bool
TCNQueueDisc::DoEnqueueWithToS (Ptr<QueueDiscItem> item, int ToS)
{
    NS_LOG_FUNCTION (this << item);

    Ptr<Packet> p = item->GetPacket ();
    if (m_mode == Queue::QUEUE_MODE_PACKETS && (GetInternalQueue (0)->GetNPackets () + 1 > m_maxPackets))
    {
        Drop (item);
        return false;
    }

    if (m_mode == Queue::QUEUE_MODE_BYTES && (GetInternalQueue (0)->GetNBytes () + item->GetPacketSize () > m_maxBytes))
    {
        Drop (item);
        return false;
    }

    TCNTimestampTag tag;
    p->AddPacketTag (tag);

    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem> (item);
    Ipv4Header header = ipv4Item -> GetHeader ();
    //GetInternalQueue (0)->Enqueue (item);
    GetInternalQueue (0)->EnqueueWithToS (item, ToS);
    return true;

}

Ptr<QueueDiscItem>
TCNQueueDisc::DoDequeue (void)
{
    NS_LOG_FUNCTION (this);

    Time now = Simulator::Now ();

    if (GetInternalQueue (0)->IsEmpty ())
    {
        return NULL;
    }

    Ptr<QueueDiscItem> item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ());
    Ptr<Packet> p = item->GetPacket ();

    TCNTimestampTag tag;
    bool found = p->RemovePacketTag (tag);
    if (!found)
    {
        NS_LOG_ERROR ("Cannot find the TCN Timestamp Tag");
        return NULL;
    }

    Time sojournTime = now - tag.GetTxTime ();

    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem> (item);
    Ipv4Header header = ipv4Item -> GetHeader ();
    if (m_separateCounters)
    {   //NS_LOG_INFO("#################################################    "<<n_packets_0 << "     " << n_packets_1);
        //queueLength << n_packets_0 << " " << n_packets_1 << std::endl;

        if ((header.GetSource() != Ipv4Address("10.1.1.1")) && (header.GetSource() != Ipv4Address("10.1.2.1")))
        {
            NS_FATAL_ERROR ("UNEXPECTED SRC IP!!!");
        }
        if (header.GetSource() == Ipv4Address("10.1.1.1"))
        {
            n_packets_0--;
            //NS_LOG_INFO("PACKETS 0: " << n_packets_0);
            if (n_packets_0 > n_threshold && m_useEcn)
            {
                TCNQueueDisc::MarkingECN (item);
            }

        }
        else
        {
            n_packets_1--;
            //NS_LOG_INFO("PACKETS 1: " << n_packets_1);
            if (n_packets_1 > n_threshold && m_useEcn)
            {
                TCNQueueDisc::MarkingECN (item);
            }
        }
    }
    else
    {
        n_packets_0--;
        if (n_packets_0 > n_threshold && m_useEcn)
        {
            TCNQueueDisc::MarkingECN (item);
        }
    }


    // if (sojournTime > m_threshold)
    // {
    //     TCNQueueDisc::MarkingECN (item);
    // }

    return item;

}

Ptr<QueueDiscItem>
TCNQueueDisc::DoDequeueWithToS (int ToS)
{
    NS_LOG_FUNCTION (this);

    Time now = Simulator::Now ();

    if (GetInternalQueue (0)->IsEmpty ())
    {
        return NULL;
    }

    //Ptr<QueueDiscItem> item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ());
    Ptr<QueueDiscItem> item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->DequeueWithToS (ToS));
    Ptr<Packet> p = item->GetPacket ();

    TCNTimestampTag tag;
    bool found = p->RemovePacketTag (tag);
    if (!found)
    {
        NS_LOG_ERROR ("Cannot find the TCN Timestamp Tag");
        return NULL;
    }

    Time sojournTime = now - tag.GetTxTime ();

    if (sojournTime > m_threshold)
    {
        TCNQueueDisc::MarkingECN (item);
    }

    return item;

}

Ptr<const QueueDiscItem>
TCNQueueDisc::DoPeek (void) const
{
    NS_LOG_FUNCTION (this);
    if (GetInternalQueue (0)->IsEmpty ())
    {
        return NULL;
    }

    Ptr<const QueueDiscItem> item = StaticCast<const QueueDiscItem> (GetInternalQueue (0)->Peek ());

    return item;

}

bool
TCNQueueDisc::CheckConfig (void)
{
    if (GetNInternalQueues () == 0)
    {
        Ptr<Queue> queue = CreateObjectWithAttributes<DropTailQueue> ("Mode", EnumValue (m_mode));
        if (m_mode == Queue::QUEUE_MODE_PACKETS)
        {
            queue->SetMaxPackets (m_maxPackets);
        }
        else
        {
            queue->SetMaxBytes (m_maxBytes);
        }
        AddInternalQueue (queue);
    }

    if (GetNInternalQueues () != 1)
    {
        NS_LOG_ERROR ("TCNQueueDisc needs 1 internal queue");
        return false;
    }

    return true;

}

void
TCNQueueDisc::InitializeParams (void)
{
    NS_LOG_FUNCTION (this);
}

bool
TCNQueueDisc::MarkingECN (Ptr<QueueDiscItem> item)
{
    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem> (item);
    if (ipv4Item == 0)   {
        NS_LOG_ERROR ("Cannot convert the queue disc item to ipv4 queue disc item");
        return false;
    }

    Ipv4Header header = ipv4Item -> GetHeader ();

    if (header.GetEcn () != Ipv4Header::ECN_ECT1)   {
        NS_LOG_ERROR ("Cannot mark because the ECN field is not ECN_ECT1");
        return false;
    }

    header.SetEcn(Ipv4Header::ECN_CE);
    ipv4Item->SetHeader(header);
    return true;
}

uint32_t
TCNQueueDisc::GetNPackets0 ()
{
    return n_packets_0;
}

uint32_t
TCNQueueDisc::GetNPackets1 ()
{
    return n_packets_1;
}

}
