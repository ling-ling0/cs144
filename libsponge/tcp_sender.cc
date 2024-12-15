#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

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
    , _stream(capacity)
    , _current_rto(retx_timeout) {}


void TCPSender::fill_window() {
    uint64_t current_window_size = _window_size?_window_size:1;
    // 循环填充窗口
    while (1)
    {
        TCPSegment segment;
        // 判断是否发送了syn包
        if(!_syn_send)
        {
            segment.header().syn = true;
            _syn_send = true;
        }
        // 设置seq
        segment.header().seqno = next_seqno();
        // 根据window_size填充,得到payload
        uint64_t payload_size = std::min(TCPConfig::MAX_PAYLOAD_SIZE, \
        current_window_size-segment.header().syn-_using_bytes);
        string payload = _stream.read(payload_size);

        // 检查fin是否可以放进去
        if(!_fin_send && _stream.eof() && current_window_size > payload.size() + _using_bytes)
        {
            // 放入fin
            _fin_send = true;
            segment.header().fin = true;
        }
        // 构造segment完成
        segment.payload() = std::move(payload);

        // 如果没有数据则停止发送
        if(segment.length_in_sequence_space()==0) break;

        // 如果没有等待的数据包,则重设更新时间
        if(_not_apply.empty())
        {
            _current_rto = _initial_retransmission_timeout;
            _waiting_time = 0;
        }

        // 发送并追踪
        _segments_out.push(segment);
        // 这里使用发送时的绝对seq作为索引,此时_next_seq还没变
        _not_apply.emplace(_next_seqno,segment);
        // 更新使用的数量
        _using_bytes += segment.length_in_sequence_space();
        // 更新_next_seq
        _next_seqno += segment.length_in_sequence_space();

        // 设置了fin则结束
        if(_fin_send) break;
    }
    
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 如果传入的ack是不可信的,直接丢弃
    // 得到seq对应的abs_seq

    uint64_t ack_seq = unwrap(ackno,_isn,_next_seqno);
    if(ack_seq >_next_seqno) return;
    // 遍历整个数据结构,如果是一个已经发送的被接受
    for(;!_not_apply.empty();)
    {
        const auto &front = _not_apply.front();
        const auto &segment = front.second;
        if (front.first + segment.length_in_sequence_space() <= ack_seq) {
            // 更新窗口大小
            _using_bytes -= segment.length_in_sequence_space();
            // 移除这个包
            _not_apply.pop();
            // 重置重传计时器和RTO
            _current_rto = _initial_retransmission_timeout;
            _waiting_time = 0;
            } 
        else break;
    }
    // 重置重传计数器
    _retry_times = 0;
    // 更新窗口大小,窗口最小为1
    _window_size = window_size;

    // 重新调用fill_window
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _waiting_time += ms_since_last_tick;
    // 说明当前有发送中的数据包 并且重传计时器超时
    if(!_not_apply.empty() && _waiting_time>=_current_rto)
    {
        // 重置重传计时器
        _waiting_time = 0;
        // 重传最小的包,第一个就是最小的包
        _segments_out.push(_not_apply.front().second);
        if(_window_size>0)
        {
            _current_rto *= 2;
            _retry_times += 1;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _retry_times;
}
size_t TCPSender::bytes_in_flight() const
{
    return _using_bytes;
}

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}
