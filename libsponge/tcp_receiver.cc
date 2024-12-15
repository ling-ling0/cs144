#include "tcp_receiver.hh"
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 如果还没有收到SYN信号 即当前是listen状态
    const TCPHeader head = seg.header();
    // 没有收到syn包
    if(!_syn_get && !head.syn) return;
    // 第一次收到syn包
    if(!_syn_get && head.syn)
    {
        _isn = head.seqno;
        _syn_get = true;
    }
    // 对当前包进行处理
    // 得到当前包的绝对地址
    uint64_t abs_seq = unwrap(head.seqno,_isn,_reassembler.stream_out().bytes_written() + 1);
    // 如果当前是syn包需要+1,所有的包都需要减去最开始的syn,-1
    uint64_t index = abs_seq -1 + head.syn;

    // 这里不能写_fin_get,会出现先给fin的情况,这时就会出错
    _reassembler.push_substring(seg.payload().copy(),index,head.fin);
    
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(_syn_get)
    {
        // 返回的ack需要+syn+fin,这里不是用_fin_get标志,而是用bytestream的终结标志,因为会出现先传入fin包再传入前面包的问题
        return wrap(_reassembler.stream_out().bytes_written()+_syn_get+_reassembler.stream_out().input_ended(),_isn);
    }
    else return nullopt;
}

size_t TCPReceiver::window_size() const { 
    return _capacity - _reassembler.stream_out().buffer_size();
}
