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

extern void obj_wasm_frob_symbol (symbolS *, int *);
extern void obj_wasm_frob_file (void);
extern void obj_wasm_frob_file_before_fix (void);
extern void obj_wasm_frob_section (segT);
extern void obj_wasm_section (int);
void wasm_fixup_section (bfd*, segT, void*);

#define obj_frob_file()              obj_wasm_frob_file ()
#define obj_frob_symbol(S,P)         obj_wasm_frob_symbol (S, & P)
#define obj_frob_section(S)          obj_wasm_frob_section (S)

#ifndef obj_begin
#define obj_begin() wasm_begin ()
#endif
extern void wasm_begin (void);

#ifndef obj_end
#define obj_end() wasm_end ()
#endif
extern void wasm_end (void);


extern void wasm_pop_insert (void);
#ifndef obj_pop_insert
#define obj_pop_insert() wasm_pop_insert ()
#endif

void wasm_obj_read_begin_hook (void);
#ifndef obj_read_begin_hook
#define obj_read_begin_hook     wasm_obj_read_begin_hook
#endif

void wasm_obj_symbol_new_hook (symbolS *);
#ifndef obj_symbol_new_hook
#define obj_symbol_new_hook     wasm_obj_symbol_new_hook
#endif

/* Symbol table entry data type.  */

typedef struct nlist obj_symbol_type;	/* Symbol table entry.  */

/* Symbol table macros and constants */
