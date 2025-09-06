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

/* The WebAssembly linking conventions define custom sections
   used for symbol tables, relocations, and other linking data.
   See: https://github.com/WebAssembly/tool-conventions/blob/main/Linking.md */

#ifndef _WASM_LINKING_H
#define _WASM_LINKING_H

bool wasm_write_linking_section (bfd *abfd);
bool wasm_read_linking_section (bfd *abfd);

#endif /* _WASM_LINKING_H */
