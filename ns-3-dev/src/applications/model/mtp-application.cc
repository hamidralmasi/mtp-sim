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
#include "ns3/boolean.h"
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
    .AddAttribute ("Local",
                   "The Address on which to bind the socket. If not set, it is generated automatically.",
                   AddressValue (),
                   MakeAddressAccessor (&MTPApplication::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. "
                   "Once these bytes are sent, "
                   "no data  is sent again. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MTPApplication::m_maxBytes),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("SimpleTOS",
                   "A simple version of TOS",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MTPApplication::m_tos),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&MTPApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddAttribute ("EnableSeqTsSizeHeader",
                   "Add SeqTsSizeHeader to each packet",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MTPApplication::m_enableSeqTsSizeHeader),
                   MakeBooleanChecker ())
    .AddTraceSource ("Tx", "A new packet is sent",
                     MakeTraceSourceAccessor (&MTPApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("TxWithSeqTsSize", "A new packet is created with SeqTsSizeHeader",
                     MakeTraceSourceAccessor (&MTPApplication::m_txTraceWithSeqTsSize),
                     "ns3::PacketSink::SeqTsSizeCallback")
  ;
  return tid;
}


MTPApplication::MTPApplication ()
  : m_socket (0),
    m_connected (false),
    m_totBytes (0),
    m_unsentPacket (0)
{
  NS_LOG_FUNCTION (this);
}

MTPApplication::~MTPApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
MTPApplication::SetMaxBytes (uint64_t maxBytes)
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
  m_unsentPacket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void MTPApplication::StartApplication (void) // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  Address from;

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      int ret = -1;

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Using MTP with an incompatible socket type. "
                          "MTP requires SOCK_STREAM or SOCK_SEQPACKET. "
                          "In other words, use TCP instead of UDP.");
        }

      if (! m_local.IsInvalid())
        {
          NS_ABORT_MSG_IF ((Inet6SocketAddress::IsMatchingType (m_peer) && InetSocketAddress::IsMatchingType (m_local)) ||
                           (InetSocketAddress::IsMatchingType (m_peer) && Inet6SocketAddress::IsMatchingType (m_local)),
                           "Incompatible peer and local address IP version");
          ret = m_socket->Bind (m_local);
        }
      else
        {
          if (Inet6SocketAddress::IsMatchingType (m_peer))
            {
              ret = m_socket->Bind6 ();
            }
          else if (InetSocketAddress::IsMatchingType (m_peer))
            {
              ret = m_socket->Bind ();
            }
        }

      if (ret == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
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
      m_socket->GetSockName (from);
      SendData (from, m_peer);
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

void MTPApplication::SendData (const Address &from, const Address &to)
{
  NS_LOG_FUNCTION (this);

  while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    { // Time to send more

      // uint64_t to allow the comparison later.
      // the result is in a uint32_t range anyway, because
      // m_sendSize is uint32_t.
      uint64_t toSend = m_sendSize;
      // Make sure we don't send too many
      if (m_maxBytes > 0)
        {
          toSend = std::min (toSend, m_maxBytes - m_totBytes);
        }

      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());

      Ptr<Packet> packet;
      if (m_unsentPacket)
        {
          packet = m_unsentPacket;
          toSend = packet->GetSize ();
        }
      else if (m_enableSeqTsSizeHeader)
        {
          SeqTsSizeHeader header;
          header.SetSeq (m_seq++);
          header.SetSize (toSend);
          NS_ABORT_IF (toSend < header.GetSerializedSize ());
          packet = Create<Packet> (toSend - header.GetSerializedSize ());
          // Trace before adding header, for consistency with PacketSink
          m_txTraceWithSeqTsSize (packet, from, to, header);
          packet->AddHeader (header);
        }
      else
        {
          packet = Create<Packet> (toSend);
        }

      int actual = m_socket->Send (packet);
      if ((unsigned) actual == toSend)
        {
          m_totBytes += actual;
          m_txTrace (packet);
          m_unsentPacket = 0;
        }
      else if (actual == -1)
        {
          // We exit this loop when actual < toSend as the send side
          // buffer is full. The "DataSent" callback will pop when
          // some buffer space has freed up.
          NS_LOG_DEBUG ("Unable to send packet; caching for later attempt");
          m_unsentPacket = packet;
          break;
        }
      else if (actual > 0 && (unsigned) actual < toSend)
        {
          // A Linux socket (non-blocking, such as in DCE) may return
          // a quantity less than the packet size.  Split the packet
          // into two, trace the sent packet, save the unsent packet
          NS_LOG_DEBUG ("Packet size: " << packet->GetSize () << "; sent: " << actual << "; fragment saved: " << toSend - (unsigned) actual);
          Ptr<Packet> sent = packet->CreateFragment (0, actual);
          Ptr<Packet> unsent = packet->CreateFragment (actual, (toSend - (unsigned) actual));
          m_totBytes += actual;
          m_txTrace (sent);
          m_unsentPacket = unsent;
          break;
        }
      else
        {
          NS_FATAL_ERROR ("Unexpected return value from m_socket->Send ()");
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
  Address from, to;
  socket->GetSockName (from);
  socket->GetPeerName (to);
  SendData (from, to);
}

void MTPApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("MTPApplication, Connection Failed");
}

void MTPApplication::DataSend (Ptr<Socket> socket, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    { // Only send new data if the connection has completed
      Address from, to;
      socket->GetSockName (from);
      socket->GetPeerName (to);
      SendData (from, to);
    }
}



} // Namespace ns3
