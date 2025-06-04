#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "parser.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // convert IP address of next hop to raw 32-bit representation (used in ARP header)
  const uint32_t next_hop_ip = next_hop.ipv4_numeric();
  // if destination Ehernet address known
  if ( arp_table.count( next_hop_ip ) ) {
    EthernetFrame to_send;
    Serializer tmp;
    dgram.serialize( tmp );
    to_send.payload = tmp.finish();
    to_send.header.dst = arp_table[next_hop_ip].first;
    to_send.header.src = ethernet_address_;
    to_send.header.type = EthernetHeader::TYPE_IPv4;
    transmit( to_send );
  } else {                                           // if destination Ethernet address unkown
    if ( !get<bool>( _arp_retransmission_timer ) ) { // no arp sent yet
      send_arp_request( next_hop_ip );
    }
    //  queue the IP datagram
    datagrames_queue.push( { dgram, next_hop } );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST )
    return ;

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    Parser t;
    frame.parse(t);
    if (t.has_error() == ParseResult::NoError ) {
      datagrams_received_ .push(frame.payload);
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    // parse
    ARPMessage arp_packet;
    if ( arp_packet.parse(frame.payload) == ParseResult::NoError ) {
      // record the sender's info
      arp_table[arp_packet.sender_ip_address] = { arp_packet.sender_ethernet_address, _curr_time + 30 * 1000 };
      // turn off timer
      if ( get<bool>( _arp_retransmission_timer )
           && get<uint32_t>( _arp_retransmission_timer ) == arp_packet.sender_ip_address ) {
        _arp_retransmission_timer = make_tuple( false, 0, 0 );
      }

      _arp_failure_time.push( { _curr_time + 30 * 1000, arp_packet.sender_ip_address } );

      // send reply
      if ( arp_packet.target_ip_address == ip_address_.ipv4_numeric()
           && arp_packet.opcode == ARPMessage::OPCODE_REQUEST ) {
        EthernetFrame arp_to_send;
        // header
        arp_to_send.header.dst = arp_packet.sender_ethernet_address;
        arp_to_send.header.src = ethernet_address_;
        arp_to_send.header.type = EthernetHeader::TYPE_ARP;
        // payload
        ARPMessage arp_reply;
        arp_reply.opcode = ARPMessage::OPCODE_REPLY;
        arp_reply.sender_ethernet_address = ethernet_address_;
        arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
        arp_reply.target_ethernet_address = arp_packet.sender_ethernet_address;
        arp_reply.target_ip_address = arp_packet.sender_ip_address;
        Serializer tmp;
        arp_reply.serialize(tmp);
        arp_to_send.payload = tmp.finish();
        // send reply
        cerr<< "send reply" << arp_reply.to_string() <<endl;
        transmit( arp_to_send );
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  debug( "unimplemented tick({}) called", ms_since_last_tick );
}

void NetworkInterface::send_arp_request( const uint32_t ip_to_find )
{
  EthernetFrame arp_to_send;
  // header
  arp_to_send.header.dst = ETHERNET_BROADCAST;
  arp_to_send.header.src = ethernet_address_;
  arp_to_send.header.type = EthernetHeader::TYPE_ARP;
  // payload
  ARPMessage arp_packet;
  arp_packet.opcode = ARPMessage::OPCODE_REQUEST;
  arp_packet.sender_ethernet_address = ethernet_address_;
  arp_packet.sender_ip_address = ip_address_.ipv4_numeric();
  // arp_packet.target_ethernet_address =
  arp_packet.target_ip_address = ip_to_find;
  Serializer tmp;
  arp_packet.serialize( tmp );
  arp_to_send.payload = tmp.finish();

  // send & reset timer
  transmit( arp_to_send );
  _arp_retransmission_timer = make_tuple( true, _curr_time, ip_to_find );
}
