#include "tcp_receiver.hh"
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (!_isn.has_value()) {
        if (!seg.header().syn) {
            return;
        }
        _isn = make_optional(WrappingInt32(seg.header().seqno));
        // hand the first segment here
        _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);
        return;
    }
    uint64_t abs_sn = unwrap(seg.header().seqno, _isn.value(), stream_out().bytes_written());
    // if it isn't the first segment where SYN flag is set, absolute sequence number must be greater than zero
    size_t written = stream_out().bytes_written();
    if (abs_sn > written - (_capacity - window_size()) && abs_sn <= written + window_size()) {
        _reassembler.push_substring(seg.payload().copy(), abs_sn - 1, seg.header().fin);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_isn.has_value()) {
        return nullopt;
    }
    WrappingInt32 wrap_sn = (wrap(stream_out().bytes_written() + 1, _isn.value()));
    // Flag FIN occupies a byte
    if (stream_out().input_ended()) {
        wrap_sn = wrap_sn + 1;
    }
    return optional<WrappingInt32>(wrap_sn);
}

size_t TCPReceiver::window_size() const { 
    return stream_out().remaining_capacity();
}
