#include "wrapping_integers.hh"
#include <iostream>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    unsigned int wrap_sn = (isn.raw_value() + n) % module;
    return WrappingInt32{wrap_sn};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t offset = (n >= isn) ? static_cast<unsigned int>(n - isn) : (module - static_cast<unsigned int>(isn - n));
    uint64_t loop_num = checkpoint / module;

    uint64_t x = loop_num * module + offset;
    uint64_t y = x + module;
    uint64_t min_distance = x > checkpoint ? x - checkpoint : checkpoint - x;
    uint64_t abs_sn = x;
    if (y - checkpoint < min_distance) {
        abs_sn = y;
        min_distance = y - checkpoint;
    }
    if (loop_num > 0) {
        uint64_t z = x - module;
        if (checkpoint - z < min_distance) {
            abs_sn = z;
        }
    }
    
    return abs_sn;
}
