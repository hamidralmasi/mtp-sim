/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Georgia Institute of Technology
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
 * Author: George F. Riley <riley@ece.gatech.edu>
 */

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/tcp-socket-factory.h"
#include "mtp-application.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MTPApplication");

NS_OBJECT_ENSURE_REGISTERED (MTPApplication);

TypeId
MTPApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MTPApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<MTPApplication> ()
    .AddAttribute ("SendSize", "The amount of data to send each time.",
                   UintegerValue (512),
                   MakeUintegerAccessor (&MTPApplication::m_sendSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&MTPApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. "
                   "Once these bytes are sent, "
                   "no data  is sent again. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MTPApplication::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("DelayThresh",
                   "How many packets can pass before we have delay, 0 for disable",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MTPApplication::m_delayThresh),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("DelayTime",
                   "The time for a delay",
                   TimeValue (MicroSeconds (100)),
                   MakeTimeAccessor (&MTPApplication::m_delayTime),
                   MakeTimeChecker())
    .AddAttribute ("SimpleTOS",
                   "A simple version of TOS",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MTPApplication::m_tos),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&MTPApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&MTPApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}


MTPApplication::MTPApplication ()
  : m_socket (0),
    m_connected (false),
    m_totBytes (0),
    m_isDelay (false),
    m_accumPackets (0)
{
  NS_LOG_FUNCTION (this);
}

MTPApplication::~MTPApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
MTPApplication::SetMaxBytes (uint32_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket>
MTPApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

void
MTPApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void MTPApplication::StartApplication (void) // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Using MTP with an incompatible socket type. "
                          "MTP requires SOCK_STREAM or SOCK_SEQPACKET. "
                          "In other words, use TCP instead of UDP.");
        }

      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind6 ();
        }
      else if (InetSocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind ();
        }

      m_socket->Connect (m_peer);
      m_socket->ShutdownRecv ();
      m_socket->SetConnectCallback (
        MakeCallback (&MTPApplication::ConnectionSucceeded, this),
        MakeCallback (&MTPApplication::ConnectionFailed, this));
      m_socket->SetSendCallback (
        MakeCallback (&MTPApplication::DataSend, this));
    }
  if (m_connected)
    {
      SendData ();
    }
}

void MTPApplication::StopApplication (void) // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("MTPApplication found null socket to close in StopApplication");
    }
}


// Private helpers

void MTPApplication::SendData (void)
{
  NS_LOG_FUNCTION (this);

  while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
      if (m_isDelay)
        {
          break;
        }

      // Time to send more
      uint32_t toSend = m_sendSize;
      // Make sure we don't send too many
      if (m_maxBytes > 0)
        {
          toSend = std::min (m_sendSize, m_maxBytes - m_totBytes);
        }
      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> (toSend);
      SocketIpTosTag tosTag;
      tosTag.SetTos (m_tos << 2);
      packet->AddPacketTag (tosTag);
      m_txTrace (packet);
      int actual = m_socket->Send (packet);
      if (actual > 0)
        {
          m_totBytes += actual;
          m_accumPackets ++;
        }

      // We exit this loop when actual < toSend as the send side
      // buffer is full. The "DataSent" callback will pop when
      // some buffer space has freed ip.
      if ((unsigned)actual != toSend)
        {
          break;
        }

      if (m_delayThresh != 0 && m_accumPackets > m_delayThresh)
        {
          m_isDelay = true;
          Simulator::Schedule (m_delayTime, &MTPApplication::ResumeSend, this);
        }
    }
  // Check if time to close (all sent)
  if (m_totBytes == m_maxBytes && m_connected)
    {
      m_socket->Close ();
      m_connected = false;
    }
}

void MTPApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("MTPApplication Connection succeeded");
  m_connected = true;
  SendData ();
}

void MTPApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("MTPApplication, Connection Failed");
}

void MTPApplication::DataSend (Ptr<Socket>, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    { // Only send new data if the connection has completed
      SendData ();
    }
}

void MTPApplication::ResumeSend (void)
{
    NS_LOG_FUNCTION (this);

    m_isDelay = false;
    m_accumPackets = 0;

    if (m_connected)
    {
        SendData ();
    }
}

} // Namespace ns3
