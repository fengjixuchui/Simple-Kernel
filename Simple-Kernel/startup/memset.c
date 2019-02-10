// Compile with GCC -O3 for best performance
// It pretty much entirely negates the need to write these by hand in asm.
#include <stddef.h>
#include <stdint.h>
#include <x86intrin.h>

void * memset (void *dest, const uint8_t val, size_t len)
{
  uint8_t *ptr = (uint8_t*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}

///=============================================================================
/// LICENSING INFORMATION
///=============================================================================
//
// The code above this comment is in the public domain.
// The code below this comment is subject to the custom attribution license found
// here: https://github.com/KNNSpeed/Simple-Kernel/blob/master/LICENSE_KERNEL
//
// AVX Memset V1.0
// Minimum requirement: x86_64 CPU with SSE2, but AVX2 or later is recommended
//

#ifdef __AVX512F__
#define BYTE_ALIGNMENT 0x3F // For 64-byte alignment
#elif __AVX__
#define BYTE_ALIGNMENT 0x1F // For 32-byte alignment
#else
#define BYTE_ALIGNMENT 0x0F // For 16-byte alignment
#endif

void * AVX_memset(void *dest, const uint8_t val, size_t numbytes);
void * memset_large(void *dest, const uint8_t val, size_t numbytes);
void * memset_zeroes(void *dest, size_t numbytes);

// Scalar
void * memset (void *dest, uint8_t val, size_t len);
void * memset_16bit(void *dest, uint16_t val, size_t len);
void * memset_32bit(void *dest, uint32_t val, size_t len);
void * memset_64bit(void *dest, uint64_t val, size_t len);

// SSE2
void * memset_128bit_u(void *dest, __m128i_u val, size_t len);
void * memset_128bit_a(void *dest, __m128i val, size_t len);

// AVX
#ifdef __AVX__
void * memset_256bit_u(void *dest, __m256i_u val, size_t len);
void * memset_256bit_a(void *dest, __m256i val, size_t len);
#endif

// AVX512
#ifdef __AVX512F__
void * memset_512bit_u(void *dest, __m512i_u val, size_t len);
void * memset_512bit_a(void *dest, __m512i val, size_t len);
#endif

// To set values of sizes > 1 byte, call the desired memset functions directly
// instead.
void * AVX_memset(void *dest, const uint8_t val, size_t numbytes)
{
  void * returnval = dest;

  if( ((uintptr_t)dest & BYTE_ALIGNMENT) == 0 ) // Check alignment
  {
    if(val == 0)
    {
      memset_zeroes(dest, numbytes);
    }
    else
    {
      memset_large(dest, val, numbytes);
    }
  }
  else
  {
    size_t numbytes_to_align = (uintptr_t)dest & BYTE_ALIGNMENT;

    if(val == 0)
    {
      // Get to an aligned position.
      // This may be a little slower, but since it'll be mostly scalar operations
      // alignment doesn't matter. Worst case it uses two vector functions, and
      // this process only needs to be done once per call if dest is unaligned.
      memset_zeroes(dest, numbytes_to_align);
      dest = (char*)dest + numbytes_to_align;
      // Now this should be near the fastest possible since stores are aligned.
      memset_zeroes(dest, numbytes - numbytes_to_align);
    }
    else
    {
      // Get to an aligned position.
      // This may be a little slower, but since it'll be mostly scalar operations
      // alignment doesn't matter. Worst case it uses two vector functions, and
      // this process only needs to be done once per call if dest is unaligned.
      memset_large(dest, val, numbytes_to_align);
      dest = (char*)dest + numbytes_to_align;
      // Now this should be near the fastest possible since stores are aligned.
      memset_large(dest, val, numbytes - numbytes_to_align);
    }
  }

  return returnval;
}

// Set arbitrarily large amounts of a single byte
void * memset_large(void *dest, const uint8_t val, size_t numbytes)
{
  void * returnval = dest; // Memset is supposed to return the initial destination

  if(val == 0) // Someone called this insted of memset_zeroes directly
  {
    memset_zeroes(dest, numbytes);
    return returnval;
  }

  size_t offset = 0; // Offset size needs to match the size of a pointer

  while(numbytes)
  // Each memset has its own loop.
  {
    if(numbytes < 16) // 1-15 bytes (the other scalars would need to be memset anyways)
    {
      memset(dest, val, numbytes);
      offset = numbytes;
      dest = (char *)dest + offset;
      numbytes = 0;
    }
#ifdef __AVX512F__
    else if(numbytes < 32) // 16 bytes
    {
      memset_128bit_u(dest, _mm_set1_epi8((char)val), numbytes >> 4);
      offset = numbytes & -16;
      dest = (char *)dest + offset;
      numbytes &= 15;
    }
    else if(numbytes < 64) // 32 bytes
    {
      memset_256bit_u(dest, _mm256_set1_epi8((char)val), numbytes >> 5);
      offset = numbytes & -32;
      dest = (char *)dest + offset;
      numbytes &= 31;
    }
    else // 64 bytes
    {
      memset_512bit_u(dest, _mm512_set1_epi8((char)val), numbytes >> 6);
      offset = numbytes & -64;
      dest = (char *)dest + offset;
      numbytes &= 63;
    }
#elif __AVX__
    else if(numbytes < 32) // 16 bytes
    {
      memset_128bit_u(dest, _mm_set1_epi8((char)val), numbytes >> 4);
      offset = numbytes & -16;
      dest = (char *)dest + offset;
      numbytes &= 15;
    }
    else // 32 bytes
    {
      memset_256bit_u(dest, _mm256_set1_epi8((char)val), numbytes >> 5);
      offset = numbytes & -32;
      dest = (char *)dest + offset;
      numbytes &= 31;
    }
#else // SSE2 only
    else // 16 bytes
    {
      memset_128bit_u(dest, _mm_set1_epi8((char)val), numbytes >> 4);
      offset = numbytes & -16;
      dest = (char *)dest + offset;
      numbytes &= 15;
    }
#endif
  }
  return returnval;
}

// Set arbitrarily large amounts of only zeroes
void * memset_zeroes(void *dest, size_t numbytes) // Worst-case scenario: 127 bytes
{
  void * returnval = dest; // Memset is supposed to return the initial destination
  size_t offset = 0;

  while(numbytes)
  // Each memset has its own loop.
  {
    if(numbytes < 2) // 1 byte
    {
      memset(dest, 0, numbytes);
      offset = numbytes & -1;
      dest = (char *)dest + offset;
      numbytes = 0;
    }
    else if(numbytes < 4) // 2 bytes
    {
      memset_16bit(dest, 0, numbytes >> 1);
      offset = numbytes & -2;
      dest = (char *)dest + offset;
      numbytes &= 1;
    }
    else if(numbytes < 8) // 4 bytes
    {
      memset_32bit(dest, 0, numbytes >> 2);
      offset = numbytes & -4;
      dest = (char *)dest + offset;
      numbytes &= 3;
    }
    else if(numbytes < 16) // 8 bytes
    {
      memset_64bit(dest, 0, numbytes >> 3);
      offset = numbytes & -8;
      dest = (char *)dest + offset;
      numbytes &= 7;
    }
#ifdef __AVX512F__
    else if(numbytes < 32) // 16 bytes
    {
      memset_128bit_u(dest, _mm_setzero_si128(), numbytes >> 4);
      offset = numbytes & -16;
      dest = (char *)dest + offset;
      numbytes &= 15;
    }
    else if(numbytes < 64) // 32 bytes
    {
      memset_256bit_u(dest, _mm256_setzero_si256(), numbytes >> 5);
      offset = numbytes & -32;
      dest = (char *)dest + offset;
      numbytes &= 31;
    }
    else // 64 bytes
    {
      memset_512bit_u(dest, _mm512_setzero_si512(), numbytes >> 6);
      offset = numbytes & -64;
      dest = (char *)dest + offset;
      numbytes &= 63;
    }
#elif __AVX__
    else if(numbytes < 32) // 16 bytes
    {
      memset_128bit_u(dest, _mm_setzero_si128(), numbytes >> 4);
      offset = numbytes & -16;
      dest = (char *)dest + offset;
      numbytes &= 15;
    }
    else // 32 bytes
    {
      memset_256bit_u(dest, _mm256_setzero_si256(), numbytes >> 5);
      offset = numbytes & -32;
      dest = (char *)dest + offset;
      numbytes &= 31;
    }
#else // SSE2 only
    else // 16 bytes
    {
      memset_128bit_u(dest, _mm_setzero_si128(), numbytes >> 4);
      offset = numbytes & -16;
      dest = (char *)dest + offset;
      numbytes &= 15;
    }
#endif
  }
  return returnval;
}

//-----------------------------------------------------------------------------
// Individual Functions:
//-----------------------------------------------------------------------------

// 16-bit (2 bytes at a time)
// Len is (# of total bytes/2), so it's "# of 16-bits"

void * memset_16bit(void *dest, const uint16_t val, size_t len)
{
  uint16_t *ptr = (uint16_t*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}

// 32-bit (4 bytes at a time - 1 pixel in a 32-bit linear frame buffer)
// Len is (# of total bytes/4), so it's "# of 32-bits"

void * memset_32bit(void *dest, const uint32_t val, size_t len)
{
  uint32_t *ptr = (uint32_t*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}

// 64-bit (8 bytes at a time - 2 pixels in a 32-bit linear frame buffer)
// Len is (# of total bytes/8), so it's "# of 64-bits"

void * memset_64bit(void *dest, const uint64_t val, size_t len)
{
  uint64_t *ptr = (uint64_t*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}

//-----------------------------------------------------------------------------
// SSE2 Unaligned:
//-----------------------------------------------------------------------------

// SSE2 (128-bit, 16 bytes at a time - 4 pixels in a 32-bit linear frame buffer)
// Len is (# of total bytes/16), so it's "# of 128-bits"

void * memset_128bit_u(void *dest, const __m128i_u val, size_t len)
{
  __m128i_u *ptr = (__m128i_u*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}

//-----------------------------------------------------------------------------
// AVX+ Unaligned:
//-----------------------------------------------------------------------------

// AVX (256-bit, 32 bytes at a time - 8 pixels in a 32-bit linear frame buffer)
// Len is (# of total bytes/32), so it's "# of 256-bits"
// Sandybridge and Ryzen and up, Haswell and up for better performance

#ifdef __AVX__
void * memset_256bit_u(void *dest, const __m256i_u val, size_t len)
{
  __m256i_u *ptr = (__m256i_u*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}
#endif

// AVX-512 (512-bit, 64 bytes at a time - 16 pixels in a 32-bit linear frame buffer)
// Len is (# of total bytes/64), so it's "# of 512-bits"
// Requires AVX512F

#ifdef __AVX512F__
void * memset_512bit_u(void *dest, const __m512i_u val, size_t len)
{
  __m512i_u *ptr = (__m512i_u*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}
#endif

//-----------------------------------------------------------------------------
// SSE2 Aligned:
//-----------------------------------------------------------------------------

// SSE2 (128-bit, 16 bytes at a time - 4 pixels in a 32-bit linear frame buffer)
// Len is (# of total bytes/16), so it's "# of 128-bits"

void * memset_128bit_a(void *dest, const __m128i val, size_t len)
{
  __m128i *ptr = (__m128i*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}

//-----------------------------------------------------------------------------
// AVX+ Aligned:
//-----------------------------------------------------------------------------

// AVX (256-bit, 32 bytes at a time - 8 pixels in a 32-bit linear frame buffer)
// Len is (# of total bytes/32), so it's "# of 256-bits"
// Sandybridge and Ryzen and up

#ifdef __AVX__
void * memset_256bit_a(void *dest, const __m256i val, size_t len)
{
  __m256i *ptr = (__m256i*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}
#endif

// AVX-512 (512-bit, 64 bytes at a time - 16 pixels in a 32-bit linear frame buffer)
// Len is (# of total bytes/64), so it's "# of 512-bits"
// Requires AVX512F

#ifdef __AVX512F__
void * memset_512bit_a(void *dest, const __m512i val, size_t len)
{
  __m512i *ptr = (__m512i*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}
#endif

// AVX-1024+ support pending existence of the standard.
