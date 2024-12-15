#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    // 把数据包 dgram 转化为以太网帧发送到 next_hop
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto it = _addr_cache.find(next_hop_ip);
    // 现有缓存中找到了
    if(it!=_addr_cache.end())
    {
        // 创建EthernetHeader::TYPE_IPv4以太网帧并发送
        EthernetFrame frame1;
        frame1.header().src=_ethernet_address;   // 本机
        frame1.header().dst=(*it).second.first;  // 从缓存中找到的目标
        frame1.header().type=EthernetHeader::TYPE_IPv4;
        frame1.payload()=dgram.serialize();

        // send
        _frames_out.emplace(frame1);
    }
    // 现有缓存中没找到
    else
    {
        // 如果arp请求不存在或者没有回应 重新发送arp
        auto arp = _addr_request_time.find(next_hop_ip);
        if(arp==_addr_request_time.end()||_current_time-(*arp).second>5000)
        {
            // 重新发送arp协议
            ARPMessage arpmge;
            arpmge.sender_ethernet_address=_ethernet_address;
            arpmge.sender_ip_address=_ip_address.ipv4_numeric();
            arpmge.target_ip_address=next_hop_ip;
            arpmge.opcode=ARPMessage::OPCODE_REQUEST;

            EthernetFrame frame2;
            frame2.header().src=_ethernet_address;   // 本机
            frame2.header().dst=ETHERNET_BROADCAST;  // 广播
            frame2.header().type=EthernetHeader::TYPE_ARP;
            frame2.payload()=BufferList(arpmge.serialize());
            _frames_out.emplace(frame2);

            // send
            // 根据是否请求过 分为更改和添加
            if(arp==_addr_request_time.end())
            {
                // _frames_out.emplace(frame2);
                _addr_request_time.emplace(make_pair(next_hop_ip,_current_time));
            }
            else 
            {
                // _frames_out.emplace(frame2);
                (*arp).second=_current_time;
            }
        }

        // 存储等待发送的内容
        _waiting_dgrams.emplace_back(next_hop_ip,dgram);

    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // DUMMY_CODE(frame);
    // return {};
    optional<InternetDatagram> res=nullopt;

    // 忽略所有不发送到这个接口的帧
    auto header = frame.header();
    // 是发送到这里的帧
    if(header.dst==_ethernet_address||header.dst==ETHERNET_BROADCAST)
    {
        // 接收帧,如果是IPv4
        if(header.type==EthernetHeader::TYPE_IPv4)
        {
            InternetDatagram dgram;
            if(dgram.parse(Buffer(frame.payload()))==ParseResult::NoError)
                res = dgram;
            return res;
        }
        // 是ARP
        else if(header.type==EthernetHeader::TYPE_ARP)
        {
            ARPMessage arpm;
            if(arpm.parse(Buffer(frame.payload()))==ParseResult::NoError)
            {
                // 记住关系映射
                auto it = _addr_cache.find(arpm.sender_ip_address);
                // 没有存储过
                if(it==_addr_cache.end())
                {
                    _addr_cache.emplace(arpm.sender_ip_address,make_pair(arpm.sender_ethernet_address,_current_time));

                }
                else    // 存储过 修改时间和IP 
                {
                    (*it).second.first = arpm.sender_ethernet_address;
                    (*it).second.second = _current_time;
                }

                // 删除等待arp请求中的这一项
                auto arp = _addr_request_time.find(arpm.sender_ip_address);
                if(arp!=_addr_request_time.end())
                {
                    _addr_request_time.erase(arp);
                }

                // 更新映射之后应该尝试发送等待发送的内容
                // 遍历所有等待的数据报 如果目标ip是这个就发出去
                for(auto dg = _waiting_dgrams.begin();dg!=_waiting_dgrams.end();)
                {
                    // 
                    if((*dg).first==arpm.sender_ip_address)
                    {
                        EthernetFrame tobesend;

                        tobesend.header().dst=_addr_cache[arpm.sender_ip_address].first;
                        tobesend.header().src=_ethernet_address;
                        tobesend.header().type=EthernetHeader::TYPE_IPv4;
                        tobesend.payload()=(*dg).second.serialize();

                        _frames_out.emplace(tobesend);
                        _waiting_dgrams.erase(dg);
                    }
                    else 
                        dg++;
                }

                // 是请求我们的IP的 arp, 给出回复
                if(arpm.opcode==ARPMessage::OPCODE_REQUEST&&arpm.target_ip_address==_ip_address.ipv4_numeric())
                {
                    // 构造返回的arp
                    ARPMessage rpl_arpmge;
                    rpl_arpmge.sender_ethernet_address=_ethernet_address;
                    rpl_arpmge.sender_ip_address=_ip_address.ipv4_numeric();
                    rpl_arpmge.target_ip_address=arpm.sender_ip_address;
                    rpl_arpmge.target_ethernet_address=arpm.sender_ethernet_address;
                    rpl_arpmge.opcode=ARPMessage::OPCODE_REPLY;

                    EthernetFrame rpl_frame;
                    rpl_frame.header().src=_ethernet_address;   // 本机
                    rpl_frame.header().dst=arpm.sender_ethernet_address;  // 广播
                    rpl_frame.header().type=EthernetHeader::TYPE_ARP;
                    rpl_frame.payload()=BufferList(rpl_arpmge.serialize());

                    _frames_out.emplace(rpl_frame);

                }
            }
        }
    }
    return res;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick)// { DUMMY_CODE(ms_since_last_tick); }
{
    _current_time+=ms_since_last_tick;
    for(auto it = _addr_cache.begin();it!=_addr_cache.end();)
    {
        if(_current_time-(*it).second.second>30000)
        {
            it=_addr_cache.erase(it);
        }
        else 
            it++;
    }
}
