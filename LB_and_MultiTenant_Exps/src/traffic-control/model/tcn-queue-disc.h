#ifndef TCN_QUEUE_DISC_H
#define TCN_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/nstime.h"

namespace ns3 {

class TCNQueueDisc : public QueueDisc
{
public:
    static TypeId GetTypeId (void);

    TCNQueueDisc ();

    virtual ~TCNQueueDisc ();

    bool MarkingECN (Ptr<QueueDiscItem> item);
    uint32_t GetNPackets0();
    uint32_t GetNPackets1();

private:
    // Operations offered by multi queue disc should be the same as queue disc
    virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
    virtual bool DoEnqueueWithToS (Ptr<QueueDiscItem> item, int ToS);

    virtual Ptr<QueueDiscItem> DoDequeue (void);
    virtual Ptr<QueueDiscItem> DoDequeueWithToS (int ToS);

    virtual Ptr<const QueueDiscItem> DoPeek (void) const;
    virtual bool CheckConfig (void);
    virtual void InitializeParams (void);

    uint32_t m_maxPackets;                  //!< Max # of packets accepted by the queue
    uint32_t m_maxBytes;                    //!< Max # of bytes accepted by the queue
    Queue::QueueMode     m_mode;            //!< The operating mode (Bytes or packets)

    bool m_separateCounters;
    bool m_useEcn;
    Time m_threshold;
    uint32_t n_packets_0;
    uint32_t n_packets_1;
    uint32_t n_threshold;
    
};

}

#endif
