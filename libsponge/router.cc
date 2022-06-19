#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`
int mask = 1 << 31;

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    Router::route_entry r;
    r.route_prefix = route_prefix;
    r.prefix_length = prefix_length;
    if (next_hop.has_value()) {
        r.next_hop = next_hop.value();
        r.direct = false;
    }
    r.interface_num = interface_num;
    _route_table.push_back(r);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) { 
    size_t select_index;
    bool exist = false;
    uint32_t dst = dgram.header().dst;
    for (uint32_t i = 0; i < _route_table.size(); i++) {
        if (_route_table[i].prefix_length == 0 && !exist) {
            select_index = i;
            exist = true;
            continue;
        }
        int new_mask = mask >> (_route_table[i].prefix_length - 1);
        if ((new_mask & dst) == _route_table[i].route_prefix 
            && (!exist || (exist && _route_table[i].prefix_length > _route_table[select_index].prefix_length))) {
            select_index = i;
            exist = true;
        }
    }
    if (exist && dgram.header().ttl > 1) {
        dgram.header().ttl -= 1;
        if (_route_table[select_index].direct) {
            _interfaces[_route_table[select_index].interface_num]
                .send_datagram(dgram, Address::from_ipv4_numeric(dst));
        } else {
            _interfaces[_route_table[select_index].interface_num]
                .send_datagram(dgram, _route_table[select_index].next_hop);
        }

    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
