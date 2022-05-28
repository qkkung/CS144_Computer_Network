#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <map>
#include <memory>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  public:
    class Substring;
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    std::map<size_t, std::shared_ptr<Substring>> _unassembled_data = {};  //!< The index of the first byte of Substring in whole stream (i.e. _output)
    size_t _insert_index = 0; //!< insert_index is the first byte that should be append to ByteStream (_output)
    int _unassembled_size = 0;  //!< unassembled_size is the size of unassembled and cached bytes

    //! put unassembled_data into ByteStream _output
    void put_unassembled_data();
    void directly_put_data(std::string data, bool eof);

    //! store unassembled data into unassembled_data
    void store_unassembled_data(const std::string &data,
                                size_t index,
                                bool eof);

    bool need_rollback();
    void unassembled_to_string();

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    std::map<size_t, std::shared_ptr<Substring>> &get_unassembled_data() { return _unassembled_data; };
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    std::map<size_t, size_t> new_unassembled_data =
        {{1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}, {9, 0}, {10, 0}, {20, 0}};

    class Substring {
      private:
        std::string _data;  // data of Substring
        bool _eof;          // eof indicates if this Substring is last bytes of the whole stream 

      public:
        Substring(std::string str, bool eof);
        std::string &get_data();
        bool is_eof();
    };
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
