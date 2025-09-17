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
#include "wasm-module.h"

/* -------------------- Type section -------------------- */

typedef struct wasm_type_section_meta
{
  htab_t type_pool;               /* Signature -> .wasm.type.N segment */
  unsigned int next_type_index;
} wasm_type_section_meta;

typedef struct wasm_type_segment_meta
{
  wasm_type_sig *sig;
} wasm_type_segment_meta;


/* -------------------- Function section -------------------- */

typedef struct wasm_function_section_meta { } wasm_function_section_meta;

typedef struct wasm_function_segment_meta
{
  wasm_section_tdata *type_segment;  /* Pointer to associated type segment */
} wasm_function_segment_meta;


/* -------------------- Code section -------------------- */

typedef struct wasm_code_section_meta
{
  unsigned int next_code_index;
} wasm_code_section_meta;

typedef struct wasm_code_segment_meta
{
  wasm_section_tdata *function_segment;
  bfd_size_type body_size;
} wasm_code_segment_meta;


/* -------------------- Import section -------------------- */

typedef struct wasm_extern_type
{
  union {
    wasm_type_segment_meta *type_segment;
    wasm_table_type table_type;
    wasm_memory_type memory_type;
    wasm_global_type global_type;    
  } e;
  int kind;
} wasm_extern_type;

typedef struct wasm_import_section_meta { } wasm_import_section_meta;

typedef struct wasm_import_segment_meta
{
  const char *import_name;
  wasm_extern_type ext;               /* external import meta, dependant on kind */
} wasm_import_segment_meta;


/* -------------------- Table section -------------------- */

typedef struct wasm_table_section_meta { } wasm_table_section_meta;
typedef struct wasm_table_segment_meta
{
  wasm_table_type table_type;
} wasm_table_segment_meta;


/* -------------------- Memory section -------------------- */

typedef struct wasm_memory_section_meta { } wasm_memory_section_meta;
typedef struct wasm_memory_segment_meta
{
  wasm_memory_type memory_type;
} wasm_memory_segment_meta;


/* -------------------- Global section -------------------- */

typedef struct wasm_global_section_meta { } wasm_global_section_meta;

typedef struct wasm_global_segment_meta
{
  wasm_global_type global_type;
} wasm_global_segment_meta;


/* -------------------- Export section -------------------- */

typedef struct wasm_export_section_meta { } wasm_export_section_meta;

typedef struct wasm_export_segment_meta
{
  const char *export_name;
  uint8_t kind;                       /* function = 0x00, table = 0x01, etc. */
  wasm_section_tdata *ext;            /* export index as external index */
} wasm_export_segment_meta;


/* -------------------- Start section -------------------- */

typedef struct wasm_start_section_meta { } wasm_start_section_meta;

typedef struct wasm_start_segment_meta
{
  wasm_section_tdata *function_segment;
} wasm_start_segment_meta;


/* -------------------- Element section -------------------- */

typedef struct wasm_element_section_meta { } wasm_element_section_meta;

typedef struct wasm_element_segment_meta
{
  /* TODO: Add table index, offset expr, function refs if needed */
} wasm_element_segment_meta;


/* -------------------- Data section -------------------- */

typedef struct wasm_data_section_meta { } wasm_data_section_meta;

typedef struct wasm_data_segment_meta
{
  /* TODO: Add offset expr, segment size, memory index */
  unsigned int address;
} wasm_data_segment_meta;


/* -------------------- Data Count section -------------------- */

typedef struct wasm_datacount_section_meta { } wasm_datacount_section_meta;

typedef struct wasm_datacount_segment_meta
{
  /* TODO: Does anything even go here? */
} wasm_datacount_segment_meta;

/* -------------------- Custom section -------------------- */

typedef struct wasm_custom_section_meta { } wasm_custom_section_meta;

/* Symbols */

bool wasm_nsec_symbols_adjust (bfd *abfd);

/* Sections */

bool wasm_nsec_section_flatten (asection *asect);
bool wasm_nsec_section_reconstruct (asection *asect);
bool wasm_nsec_section_initialize (asection *asect);

/* Segments */

int wasm_nsec_subsec_parse_meta (asection *asect, bfd_byte *start, bfd_byte *end);
int wasm_nsec_subsec_serialize_meta (asection *asect, bfd_byte *start);
int wasm_nsec_subsec_meta_len (asection *asect);
int wasm_nsec_subsec_len (asection *asect);
bool wasm_nsec_subsec_initialize (asection *asect);

#endif /* _WASM_NSEC_H */
