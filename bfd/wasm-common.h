/* BFD back-end for WebAssembly modules.
   Copyright (C) 2017-2025 Free Software Foundation, Inc.

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

#ifndef _WASM_COMMON_H
#define _WASM_COMMON_H

#include "bfd.h"
#include <limits.h>

/* ULEB128 helpers */

#define WASM_ULEB128_PAD_LEN 5

/* Read the LEB128 integer at P, saving it to X; at end of buffer,
   jump to error_return.  */
#define READ_LEB128(x, p, end)                                          \
  do                                                                    \
    {                                                                   \
      if ((p) >= (end))                                                 \
        goto error_return;                                              \
      (x) = _bfd_safe_read_leb128 (abfd, &(p), false, (end));           \
    }                                                                   \
  while (0)

bfd_vma wasm_read_leb128 (bfd *abfd,
                          bool *error_return,
                          unsigned int *length_return,
                          bool sign);

bool wasm_write_uleb128 (bfd *abfd, bfd_vma v);

unsigned int wasm_sizeof_uleb128 (bfd_vma value);

unsigned int wasm_write_uleb128_buf_min (void *buf, bfd_vma v,
					 unsigned int min);

unsigned int wasm_write_uleb128_buf_pad (void *buf, bfd_vma v);

unsigned int wasm_write_uleb128_buf (void *buf, bfd_vma v);

unsigned int wasm_read_uleb128_buf (void *start, void *limit /* exclusive */,
                                    bfd_vma *v);

size_t wasm_estimate_digit (unsigned int num);

#endif /* _WASM_COMMON_H */
