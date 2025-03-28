#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t num = checkpoint >> 32;
  uint32_t raw2zero = this->raw_value_ - zero_point.raw_value_;
  uint64_t raw = ( num << 32 ) + raw2zero;

  if ( raw >= checkpoint ) {
    if ( raw - checkpoint < static_cast<uint64_t>( 1ULL << 31 ) )
      return raw;
    else {
      if ( num == 0 )
        raw = raw;
      else
        raw = raw - static_cast<uint64_t>( 1ULL << 32 );
    }
  } else {
    if ( checkpoint - raw < static_cast<uint64_t>( 1ULL << 31 ) )
      return raw;
    else {
      if ( num == ( 1ULL << 32 ) - 1 )
        raw = raw;
      else
        raw = raw + static_cast<uint64_t>( 1ULL << 32 );
    }
  }
  return raw;
}
