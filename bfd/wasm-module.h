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

#ifndef _WASM_MODULE_H
#define _WASM_MODULE_H

/* WebAssembly module file header.  Note that WASM_VERSION is a 32-bit
   little-endian integer, not an LEB128-encoded integer.  */
#define SIZEOF_WASM_MAGIC    4
#define WASM_MAGIC	     { 0x00, 'a', 's', 'm' }
#define SIZEOF_WASM_VERSION  4
#define WASM_VERSION	     { 0x01, 0x00, 0x00, 0x00 }
#define WASM_LINKING_VERSION 2

/* Prefix to use to form section names.  */
#define WASM_SECTION_PREFIX ".wasm."

/* NUMBER is currently unused, but is included for error checking purposes.  */
#define WASM_SECTION(number, name) (WASM_SECTION_PREFIX name)

/* Section names.  WASM_NAME_SECTION is the name of the named section
   named "name".  */
#define WASM_NAME_SECTION	   WASM_SECTION (0, "name")
#define WASM_RELOC_SECTION_PREFIX  WASM_SECTION (0, "reloc.")
#define WASM_LINKING_SECTION	   WASM_SECTION (0, "linking")
#define WASM_DYLINK_SECTION	   WASM_SECTION (0, "dylink")

/* Subsection indices.  Right now, that's subsections of the "name"
   section only.  */
#define WASM_FUNCTION_SUBSECTION 1 /* Function names.  */
#define WASM_LOCALS_SUBSECTION   2 /* Names of locals by function.  */

/* The section to report wasm symbols in.  */
#define WASM_SECTION_FUNCTION_INDEX ".space.function_index"

/* Numbered section name table */
//FIXME: extern const char * const wasm_numbered_sections[];
//FIXME: extern const size_t wasm_numbered_sections_size;

/* Numbered section amount */
#define WASM_NUMBERED_SECTIONS 13 /* FIXME: wasm_numbered_sections_size */

typedef struct wasm_symbol_type
{
  /* The actual symbol which the rest of BFD works with.  */
  asymbol symbol;

  /* Wasm symbol fields.  */
  unsigned int index;
  unsigned int kind;
  unsigned int size;
} wasm_symbol_type;

/* We take the address of the first element of an asymbol to ensure that the
   macro is only ever applied to an asymbol.  */
#define wasmsymbol(asymbol) ((wasm_symbol_type *) (&((asymbol)->the_bfd)))

typedef struct wasm_section_tdata
{
  asection *     section;
  struct wasm_section_tdata *  parent;
  struct wasm_section_tdata *  sibling_next;
  struct wasm_section_tdata *  children_head;
  struct wasm_section_tdata *  children_tail;
  bfd_size_type subsec_count;
  unsigned int offset;
  unsigned int index;
} wasm_section_tdata;

/* An accessor macro for the ecoff_section_tdata structure.  */
#define wasm_section_data(sec) \
  ((wasm_section_tdata *) (sec)->used_by_bfd)

/* Check if an asection is a parent (section) */
#define wasm_is_parent(sec) \
  (wasm_section_data(sec)->parent == NULL)

/* Check if an asection is a segment (subsection) */
#define wasm_is_segment(sec) \
  (wasm_section_data(sec)->parent != NULL)

/* Numbered section name mapping helpers */
const char * wasm_section_code_to_name (bfd_byte section_code);
unsigned int wasm_section_name_to_code (const char *name);
void wasm_flatten_subsections (bfd *abfd);

#ifndef wasm_close_and_cleanup
#define wasm_close_and_cleanup              _bfd_generic_close_and_cleanup
#endif

#ifndef wasm_bfd_free_cached_info
#define wasm_bfd_free_cached_info           _bfd_generic_bfd_free_cached_info
#endif

#ifndef wasm_get_section_contents
#define wasm_get_section_contents           _bfd_generic_get_section_contents
#endif

#ifndef wasm_get_section_contents_in_window
#define wasm_get_section_contents_in_window _bfd_generic_get_section_contents_in_window
#endif

#endif /* _WASM_MODULE_H */
