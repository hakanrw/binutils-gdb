/* obj-wasm.h, WASM object file format for gas, the assembler.
   Copyright (C) 1989-2025 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* Tag to validate WASM object file format processing */
#define OBJ_WASM 1

#include "targ-cpu.h"

#define OUTPUT_FLAVOR bfd_target_unknown_flavour

extern void obj_wasm_section (int);

extern void wasm_pop_insert (void);
#ifndef obj_pop_insert
#define obj_pop_insert() wasm_pop_insert ()
#endif

/* Symbol table entry data type.  */

typedef struct nlist obj_symbol_type;	/* Symbol table entry.  */

/* Symbol table macros and constants */

#define obj_read_begin_hook()   {;}
#define obj_symbol_new_hook(s)  {;}
