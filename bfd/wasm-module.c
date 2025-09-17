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

#include "wasm-module.h"
#include "wasm-common.h"
#include "wasm-nsec.h"

#include <limits.h>
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

static const char * const wasm_numbered_sections[] =
{
  NULL, /* Custom section, different layout.  */
  WASM_SECTION ( 1, "type"),
  WASM_SECTION ( 2, "import"),
  WASM_SECTION ( 3, "function"),
  WASM_SECTION ( 4, "table"),
  WASM_SECTION ( 5, "memory"),
  WASM_SECTION ( 6, "global"),
  WASM_SECTION ( 7, "export"),
  WASM_SECTION ( 8, "start"),
  WASM_SECTION ( 9, "element"),
  WASM_SECTION (10, "code"),
  WASM_SECTION (11, "data"),
  WASM_SECTION (12, "datacount"),
};

asection *
bfd_wasm_get_section_by_number (bfd *abfd, int number)
{
  if (number <= 0 || number >= WASM_NUMBERED_SECTIONS)
    return NULL;

  return wasmdata (abfd)->numbered_sections[number];
}

/* Resolve SECTION_CODE to a section name if there is one, NULL
   otherwise.  */

const char *
bfd_wasm_section_code_to_name (bfd_byte section_code)
{
  if (section_code < WASM_NUMBERED_SECTIONS)
    return wasm_numbered_sections[section_code];

  return NULL;
}

/* Translate section name NAME to a section code, or 0 if it's a
   custom name.  */

unsigned int
bfd_wasm_section_name_to_code (const char *name)
{
  unsigned i;

  for (i = 1; i < WASM_NUMBERED_SECTIONS; i++)
    if (strcmp (name, wasm_numbered_sections[i]) == 0)
      return i;

  return 0;
}

/* Create a new empty segment as part of a parent section */

asection *
bfd_wasm_make_empty_segment (bfd *abfd, asection *parent)
{
  asection *segsec;
  unsigned int segidx;
  unsigned int segnamelen;
  char *segname;
  wasm_section_tdata *parentdata;
  BFD_ASSERT (wasm_is_parent (parent));

  segidx = wasm_section_data (parent)->subsec_count;
  segnamelen = strlen (parent->name) + 1 /* . */
    + wasm_estimate_digit (segidx) + 1 /* NUL */;
  segname = bfd_alloc (abfd, segnamelen);
  snprintf (segname, segnamelen, "%s.%u", parent->name, segidx);

  segsec = bfd_make_section (abfd, segname);
  BFD_ASSERT (segsec);
  parentdata = wasm_section_data (segsec)->parent;
  BFD_ASSERT (parentdata && parentdata->section == parent);
  return segsec;
}

int
bfd_wasm_externtype_to_sectype (unsigned int externtype)
{
  switch (externtype)
    {
    case WASM_EXTERN_FUNCTION:
      return WASM_SEC_FUNCTION;
    case WASM_EXTERN_TABLE:
      return WASM_SEC_TABLE;
    case WASM_EXTERN_MEMORY:
      return WASM_SEC_MEMORY;
    case WASM_EXTERN_GLOBAL:
      return WASM_SEC_GLOBAL;
    default:
      return -1;
    }
}

int
bfd_wasm_sectype_to_externtype (unsigned int sectype)
{
  switch (sectype)
    {
    case WASM_SEC_FUNCTION:
      return WASM_EXTERN_FUNCTION;
    case WASM_SEC_TABLE:
      return WASM_EXTERN_TABLE;
    case WASM_SEC_MEMORY:
      return WASM_EXTERN_MEMORY;
    case WASM_SEC_GLOBAL:
      return WASM_EXTERN_GLOBAL;
    default:
      return -1;
    }  
}

asection *
bfd_wasm_make_import_segment (bfd *abfd, const char *modname,
			      const char *name, int externtype)
{
  asection *importsec = bfd_wasm_get_section_by_number (abfd, WASM_SEC_IMPORT);
  asection *segsec = bfd_wasm_make_empty_segment (abfd, importsec);
  wasm_import_segment_meta *meta = wasm_section_data (segsec)->meta;
  meta->import_module = modname;
  meta->import_name = name;
  meta->ext.kind = externtype;
  return segsec;
}

const char *
bfd_wasm_get_import_modname (asection *importsec)
{
  wasm_import_segment_meta *meta = wasm_section_data (importsec)->meta;
  return meta->import_module;
}

bool
bfd_wasm_set_import_modname (asection *importsec, const char *name)
{
  wasm_import_segment_meta *meta = wasm_section_data (importsec)->meta;
  meta->import_module = name;
  return true;
}

const char *
bfd_wasm_get_import_name (asection *importsec)
{
  wasm_import_segment_meta *meta = wasm_section_data (importsec)->meta;
  return meta->import_name;
}

bool
bfd_wasm_set_import_name (asection *importsec, const char *name)
{
  wasm_import_segment_meta *meta = wasm_section_data (importsec)->meta;
  meta->import_name = name;
  return true;
}

int
bfd_wasm_get_import_type (asection *importsec)
{
  wasm_import_segment_meta *meta = wasm_section_data (importsec)->meta;
  return meta->ext.kind;
}

bool
bfd_wasm_set_import_type (asection *importsec, int externtype)
{
  wasm_import_segment_meta *meta = wasm_section_data (importsec)->meta;
  meta->ext.kind = externtype;
  return true;
}

asection *
bfd_wasm_get_segment_by_index (bfd *abfd, int sectype, int idx)
{
  /* FIXME: optimize */
  asection *importsec = bfd_wasm_get_section_by_number (abfd, WASM_SEC_IMPORT);
  asection *objsec = bfd_wasm_get_section_by_number (abfd, sectype);
  int externtype = bfd_wasm_sectype_to_externtype (sectype);
  int objimports = 0;
  int objidx = 0;
  wasm_section_tdata *imcurr, *objcurr;
  imcurr = wasm_section_data (importsec)->children_head;
  objcurr = wasm_section_data (objsec)->children_head;

  for (; imcurr && externtype != -1; imcurr = imcurr->sibling_next)
    {
      wasm_import_segment_meta *imeta = imcurr->meta;
      if (objimports == idx)
	return imcurr->section;
      if (imeta->ext.kind == externtype)
	objimports++;
    }

  for (; objcurr; objcurr = objcurr->sibling_next)
    {
      if (objimports + objidx == idx)
	return objcurr->section;
      objidx++;
    }

  return NULL;
}

asection *
bfd_wasm_get_segment_by_local_index (bfd *abfd, int objtype, int idx)
{
  /* FIXME: optimize */
  asection *objsec = bfd_wasm_get_section_by_number (abfd, objtype);
  int objidx = 0;
  wasm_section_tdata *objcurr = wasm_section_data (objsec)->children_head;

  for (; objcurr; objcurr = objcurr->sibling_next)
    {
      if (objidx == idx)
	return objcurr->section;
      objidx++;
    }

  return NULL;
}

int
bfd_wasm_index_of (asection *segment)
{
  /* FIXME: optimize */
  bfd *abfd = segment->owner;
  asection *importsec = bfd_wasm_get_section_by_number (abfd, WASM_SEC_IMPORT);
  wasm_section_tdata *imdata = wasm_section_data (segment);
  int externtype = bfd_wasm_sectype_to_externtype (imdata->parent->type);
  int objimports = 0;
  wasm_section_tdata *imcurr;
  imcurr = wasm_section_data (importsec)->children_head;

  for (; imcurr; imcurr = imcurr->sibling_next)
    {
      wasm_import_segment_meta *imeta = imcurr->meta;
      if (imcurr->section == segment)
	return objimports;
      if (imeta->ext.kind == externtype)
	objimports++;
    }

  return objimports + wasm_section_data (segment)->index;
}

int
bfd_wasm_local_index_of (asection *segment)
{
  return wasm_section_data (segment)->index;
}


/* Verify the magic number at the beginning of a WebAssembly module
   ABFD, setting ERRORPTR if there's a mismatch.  */

static bool
wasm_read_magic (bfd *abfd, bool *errorptr)
{
  bfd_byte magic_const[SIZEOF_WASM_MAGIC] = WASM_MAGIC;
  bfd_byte magic[SIZEOF_WASM_MAGIC];

  if (bfd_read (magic, sizeof (magic), abfd) == sizeof (magic)
      && memcmp (magic, magic_const, sizeof (magic)) == 0)
    return true;

  *errorptr = true;
  return false;
}

/* Read the version number from ABFD, returning TRUE if it's a supported
   version. Set ERRORPTR otherwise.  */

static bool
wasm_read_version (bfd *abfd, bool *errorptr)
{
  bfd_byte vers_const[SIZEOF_WASM_VERSION] = WASM_VERSION;
  bfd_byte vers[SIZEOF_WASM_VERSION];

  if (bfd_read (vers, sizeof (vers), abfd) == sizeof (vers)
      /* Don't attempt to parse newer versions, which are likely to
	 require code changes.  */
      && memcmp (vers, vers_const, sizeof (vers)) == 0)
    return true;

  *errorptr = true;
  return false;
}

/* Read the WebAssembly header (magic number plus version number) from
   ABFD, setting ERRORPTR to TRUE if there is a mismatch.  */

static bool
wasm_read_header (bfd *abfd, bool *errorptr)
{
  if (! wasm_read_magic (abfd, errorptr))
    return false;

  if (! wasm_read_version (abfd, errorptr))
    return false;

  return true;
}

/* Scan the "function" subsection of the "name" section ASECT in the
   wasm module ABFD. Create symbols. Return TRUE on success.  */

static bool
wasm_scan_name_function_section (bfd *abfd, sec_ptr asect)
{
  bfd_byte *p;
  bfd_byte *end;
  bfd_vma payload_size;
  bfd_vma symcount = 0;
  wasm_tdata_type *tdata = wasmdata (abfd);
  asymbol *symbols = NULL;
  sec_ptr space_function_index;
  size_t amt;

  p = asect->contents;
  end = asect->contents + asect->size;

  if (!p)
    return false;

  while (p < end)
    {
      bfd_byte subsection_code = *p++;
      if (subsection_code == WASM_FUNCTION_SUBSECTION)
	break;

      /* subsection_code is documented to be a varuint7, meaning that
	 it has to be a single byte in the 0 - 127 range.  If it isn't,
	 the spec must have changed underneath us, so give up.  */
      if (subsection_code & 0x80)
	return false;

      READ_LEB128 (payload_size, p, end);

      if (payload_size > (size_t) (end - p))
	return false;

      p += payload_size;
    }

  if (p >= end)
    return false;

  READ_LEB128 (payload_size, p, end);

  if (payload_size > (size_t) (end - p))
    return false;

  end = p + payload_size;

  READ_LEB128 (symcount, p, end);

  /* Sanity check: each symbol has at least two bytes.  */
  if (symcount > payload_size / 2)
    return false;

  tdata->symcount = symcount;

  space_function_index
    = bfd_make_section_with_flags (abfd, WASM_SECTION_FUNCTION_INDEX,
				   SEC_READONLY | SEC_CODE);

  if (!space_function_index)
    space_function_index
      = bfd_get_section_by_name (abfd, WASM_SECTION_FUNCTION_INDEX);

  if (!space_function_index)
    return false;

  if (_bfd_mul_overflow (tdata->symcount, sizeof (asymbol), &amt))
    {
      bfd_set_error (bfd_error_file_too_big);
      return false;
    }
  symbols = bfd_alloc (abfd, amt);
  if (!symbols)
    return false;

  for (symcount = 0; p < end && symcount < tdata->symcount; symcount++)
    {
      bfd_vma idx;
      bfd_vma len;
      char *name;
      asymbol *sym;

      READ_LEB128 (idx, p, end);
      READ_LEB128 (len, p, end);

      if (len > (size_t) (end - p))
	goto error_return;

      name = bfd_alloc (abfd, len + 1);
      if (!name)
	goto error_return;

      memcpy (name, p, len);
      name[len] = 0;
      p += len;

      sym = &symbols[symcount];
      sym->the_bfd = abfd;
      sym->name = name;
      sym->value = idx;
      sym->flags = BSF_GLOBAL | BSF_FUNCTION;
      sym->section = space_function_index;
      sym->udata.p = NULL;
    }

  if (symcount < tdata->symcount)
    goto error_return;

  tdata->symbols = symbols;
  abfd->symcount = symcount;

  return true;

 error_return:
  if (symbols)
    bfd_release (abfd, symbols);
  tdata->symcount = 0;
  return false;
}

/* Read a byte from ABFD and return it, or EOF for EOF or error.
   Set ERRORPTR on non-EOF error.  */

static int
wasm_read_byte (bfd *abfd, bool *errorptr)
{
  bfd_byte byte;

  if (bfd_read (&byte, 1, abfd) != 1)
    {
      if (bfd_get_error () != bfd_error_file_truncated)
	*errorptr = true;
      return EOF;
    }

  return byte;
}

/* Scan the wasm module ABFD, creating sections and symbols.
   Return TRUE on success.  */

static bool
wasm_scan (bfd *abfd)
{
  bool error = false;
  /* Fake VMAs for now. Choose 0x80000000 as base to avoid clashes
     with actual data addresses.  */
  bfd_vma vma = 0x80000000;
  int section_code;
  unsigned int bytes_read;
  asection *bfdsec;

  if (bfd_seek (abfd, 0, SEEK_SET) != 0)
    goto error_return;

  if (!wasm_read_header (abfd, &error))
    goto error_return;

  while ((section_code = wasm_read_byte (abfd, &error)) != EOF)
    {
      if (section_code != 0)
	{
	  const char *sname = bfd_wasm_section_code_to_name (section_code);
	  if (!sname)
	    sname = WASM_SECTION_PREFIX ".unknown";

	  bfdsec = bfd_make_section_old_way (abfd, sname);
	  if (bfdsec == NULL || ! bfd_set_section_flags (bfdsec, SEC_HAS_CONTENTS))
	    goto error_return;

	  bfdsec->size = wasm_read_leb128 (abfd, &error, &bytes_read, false);
	  if (error)
	    goto error_return;
	}
      else
	{
	  bfd_vma payload_len;
	  bfd_vma namelen;
	  char *name;
	  char *prefix = WASM_SECTION_PREFIX;
	  size_t prefixlen = strlen (prefix);
	  ufile_ptr filesize;

	  payload_len = wasm_read_leb128 (abfd, &error, &bytes_read, false);
	  if (error)
	    goto error_return;
	  namelen = wasm_read_leb128 (abfd, &error, &bytes_read, false);
	  if (error || bytes_read > payload_len
	      || namelen > payload_len - bytes_read)
	    goto error_return;
	  payload_len -= namelen + bytes_read;
	  filesize = bfd_get_file_size (abfd);
	  if (filesize != 0 && namelen > filesize)
	    {
	      bfd_set_error (bfd_error_file_truncated);
	      return false;
	    }
	  name = bfd_alloc (abfd, namelen + prefixlen + 1);
	  if (!name)
	    goto error_return;
	  memcpy (name, prefix, prefixlen);
	  if (bfd_read (name + prefixlen, namelen, abfd) != namelen)
	    goto error_return;
	  name[prefixlen + namelen] = 0;

	  bfdsec = bfd_make_section_anyway_with_flags (abfd, name,
						       SEC_HAS_CONTENTS);
	  if (bfdsec == NULL)
	    goto error_return;

	  bfdsec->size = payload_len;
	}

      bfdsec->vma = vma;
      bfdsec->lma = vma;
      bfdsec->alignment_power = 0;
      bfdsec->filepos = bfd_tell (abfd);
      if (bfdsec->size != 0)
	{
	  bfdsec->contents = _bfd_alloc_and_read (abfd, bfdsec->size,
						  bfdsec->size);
	  if (!bfdsec->contents)
	    goto error_return;
	  bfdsec->alloced = 1;
	}

      vma += bfdsec->size;
    }

  /* Make sure we're at actual EOF.  There's no indication in the
     WebAssembly format of how long the file is supposed to be.  */
  if (error)
    goto error_return;

  return true;

 error_return:
  return false;
}

/* Check if given section is a subsection. If so, return main
   section name, otherwise return NULL */
static char *
wasm_check_subsection (asection *asect)
{
  char * mname = NULL;
  const char * c = asect->name;
  unsigned int dotcount = 0;

  if (strncmp (c, WASM_SECTION_PREFIX,
	       sizeof(WASM_SECTION_PREFIX) - 2) == 0)
    c += sizeof(WASM_SECTION_PREFIX) - 2; /* skip wasm */

  for (; *c; c++)
    {
      if (*c == '.')
        dotcount++;

      if (dotcount == 2)
        {
          unsigned int len = c - asect->name;
          mname = xmalloc (len + 1);
          memcpy (mname, asect->name, len);
          mname[len] = '\0'; /* null termination */
          break;
        }
    }

  return mname;
}

static void
wasm_section_set_child (asection *parent, asection *child)
{
  wasm_section_tdata *parent_sdata = wasm_section_data (parent);
  wasm_section_tdata *child_sdata = wasm_section_data (child);

  child_sdata->parent = parent_sdata;
  child_sdata->type = parent_sdata->type;

  if (! parent_sdata->children_tail)
    {
      parent_sdata->children_tail = parent_sdata->children_head = child_sdata;
    }
  else
    {
      parent_sdata->children_tail->sibling_next = child_sdata;
      parent_sdata->children_tail = child_sdata;
    }

  child_sdata->index = parent_sdata->subsec_count++;
}

/* Link subsections and sections. */

static void
wasm_section_link (bfd *abfd, asection *asect)
{
  char *mname = wasm_check_subsection (asect);
  asection *parent;
  if (mname)
    {
      if (bfd_wasm_section_name_to_code (mname) == 0)
        return; /* Custom sections do not get linked */

      /* This is a subsection. */
      parent = bfd_get_section_by_name (abfd, mname);
      /* We create all numbered section parents in in wasm_mkobject */
      BFD_ASSERT (parent);
      
      wasm_section_set_child (parent, asect);
      free (mname);
    }
}

/* Compute segments parent-children offset and size information */

static void
wasm_flatten_section (bfd *abfd ATTRIBUTE_UNUSED,
                      asection *asect,
                      void *fsarg)
{
  bool *fine = (bool *)fsarg;
  if (! wasm_section_data (asect)->type || wasm_is_segment (asect))
    return;

  if (! wasm_nsec_section_flatten (asect))
    *fine = false;
}

static bool
wasm_flatten_sections (bfd *abfd)
{
  bool fine = true;
  bfd_map_over_sections (abfd, wasm_flatten_section, &fine);
  return fine;
}

/* Reconstruct sections after read */

static void
wasm_reconstruct_section (bfd *abfd ATTRIBUTE_UNUSED,
                          asection *asect,
                          void *fsarg)
{
  bool *fine = (bool *)fsarg;
  if (! wasm_section_data (asect)->type
      || ! asect->contents || wasm_is_segment (asect))
    return;

  if (! wasm_nsec_section_reconstruct (asect))
    *fine = false;
}

static bool
wasm_reconstruct_sections (bfd *abfd)
{
  bool fine = true;
  bfd_map_over_sections (abfd, wasm_reconstruct_section, &fine);
  return fine;
}

/* Put a numbered section ASECT of ABFD into the table of numbered
   sections pointed to by FSARG.  */

static void
wasm_register_section (bfd *abfd,
		       asection *asect)
{
  sec_ptr *numbered_sections = wasmdata (abfd)->numbered_sections;
  int idx = bfd_wasm_section_name_to_code (asect->name);

  if (idx == 0)
    return;

  numbered_sections[idx] = asect;
}

/* A hook to set up object file dependent section information.  */

static bool
wasm_new_section_hook (bfd *abfd, asection *newsect)
{
  size_t amt = sizeof (struct wasm_section_tdata);

  wasm_section_tdata *wasm_section = bfd_zalloc (abfd, amt);
  newsect->used_by_bfd = wasm_section;
  if (! wasm_section)
    return false;
  
  wasm_section->section = newsect;
  wasm_section->type = bfd_wasm_section_name_to_code (newsect->name);
  wasm_section_link (abfd, newsect);
  wasm_register_section (abfd, newsect);
  if (! newsect->owner)
    { /* *ABS*, *UND*, *COM*, *IND* */ }
  else if (wasm_is_segment (newsect))
    wasm_nsec_subsec_initialize (newsect);
  else
    wasm_nsec_section_initialize (newsect);

  /* We allow more than three sections internally.  */
  return _bfd_generic_new_section_hook (abfd, newsect);
}

struct compute_section_arg
{
  bfd_vma pos;
  bool failed;
};

/* Compute the file position of ABFD's section ASECT.  FSARG is a
   pointer to the current file position.

   We allow section names of the form .wasm.id to encode the numbered
   section with ID id, if it exists; otherwise, a custom section with
   ID "id" is produced.  Arbitrary section names are for sections that
   are assumed already to contain a section header; those are appended
   to the WebAssembly module verbatim.  */

static void
wasm_compute_custom_section_file_position (bfd *abfd,
					   sec_ptr asect,
					   void *fsarg)
{
  struct compute_section_arg *fs = fsarg;
  wasm_section_tdata *sdata;
  int idx;

  if (fs->failed)
    return;

  idx = bfd_wasm_section_name_to_code (asect->name);

  if (idx != 0)
    return;

  sdata = wasm_section_data(asect);

  if (sdata && sdata->parent)
    {
      /* This is a subsection. Skip it and handle it in main section */
      return;
    }

  if (startswith (asect->name, WASM_SECTION_PREFIX))
    {
      const char *name = asect->name + strlen (WASM_SECTION_PREFIX);
      bfd_size_type payload_len = asect->size;
      bfd_size_type name_len = strlen (name);
      bfd_size_type nl = name_len;

      payload_len += name_len;

      do
	{
	  payload_len++;
	  nl >>= 7;
	}
      while (nl);

      if (bfd_seek (abfd, fs->pos, SEEK_SET) != 0
	  || ! wasm_write_uleb128 (abfd, 0)
	  || ! wasm_write_uleb128 (abfd, payload_len)
	  || ! wasm_write_uleb128 (abfd, name_len)
	  || bfd_write (name, name_len, abfd) != name_len)
	goto error_return;
      fs->pos = asect->filepos = bfd_tell (abfd);
    }
  else
    {
      asect->filepos = fs->pos;
    }


  fs->pos += asect->size;
  return;

 error_return:
  fs->failed = true;
}

/* Compute the file positions for the sections of ABFD.  Currently,
   this writes all numbered sections first, in order, then all custom
   sections, in section order.

   The spec says that the numbered sections must appear in order of
   their ids, but custom sections can appear in any position and any
   order, and more than once. FIXME: support that.  */

static bool
wasm_compute_section_file_positions (bfd *abfd)
{
  bfd_byte magic[SIZEOF_WASM_MAGIC] = WASM_MAGIC;
  bfd_byte vers[SIZEOF_WASM_VERSION] = WASM_VERSION;
  struct compute_section_arg fs;
  unsigned int i;

  if (bfd_seek (abfd, (bfd_vma) 0, SEEK_SET) != 0
      || bfd_write (magic, sizeof (magic), abfd) != (sizeof magic)
      || bfd_write (vers, sizeof (vers), abfd) != sizeof (vers))
    return false;

  fs.pos = bfd_tell (abfd);
  for (i = 0; i < WASM_NUMBERED_SECTIONS; i++)
    {
      sec_ptr sec = wasmdata (abfd)->numbered_sections[i];
      bfd_size_type size;

      if (! sec || ! sec->contents)
	continue;
      size = sec->size;
      if (bfd_seek (abfd, fs.pos, SEEK_SET) != 0)
	return false;
      if (! wasm_write_uleb128 (abfd, i)
	  || ! wasm_write_uleb128 (abfd, size))
	return false;
      fs.pos = sec->filepos = bfd_tell (abfd);
      fs.pos += size;
    }

  fs.failed = false;

  bfd_map_over_sections (abfd, wasm_compute_custom_section_file_position, &fs);

  if (fs.failed)
    return false;

  abfd->output_has_begun = true;

  return true;
}

static bool
wasm_set_section_contents (bfd *abfd,
			   sec_ptr section,
			   const void *location,
			   file_ptr offset,
			   bfd_size_type count)
{
  flagword flags;
  if (count == 0)
    return true;

  if (! section->contents)
    section->contents = (bfd_byte*) bfd_zalloc (abfd, section->size);

  if (! section->contents)
    return false;

  flags = bfd_section_flags (section);
  flags |= SEC_IN_MEMORY;
  if (! bfd_set_section_flags (section, flags))
    return false;

  section->alloced = true;
  memmove (section->contents + offset, location, count);

  return true;
}

static void
wasm_write_section (bfd* abfd ATTRIBUTE_UNUSED, sec_ptr section ATTRIBUTE_UNUSED, void *fsarg ATTRIBUTE_UNUSED)
{
  BFD_ASSERT (abfd->output_has_begun);
  if (! section->contents)
    return;

  if (wasm_section_data(section)->parent)
    return; /* Subsec, do not write. */

  BFD_ASSERT (bfd_seek (abfd, section->filepos, SEEK_SET) == 0
	      && bfd_write (section->contents, section->size, abfd) == section->size);
}

static bool
wasm_write_object_contents (bfd* abfd)
{
  bfd_byte magic[] = WASM_MAGIC;
  bfd_byte vers[] = WASM_VERSION;

  if (! wasm_flatten_sections (abfd)
      || ! wasm_nsec_symbols_adjust (abfd))
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  if (! wasm_compute_section_file_positions (abfd))
    return false;

  if (bfd_seek (abfd, 0, SEEK_SET) != 0)
    return false;

  if (bfd_write (magic, sizeof (magic), abfd) != sizeof (magic)
      || bfd_write (vers, sizeof (vers), abfd) != sizeof (vers))
    return false;

  bfd_map_over_sections (abfd, wasm_write_section, NULL);

  return true;
}

static bool
wasm_mkobject (bfd *abfd)
{
  wasm_tdata_type *tdata = (wasm_tdata_type *) bfd_alloc (abfd, sizeof (wasm_tdata_type));
  size_t i;

  if (! tdata)
    return false;

  tdata->symbols = NULL;
  tdata->symcount = 0;
  for (i = 0; i < WASM_NUMBERED_SECTIONS; i++)
    tdata->numbered_sections[i] = NULL;

  abfd->tdata.any = tdata;
  abfd->flags |= BFD_DEFER_CONTENTS; /* Backend flag, defer contents! */

  /* Create empty sections for each numbered section (except custom). */
  for (i = 1; i < WASM_NUMBERED_SECTIONS; ++i)
    {
      const char *name = wasm_numbered_sections[i];
      if (!name)
        continue;

      asection *sec = bfd_make_section (abfd, name);
      if (!sec)
        return false;
    }

  return true;
}

static long
wasm_get_symtab_upper_bound (bfd *abfd)
{
  wasm_tdata_type *tdata = wasmdata (abfd);

  return (tdata->symcount + 1) * (sizeof (asymbol *));
}

static long
wasm_canonicalize_symtab (bfd *abfd, asymbol **alocation)
{
  wasm_tdata_type *tdata = wasmdata (abfd);
  size_t i;

  for (i = 0; i < tdata->symcount; i++)
    alocation[i] = &tdata->symbols[i];
  alocation[i] = NULL;

  return tdata->symcount;
}

static asymbol *
wasm_make_empty_symbol (bfd *abfd)
{
  size_t amt = sizeof (asymbol);
  asymbol *new_symbol = (asymbol *) bfd_zalloc (abfd, amt);

  if (! new_symbol)
    return NULL;
  new_symbol->the_bfd = abfd;
  return new_symbol;
}

static void
wasm_print_symbol (bfd *abfd,
		   void * filep,
		   asymbol *symbol,
		   bfd_print_symbol_type how)
{
  FILE *file = (FILE *) filep;

  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;

    default:
      bfd_print_symbol_vandf (abfd, filep, symbol);
      fprintf (file, " %-5s %s", symbol->section->name, symbol->name);
    }
}

static void
wasm_get_symbol_info (bfd *abfd ATTRIBUTE_UNUSED,
		      asymbol *symbol,
		      symbol_info *ret)
{
  bfd_symbol_info (symbol, ret);
}

/* Check whether ABFD is a WebAssembly module; if so, scan it.  */

static bfd_cleanup
wasm_object_p (bfd *abfd)
{
  bool error;
  asection *s;

  if (bfd_seek (abfd, 0, SEEK_SET) != 0)
    return NULL;

  if (!wasm_read_header (abfd, &error))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (!wasm_mkobject (abfd))
    return NULL;

  if (!wasm_scan (abfd)
      || !bfd_default_set_arch_mach (abfd, bfd_arch_wasm32, 0))
    {
      bfd_release (abfd, abfd->tdata.any);
      abfd->tdata.any = NULL;
      return NULL;
    }

  s = bfd_get_section_by_name (abfd, WASM_NAME_SECTION);
  if (s != NULL && wasm_scan_name_function_section (abfd, s))
    abfd->flags |= HAS_SYMS;

  if (! wasm_reconstruct_sections (abfd))
    {
      bfd_set_error (bfd_error_bad_value);
      return NULL;
    }

  return _bfd_no_cleanup;
}

/* BFD_JUMP_TABLE_GENERIC */
#define wasm_close_and_cleanup              _bfd_generic_close_and_cleanup
#define wasm_bfd_free_cached_info           _bfd_generic_bfd_free_cached_info
#define wasm_get_section_contents           _bfd_generic_get_section_contents
#define wasm_get_section_contents_in_window _bfd_generic_get_section_contents_in_window

/* BFD_JUMP_TABLE_WRITE */
#define wasm_set_arch_mach		  _bfd_generic_set_arch_mach

/* BFD_JUMP_TABLE_SYMBOLS */
#define wasm_get_symbol_version_string	  _bfd_nosymbols_get_symbol_version_string
#define wasm_bfd_is_local_label_name	   bfd_generic_is_local_label_name
#define wasm_bfd_is_target_special_symbol _bfd_bool_bfd_asymbol_false
#define wasm_get_lineno			  _bfd_nosymbols_get_lineno
#define wasm_find_nearest_line		  _bfd_nosymbols_find_nearest_line
#define wasm_find_nearest_line_with_alt	  _bfd_nosymbols_find_nearest_line_with_alt
#define wasm_find_line			  _bfd_nosymbols_find_line
#define wasm_find_inliner_info		  _bfd_nosymbols_find_inliner_info
#define wasm_bfd_make_debug_symbol	  _bfd_nosymbols_bfd_make_debug_symbol
#define wasm_read_minisymbols		  _bfd_generic_read_minisymbols
#define wasm_minisymbol_to_symbol	  _bfd_generic_minisymbol_to_symbol

const bfd_target wasm_vec =
{
  "wasm",			/* Name.  */
  bfd_target_unknown_flavour,
  BFD_ENDIAN_LITTLE,
  BFD_ENDIAN_LITTLE,
  (HAS_SYMS | WP_TEXT),		/* Object flags.  */
  (SEC_CODE | SEC_DATA | SEC_HAS_CONTENTS), /* Section flags.  */
  0,				/* Leading underscore.  */
  ' ',				/* AR_pad_char.  */
  255,				/* AR_max_namelen.  */
  0,				/* Match priority.  */
  TARGET_KEEP_UNUSED_SECTION_SYMBOLS, /* keep unused section symbols.  */
  /* Routines to byte-swap various sized integers from the data sections.  */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,

  /* Routines to byte-swap various sized integers from the file headers.  */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,

  {
    _bfd_dummy_target,
    wasm_object_p,		/* bfd_check_format.  */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {
    _bfd_bool_bfd_false_error,
    wasm_mkobject,
    _bfd_bool_bfd_false_error,
    _bfd_bool_bfd_false_error,
  },
  {				/* bfd_write_contents.  */
    _bfd_bool_bfd_false_error,
    wasm_write_object_contents,
    _bfd_bool_bfd_false_error,
    _bfd_bool_bfd_false_error,
  },

  BFD_JUMP_TABLE_GENERIC (wasm),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (wasm),
  BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
  BFD_JUMP_TABLE_WRITE (wasm),
  BFD_JUMP_TABLE_LINK (_bfd_nolink),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  NULL,
};
