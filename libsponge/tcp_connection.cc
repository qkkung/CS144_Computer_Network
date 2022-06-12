#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    //cout << "fill_window is invoked, isn:" << _sender.next_seqno().raw_value() << endl;
    _time_since_last_segment_received = 0;
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _linger_after_streams_finish = false;
        _active_flag = false;
        return;
    }
    if (seg.length_in_sequence_space() > 0) {
        _receiver.segment_received(seg);
        // when in listening, receive a syn
        if (_sender.next_seqno_absolute() == 0 && seg.header().syn) {
            _sender.fill_window();
            move_to_connection_queue(true);
            return;
        }
    }
    if (seg.header().ack && _sender.next_seqno_absolute() > 0) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
        //cout << "fill_window is invoked after ack" << endl;
        // It's possible that no segment is sent, so need to check
        if (_sender.segments_out().size() > 0) {
            //cout << "put segment into queue" << endl;
            move_to_connection_queue(true);
            return;
        }
    }
    if (seg.length_in_sequence_space() > 0) {
        _sender.send_empty_segment();
        move_to_connection_queue(true);
        return;
    }

    if (_receiver.ackno().has_value() 
        && seg.length_in_sequence_space() == 0 
        && seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        move_to_connection_queue(false);
    }
    //cout << "fill_window is invoked at last" << endl;
    return;
}

bool TCPConnection::active() const { 
    return _active_flag; 
}

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    move_to_connection_queue(true);
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if (!active()) {
        return;
    }
    _time_since_last_segment_received += ms_since_last_tick;
    
    if (_sender.consecutive_retransmissions() < TCPConfig::MAX_RETX_ATTEMPTS) {
        _sender.tick(ms_since_last_tick);
        move_to_connection_queue(true);
    } else {
        _sender.send_empty_segment();
        _sender.segments_out().back().header().rst = true;
        move_to_connection_queue(false);

        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active_flag = false;
        return;
    }
    //cout << "rt queue size:" << _sender.rt_queue_size() << endl;
    deal_linger_after_streams_finish();

    if (_receiver.stream_out().input_ended() 
        && _sender.stream_in().eof() 
        && _sender.rt_queue_size() == 0) {
        if (!_linger_after_streams_finish) {
            _active_flag = false;
            return;
        }
        // need to linger for 10 ¡Á cfg.rt timeout
        if (time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
            _active_flag = false;
            return;
        }
    }
}

void TCPConnection::end_input_stream() { 
    deal_linger_after_streams_finish();
    _sender.stream_in().end_input();
    _sender.fill_window();
    move_to_connection_queue(true);
}

void TCPConnection::connect() {
    if (_sender.next_seqno_absolute() == 0) {
        _sender.fill_window();
        move_to_connection_queue(true);
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cout << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();
            _sender.segments_out().back().header().rst = true;
            move_to_connection_queue(false);
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            _active_flag = false;
        }
    } catch (const exception &e) {
        std::cout << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::move_to_connection_queue(bool set_ackno_and_window) {
    while (!_sender.segments_out().empty()) {
        if (_receiver.ackno().has_value() && set_ackno_and_window) {
            _sender.segments_out().front().header().ack = true;
            _sender.segments_out().front().header().ackno = _receiver.ackno().value();
            _sender.segments_out().front().header().win = _receiver.window_size();
        }
        segments_out().push(_sender.segments_out().front());
        _sender.segments_out().pop();
    }
}

void TCPConnection::deal_linger_after_streams_finish() {
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }
}
