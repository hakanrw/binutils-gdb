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

#include "wasm-nsec.h"
#include "wasm-module.h"
#include "wasm-utils.h"

#include "assert.h" /* FIXME: remove */

struct wasm_section_ops
{
  int (*flatten)(asection *sec);       // Optional
  int (*reconstruct)(asection *sec);   // Optional
  int (*initialize)(asection *sec);    // Optional
};

struct wasm_segment_meta_ops
{
  int (*parse)(asection *sec, bfd_byte *start, bfd_byte *end);
  int (*serialize)(asection *sec, bfd_byte *start);
  int (*length)(asection *sec);
  int (*initialize)(asection *sec);    // Optional
};

#define STR(x) #x

#define DECLARE_WASM_SECTION_OPS_SIZE_META_CONTENTS(NAME) \
  [WASM_SEC_##NAME] = { \
    .flatten = wasm_nsec_section_generic_flatten_meta_and_contents, \
    .reconstruct = wasm_nsec_section_generic_reconstruct_meta_and_contents, \
    .initialize = wasm_nsec_section_##NAME##_initialize \
  }

#define DECLARE_WASM_SECTION_OPS_META(NAME) \
  [WASM_SEC_##NAME] = { \
    .flatten = wasm_nsec_section_generic_flatten_meta, \
    .reconstruct = wasm_nsec_section_generic_reconstruct_meta, \
    .initialize = wasm_nsec_section_##NAME##_initialize \
  }

#define DECLARE_WASM_SECTION_OPS(NAME) \
  [WASM_SEC_##NAME] = { \
    .flatten = wasm_nsec_section_##NAME##_flatten, \
    .reconstruct = wasm_nsec_section_##NAME##_reconstruct, \
    .initialize = wasm_nsec_section_##NAME##_initialize \
  }

#define DECLARE_WASM_SUBSEC_OPS_DEFAULT(NAME) \
  [WASM_SEC_##NAME] = { \
    .parse = wasm_nsec_subsec_generic_parse_meta, \
    .serialize = wasm_nsec_subsec_generic_serialize_meta, \
    .length = wasm_nsec_subsec_generic_meta_len, \
    .initialize = wasm_nsec_subsec_##NAME##_initialize \
  }

#define DECLARE_WASM_SUBSEC_OPS(NAME) \
  [WASM_SEC_##NAME] = { \
    .parse = wasm_nsec_subsec_##NAME##_parse_meta, \
    .serialize = wasm_nsec_subsec_##NAME##_serialize_meta, \
    .length = wasm_nsec_subsec_##NAME##_meta_len, \
    .initialize = wasm_nsec_subsec_##NAME##_initialize \
  }

#define DEFINE_WASM_SUBSEC_INITIALIZER(NAME)                           \
  static int wasm_nsec_subsec_##NAME##_initialize(asection *sec)       \
  {                                                                    \
    wasm_##NAME##_segment_meta *meta =                                 \
      (wasm_##NAME##_segment_meta *) bfd_zalloc (sec->owner, sizeof (*meta));  \
    wasm_section_data (sec)->meta = meta;                              \
    printf ("%s size: %ld\n", STR(wasm_nsec_subsec_##NAME##_initialize), sizeof (*meta)); \
    return 0;                                                          \
  }

#define DEFINE_WASM_SECTION_INITIALIZER(NAME)                          \
  static int wasm_nsec_section_##NAME##_initialize(asection *sec)      \
  {                                                                    \
    wasm_##NAME##_section_meta *meta =                                 \
      (wasm_##NAME##_section_meta *) bfd_zalloc (sec->owner, sizeof (*meta));   \
    wasm_section_data (sec)->meta = meta;                              \
    printf ("%s size: %ld\n", STR(wasm_nsec_section_##NAME##_initialize), sizeof (*meta)); \
    return 0;                                                          \
  }

/* ---------------------- Segments ---------------------- */

ATTRIBUTE_UNUSED
static int
wasm_nsec_subsec_function_parse_meta (asection *sec, bfd_byte *start, bfd_byte *end)
{
  wasm_section_tdata *seg = wasm_section_data (sec);
  wasm_function_segment_meta *meta =
    (wasm_function_segment_meta *) seg->meta;

  return 0;

  if (start >= end) // UB!!
    return -1;

  bfd_byte *cursor = start;
  bfd_vma type_index;
  bfd_size_type consumed = wasm_read_uleb128_buf (cursor, end, &type_index);
  assert (consumed);
  cursor += consumed;

  // Locate the parent .wasm.type section
  wasm_section_tdata *parent = seg->parent;
  assert (parent);

  wasm_section_tdata *type_section;/* = wasm_find_section_by_code (sec->owner, WASM_SEC_TYPE)*/;
  if (!type_section)
    return -1;

  wasm_type_section_meta *type_meta = (wasm_type_section_meta *) type_section->meta;
  if (!type_meta)
    return -1;

  // Lookup type_index in the child segments of .wasm.type
  wasm_section_tdata *cur = type_section->children_head;
  while (cur)
    {
      if (cur->index == type_index)
        {
          meta->type_segment = cur;
          return consumed;
        }
      cur = cur->sibling_next;
    }

  return -1; // not found
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
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(type),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(import),
  DECLARE_WASM_SUBSEC_OPS(function),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(table),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(memory),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(global),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(export),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(start),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(element),
  DECLARE_WASM_SUBSEC_OPS(code),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(data),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(datacount)
};

int
wasm_nsec_subsec_parse_meta (asection *sec, bfd_byte *start, bfd_byte *end /* exclusive */)
{
  assert (wasm_is_segment (sec));

  wasm_section_tdata *parent = wasm_section_data(sec)->parent;
  assert (parent);
  int type = parent->type;
  printf ("wasm_nsec_subsec_parse_meta %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].parse)
    return -1; // or bfd_error, or something less panicky
  return wasm_meta_ops[type].parse(sec, start, end);
}

int
wasm_nsec_subsec_serialize_meta (asection *sec, bfd_byte *start)
{
  assert (wasm_is_segment (sec));

  wasm_section_tdata *parent = wasm_section_data(sec)->parent;
  assert (parent);
  int type = parent->type;
  printf ("wasm_nsec_subsec_serialize_meta %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].serialize)
    return -1;
  return wasm_meta_ops[type].serialize(sec, start);
}

int
wasm_nsec_subsec_meta_len (asection *sec)
{
  assert (wasm_is_segment (sec));

  wasm_section_tdata *parent = wasm_section_data(sec)->parent;
  assert (parent);
  int type = parent->type;
  printf ("wasm_nsec_subsec_meta_len %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].length)
    return -1;
  return wasm_meta_ops[type].length(sec);
}

int
wasm_nsec_subsec_initialize (asection *sec)
{
  assert (wasm_is_segment (sec));

  wasm_section_tdata *parent = wasm_section_data(sec)->parent;
  assert (parent);
  int type = parent->type;
  printf ("wasm_nsec_subsec_initialize %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].length)
    return -1;
  return wasm_meta_ops[type].length(sec);
}

int
wasm_nsec_subsec_len (asection *sec)
{
  assert (wasm_is_segment (sec));

  int meta_len = wasm_nsec_subsec_meta_len (sec);
  if (meta_len < 0)
    return -1;

  return sec->size + meta_len;
}

/* ---------------------- Sections ---------------------- */

static int
wasm_nsec_section_datacount_flatten (asection *sec ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
wasm_nsec_section_datacount_reconstruct (asection *sec ATTRIBUTE_UNUSED)
{
  return 0;
}

/* Compute segments parent-children offset and size information */

static void
wasm_nsec_flatten_compute_offsets (asection *asect, bool has_content)
{
  wasm_section_tdata *sdata = wasm_section_data (asect);

  assert (!asect->contents); /* FIXME: Be empty, or face destruction */

  unsigned long pos = 0;
  pos += 5; /* Uleb size padding. */

  wasm_section_tdata *curs = sdata->children_head;
  for (; curs; curs = curs->sibling_next)
    {
      curs->offset = pos;
      pos += wasm_nsec_subsec_meta_len (curs->section);
      if (! has_content)
	assert (curs->section->size == 0);
      pos += curs->section->size;

      printf ("offset of %s within %s: %d\n", curs->section->name, asect->name, curs->offset);
    }
  asect->size = pos;
}

static void
wasm_nsec_flatten_merge_contents (asection *asect, bool has_content)
{
  wasm_section_tdata *sdata = wasm_section_data (asect);

  bfd_byte* contents = (bfd_byte*) bfd_malloc (asect->size);

  contents[0] = 0x80 + sdata->subsec_count;
  contents[1] = 0x80;
  contents[2] = 0x80;
  contents[3] = 0x80;
  contents[4] = 0x00;

  wasm_section_tdata *curs = sdata->children_head;
  for (; curs; curs = curs->sibling_next)
    {
      bfd_byte* curr_data = contents + curs->offset;
      curr_data += wasm_nsec_subsec_serialize_meta (curs->section, curr_data);
      if (has_content)
        memmove (curr_data, curs->section->contents, curs->section->size);
    }

  if (! bfd_set_section_contents (asect->owner, asect, contents, 0, asect->size))
    assert (false);

  bfd_realloc_or_free (contents, 0);
}

static int
wasm_nsec_section_generic_flatten_meta_and_contents (asection *sec)
{
  printf ("wasm_nsec_section_generic_flatte_meta_and_contents\n");
  if (wasm_section_data (sec)->subsec_count == 0)
    return 0;

  wasm_nsec_flatten_compute_offsets (sec, true);
  wasm_nsec_flatten_merge_contents (sec, true);
  return 0;
}

static int
wasm_nsec_section_generic_flatten_meta (asection *sec)
{
  printf ("wasm_nsec_section_generic_flatten_meta\n");
  if (wasm_section_data (sec)->subsec_count == 0)
    return 0;

  wasm_nsec_flatten_compute_offsets (sec, false);
  wasm_nsec_flatten_merge_contents (sec, false);
  return 0;
}

static void
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
      fprintf (stderr,
               "wasm_nsec_reconstruct_section: failed to read ULEB128 for segment count "
               "in section '%s' (offset 0x%zx)\n",
               asect->name ? asect->name : "<unknown>",
               (size_t)(cursor - asect->contents));
      return;
    }

  for (unsigned int i = 0; i < segcount; i++)
    {
      asection *segsec;
      unsigned int segnamelen = strlen (asect->name) + 1 /* . */ + wasm_estimate_digit (i) + 1 /* NUL */;
      char *segname = bfd_alloc (asect->owner, segnamelen);
      snprintf (segname, segnamelen, "%s.%u", asect->name, i);

      segsec = bfd_make_section_anyway_with_flags (asect->owner, segname,
                                                   SEC_HAS_CONTENTS | SEC_IN_MEMORY);

      int metalen = wasm_nsec_subsec_parse_meta (segsec, cursor, end);
      if (metalen < 0)
        {
            fprintf (stderr,
                     "wasm_nsec_reconstruct_section: parsing meta for segment failed "
                     "in section '%s' (offset 0x%zx)\n",
                     asect->name ? asect->name : "<unknown>",
                     (size_t)(cursor - asect->contents));
              return;
        }
      cursor += metalen;

      if (has_content && segsec->size)
        {
           segsec->contents = bfd_alloc (asect->owner, segsec->size);
           segsec->alloced = true;
           if (! segsec->contents)
             assert (false);
           memmove (segsec->contents, cursor, segsec->size);
           cursor += segsec->size;
        }
      else
	{
	  assert (segsec->size == 0);
	}
    }

  if (cursor != end)
    {
      fprintf (stderr,
               "wasm_nsec_reconstruct_section: section has extraneous data at end "
               "in section '%s' (extraneous size 0x%zx)\n",
               asect->name ? asect->name : "<unknown>",
               (size_t)(end - cursor));

    }
}

static int
wasm_nsec_section_generic_reconstruct_meta_and_contents (asection *sec)
{
  printf ("wasm_nsec_section_generic_reconstruct_meta_and_contents\n");
  assert (sec->contents);

  wasm_nsec_reconstruct_section (sec, true);
  return 0;
}

static int
wasm_nsec_section_generic_reconstruct_meta (asection *sec)
{
  printf ("wasm_nsec_section_generic_meta\n");
  assert (sec->contents);

  wasm_nsec_reconstruct_section (sec, false);
  return 0;
}

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
  DECLARE_WASM_SECTION_OPS_META(type),
  DECLARE_WASM_SECTION_OPS_META(import),
  DECLARE_WASM_SECTION_OPS_META(function),
  DECLARE_WASM_SECTION_OPS_META(table),
  DECLARE_WASM_SECTION_OPS_META(memory),
  DECLARE_WASM_SECTION_OPS_META(global),
  DECLARE_WASM_SECTION_OPS_META(export),
  DECLARE_WASM_SECTION_OPS_META(start),
  DECLARE_WASM_SECTION_OPS_SIZE_META_CONTENTS(element),
  DECLARE_WASM_SECTION_OPS_SIZE_META_CONTENTS(code),
  DECLARE_WASM_SECTION_OPS_SIZE_META_CONTENTS(data),
  DECLARE_WASM_SECTION_OPS(datacount)
};

int
wasm_nsec_section_flatten (asection *sec)
{
  assert (! wasm_is_segment (sec));

  int type = wasm_section_data (sec)->type;
  printf ("wasm_nsec_section_flatten %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS)
    return -1;

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

  return 0;
}

int
wasm_nsec_section_reconstruct (asection *sec)
{
  assert (! wasm_is_segment (sec));

  int type = wasm_section_data (sec)->type;
  printf ("wasm_nsec_section_reconstruct %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS)
    return -1;

  const struct wasm_section_ops *ops = &wasm_section_ops[type];
  if (ops->reconstruct)
    return ops->reconstruct(sec);

  return 0;
}

int
wasm_nsec_section_initialize (asection *sec)
{
  assert (! wasm_is_segment (sec));

  int type = wasm_section_data (sec)->type;
  printf ("wasm_nsec_section_initialize %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS)
    return -1;

  const struct wasm_section_ops *ops = &wasm_section_ops[type];
  if (ops->initialize)
    return ops->initialize(sec);

  return 0;
}
