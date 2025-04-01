#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include <iostream>
using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return outstanding_bytes;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_nums;
}


void TCPSender::push(const TransmitFunction& transmit)
{
  // 首先检查Writer是否存在错误并设置错误状态
  if (writer().has_error()) {
    _has_error = true;
  }
  if (_has_error) {
    TCPSenderMessage rst_msg = make_empty_message();
    transmit(rst_msg);
    return;
  }
  
  // 如果没有错误，正常处理...
  uint64_t effective_window = (received_msg.window_size == 0 && outstanding_bytes == 0) ? 1 : received_msg.window_size;
  
  while (outstanding_bytes < effective_window) {
    TCPSenderMessage msg;
    if (isSent_ISN == false) {
      msg.SYN = true;
      msg.seqno = isn_;
      isSent_ISN = true; 
    } else {
      msg.seqno = Wrap32::wrap(abs_seqno, isn_);
    }
    
    // 计算可用窗口大小（考虑已发送但未确认的字节）
    size_t remaining_window = effective_window - outstanding_bytes;
    // 如果是SYN消息，需要减去一个字节，因为SYN占用一个序列号
    if (msg.SYN) remaining_window = remaining_window > 0 ? remaining_window - 1 : 0;
    
    // 计算可以发送的数据大小
    size_t payload_size = min(min(remaining_window, TCPConfig::MAX_PAYLOAD_SIZE),reader().bytes_buffered());
    read(reader(), payload_size, msg.payload);
    
    // 修改FIN逻辑：只有当发送完所有数据后，且确保FIN的一个字节也能放入窗口时才添加FIN
    if (writer().is_closed() && !isSent_FIN && 
        reader().bytes_buffered() == 0 && 
        outstanding_bytes + msg.sequence_length()  < effective_window) {
      isSent_FIN = true;
      msg.FIN = true;
    }
    
    if (!msg.sequence_length()) break;

    outstanding_collections.push_back(msg);
    outstanding_bytes += msg.sequence_length();
    abs_seqno += msg.sequence_length();
    
    transmit(msg);
    // 如果有未确认的数据，启动计时器
    if (outstanding_bytes > 0 && !is_start_timer) {
      is_start_timer = true;
      cur_RTO_ms = initial_RTO_ms_;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap(abs_seqno, isn_);
  
  // 检查是否有错误，无论是来自内部标志还是Writer
  bool has_error = _has_error || writer().has_error();
  msg.RST = has_error;
  return msg;
}

void TCPSender::receive(const TCPReceiverMessage& msg)
{
  // 检查收到的RST标志
  if (msg.RST) {
    _has_error = true;
    const_cast<Writer&>(writer()).set_error();
    return;
  }
  if (_has_error) return; 
  
  received_msg = msg;
  primitive_window_size = msg.window_size;
  if (msg.ackno.has_value() == true) {
    uint64_t ackno_unwrapped = static_cast<uint64_t>(msg.ackno.value().unwrap(isn_, abs_seqno));
    if (ackno_unwrapped > abs_seqno) return;
    while (outstanding_bytes != 0 && 
           static_cast<uint64_t>(outstanding_collections.front().seqno.unwrap(isn_, abs_seqno)) + 
           outstanding_collections.front().sequence_length() <= ackno_unwrapped) {
      outstanding_bytes -= outstanding_collections.front().sequence_length();
      outstanding_collections.pop_front();
      consecutive_retransmissions_nums = 0;
      cur_RTO_ms = initial_RTO_ms_;
      if (outstanding_bytes == 0) is_start_timer = false;
      else is_start_timer = true;
    }
  }
}

void TCPSender::tick(uint64_t ms_since_last_tick, const TransmitFunction& transmit)
{
  if (_has_error) return; 
  
  // 只有当有未确认的数据且计时器启动时才减少时间
  if (is_start_timer && outstanding_bytes > 0) {
    if (cur_RTO_ms <= ms_since_last_tick) {
      transmit(outstanding_collections.front());
      consecutive_retransmissions_nums++;
      if (received_msg.window_size) cur_RTO_ms = (1UL << consecutive_retransmissions_nums) * initial_RTO_ms_;
      else cur_RTO_ms = initial_RTO_ms_;
    } else {
      cur_RTO_ms -= ms_since_last_tick;
    }
  }
}