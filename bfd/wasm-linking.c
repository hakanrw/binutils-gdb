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

/* The WebAssembly linking conventions define custom sections
   used for symbol tables, relocations, and other linking data.
   See: https://github.com/WebAssembly/tool-conventions/blob/main/Linking.md */

#include "sysdep.h"
#include "bfd.h"
#include "libiberty.h"
#include "libbfd.h"

#include "wasm-linking.h"
#include "wasm-common.h"
#include "wasm-module.h"

#include <assert.h> /* FIXME: remove */

#include "bfd.h"

#define WASM_LINKING_VERSION 2

#define WASM_SEGMENT_INFO    5
#define WASM_INIT_FUNCS      6
#define WASM_COMDAT_INFO     7
#define WASM_SYMBOL_TABLE    8

#define WASM_SEGMENT_FLAG_STRINGS 1
#define WASM_SEGMENT_FLAG_TLS     2
#define WASM_SEGMENT_FLAG_RETAIN  4

#define WASM_COMDAT_DATA     0
#define WASM_COMDAT_FUNCTION 1
#define WASM_COMDAT_GLOBAL   2
#define WASM_COMDAT_EVENT    3
#define WASM_COMDAT_TABLE    4
#define WASM_COMDAT_SECTION  5

#define WASM_SYM_BINDING_WEAK      0x1
#define WASM_SYM_BINDING_LOCAL     0x2
#define WASM_SYM_VISIBILITY_HIDDEN 0x4
#define WASM_SYM_UNDEFINED         0x10
#define WASM_SYM_EXPORTED          0x20
#define WASM_SYM_EXPLICIT_NAME     0x40
#define WASM_SYM_NO_STRIP          0x80
#define WASM_SYM_TLS               0x100
#define WASM_SYM_ABSOLUTE          0x200

static bool
wasm_compute_symtab (bfd *abfd, bfd_size_type *size)
{
  int count = bfd_get_symcount (abfd);
  int realcount = 0;
  bfd_size_type len = 0;
  asymbol **table;
  int i;

  if (! count)
    {
      *size = 0;
      return true;
    }
  
  table = bfd_get_outsymbols (abfd);

  for (i = 0; i < count; i++)
    {
      asymbol *s = table[i];

      if (! bfd_is_local_label (abfd, s)
	  && (s->flags & BSF_DEBUGGING) == 0
	  && s->section != NULL
	  && s->section->output_section != NULL)
	{
	  wasm_symbol_type *ws = wasmsymbol (s);
	  unsigned int namelen = strlen (s->name);
	  unsigned int kind = ws->kind;

	  realcount++;
	  len += 1; /* syminfo kind, uint8 */
	  len += wasm_sizeof_uleb128 (0); /* syminfo flags, varuint32 */

	  if (kind == WASM_SYMTAB_DATA)
	    {
	      len += wasm_sizeof_uleb128 (namelen);
	      len += namelen;
	      len += wasm_sizeof_uleb128 (ws->index);
	      len += wasm_sizeof_uleb128 (s->value);
	      len += wasm_sizeof_uleb128 (ws->size);
	    }
	  else if (kind == WASM_SYMTAB_SECTION)
	    {
	      len += wasm_sizeof_uleb128 (0); /* FIXME */
	    }
	  else if (kind <= WASM_SYMTAB_TABLE)
	    {
	      len += wasm_sizeof_uleb128 (wasm_section_data (s->section)->index);
	      len += wasm_sizeof_uleb128 (namelen);
	      len += namelen;
	    }
	  else
	    {
	      assert (false);
	    }
	}
    }
 
  len += wasm_sizeof_uleb128 (realcount);
  *size = len;
  return true;
}

static bool
wasm_write_symtab (bfd *abfd, bfd_byte *content, bfd_size_type *size)
{
  int count = bfd_get_symcount (abfd);
  int realcount = 0;
  bfd_size_type pos = 0;
  asymbol **table;
  int i;

  table = bfd_get_outsymbols (abfd);

  for (i = 0; i < count; i++)
    {
      asymbol *s = table[i];

      if (! bfd_is_local_label (abfd, s)
          && (s->flags & BSF_DEBUGGING) == 0
          && s->section != NULL
          && s->section->output_section != NULL)
	{
	  realcount++;
	}
    }
  
  pos += wasm_write_uleb128_buf (content + pos, realcount);

  for (i = 0; i < count; i++)
    {
      asymbol *s = table[i];
      printf ("%s in %p\n",  s->name ? s->name : "<unknown>", s->section->output_section);
      
      if (! bfd_is_local_label (abfd, s)
          && (s->flags & BSF_DEBUGGING) == 0
          && s->section != NULL
          && s->section->output_section != NULL)
	{
	  wasm_symbol_type *ws = wasmsymbol (s);

	  unsigned int kind = ws->kind;
	  unsigned int namelen = strlen (s->name);

	  content[pos++] = kind;
	  pos += wasm_write_uleb128_buf (content + pos, 0); /* FIXME: FLAGS */

	  if (kind == WASM_SYMTAB_DATA)
	    {
	      pos += wasm_write_uleb128_buf (content + pos, namelen);
	      memmove (content + pos, s->name, namelen);
	      pos += namelen;
	      pos += wasm_write_uleb128_buf (content + pos, ws->index);
	      pos += wasm_write_uleb128_buf (content + pos, s->value);
	      pos += wasm_write_uleb128_buf (content + pos, ws->size);
	    }
	  else if (kind == WASM_SYMTAB_SECTION)
	    {
	      pos += wasm_write_uleb128_buf (content + pos, 0); /* FIXME */
	    }
	  else if (kind <= WASM_SYMTAB_TABLE)
	    {
	      pos += wasm_write_uleb128_buf (content + pos, wasm_section_data (s->section)->index);
	      pos += wasm_write_uleb128_buf (content + pos, namelen);
	      memmove (content + pos, s->name, namelen);
	      pos += namelen;
	    }
	  else
	    {
	      assert (false);
	      return false;
	    }
	}
    }

  *size = pos;
  return true;
}

bool
wasm_write_linking_section (bfd* abfd)
{
  asection *alink;
  bfd_byte *symbuf;
  bfd_size_type symtablen, writelen;
  bfd_size_type pos;

  printf ("create wasm linking, output_has_begun: %d\n", abfd->output_has_begun);
  alink = bfd_make_section_with_flags (abfd, WASM_LINKING_SECTION, SEC_HAS_CONTENTS);
  assert (alink);

  wasm_compute_symtab (abfd, &symtablen);
  if (symtablen)
    {
      symbuf = bfd_malloc (symtablen);
      wasm_write_symtab (abfd, symbuf, &writelen);
      BFD_ASSERT (writelen == symtablen);      
    }
  
  alink->size = wasm_sizeof_uleb128 (WASM_LINKING_VERSION);

  if (symtablen)
    alink->size += 1 /* WASM_SYMBOL_TABLE marker */
      + wasm_sizeof_uleb128 (symtablen)
      + symtablen;

  /* Start emission */

  pos = 0;
  unsigned char stub = 0;
  if (! bfd_set_section_contents (abfd, alink, &stub, 0, sizeof (stub))) /* FIXME: allocation hack */
    assert (false);
  assert (alink->contents);

  pos += wasm_write_uleb128_buf (alink->contents + pos, WASM_LINKING_VERSION);

  if (symtablen)
    {
      alink->contents[pos++] = WASM_SYMBOL_TABLE;
      pos += wasm_write_uleb128_buf (alink->contents + pos, symtablen);
      memmove (alink->contents + pos, symbuf, symtablen);
      pos += symtablen;

      bfd_realloc_or_free (symbuf, 0);
    }

  if (alink->size != pos)
    {
      /* Fatal serialization mismatch */
      assert (false);
      return false;
    }

  return true;
}

static bfd_size_type
wasm_read_symtab (bfd *abfd, bfd_byte *start, bfd_byte *end)
{
  asection *datasec = bfd_wasm_get_section_by_number (abfd, WASM_SEC_DATA);
  wasm_symbol_type *symbols = NULL;
  bfd_byte *cursor = start;
  bfd_vma total;
  unsigned int ulen;
  bfd_size_type consumed = 0;
  bfd_size_type amt;
  int i;

  /* First ULEB: number of symbols in the subsection.  */
  ulen = wasm_read_uleb128_buf (cursor, end, &total);
  if (ulen == 0)
    {
      _bfd_error_handler (_("%pB: malformed symbol table (missing count)"),
                          abfd);
      return 0;
    }
  cursor += ulen;

  amt = (bfd_size_type) total * sizeof (wasm_symbol_type);
  if (total == 0)
    return 0;
  
  symbols = bfd_alloc (abfd, amt);
  for (i = 0; i < (int) total; i++)
    {
      bfd_vma flags;
      unsigned int kind;
      wasm_symbol_type *wsym;
      asymbol *sym;
      int objtype;
      wsym = &symbols[i];
      sym = &wsym->symbol;
      sym->the_bfd = abfd;

      if (cursor >= end)
	{
	  _bfd_error_handler (_("%pB: symbol table ended prematurely"),
			      abfd);
	  return (bfd_size_type) (cursor - start);
	}

      kind = *cursor++;
      wsym->kind = kind;
      ulen = wasm_read_uleb128_buf (cursor, end, &flags);
      if (ulen == 0)
	return consumed;
      cursor += ulen;

      if (kind == WASM_SYMTAB_DATA)
	{
	  char *name;
	  bfd_vma namelen, idx, offset, size;

	  /* name */
	  ulen = wasm_read_uleb128_buf (cursor, end, &namelen);
	  if (ulen == 0 || cursor + ulen + namelen > end)
	    return 0;
	  name = bfd_alloc (abfd, namelen + 1);
	  if (! name)
	    return 0;
	  cursor += ulen;
	  memcpy (name, cursor, namelen);
	  name[namelen] = 0;
	  cursor += namelen;

	  /* section idx, value, size */
	  ulen = wasm_read_uleb128_buf (cursor, end, &idx);
	  if (ulen == 0) return 0;
	  cursor += ulen;

	  ulen = wasm_read_uleb128_buf (cursor, end, &offset);
	  if (ulen == 0) return 0;
	  cursor += ulen;

	  ulen = wasm_read_uleb128_buf (cursor, end, &size);
	  if (ulen == 0) return 0;
	  cursor += ulen;
	  printf ("newsym %s idx %ld size %ld\n", name, idx, size);
	  
	  sym->name = name;
	  sym->value = offset;
	  sym->flags = BSF_GLOBAL;
	  sym->section = datasec;
	  wsym->size = size;
	  wsym->index = idx;
	  sym->udata.p = NULL;
	}
      else
	{
	  _bfd_error_handler (_("%pB: unknown WASM symbol kind %u"),
			      abfd, kind);
	  return 0;
	}

      objtype = bfd_wasm_symtype_to_sectype (kind); 

      if (objtype != -1
	  && ! bfd_wasm_get_segment_by_index (abfd, objtype, wsym->index))
	{
	  _bfd_error_handler (_("%pB: malformed symbol %s (object index %d"
				" does not exist for type %d)"),
			      abfd, sym->name, wsym->index, objtype);
	  return 0;	  
	}
    }

  wasmdata (abfd)->symbols = symbols;
  wasmdata (abfd)->symcount = (unsigned int) total;
  abfd->symcount = (unsigned int) total;
  printf ("total: %d\n", abfd->symcount);
  abfd->flags |= HAS_SYMS;
  
  consumed = (bfd_size_type) (cursor - start);
  return consumed;
}

bool
wasm_read_linking_section (bfd* abfd)
{
  asection *alink;
  bfd_byte *cursor, *end;
  bfd_vma version;
  unsigned int ulen;
  
  alink = bfd_get_section_by_name (abfd, WASM_LINKING_SECTION);
  if (! alink)
    return true;

  cursor = alink->contents;
  end = alink->contents + alink->size;
  ulen = wasm_read_uleb128_buf (cursor, end, &version);
  cursor += ulen;
  
  if (! ulen)
    {
      _bfd_error_handler (_("%pB: failed to obtain ULEB128 version information"
			    " for the linking section '%s'"),
			  abfd,
			  alink->name ? alink->name : "<unknown>");
      return false;
    }

  if (version != WASM_LINKING_VERSION)
    {
      _bfd_error_handler (_("%pB: unsupported linking convention version"
			    " for the linking section '%s' (expected %d, got %ld)"),
			  abfd,
			  alink->name ? alink->name : "<unknown>",
			  WASM_LINKING_VERSION, (long)(version));
      return false;      
    }

  while (cursor < end)
    {
      unsigned int type;
      bfd_size_type size, subsecsize;
      type = *cursor;
      cursor++;
      ulen = wasm_read_uleb128_buf (cursor, end, &size);
      if (! ulen)
	{
	  _bfd_error_handler (_("%pB: linking section '%s' ended prematurely while"
				" reading subsection length (subsection type %d)"),
			      abfd,
			      alink->name ? alink->name : "<unknown>",
			      type);
	  return false;      
	  
	}
      cursor += ulen;
      if (type == WASM_SYMBOL_TABLE)
	{
	  subsecsize = wasm_read_symtab (abfd, cursor, end);
	}
      else
	{
	  _bfd_error_handler (_("%pB: unknown subsection type %d in linking"
				" section '%s' (at offset %ld)"),
			      abfd, type,
			      alink->name ? alink->name : "<unknown>",
			      (long)(cursor - alink->contents - ulen - 1));
	  return false;
	}

      if (subsecsize == 0)
	{
	  _bfd_error_handler (_("%pB: parsing subsection of type %d in linking"
				" section '%s' failed"),
			      abfd, type,
			      alink->name ? alink->name : "<unknown>");
	  return false;
	}
      else if (subsecsize != size)
	{
	  _bfd_error_handler (_("%pB: subsection of type %d in linking"
				" section '%s' ended prematurely"),
			      abfd, type,
			      alink->name ? alink->name : "<unknown>");	  
	  return false;
	}
      cursor += subsecsize;
    }

  return true;
}
