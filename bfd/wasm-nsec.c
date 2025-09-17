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
#include "bfd.h"
#include "libiberty.h"
#include "libbfd.h"

#include "wasm-nsec.h"
#include "wasm-module.h"
#include "wasm-common.h"

struct wasm_section_ops
{
  bool (*flatten)(asection *sec);
  bool (*reconstruct)(asection *sec);
  bool (*initialize)(asection *sec);
};

struct wasm_segment_meta_ops
{
  int (*parse)(asection *sec, bfd_byte *start, bfd_byte *end);
  int (*serialize)(asection *sec, bfd_byte *start);
  int (*length)(asection *sec);
  bool (*initialize)(asection *sec);
};

#define STR(x) #x

#define DECLARE_WASM_SECTION_OPS_META_CONTENTS(INDEX,NAME)	\
  [INDEX] = { \
    .flatten = wasm_nsec_section_generic_flatten_meta_and_contents, \
    .reconstruct = wasm_nsec_section_generic_reconstruct_meta_and_contents, \
    .initialize = wasm_nsec_section_##NAME##_initialize \
  }

#define DECLARE_WASM_SECTION_OPS_META(INDEX,NAME)	\
  [INDEX] = { \
    .flatten = wasm_nsec_section_generic_flatten_meta, \
    .reconstruct = wasm_nsec_section_generic_reconstruct_meta, \
    .initialize = wasm_nsec_section_##NAME##_initialize \
  }

#define DECLARE_WASM_SECTION_OPS(INDEX,NAME)	\
  [INDEX] = { \
    .flatten = wasm_nsec_section_##NAME##_flatten, \
    .reconstruct = wasm_nsec_section_##NAME##_reconstruct, \
    .initialize = wasm_nsec_section_##NAME##_initialize \
  }

#define DECLARE_WASM_SUBSEC_OPS_DEFAULT(INDEX,NAME)	\
  [INDEX] = { \
    .parse = wasm_nsec_subsec_generic_parse_meta, \
    .serialize = wasm_nsec_subsec_generic_serialize_meta, \
    .length = wasm_nsec_subsec_generic_meta_len, \
    .initialize = wasm_nsec_subsec_##NAME##_initialize \
  }

#define DECLARE_WASM_SUBSEC_OPS(INDEX,NAME)	\
  [INDEX] = { \
    .parse = wasm_nsec_subsec_##NAME##_parse_meta, \
    .serialize = wasm_nsec_subsec_##NAME##_serialize_meta, \
    .length = wasm_nsec_subsec_##NAME##_meta_len, \
    .initialize = wasm_nsec_subsec_##NAME##_initialize \
  }

#define DEFINE_WASM_SUBSEC_INITIALIZER(NAME)                           \
  static bool wasm_nsec_subsec_##NAME##_initialize(asection *sec)      \
  {                                                                    \
    wasm_##NAME##_segment_meta *meta =                                 \
      (wasm_##NAME##_segment_meta *) bfd_zalloc (sec->owner, sizeof (*meta));  \
    wasm_section_data (sec)->meta = meta;                              \
    printf ("%s size: %ld\n", STR(wasm_nsec_subsec_##NAME##_initialize), sizeof (*meta)); \
    return true;                                                       \
  }

#define DEFINE_WASM_SECTION_INITIALIZER(NAME)                          \
  static bool wasm_nsec_section_##NAME##_initialize(asection *sec)     \
  {                                                                    \
    wasm_##NAME##_section_meta *meta =                                 \
      (wasm_##NAME##_section_meta *) bfd_zalloc (sec->owner, sizeof (*meta));   \
    wasm_section_data (sec)->meta = meta;                              \
    printf ("%s size: %ld\n", STR(wasm_nsec_section_##NAME##_initialize), sizeof (*meta)); \
    return true;                                                       \
  }

#define ADVANCE(x) \
  do						\
    {						\
      unsigned int tmp = x;			\
      BFD_ASSERT (tmp > 0);			\
      if (tmp == 0) return -1;			\
      else cursor += tmp;			\
    } while (0);

#define CHECK(x) \
  do							\
    {							\
      unsigned int tmp = x;				\
      BFD_ASSERT (tmp > 0);				\
      if (tmp == 0)					\
	return -1;					\
    } while (0);

#define ENSURE(x) \
  do							\
    {							\
      unsigned int tmp = x;				\
      if ((uintptr_t)end - (uintptr_t)cursor < tmp)	\
	{						\
	  BFD_FAIL ();					\
	  return -1;					\
	}						\
    } while (0);

#define OFFSET() \
  (uintptr_t)cursor - (uintptr_t)start;


/* ---------------------- Segments ---------------------- */

static int
wasm_nsec_subsec_type_parse_meta (asection *sec ATTRIBUTE_UNUSED, bfd_byte *start ATTRIBUTE_UNUSED, bfd_byte *end ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_subsec_type_serialize_meta (asection *sec ATTRIBUTE_UNUSED, bfd_byte *start ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_subsec_type_meta_len (asection *sec ATTRIBUTE_UNUSED)
{
  return 0;
}

static unsigned int
wasm_nsec_externtype_parse (bfd *abfd, bfd_byte *start, bfd_byte *end, wasm_extern_type *externtype)
{
  bfd_byte *cursor = start;
  ENSURE (1);
  externtype->kind = *cursor;

  switch (externtype->kind)
    {
    case 0: /* func */
      asection *type_seg;
      bfd_vma idx;
      ADVANCE (wasm_read_uleb128_buf (cursor, end, &idx));
      type_seg = bfd_wasm_get_segment_by_local_index (abfd, WASM_SEC_TYPE, idx);
      BFD_ASSERT (type_seg);
      break;
    case 1: /* table */
      ADVANCE (wasm_read_table_type (cursor, end, &externtype->e.table_type));
      break;
    case 2: /* mem */
      ADVANCE (wasm_read_memory_type (cursor, end, &externtype->e.memory_type));
      break;
    case 3: /* global */
      ADVANCE (wasm_read_global_type (cursor, end, &externtype->e.global_type));
      break;
    default:
      BFD_FAIL ();
      return -1;
    }
  
  return OFFSET ();
}

static unsigned int
wasm_nsec_externtype_serialize (bfd_byte *start ATTRIBUTE_UNUSED, wasm_extern_type *externtype ATTRIBUTE_UNUSED)
{
  bfd_byte *cursor = start;
  
  switch (externtype->kind)
    {
    case 0: /* func */
      bfd_vma idx = 0; /* FIXME */
      ADVANCE (wasm_write_uleb128_buf (cursor, idx));
      break;
    case 1: /* table */
      ADVANCE (wasm_write_table_type (cursor, externtype->e.table_type));
      break;
    case 2: /* mem */
      ADVANCE (wasm_write_memory_type (cursor, externtype->e.memory_type));
      break;
    case 3: /* global */
      ADVANCE (wasm_write_global_type (cursor, externtype->e.global_type));
      break;
    default:
      BFD_FAIL ();
      return -1;
    }
  
  return OFFSET ();

}

static unsigned int
wasm_nsec_externtype_len (wasm_extern_type *externtype ATTRIBUTE_UNUSED)
{
  unsigned int len = 0;
  switch (externtype->kind)
    {
    case 0: /* func */
      len += wasm_sizeof_uleb128 (0); /* FIXME */
      break;
    case 1: /* table */
      len += wasm_sizeof_table_type (externtype->e.table_type);
      break;
    case 2: /* mem */
      len += wasm_sizeof_memory_type (externtype->e.memory_type);
      break;
    case 3: /* global */
      len += wasm_sizeof_global_type (externtype->e.global_type);
      break;
    default:
      BFD_FAIL ();
      return -1;
    }
  
  return len;
}

static int
wasm_nsec_subsec_import_parse_meta (asection *sec, bfd_byte *start, bfd_byte *end)
{
  wasm_import_segment_meta *meta =
    (wasm_import_segment_meta *)wasm_section_data (sec)->meta;
  bfd_byte *cursor = start;
  bfd_vma module_len, name_len;
  char *module, *name;
  CHECK (wasm_read_name_len (cursor, end, &module_len));
  module = bfd_alloc (sec->owner, module_len + 1);
  ADVANCE (wasm_read_name (cursor, end, module));
  meta->import_module = module;
  CHECK (wasm_read_name_len (cursor, end, &name_len));
  name = bfd_alloc (sec->owner, name_len + 1);
  ADVANCE (wasm_read_name (cursor, end, name));
  meta->import_name = name;
  ADVANCE (wasm_nsec_externtype_parse (sec->owner, cursor, end, &meta->ext));
  return OFFSET ();
}

static int
wasm_nsec_subsec_import_serialize_meta (asection *sec, bfd_byte *start)
{
  wasm_import_segment_meta *meta =
    (wasm_import_segment_meta *)wasm_section_data (sec)->meta;
  bfd_byte *cursor = start;
  ADVANCE (wasm_write_name (cursor, meta->import_module));
  ADVANCE (wasm_write_name (cursor, meta->import_name));
  ADVANCE (wasm_nsec_externtype_serialize (cursor, &meta->ext));
  return OFFSET ();
}

static int
wasm_nsec_subsec_import_meta_len (asection *sec)
{
  wasm_import_segment_meta *meta =
    (wasm_import_segment_meta *)wasm_section_data (sec)->meta;
  int len = 0;
  len += wasm_sizeof_name (meta->import_module);
  len += wasm_sizeof_name (meta->import_name);
  len += wasm_nsec_externtype_len (&meta->ext);
  return len;
}

static int
wasm_nsec_subsec_function_parse_meta (asection *sec ATTRIBUTE_UNUSED, bfd_byte *start ATTRIBUTE_UNUSED, bfd_byte *end ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_subsec_function_serialize_meta (asection *sec ATTRIBUTE_UNUSED, bfd_byte *start ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_subsec_function_meta_len (asection *sec ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_subsec_code_parse_meta (asection *sec ATTRIBUTE_UNUSED, bfd_byte *start ATTRIBUTE_UNUSED, bfd_byte *end ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_subsec_code_serialize_meta (asection *sec ATTRIBUTE_UNUSED, bfd_byte *start ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_subsec_code_meta_len (asection *sec ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_subsec_generic_parse_meta (asection *sec ATTRIBUTE_UNUSED, bfd_byte *start ATTRIBUTE_UNUSED, bfd_byte *end ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_subsec_generic_serialize_meta (asection *sec ATTRIBUTE_UNUSED, bfd_byte *start ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_subsec_generic_meta_len (asection *sec ATTRIBUTE_UNUSED)
{
  return 0;
}

DEFINE_WASM_SUBSEC_INITIALIZER(type)
DEFINE_WASM_SUBSEC_INITIALIZER(import)
DEFINE_WASM_SUBSEC_INITIALIZER(function)
DEFINE_WASM_SUBSEC_INITIALIZER(table)
DEFINE_WASM_SUBSEC_INITIALIZER(memory)
DEFINE_WASM_SUBSEC_INITIALIZER(global)
DEFINE_WASM_SUBSEC_INITIALIZER(export)
DEFINE_WASM_SUBSEC_INITIALIZER(start)
DEFINE_WASM_SUBSEC_INITIALIZER(element)
DEFINE_WASM_SUBSEC_INITIALIZER(code)
DEFINE_WASM_SUBSEC_INITIALIZER(data)
DEFINE_WASM_SUBSEC_INITIALIZER(datacount)

static const struct wasm_segment_meta_ops wasm_meta_ops[WASM_NUMBERED_SECTIONS] =
{
  DECLARE_WASM_SUBSEC_OPS         (WASM_SEC_TYPE,      type),
  DECLARE_WASM_SUBSEC_OPS         (WASM_SEC_IMPORT,    import),
  DECLARE_WASM_SUBSEC_OPS         (WASM_SEC_FUNCTION,  function),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT (WASM_SEC_TABLE,     table),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT (WASM_SEC_MEMORY,    memory),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT (WASM_SEC_GLOBAL,    global),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT (WASM_SEC_EXPORT,    export),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT (WASM_SEC_START,     start),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT (WASM_SEC_ELEMENT,   element),
  DECLARE_WASM_SUBSEC_OPS         (WASM_SEC_CODE,      code),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT (WASM_SEC_DATA,      data),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT (WASM_SEC_DATACOUNT, datacount)
};

int
wasm_nsec_subsec_parse_meta (asection *sec, bfd_byte *start, bfd_byte *end /* exclusive */)
{
  wasm_section_tdata *parent;
  int type;
  BFD_ASSERT (wasm_is_segment (sec));

  parent = wasm_section_data(sec)->parent;
  BFD_ASSERT (parent);

  type = parent->type;

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].parse)
    return -1; // or bfd_error, or something less panicky
  return wasm_meta_ops[type].parse(sec, start, end);
}

int
wasm_nsec_subsec_serialize_meta (asection *sec, bfd_byte *start)
{
  wasm_section_tdata *parent;
  int type;
  BFD_ASSERT (wasm_is_segment (sec));

  parent = wasm_section_data(sec)->parent;
  BFD_ASSERT (parent);

  type = parent->type;

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].serialize)
    return -1;
  return wasm_meta_ops[type].serialize(sec, start);
}

int
wasm_nsec_subsec_meta_len (asection *sec)
{
  wasm_section_tdata *parent;
  int type;
  BFD_ASSERT (wasm_is_segment (sec));

  parent = wasm_section_data(sec)->parent;
  BFD_ASSERT (parent);

  type = parent->type;

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].length)
    return -1;
  return wasm_meta_ops[type].length(sec);
}

bool
wasm_nsec_subsec_initialize (asection *sec)
{
  wasm_section_tdata *parent;
  int type;
  BFD_ASSERT (wasm_is_segment (sec));

  parent = wasm_section_data(sec)->parent;
  BFD_ASSERT (parent);

  type = parent->type;

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].length)
    return false;
  return wasm_meta_ops[type].initialize(sec);
}

int
wasm_nsec_subsec_len (asection *sec)
{
  int meta_len;
  BFD_ASSERT (wasm_is_segment (sec));

  meta_len = wasm_nsec_subsec_meta_len (sec);
  if (meta_len < 0)
    return -1;

  return sec->size + meta_len;
}

/* ---------------------- Sections ---------------------- */

static bool
wasm_nsec_section_datacount_flatten (asection *sec ATTRIBUTE_UNUSED)
{
  return true;
}

static bool
wasm_nsec_section_datacount_reconstruct (asection *sec ATTRIBUTE_UNUSED)
{
  return true;
}

static bool
wasm_nsec_section_custom_flatten (asection *sec ATTRIBUTE_UNUSED)
{
  /* This should never reach */
  BFD_ASSERT (false);
  return false;
}

static bool
wasm_nsec_section_custom_reconstruct (asection *sec ATTRIBUTE_UNUSED)
{
  /* This should never reach */
  BFD_ASSERT (false);
  return false;
}

/* Compute segments parent-children offset and size information */

static bool
wasm_nsec_flatten_compute_offsets (asection *asect, bool has_content)
{
  wasm_section_tdata *sdata = wasm_section_data (asect);
  wasm_section_tdata *curs = sdata->children_head;
  unsigned long pos = 0;

  BFD_ASSERT (!asect->contents); /* FIXME: Be empty, or face destruction */
  pos += wasm_sizeof_uleb128 (sdata->subsec_count); /* Uleb size padding. */
  
  for (; curs; curs = curs->sibling_next)
    {
      int metalen = wasm_nsec_subsec_meta_len (curs->section);
      if (metalen < 0)
	{
	  _bfd_error_handler (_("%pB: failed to obtain ULEB128 meta length"
				" for segment '%s' in section '%s' (offset %ld)"),
			      asect->owner,
			      curs->section->name ? curs->section->name : "<unknown>",
			      asect->name ? asect->name : "<unknown>",
			      (long)(pos));
	  return false;
	}
      
      curs->offset = pos;
      pos += metalen;
      if (has_content)
	{
	  pos += wasm_sizeof_uleb128 (curs->section->size);
	  pos += curs->section->size;
	}
      else
	BFD_ASSERT (curs->section->size == 0);

      printf ("offset of %s within %s: %d\n", curs->section->name, asect->name, curs->offset);
    }
  asect->size = pos;

  return true;
}

static bool
wasm_nsec_flatten_merge_contents (asection *asect, bool has_content)
{
  wasm_section_tdata *sdata = wasm_section_data (asect);
  bfd_byte *contents = (bfd_byte*) bfd_alloc (asect->owner, asect->size);
  wasm_section_tdata *curs = sdata->children_head;
  bfd_vma pos = 0;
  
  pos += wasm_write_uleb128_buf (contents, sdata->subsec_count);
  
  for (; curs; curs = curs->sibling_next)
    {
      int metalen;
      if (pos != curs->offset)
	{
	  BFD_ASSERT (false);
	  return false;
	}
      metalen = wasm_nsec_subsec_serialize_meta (curs->section, contents + pos);
      if (metalen < 0)
	{
	  _bfd_error_handler (_("%pB: failed to obtain ULEB128 meta length"
				" for segment '%s' in section '%s' (offset %ld)"),
			      asect->owner,
			      curs->section->name ? curs->section->name : "<unknown>",
			      asect->name ? asect->name : "<unknown>",
			      (long)(pos));
	  return false;
	}
      pos += metalen;
      if (pos + curs->section->size > asect->size)
	{
	  BFD_ASSERT (false);
	  return false;
	}
      if (has_content)
	{
	  pos += wasm_write_uleb128_buf (contents + pos, curs->section->size);
	  memmove (contents + pos, curs->section->contents, curs->section->size);
	  /* Repoint child content to parent slice for consistency */
	  curs->section->contents = contents + pos;
	  /* The invalidated previous buffer of the child could be freed here,
	     but BFD's abfd-tied bump allocator does not allow for it.
	     Thus, the duplicate data will live as junk until the abfd is
	     closed. */
	  pos += curs->section->size;
	}
      else
	{
	  BFD_ASSERT (! curs->section->contents && ! curs->section->alloced);
	}
    }

  BFD_ASSERT (pos == asect->size);

  asect->contents = contents;
  asect->alloced = true;
  BFD_ASSERT (bfd_set_section_flags (asect, SEC_HAS_CONTENTS | SEC_IN_MEMORY));

  return true;
}

static bool
wasm_nsec_flatten_adjust_relocs (asection *sec)
{
  wasm_section_tdata *sdata = wasm_section_data (sec);
  wasm_section_tdata *curs = sdata->children_head;
  unsigned int total_reloc_count = 0;
  
  for (; curs; curs = curs->sibling_next)
      total_reloc_count += curs->section->reloc_count;

  sec->orelocation = bfd_alloc (sec->owner, sizeof (arelent) * total_reloc_count);
  sec->reloc_count = 0;
  
  for (; curs; curs = curs->sibling_next)
    {
      arelent **curr_relocs = curs->section->orelocation;
      unsigned int curr_reloc_count = curs->section->reloc_count;
      unsigned int i;
      for (i = 0; i < curr_reloc_count; i++)
	{
	  if (sdata->type == WASM_SEC_DATA)
	    curr_relocs[i]->address += ((wasm_data_segment_meta *) curs->meta)->address;
	  else
	    curr_relocs[i]->address += curs->offset;
	  sec->orelocation[sec->reloc_count] = curr_relocs[i];
	  ++sec->reloc_count;
	}

      /* Invalidate children relocations */
      curs->section->orelocation = NULL;
      curs->section->reloc_count = 0;
      /* The invalidated reloc_ptr vector of the child could be freed here,
	 but BFD's abfd-tied bump allocator does not allow for it.
	 Thus, the duplicate reloc_ptr vec will live as junk until the abfd is
	 closed. */
    }

  return true;
}

static bool
wasm_nsec_section_generic_flatten_meta_and_contents (asection *sec)
{
  if (wasm_section_data (sec)->subsec_count == 0)
    return true;

  sec->output_section = sec;
  
  if (! wasm_nsec_flatten_compute_offsets (sec, true))
    return false;

  if (! wasm_nsec_flatten_adjust_relocs (sec))
    return false;
  
  return wasm_nsec_flatten_merge_contents (sec, true);
}

static bool
wasm_nsec_section_generic_flatten_meta (asection *sec)
{
  if (wasm_section_data (sec)->subsec_count == 0)
    return true;

  if (! wasm_nsec_flatten_compute_offsets (sec, false))
    return false;
  
  return wasm_nsec_flatten_merge_contents (sec, false);
}

static bool
wasm_nsec_section_data_flatten (asection *asect)
{
  wasm_section_tdata *sdata = wasm_section_data (asect);
  wasm_section_tdata *curs = sdata->children_head;
  bfd_vma pos = 0;

  for (; curs; curs = curs->sibling_next)
    {
      wasm_data_segment_meta *curs_meta = curs->meta;
      curs_meta->address = pos;
      pos += curs->section->size;
    }

  return wasm_nsec_section_generic_flatten_meta_and_contents (asect);
}

static bool
wasm_nsec_reconstruct_section (asection *asect, bool has_content)
{
  bfd_byte *cursor = asect->contents;
  bfd_byte *end = cursor + asect->size;
  bfd_vma segcount;
  unsigned int ulen;

  ulen = wasm_read_uleb128_buf (cursor, end, &segcount);
  cursor += ulen;
  if (! ulen)
    {
      _bfd_error_handler (_("%pB: failed to read ULEB128 for segment count"
			    " in section '%s' (offset %ld)"),
			  asect->owner, asect->name ? asect->name : "<unknown>",
			  (long)(cursor - asect->contents));
      return false;
    }

  for (unsigned int i = 0; i < segcount; i++)
    {
      asection *segsec = bfd_wasm_make_empty_segment (asect->owner, asect);
      wasm_section_data (segsec)->offset = (long)(cursor - asect->contents);
      int metalen = wasm_nsec_subsec_parse_meta (segsec, cursor, end);
      if (metalen < 0)
        {
	  _bfd_error_handler (_("%pB: parsing meta for segment failed"
			      " in section '%s' (offset %ld)"),
			      asect->owner, asect->name ? asect->name : "<unknown>",
			      (long)(cursor - asect->contents));
	  return false;
        }
      cursor += metalen;

      if (has_content)
        {
	  ulen = wasm_read_uleb128_buf (cursor, end, &segsec->size);
	  if (! ulen)
	    {
	      _bfd_error_handler (_("%pB: failed to read ULEB128 for segment size"
				    " in section '%s' (offset %ld)"),
				  asect->owner, asect->name ? asect->name : "<unknown>",
				  (long)(cursor - asect->contents));
	      return false;
	    }
	  cursor += ulen;
	  
	  /* Reuse parent section buffer */
	  segsec->contents = cursor;
	  segsec->alloced = true;
	  cursor += segsec->size;
        }
      else
	{
	  BFD_ASSERT (segsec->size == 0);
	}
    }

  if (cursor != end)
    {
      /* This is a warning */
      _bfd_error_handler (_("%pB: section %s has extraneous data"
			    " at end (%ld bytes)"),
			  asect->owner, asect->name ? asect->name : "<unknown>",
			  (long)(end - cursor));
    }

  return true;
}

static bool
wasm_nsec_section_generic_reconstruct_meta_and_contents (asection *sec)
{
  BFD_ASSERT (sec->contents);
  return wasm_nsec_reconstruct_section (sec, true);
}

static bool
wasm_nsec_section_generic_reconstruct_meta (asection *sec)
{
  BFD_ASSERT (sec->contents);
  return wasm_nsec_reconstruct_section (sec, false);
}

static bool
wasm_nsec_section_data_reconstruct (asection *sec)
{
  return wasm_nsec_section_generic_reconstruct_meta_and_contents (sec);
}

DEFINE_WASM_SECTION_INITIALIZER(custom)
DEFINE_WASM_SECTION_INITIALIZER(type)
DEFINE_WASM_SECTION_INITIALIZER(import)
DEFINE_WASM_SECTION_INITIALIZER(function)
DEFINE_WASM_SECTION_INITIALIZER(table)
DEFINE_WASM_SECTION_INITIALIZER(memory)
DEFINE_WASM_SECTION_INITIALIZER(global)
DEFINE_WASM_SECTION_INITIALIZER(export)
DEFINE_WASM_SECTION_INITIALIZER(start)
DEFINE_WASM_SECTION_INITIALIZER(element)
DEFINE_WASM_SECTION_INITIALIZER(code)
DEFINE_WASM_SECTION_INITIALIZER(data)
DEFINE_WASM_SECTION_INITIALIZER(datacount)

static const struct wasm_section_ops wasm_section_ops[WASM_NUMBERED_SECTIONS] =
{
  DECLARE_WASM_SECTION_OPS               (WASM_SEC_CUSTOM,    custom),
  DECLARE_WASM_SECTION_OPS_META          (WASM_SEC_TYPE,      type),
  DECLARE_WASM_SECTION_OPS_META          (WASM_SEC_IMPORT,    import),
  DECLARE_WASM_SECTION_OPS_META          (WASM_SEC_FUNCTION,  function),
  DECLARE_WASM_SECTION_OPS_META          (WASM_SEC_TABLE,     table),
  DECLARE_WASM_SECTION_OPS_META          (WASM_SEC_MEMORY,    memory),
  DECLARE_WASM_SECTION_OPS_META          (WASM_SEC_GLOBAL,    global),
  DECLARE_WASM_SECTION_OPS_META          (WASM_SEC_EXPORT,    export),
  DECLARE_WASM_SECTION_OPS_META          (WASM_SEC_START,     start),
  DECLARE_WASM_SECTION_OPS_META_CONTENTS (WASM_SEC_ELEMENT,   element),
  DECLARE_WASM_SECTION_OPS_META_CONTENTS (WASM_SEC_CODE,      code),
  DECLARE_WASM_SECTION_OPS               (WASM_SEC_DATA,      data),
  DECLARE_WASM_SECTION_OPS               (WASM_SEC_DATACOUNT, datacount)
};

bool
wasm_nsec_section_flatten (asection *sec)
{
  int type = wasm_section_data (sec)->type;
  BFD_ASSERT (! wasm_is_segment (sec));

  if (type >= WASM_NUMBERED_SECTIONS)
    return false;

  const struct wasm_section_ops *ops = &wasm_section_ops[type];
  if (ops->flatten && sec->contents)
    {
      fprintf (stderr,
               "wasm_nsec_section_flatten: "
               "overriding main section '%s'\n",
               sec->name ? sec->name : "<unknown>");
      sec->contents = NULL;
      sec->size = 0;
    }

  if (ops->flatten)
    return ops->flatten(sec);

  return true;
}

bool
wasm_nsec_section_reconstruct (asection *sec)
{
  int type = wasm_section_data (sec)->type;
  BFD_ASSERT (! wasm_is_segment (sec));

  if (type >= WASM_NUMBERED_SECTIONS)
    return false;

  const struct wasm_section_ops *ops = &wasm_section_ops[type];
  if (ops->reconstruct)
    return ops->reconstruct(sec);

  return true;
}

bool
wasm_nsec_section_initialize (asection *sec)
{
  int type = wasm_section_data (sec)->type;
  BFD_ASSERT (! wasm_is_segment (sec));

  if (type >= WASM_NUMBERED_SECTIONS)
    return false;

  const struct wasm_section_ops *ops = &wasm_section_ops[type];
  if (ops->initialize)
    return ops->initialize(sec);

  return true;
}

bool
wasm_nsec_symbols_adjust (bfd *abfd)
{
  asymbol **syms = abfd->outsymbols;
  unsigned int i;

  for (i = 0; i < abfd->symcount; i++)
    {
      asymbol *currsym = syms[i];
      wasm_section_tdata *sdata;
      if (! currsym->section)
	continue;

      sdata = wasm_section_data (currsym->section);
      if (wasm_is_segment (currsym->section))
	{
	  wasmsymbol (currsym)->index = bfd_wasm_index_of (currsym->section);
	  currsym->section = sdata->parent->section;
	  if (sdata->parent->type == WASM_SEC_DATA)
	    currsym->value += ((wasm_data_segment_meta *) sdata->meta)->address;
	  else
	    currsym->value += sdata->offset;
	}

      printf ("symbol %s at %s value %ld idx %d\n", currsym->name ? currsym->name : "<unknown>", currsym->section->name ? currsym->section->name : "<unknown>", currsym->value, wasmsymbol (currsym)->index);
    }

  return true;
}
