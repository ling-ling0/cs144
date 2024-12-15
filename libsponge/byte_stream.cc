#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity ) :_error(false),_capacity( capacity ),  _used_capacity(0), _available_capacity(capacity), \
_bytes_poped(0), _bytes_pushed(0), _is_closed(false), _is_finished(false), _stream() {}


size_t ByteStream::write(const string &data) {
    const size_t data_size = data.length();

    if(data_size==0||_available_capacity==0) return 0;

    if(data_size>_available_capacity)
    {
        _stream.push_back(data.substr(0,_available_capacity));
        _used_capacity+=_available_capacity;
        _bytes_pushed+=_available_capacity;
        size_t _available_capacity_copy = _available_capacity;
        _available_capacity-=_available_capacity;
        return _available_capacity_copy;
    }
    else
    {
        _stream.push_back(data);
        _used_capacity+=data_size;
        _bytes_pushed+=data_size;
        _available_capacity-=data_size;
        return data_size;
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t len_copy = len;
    std::string result;
    size_t i = 0;

    if(_stream.empty()) return result;

    while(i<_stream.size() && len_copy>=_stream.at(i).length())
    {
        size_t peeked_size = _stream.at(i).length();
        len_copy-=peeked_size;
        result+=_stream.at(i);
        i++;
    }

    if(i==_stream.size())   // 说明len>used_size
        return result;

    // 说明还有一部分需要拼进去
    result+=_stream.at(i).substr(0,len_copy);

    return result;

}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) 
{
    size_t len_copy = len;
    if(len_copy>=_used_capacity)
    {
        _stream={};
        _available_capacity+=_used_capacity;
        _bytes_poped+=_used_capacity;
        _used_capacity-=_used_capacity;
    }
    else
    {
        while(len_copy>=_stream.front().size())
        {
            size_t poped_size = _stream.front().size();
            _stream.pop_front();
            _used_capacity-=poped_size;
            _available_capacity+=poped_size;
            _bytes_poped+=poped_size;
            len_copy-=poped_size;
        }
        _stream.at(0) = _stream.at(0).substr(len_copy);
        _used_capacity-=len_copy;
        _available_capacity+=len_copy;
        _bytes_poped+=len_copy;
    }

    if(_is_closed&&_used_capacity==0) _is_finished=true;

    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string result = peek_output(len);
    pop_output(len);
    return result;
}

void ByteStream::end_input() {
    _is_closed=true;
    if(_used_capacity==0)
        _is_finished=true;
    return;
}

bool ByteStream::input_ended() const { return _is_closed; }

size_t ByteStream::buffer_size() const { return _used_capacity; }

bool ByteStream::buffer_empty() const { return !_used_capacity; }

bool ByteStream::eof() const { return _is_finished; }

size_t ByteStream::bytes_written() const { return _bytes_pushed; }

size_t ByteStream::bytes_read() const { return _bytes_poped; }

size_t ByteStream::remaining_capacity() const { return _available_capacity; }
