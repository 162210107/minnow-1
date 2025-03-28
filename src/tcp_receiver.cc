#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) { // 设置流的错误状态
    const_cast<Writer&>( reassembler_.writer() ).set_error();
    return;
  }
  // 如果还没收到SYN且当前消息不含SYN，则忽略
  if ( !is_syn && !message.SYN )
    return;
  uint64_t stream_index;
  if ( message.SYN && !is_syn ) {
    ISN = message.seqno;
    is_syn = true;
  }
  if(message.SYN){
    stream_index = 0;
  } else {
    // 非SYN消息：需要转换为流索引
    stream_index = message.seqno.unwrap( ISN, reassembler_.writer().bytes_pushed() ) - 1;//
  }
  // 插入数据到重组器
  reassembler_.insert(stream_index, message.payload, message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const
{
  uint16_t window_size = static_cast<uint16_t>(
    min( reassembler_.writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) ) );
  TCPReceiverMessage S;
  if ( is_syn ) {
    uint64_t abs_ackno = reassembler_.writer().bytes_pushed() + 1;
    if ( reassembler_.writer().is_closed() ) {//!!! attention!!! 
      abs_ackno += 1;
    }
    S.ackno = ISN.wrap(abs_ackno, ISN );
    S.window_size = window_size;
    S.RST = reassembler_.writer().has_error() | reassembler_.reader().has_error();
  } else {
    S.window_size = window_size;
    S.RST = reassembler_.writer().has_error() | reassembler_.reader().has_error();
  }
  return S;
}
