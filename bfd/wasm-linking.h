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

#ifndef _WASM_NSEC_H
#define _WASM_NSEC_H

#include "bfd.h"

/* Sections */

int wasm_nsec_section_flatten (asection* asect);
int wasm_nsec_section_reconstruct (asection* asect);

/* Segments */

int wasm_nsec_subsec_parse_meta (asection *asect, bfd_byte *start, bfd_byte *end);
int wasm_nsec_subsec_serialize_meta (asection *asect, bfd_byte *start);
int wasm_nsec_subsec_meta_len (asection *asect);

#endif /* _WASM_NSEC_H */
