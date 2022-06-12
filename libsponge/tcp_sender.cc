#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {

    _rt.rto() = retx_timeout;
}

uint64_t TCPSender::bytes_in_flight() const { 
    return _next_seqno - _max_ackno_abs;
}

void TCPSender::fill_window() { 
    
    TCPSegment tcp_segment;
    bool need_send = false;
    do {
        //set sequence number
        tcp_segment.header().seqno = wrap(_next_seqno, _isn);
        uint64_t seqno_abs = _next_seqno;

        uint64_t _right_edge_seqno = _max_ackno_abs + _window_size;
        if (_window_size == 0 
            && next_seqno_absolute() > bytes_in_flight() 
            && !_sent_flag_win_zero) {  // act like window size is 1, just for an ack from receiver
            _right_edge_seqno = _next_seqno + 1;
            _sent_flag_win_zero = true;
        }

        // SYN flag isn't sent
        if (next_seqno_absolute() == 0) {
            tcp_segment.header().syn = true;
            _next_seqno += 1;
            need_send = true;
        }
        // payload that need to be sent
    
        if (_right_edge_seqno > _next_seqno && !stream_in().buffer_empty()) {
            size_t length = _right_edge_seqno - _next_seqno;
            if (length > TCPConfig::MAX_PAYLOAD_SIZE) {
                length = TCPConfig::MAX_PAYLOAD_SIZE;
            }
            string payload_str = stream_in().read(length);
            _next_seqno += payload_str.size();
            tcp_segment.payload() = Buffer(move(payload_str));
            if (tcp_segment.payload().size() > 0) {
                need_send = true;
            }
        }
        //cout << "right_edge:" << _right_edge_seqno << " stream_in eof:" << stream_in().eof()
        //     << " input_end:" << stream_in().input_ended()
        //     << " stream+2:" << (stream_in().bytes_written() + 2) << endl;
        // outgoing stream reach end
        if (_right_edge_seqno > _next_seqno 
            && stream_in().eof()
            && next_seqno_absolute() < stream_in().bytes_written() + 2) {
            //cout << "has sent fin" << endl;
            tcp_segment.header().fin = true;
            _next_seqno += 1;
            need_send = true;
        }
    
        if (need_send) {
            segments_out().push(tcp_segment);
            _rt.rt_queue().push({seqno_abs, tcp_segment});
            if (_rt.rt_queue().size() == 1) {
                _rt.time_amount() = 0;
            }
            need_send = false;
        }
    } while (next_seqno_absolute() < _max_ackno_abs + _window_size
        && !stream_in().buffer_empty());
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    uint64_t ackno_abs = unwrap(ackno, _isn, _next_seqno);
    if (ackno_abs > next_seqno_absolute()) {
        //cout << "sender ack_received return directly" << endl;
        return;
    }
    if (ackno_abs > _max_ackno_abs) {
        _rt.deal_retransmission_entry(ackno_abs, _max_ackno_abs, _initial_retransmission_timeout);
        _max_ackno_abs = ackno_abs;
        _window_size = window_size;
        _sent_flag_win_zero = false;
        return;
    } else if (ackno_abs == _max_ackno_abs && _window_size < window_size) {
        _window_size = window_size;
        _sent_flag_win_zero = false;
        return;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _rt.time_amount() = _rt.time_amount() + ms_since_last_tick;
    if (_rt.is_timeout() && _rt.rt_queue().size() > 0) {
        _segments_out.push(_rt.rt_queue().front().second);

        if (_window_size > 0 
            || (_window_size == 0 && next_seqno_absolute() == bytes_in_flight())) {

            _rt.count_rt() += 1;
            _rt.rto() *= 2;
        }

        _rt.time_amount() = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { 
    unsigned int count_retransmission = _rt.get_count_rt();
    return count_retransmission;
}

void TCPSender::send_empty_segment() {
    TCPSegment tcp_segment;
    tcp_segment.header().seqno = wrap(next_seqno_absolute(), _isn);
    segments_out().push(tcp_segment);
}

void TCPRetransmission::remove_retransmission_entry(uint64_t ackno_absolute) {
    while (_rt_queue.size() > 0) {
        if (ackno_absolute < 
            _rt_queue.front().first + _rt_queue.front().second.length_in_sequence_space()) {
            return;
        }
        _rt_queue.pop();
    }
}


void TCPRetransmission::deal_retransmission_entry(
    uint64_t ackno_absolute, uint64_t max_seqno_abs, unsigned int initial_timeout) {

    remove_retransmission_entry(ackno_absolute);

    if (ackno_absolute > max_seqno_abs) {
        rto() = initial_timeout;
        time_amount() = 0;
        count_rt() = 0;
    }
}
