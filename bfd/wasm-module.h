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

/* Numbered section amount */
#define WASM_NUMBERED_SECTIONS 13

/* Numbered sections */
#define WASM_SEC_CUSTOM        0
#define WASM_SEC_TYPE          1
#define WASM_SEC_IMPORT        2
#define WASM_SEC_FUNCTION      3
#define WASM_SEC_TABLE         4
#define WASM_SEC_MEMORY        5
#define WASM_SEC_GLOBAL        6
#define WASM_SEC_EXPORT        7
#define WASM_SEC_START         8
#define WASM_SEC_ELEMENT       9
#define WASM_SEC_CODE          10
#define WASM_SEC_DATA          11
#define WASM_SEC_DATACOUNT     12

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

/* Backend types */
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

/* Backend specific data */
typedef struct wasm_tdata_type
{
  asymbol *      symbols;
  bfd_size_type  symcount;
  asection *     numbered_sections[WASM_NUMBERED_SECTIONS];
} wasm_tdata_type;

/* Macro to access backend data */
#define wasmdata(abfd) ((wasm_tdata_type *) ((abfd)->tdata.any))

/* Section specific data */
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
  unsigned int type;
  void * meta;
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

/* Get the type of an asection or segment */
#define wasm_section_type(sec) \
  (wasm_section_data(sec)->type)

/* Function signature type */
typedef struct wasm_type_sig
{
  size_t nparams;
  size_t nresults;
  uint8_t *params;
  uint8_t *results;
} wasm_type_sig;

/* WebAssembly types */
typedef struct wasm_ref_type
{
  /* No idea wtf this is */
} wasm_reference_type;

typedef struct wasm_limits_type
{
  unsigned int at, lim;
} wasm_limits_type;

typedef struct wasm_global_type
{
  unsigned int valtype;
  bool mut;
} wasm_global_type;

typedef struct wasm_memory_type
{
  wasm_limits_type limits;
} wasm_memory_type;

typedef struct wasm_table_type
{
  wasm_reference_type ref;
  wasm_limits_type limits;
} wasm_table_type;

/* External types */
#define WASM_NUMBERED_EXTERNS 4
#define WASM_EXTERN_FUNCTION  0
#define WASM_EXTERN_TABLE     1
#define WASM_EXTERN_MEMORY    2
#define WASM_EXTERN_GLOBAL    3

/* Check if an asection is an import */
#define wasm_is_import(sec) \
  (wasm_is_segment(sec) \
   && wasm_section_data(sec)->parent->type == WASM_SEC_IMPORT)

/* Backend API */
asection * bfd_wasm_get_section_by_number (bfd *abfd, int number);
const char * bfd_wasm_section_code_to_name (bfd_byte section_code);
unsigned int bfd_wasm_section_name_to_code (const char *name);
asection * bfd_wasm_make_empty_segment (bfd *abfd, asection *parent);
asection * bfd_wasm_get_segment_by_index (bfd *abfd, int sectype, int idx);
asection * bfd_wasm_get_segment_by_local_index (bfd *abfd, int sectype, int idx);
int bfd_wasm_index_of (asection *segment);
int bfd_wasm_local_index_of (asection *segment);

/* Import specific */
int bfd_wasm_externtype_to_sectype (unsigned int externtype);
int bfd_wasm_sectype_to_externtype (unsigned int sectype);
asection * bfd_wasm_make_import_segment (bfd *abfd, const char *modname,
					 const char *name, int externtype);
const char * bfd_wasm_get_import_modname (asection *importsec);
bool bfd_wasm_set_import_modname (asection *importsec, const char *name);
const char * bfd_wasm_get_import_name (asection *importsec);
bool bfd_wasm_set_import_name (asection *importsec, const char *name);
int bfd_wasm_get_import_type (asection *importsec);
bool bfd_wasm_set_import_type (asection *importsec, int externtype);

#endif /* _WASM_MODULE_H */
