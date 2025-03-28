#include "reassembler.hh"
#include "debug.hh"
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{ 
  size_t _first_unassembled = output_.writer().bytes_pushed();
  size_t _first_unaccept = _first_unassembled + _capacity;
  // 处理空的最后子串的特殊情况
  if (data.empty() && is_last_substring) {
    _is_eof = true;
    _eof_index = first_index;
    // 如果当前已组装到EOF位置，立即关闭流
    if (_first_unassembled == _eof_index) {
      output_.writer().close();
    }
    return;
  }
  // 如果数据完全超出范围，直接返回
  if(first_index >= _first_unaccept || first_index + data.size() <= _first_unassembled)
  {
    // 如果是最后子串，记录EOF位置
    if(is_last_substring) {
      _is_eof = true;
      _eof_index = first_index + data.size();
    }
    return;
  }
  /*TIME OUT
  uint64_t bytes_unassembled=output_.writer().bytes_pushed();
  uint64_t bytes_unaccepted=bytes_unassembled+output_.writer().available_capacity();
  
  if(data.empty()&&is_last_substring){
    _is_eof = true;
    _eof_index = first_index + data.size(); 
    if(bytes_unassembled==_eof_index) output_.writer().close();
    return ;
  }

  if (first_index>=bytes_unaccepted||first_index+data.size()<=bytes_unassembled){
    if(is_last_substring){
      _is_eof=true;
      _eof_index=first_index+data.size();
    }
    return;
  }
  // uint64_t end=min(bytes_unaccepted,first_index+data.size());
  // for ( uint64_t i = 0; i < end-first_index; ++i ) {
  //   uint64_t index = first_index + i;
  //   if ( index < bytes_unassembled)
  //     continue;
  //   byte_map_[index] = data[i];
  //   if ( index == bytes_unassembled ) {
  //     while ( byte_map_.count(bytes_unassembled) ) {
  //       output_.writer().push( ( string( 1, byte_map_[bytes_unassembled] ) ) );
  //       byte_map_.erase( bytes_unassembled++ );
  //     }
  //   }
  // }*/

  size_t begin_index = max(first_index, _first_unassembled);
  size_t end_index = min(first_index + data.size(), _first_unaccept);
  for(size_t i = begin_index; i < end_index; i++)
  {
    if (!_flag[i - _first_unassembled]) {
      buffer[i - _first_unassembled] = data[i - first_index];
      _flag[i - _first_unassembled] = true;
      _unassembled_bytes++;
    }
  }
  string intermediate_data = "";
  while (!_flag.empty() && _flag.front())
  {
    intermediate_data += buffer.front();
    buffer.pop_front();
    _flag.pop_front();
    buffer.emplace_back('\0');
    _flag.emplace_back(false);
  }
  if (intermediate_data.size()) {
    output_.writer().push(intermediate_data);
    _unassembled_bytes -= intermediate_data.size();
  }

  if(is_last_substring)
  {
    _is_eof = true;
    _eof_index = first_index + data.size(); 
  }
  // 只有当所有数据都已处理时才关闭流!!many writers
  if(_is_eof && output_.writer().bytes_pushed() == _eof_index)    output_.writer().close();
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return _unassembled_bytes;
}
