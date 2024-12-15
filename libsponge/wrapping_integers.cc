#include "wrapping_integers.hh"
#include <cmath>
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // n是绝对序列号 isn是初始序列号 要生成序列号
    uint32_t result;

    result = (n + isn.raw_value()) & 0xffffffff; // 将超过32位的部分变成0

    WrappingInt32 seq = WrappingInt32(result);
    return seq;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // n是序列号 isn是初始序列号 checkpoint是绝对检查点序列号

    uint64_t times = (checkpoint & 0xffffffff00000000);
    uint64_t offset = (checkpoint & 0x00000000ffffffff);
    uint64_t abs_seq = 0;
    
    if(n.raw_value()>=isn.raw_value())   // 序列号在初始右边
    {
        uint32_t seq = n.raw_value()-isn.raw_value();
        if(seq>=offset)
        {
            if(seq-offset <= (1UL<<31))
                abs_seq = seq + times;
            else if(seq-offset > (1UL<<31))
            {
                if(times)
                    abs_seq = seq + times - (1UL<<32);
                else if(times==0)
                    abs_seq = seq + times;
            }
        }
        else if(seq<offset)
        {
            if(offset-seq <= (1UL<<31))
                abs_seq = seq + times;
            else if(offset-seq > (1UL<<31))
                abs_seq = seq + times + (1UL<<32);
        }
    }
    else// if(n.raw_value()<isn.raw_value())
    {
        uint64_t seq = n.raw_value()+(1UL<<32)-isn.raw_value();
        if(seq>=offset)
        {
            if(seq-offset <= (1UL<<31))
                abs_seq = seq + times;
            else if(seq-offset > (1UL<<31))
            {
                if(times)
                    abs_seq = seq + times - (1UL<<32);
                else if(times==0)
                    abs_seq = seq;
            }
        }
        else if(seq<offset)
        {
            if(offset-seq <= (1UL<<31))
                abs_seq = seq + times;
            else if(offset-seq > (1UL<<31))
                abs_seq = seq + times + (1UL<<32);
        }
    }
    return abs_seq;
}
