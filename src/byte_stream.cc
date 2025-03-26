#include "byte_stream.hh"
using namespace std;

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( is_closed_ == true )
    return;
  if ( data.size() > available_capacity() )
    data.resize( available_capacity() );

  if ( !data.empty() ) {
    pushed_ += data.size();
    bufed_ += data.size();
    q.push( std::move( data ) );
  }

  if(f.empty()&&!q.empty()) f=q.front();
}

void Writer::close()
{
  if ( is_closed_ == false ) {
    is_closed_ = true;
    q.emplace( string( 1, EOF ) );
  }
  // Your code here.
}

bool Writer::is_closed() const
{
  return is_closed_; // Your code here.
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - bufed_;
}

uint64_t Writer::bytes_pushed() const
{
  return pushed_; // Your code here.
}

string_view Reader::peek() const
{
  return f; // Your code here.
}

void Reader::pop( uint64_t len )
{
  uint64_t tmp = len;
  if(tmp>bufed_) tmp=bufed_;
  while (tmp >= f.size() && tmp > 0 ) {
    tmp -= f.size();
    q.pop();
    f=q.empty() ? ""sv:q.front();
    //f=q.front();
  }
  if(tmp>0){
    f.remove_prefix(tmp);
  }

  poped_+=len;
  bufed_-=len;
}

bool Reader::is_finished() const
{
  return is_closed_ && bytes_buffered() == 0; // Your code here.
}

uint64_t Reader::bytes_buffered() const
{
  return bufed_;
}

uint64_t Reader::bytes_popped() const
{
  return poped_; // Your code here.
}
