#include "byte_stream.hh"
#include <algorithm>
#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    int insert = std::min(this->remaining_capacity(), data.length());
    if (insert <= 0) {
        return 0;
    }
    for (int i = 0; i < insert; i++) {
        _buffer.push_back(data[i]);
    }
    _total_written += insert;
    return insert;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (_buffer.empty()) {
        return {};
    }
    int peek = std::min(len, _buffer.size());
    //std::string output;
    //output.append(&(_buffer.front()), peek);
    std::cout << "_buffer:" << _buffer.size() << " len:" << len << " peek:" << peek << " " << _buffer.back() << std::endl;
    return string().assign(_buffer.begin(), _buffer.begin() + peek);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    if (_buffer.empty()) {
        return;
    }
    int pop = std::min(len, _buffer.size());
    for (int i = 0; i < pop; i++) {
        _buffer.pop_front();
    }
    _total_read += pop;

}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) { 
    string peek = peek_output(len);
    pop_output(len);
    return peek;

}

void ByteStream::end_input() { _input_end = true; }

bool ByteStream::input_ended() const { return _input_end; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return this->buffer_empty() && this->input_ended(); }

size_t ByteStream::bytes_written() const { return _total_written; }

size_t ByteStream::bytes_read() const { return _total_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - this->buffer_size(); }
