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

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_reveived; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    // 如果当前不是active状态 直接返回
    if(!_is_active)
    {
        return;
    }
    // 重置计时器
    _time_since_last_segment_reveived = 0;
    bool need_send_ack = seg.length_in_sequence_space();
    // you code here.
    //你需要考虑到ACK包、RST包等多种情况

    // 如果设置 RST, 将入站流和出站流都设置为错误状态,永久终止链接
    if(seg.header().rst)
    {
        unclear_shutdown();
        return;
    }
    // 将包交给接受者
    _receiver.segment_received(seg);
    // 如果是ack包
    if(seg.header().ack)
    {
        _sender.ack_received(seg.header().ackno,seg.header().win);
        if(!_sender.segments_out().empty())
        {
            need_send_ack=false;
        }
    }
    
    //状态变化(按照个人的情况可进行修改)
    // 如果是 LISEN 到了 SYN_RCVD
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        // 此时肯定是第一次调用 fill_window，因此会发送 SYN + ACK
        connect();
        return;
    }

    // 判断 TCP 断开连接时是否时需要等待
    // CLOSE_WAIT
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
        _linger_after_streams_finish = false;

    // 如果到了准备断开连接的时候。服务器端先断
    // CLOSED
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        _is_active = false;
        return;
    }

    // 如果收到的数据包里没有任何数据，则这个数据包可能只是为了 keep-alive
    if (need_send_ack)
        _sender.send_empty_segment();

    send_segment();
}

bool TCPConnection::active() const { return _is_active ; }

size_t TCPConnection::write(const string &data) {
    // DUMMY_CODE(data);
    // return {};
    if(!_is_active)
    {
        return 0;
    }
    size_t num = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segment();
    return num;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 

    if(!_is_active) return;
    
    _time_since_last_segment_reveived += ms_since_last_tick;
    // sender运行tick
    _sender.tick(ms_since_last_tick);

    // 重传超过次数
    if(_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS)
    {
        // 发送重置段
        _is_active = false;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        TCPSegment seg;
        seg.header().rst = true;
        _segments_out.push(seg);
        return;
    }

    // 正常结束
    // 入站流接受完毕 出站流发送完毕 发送的数据全部确认
    if(_receiver.stream_out().input_ended()&&   \
        _sender.stream_in().eof()&&             \
        _sender.bytes_in_flight()==0)
    {
        if(_linger_after_streams_finish==false || \
            _time_since_last_segment_reveived >= 10 * _cfg.rt_timeout)
        {
            _is_active=false;
            return;
        }
    }

    // 发送数据包
    send_segment();

}

// 用来从服务端结束
void TCPConnection::end_input_stream() {
    // 结束发送的数据流
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segment();
}

// 发送syn和客户端建立连接
void TCPConnection::connect() {
    // 直接调用 fillwindow发送syn包
    _sender.fill_window();
    // 然后将sender中的包放到connection中 应该只有这一个包
    send_segment();
    // auto seg = _sender.segments_out().front();
    // _segments_out.push(seg);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::unclear_shutdown()
{
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _is_active = false;
}
void TCPConnection::send_segment()
{
    // 循环将sender中的包移到connection中
    while(!_sender.segments_out().empty())
    {
        auto seg = _sender.segments_out().front();

        if (_receiver.ackno()!=nullopt) {
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
            seg.header().ack = true;
        }

        _segments_out.push(seg);
        _sender.segments_out().pop();
    }
}