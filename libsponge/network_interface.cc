#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`
using namespace std;

static constexpr size_t _ip_to_ethernet_timeout = 30000;
static constexpr size_t _count_down_time = 5000;


//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    map<uint32_t, pair<EthernetAddress, size_t>>::iterator it = ip_to_ethernet().find(next_hop_ip);
    if (it != ip_to_ethernet().end()) {
        BufferList b_list = dgram.serialize();
        send_frame(it->second.first, b_list, EthernetHeader::TYPE_IPv4);
        return;
    }
    // don't have the ethernet address of next hop, then just cache the datagram
    if (ip_to_datagram().count(next_hop_ip) == 0) {
        queue<InternetDatagram> q_datagram;
        q_datagram.push(dgram);
        ip_to_datagram()[next_hop_ip] = q_datagram;
    } else {
        ip_to_datagram()[next_hop_ip].push(dgram);
    }
    // if last ARP request was sent beyond 5000ms, then send agian  
    if (timer().count(next_hop_ip) == 0) {
        send_arp_frame(next_hop_ip, ETHERNET_BROADCAST, true);
        return;
    }
    return;
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (!compare_array(frame.header().dst, _ethernet_address) 
        && !compare_array(frame.header().dst, ETHERNET_BROADCAST)) {
        return nullopt;
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        auto ret = datagram.parse(frame.payload().buffers().front());
        if (ret != ParseResult::NoError) {
            cerr << "parse frame header failed when receive frame" << endl;
            return nullopt;
        }
        if (datagram.payload().size() != datagram.header().payload_length()) {
            cerr << "packet is too short" << endl;
            return nullopt;
        }
        return optional<InternetDatagram>{datagram};
    }
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        arp.parse(frame.payload().buffers().front());
        // learn sender ip and ethernet address
        ip_to_ethernet()[arp.sender_ip_address] = {arp.sender_ethernet_address, _ip_to_ethernet_timeout};
        auto it = ip_to_datagram().find(arp.sender_ip_address);
        if (it != ip_to_datagram().end()) {
            for (unsigned int i = 0; i < it->second.size(); i++) {
                InternetDatagram &dgram = it->second.front();
                BufferList b_list = dgram.serialize();
                send_frame(arp.sender_ethernet_address, b_list, EthernetHeader::TYPE_IPv4);
                it->second.pop();
            }
        }
        if (arp.opcode == ARPMessage::OPCODE_REQUEST 
            && arp.target_ip_address == _ip_address.ipv4_numeric()) {
            send_arp_frame(arp.sender_ip_address, arp.sender_ethernet_address, false);
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    // expire any ip_to_ethernet mapping that have expired 
    for (auto it = ip_to_ethernet().begin(); it != ip_to_ethernet().end();) {
        if (it->second.second <= ms_since_last_tick) {
            it = ip_to_ethernet().erase(it);
        } else {
            it->second.second -= ms_since_last_tick;
            it++;
        }
    }
    // expire any timer mapping that have expired
    for (auto it = timer().begin(); it != timer().end();) {
        if (it->second <= ms_since_last_tick) {
            it = timer().erase(it);
        } else {
            it->second -= ms_since_last_tick;
            it++;
        }
    }
}

void NetworkInterface::send_frame(EthernetAddress address, BufferList &buffer_list, uint16_t type) {
    EthernetFrame frame;
    frame.header().dst = address;
    frame.header().src = _ethernet_address;
    frame.header().type = type;

    frame.payload().append(buffer_list);
    frames_out().push(frame);
    return;
}

void NetworkInterface::send_arp_frame(uint32_t target_ip, EthernetAddress e_addr, bool broadcast) {
    ARPMessage arp;
    arp.opcode = ARPMessage::OPCODE_REQUEST;
    arp.sender_ip_address = _ip_address.ipv4_numeric();
    arp.sender_ethernet_address = _ethernet_address;
    arp.target_ip_address = target_ip;
    if (!broadcast) {
        arp.target_ethernet_address = e_addr;
        arp.opcode = ARPMessage::OPCODE_REPLY;
    }
    BufferList buffer_list(arp.serialize());
    send_frame(e_addr, buffer_list, EthernetHeader::TYPE_ARP);
    timer()[target_ip] = _count_down_time;
    return;
}

bool NetworkInterface::compare_array(const EthernetAddress &left, const EthernetAddress &right) { 
    if (left.size() != right.size()) {
        return false;
    } 
    for (unsigned int i = 0; i < left.size(); i++) {
        if (left[i] != right[i]) {
            return false;
        }
    }
    return true;
}
