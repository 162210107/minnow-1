#include "reassembler.hh"
#include "debug.hh"
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{ 
  uint64_t bytes_unassembled=output_.writer().bytes_pushed();
  uint64_t bytes_unaccepted=bytes_unassembled+output_.writer().available_capacity();

  if (first_index>=bytes_unaccepted||first_index+data.size()<=bytes_unassembled)   return;

  uint64_t end=min(bytes_unaccepted,first_index+data.size());
  for ( uint64_t i = 0; i < end-first_index; ++i ) {
    uint64_t index = first_index + i;
    if ( index < bytes_unassembled)
      continue;
    byte_map_[index] = data[i];
    if ( index == bytes_unassembled ) {
      while ( byte_map_.count(bytes_unassembled) ) {
        output_.writer().push( ( string( 1, byte_map_[bytes_unassembled] ) ) );
        byte_map_.erase( bytes_unassembled++ );
      }
    }
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
  return byte_map_.size();
}
