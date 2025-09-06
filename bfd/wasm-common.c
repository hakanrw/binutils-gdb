/* BFD back-end for WebAssembly modules.
   Copyright (C) 2017-2025 Free Software Foundation, Inc.

   Based on srec.c, cofflink.c, mmo.c, and binary.c

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

/* The WebAssembly module format is a simple object file format
   including up to 12 numbered sections, plus any number of named
   "custom" sections. It is described at:
   https://github.com/WebAssembly/design/blob/master/BinaryEncoding.md. */

#include "sysdep.h"
#include "wasm-common.h"

/* WebAssembly LEB128 integers are sufficiently like DWARF LEB128
   integers that we use _bfd_safe_read_leb128, but there are two
   points of difference:

   - WebAssembly requires a 32-bit value to be encoded in at most 5
     bytes, etc.
   - _bfd_safe_read_leb128 accepts incomplete LEB128 encodings at the
     end of the buffer, while these are invalid in WebAssembly.

   Those differences mean that we will accept some files that are
   invalid WebAssembly.  */

/* Read an LEB128-encoded integer from ABFD's I/O stream, reading one
   byte at a time.  Set ERROR_RETURN if no complete integer could be
   read, LENGTH_RETURN to the number of bytes read (including bytes in
   incomplete numbers).  SIGN means interpret the number as SLEB128. */

bfd_vma
wasm_read_leb128 (bfd *abfd,
		  bool *error_return,
		  unsigned int *length_return,
		  bool sign)
{
  bfd_vma result = 0;
  unsigned int num_read = 0;
  unsigned int shift = 0;
  unsigned char byte = 0;
  unsigned char lost, mask;
  int status = 1;

  while (bfd_read (&byte, 1, abfd) == 1)
    {
      num_read++;

      if (shift < CHAR_BIT * sizeof (result))
	{
	  result |= ((bfd_vma) (byte & 0x7f)) << shift;
	  /* These bits overflowed.  */
	  lost = byte ^ (result >> shift);
	  /* And this is the mask of possible overflow bits.  */
	  mask = 0x7f ^ ((bfd_vma) 0x7f << shift >> shift);
	  shift += 7;
	}
      else
	{
	  lost = byte;
	  mask = 0x7f;
	}
      if ((lost & mask) != (sign && (bfd_signed_vma) result < 0 ? mask : 0))
	status |= 2;

      if ((byte & 0x80) == 0)
	{
	  status &= ~1;
	  if (sign && shift < CHAR_BIT * sizeof (result) && (byte & 0x40))
	    result |= -((bfd_vma) 1 << shift);
	  break;
	}
    }

  if (length_return != NULL)
    *length_return = num_read;
  if (error_return != NULL)
    *error_return = status != 0;

  return result;
}

/* Encode an integer V as LEB128 and write it to ABFD, return TRUE on
   success.  */

bool
wasm_write_uleb128 (bfd *abfd, bfd_vma v)
{
  do
    {
      bfd_byte c = v & 0x7f;
      v >>= 7;

      if (v)
	c |= 0x80;

      if (bfd_write (&c, 1, abfd) != 1)
	return false;
    }
  while (v);

  return true;
}

unsigned int
wasm_write_uleb128_buf_min (void *buf, bfd_vma v, unsigned int min)
{
  unsigned int count = 0;
  do
    {
      bfd_byte c = v & 0x7f;
      v >>= 7;

      if (v || count + 1 < min)
        c |= 0x80;

      *((char*)buf++) = c;
      count++;
    }
  while (v || count < min);

  return count;
}

unsigned int
wasm_write_uleb128_buf_pad (void *buf, bfd_vma v)
{
  return wasm_write_uleb128_buf_min (buf, v, WASM_ULEB128_PAD_LEN);
}

unsigned int
wasm_write_uleb128_buf (void *buf, bfd_vma v)
{
  return wasm_write_uleb128_buf_min (buf, v, 1);
}

unsigned int
wasm_read_uleb128_buf (void *start, void *limit /* exclusive */, bfd_vma *v)
{
  char *p = (char *) start;
  char *end = (char *) limit;

  bfd_vma result = 0;
  unsigned int shift = 0;
  unsigned int count = 0;

  while (p < end)
    {
      uint8_t byte = *p++;
      count++;

      result |= ((bfd_vma)(byte & 0x7F)) << shift;
      shift += 7;

      if ((byte & 0x80) == 0)
        {
          *v = result;
          return count;
        }
    }

  /* Buffer ran out before we reached the end of the LEB128. */
  return 0;
}


/* Get variable uleb size from value */

unsigned int
wasm_sizeof_uleb128 (bfd_vma value)
{
  int size = 0;

  do
    {
      value >>= 7;
      size += 1;
    }
  while (value != 0);

  return size;
}

size_t
wasm_estimate_digit (unsigned int num)
{
  size_t digit = 0;
  if (num == 0)
    return 1;

  for (digit = 0; num ; num /= 10)
    digit++;

  return digit;
}
