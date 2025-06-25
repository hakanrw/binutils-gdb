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

#define DECLARE_WASM_SECTION_OPS_DEFAULT(NAME) \
  [WASM_SEC_##NAME] = { \
    .flatten = wasm_nsec_section_generic_flatten, \
    .reconstruct = wasm_nsec_section_generic_reconstruct, \
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
    printf ("%s %ld\n", STR(wasm_nsec_subsec_##NAME##_initialize), sizeof (*meta)); \
    return 0;                                                          \
  }

#define DEFINE_WASM_SECTION_INITIALIZER(NAME)                          \
  static int wasm_nsec_section_##NAME##_initialize(asection *sec)      \
  {                                                                    \
    wasm_##NAME##_section_meta *meta =                                 \
      (wasm_##NAME##_section_meta *) bfd_zalloc (sec->owner, sizeof (*meta));   \
    wasm_section_data (sec)->meta = meta;                              \
    printf ("%s %ld\n", STR(wasm_nsec_section_##NAME##_initialize), sizeof (*meta)); \
    return 0;                                                          \
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

static const struct wasm_segment_meta_ops wasm_meta_ops[WASM_NUMBERED_SECTIONS] =
{
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(type),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(import),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(function),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(table),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(memory),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(global),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(export),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(start),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(element),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(code),
  DECLARE_WASM_SUBSEC_OPS_DEFAULT(data),
};

int
wasm_nsec_subsec_parse_meta (asection *sec, bfd_byte *start, bfd_byte *end)
{
  wasm_section_tdata *parent = wasm_section_data(sec)->parent;
  assert (parent);
  int type = wasm_section_name_to_code (parent->section->name);
  printf ("wasm_nsec_subsec_parse_meta %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].parse)
    return -1; // or bfd_error, or something less panicky
  return wasm_meta_ops[type].parse(sec, start, end);
}

int
wasm_nsec_subsec_serialize_meta (asection *sec, bfd_byte *start)
{
  wasm_section_tdata *parent = wasm_section_data(sec)->parent;
  assert (parent);
  int type = wasm_section_name_to_code (parent->section->name);
  printf ("wasm_nsec_subsec_serialize_meta %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].serialize)
    return -1;
  return wasm_meta_ops[type].serialize(sec, start);
}

int
wasm_nsec_subsec_meta_len (asection *sec)
{
  wasm_section_tdata *parent = wasm_section_data(sec)->parent;
  assert (parent);
  int type = wasm_section_name_to_code (parent->section->name);
  printf ("wasm_nsec_subsec_meta_len %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].length)
    return -1;
  return wasm_meta_ops[type].length(sec);
}

int
wasm_nsec_subsec_initialize (asection *sec)
{
  wasm_section_tdata *parent = wasm_section_data(sec)->parent;
  assert (parent);
  int type = wasm_section_name_to_code (parent->section->name);
  printf ("wasm_nsec_subsec_initialize %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS || !wasm_meta_ops[type].length)
    return -1;
  return wasm_meta_ops[type].length(sec);
}

/* Compute segments parent-children offset and size information */

static void
wasm_nsec_flatten_compute_offsets (asection *asect)
{
  wasm_section_tdata *sdata = wasm_section_data (asect);

  assert (!asect->contents); /* FIXME: Be empty, or face destruction */

  unsigned long pos = 0;
  pos += 5; /* Uleb size padding. */

  wasm_section_tdata *curs = sdata->children_head;
  for (; curs; curs = curs->sibling_next)
    {
      pos += wasm_sizeof_uleb128 (curs->section->size); /* Entry uleb size */
      curs->offset = pos;
      pos += curs->section->size;

      printf ("offset of %s within %s: %d\n", curs->section->name, asect->name, curs->offset);
    }
  asect->size = pos;
}

static void
wasm_nsec_flatten_merge_contents (asection *asect)
{
  wasm_section_tdata *sdata = wasm_section_data (asect);

  bfd_byte* contents = (bfd_byte*) bfd_malloc (asect->size);

  contents[0] = 0x80;
  contents[1] = 0x80;
  contents[2] = 0x80;
  contents[3] = 0x80;
  contents[4] = 0x00;

  wasm_section_tdata *curs = sdata->children_head;
  for (; curs; curs = curs->sibling_next)
    {
      void* cursize_off = contents + curs->offset - wasm_sizeof_uleb128 (curs->section->size);
      wasm_write_uleb128_buf (cursize_off, curs->section->size);
      memmove (contents + curs->offset, curs->section->contents, curs->section->size);
    }

  if (! bfd_set_section_contents (asect->owner, asect, contents, 0, asect->size))
    assert (false);

  bfd_realloc_or_free (contents, 0);
}

static int
wasm_nsec_section_generic_flatten(asection *sec)
{
  assert (! wasm_is_segment (sec) && wasm_section_data(sec)->subsec_count != 0);

  wasm_nsec_flatten_compute_offsets (sec);
  wasm_nsec_flatten_merge_contents (sec);
  return 0;
}

static int
wasm_nsec_section_generic_reconstruct(asection *sec ATTRIBUTE_UNUSED)
{
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

static const struct wasm_section_ops wasm_section_ops[WASM_NUMBERED_SECTIONS] =
{
  DECLARE_WASM_SECTION_OPS_DEFAULT(type),
  DECLARE_WASM_SECTION_OPS_DEFAULT(import),
  DECLARE_WASM_SECTION_OPS_DEFAULT(function),
  DECLARE_WASM_SECTION_OPS_DEFAULT(table),
  DECLARE_WASM_SECTION_OPS_DEFAULT(memory),
  DECLARE_WASM_SECTION_OPS_DEFAULT(global),
  DECLARE_WASM_SECTION_OPS_DEFAULT(export),
  DECLARE_WASM_SECTION_OPS_DEFAULT(start),
  DECLARE_WASM_SECTION_OPS_DEFAULT(element),
  DECLARE_WASM_SECTION_OPS_DEFAULT(code),
  DECLARE_WASM_SECTION_OPS_DEFAULT(data),
};

int
wasm_nsec_section_flatten(asection *sec)
{
  int type = wasm_section_name_to_code (sec->name);
  printf ("wasm_nsec_section_flatten %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS)
    return -1;

  const struct wasm_section_ops *ops = &wasm_section_ops[type];
  if (ops->flatten)
    return ops->flatten(sec);

  return 0;
}

int
wasm_nsec_section_reconstruct(asection *sec)
{
  int type = wasm_section_name_to_code (sec->name);
  printf ("wasm_nsec_section_reconstruct %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS)
    return -1;

  const struct wasm_section_ops *ops = &wasm_section_ops[type];
  if (ops->reconstruct)
    return ops->reconstruct(sec);

  return 0;
}

int
wasm_nsec_section_initialize(asection *sec)
{
  int type = wasm_section_name_to_code (sec->name);
  printf ("wasm_nsec_section_initialize %s (%d)\n", sec->name, type);

  if (type >= WASM_NUMBERED_SECTIONS)
    return -1;

  const struct wasm_section_ops *ops = &wasm_section_ops[type];
  if (ops->initialize)
    return ops->initialize(sec);

  return 0;
}
