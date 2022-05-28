#include <iostream>
#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // if memory is full, don't store any more data
    if (_capacity == _output.buffer_size()) {
        return;
    }

    // define two variables preparing for rollback
    size_t new_insert_count = 0;


    if (index > _insert_index){
        store_unassembled_data(data, index, eof);
        return;
    } 

    if (index == _insert_index) {
        directly_put_data(data, eof);
        new_insert_count += data.size();
    }else if (index + data.size() > _insert_index) {
        directly_put_data(data.substr(_insert_index - index), eof);
        new_insert_count += index + data.size() - _insert_index;
    }
    put_unassembled_data();
    return;
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_size; }

bool StreamReassembler::empty() const { return _unassembled_size == 0; }

void StreamReassembler::put_unassembled_data() {
    map<size_t, shared_ptr<Substring>>::iterator it = _unassembled_data.begin();
    while (it != _unassembled_data.end()) {
        if (it->first == _insert_index) {
            directly_put_data(it->second->get_data(), it->second->is_eof());
            _unassembled_size -= it->second->get_data().size(); 
            it = _unassembled_data.erase(it);
            
            continue;
        }
        if (it->first < _insert_index) {
            size_t entry_length = it->first + it->second->get_data().size();
            cout << (it->first) << " :" << entry_length << " :" << _insert_index << endl;
            if (entry_length > _insert_index) {
                directly_put_data(it->second->get_data().substr(_insert_index - it->first), it->second->is_eof());
            }
            _unassembled_size -= it->second->get_data().size();
            it = _unassembled_data.erase(it);
            continue;
        }
        break;
    }
}

void StreamReassembler::directly_put_data(std::string data, bool eof) {
    size_t written_num = _output.write(data);
    if (written_num == data.size() && eof) {
        _output.end_input();
    }
    _insert_index += written_num;
}

void StreamReassembler::store_unassembled_data(const std::string& data, 
    size_t index, bool eof) {

    map<size_t, shared_ptr<Substring>>::iterator it = _unassembled_data.lower_bound(index);

    if (it == _unassembled_data.end()) {
        if (_unassembled_data.size() == 0) {
            _unassembled_data[index] = make_shared<Substring>(data, eof);
            _unassembled_size += data.size();
            return;
        }
        auto it_prev = it;
        it_prev--;
        size_t prev_end_index = it_prev->first + it_prev->second->get_data().size();
        if (prev_end_index <= index) {
            _unassembled_data[index] = make_shared<Substring>(data, eof);
            _unassembled_size += data.size();
            return;
        }
        if (prev_end_index < index + data.size()) {
            string str3 = data.substr(prev_end_index - index);
            cout << "3 index:" << prev_end_index << endl;
            _unassembled_data[prev_end_index] = make_shared<Substring>(str3, eof);
            _unassembled_size += index + data.size() - prev_end_index;
            return;
        }
        return;
    }

    size_t entry_length = it->second->get_data().size();
    if (it->first == index) {
        // discard directly
        if (entry_length >= data.size()) {
            return;
        }
        store_unassembled_data(data.substr(entry_length), index + entry_length, eof);
        return;
    }

    // if the iterator `it` is the first entry in _unassembled_data
    if (it == _unassembled_data.begin()) {
        //cout << "index:" << index << "first:" << it->first << endl;
        if (data.size() <= it->first - index) {
            _unassembled_data[index] = make_shared<Substring>(data, eof);
            _unassembled_size += data.size();
            return;
        } 
        _unassembled_data[index] = make_shared<Substring>(data.substr(0, it->first - index), false);
        _unassembled_size += it->first - index;
        store_unassembled_data(data.substr(it->first - index), it->first, eof);
        return;
    }

    // in case where two entries are before and after `index` or `data` seperately
    map<size_t, shared_ptr<Substring>>::iterator it_backward = it;
    it_backward--;
    entry_length = it_backward->second->get_data().size();
    // `it_backward` containes 'data'
    if (it_backward->first + entry_length >= index + data.size()) {
        return;
    }

    // there are spaces between `it_backward` and `it`
    if (it_backward->first + entry_length < it->first) {
        int start_position = (it_backward->first + entry_length) <= index 
            ? 0 : (it_backward->first + entry_length - index);
        // The spaces between `it_backward` and `it` can fit `data`
        if (index + data.size() <= it->first) {
            _unassembled_data[index + start_position] = 
                make_shared<Substring>(data.substr(start_position, data.size() - start_position), eof);
            _unassembled_size += data.size() - start_position;
            return;
        }
        // if the spaces between `it_backward` and `it` can not fit `data`, spare spaces from `data` and store it
        _unassembled_data[index + start_position] =
            make_shared<Substring>(data.substr(start_position, it->first - index - start_position), false);
        _unassembled_size += it->first - index - start_position;
    }
    // recursively call to deal with that part of `data` beyond the spaces between `it_backward` and `it`
    if (index + data.size() > it->first) {
        store_unassembled_data(data.substr(it->first - index), it->first, eof);
    }
    return;
}

bool StreamReassembler::need_rollback() {
    if (_insert_index + _unassembled_size > _capacity) {
        return true;
    }
    return false;
}

void StreamReassembler::unassembled_to_string() {
    cout << "unassembled_to_string:";
    for (auto it = _unassembled_data.begin(); it != _unassembled_data.end(); it++) {
        cout << it->first << ":" << it->second->get_data().size() << "  ";
    }
    cout << endl << endl;
}

// inner class Substring functions
StreamReassembler::Substring::Substring(std::string data, bool eof) : _data(data), _eof(eof) {}

std::string &StreamReassembler::Substring::get_data() { return this->_data; }
bool StreamReassembler::Substring::is_eof() { return this->_eof; }



