/* WASM object file format
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

#define OBJ_HEADER "obj-wasm.h"

#include "as.h"
#undef NO_RELOC

#include "subsegs.h"

static const pseudo_typeS wasm_pseudo_table[];

void
wasm_pop_insert (void)
{
  pop_insert (wasm_pseudo_table);
}

void
obj_wasm_section (int ignore ATTRIBUTE_UNUSED)
{
  /* Strip out the section name.  */
  char *section_name;
  char c;
  int alignment = -1;
  char *name;
  unsigned int exp;
  flagword flags, oldflags;
  asection *sec;
  bool is_bss = false;

  c = get_symbol_name (&section_name);
  name = notes_memdup0 (section_name, input_line_pointer - section_name);
  restore_line_pointer (c);
  SKIP_WHITESPACE ();

  exp = 0;
  flags = SEC_NO_FLAGS;

  sec = subseg_new (name, (subsegT) exp);

  if (is_bss)
    seg_info (sec)->bss = 1;

  if (alignment >= 0)
    sec->alignment_power = alignment;

  oldflags = bfd_section_flags (sec);
  if (oldflags == SEC_NO_FLAGS)
    {
      /* Set section flags for a new section just created by subseg_new.
         Provide a default if no flags were parsed.  */
      if (flags == SEC_NO_FLAGS)
	flags = SEC_HAS_CONTENTS | SEC_IN_MEMORY;

      if (!bfd_set_section_flags (sec, flags))
	as_warn (_("error setting flags for \"%s\": %s"),
		 bfd_section_name (sec),
		 bfd_errmsg (bfd_get_error ()));
    }
  else if (flags != SEC_NO_FLAGS)
    {
      /* This section's attributes have already been set.  Warn if the
         attributes don't match.  */
    }

  demand_empty_rest_of_line ();
}

#ifdef USE_EMULATIONS /* Support for an AOUT emulation.  */

/* When changed, make sure these table entries match the single-format
   definitions in obj-wasm.h.  */

const struct format_ops wasm_format_ops =
{
  bfd_target_unkown_flavour,
  1,	/* dfl_leading_underscore.  */
  0,	/* emit_section_symbols.  */
  0,	/* begin.  */
  0,	/* end.  */
  0,	/* app_file.  */
  NULL, /* assign_symbol */
  0,	/* frob_symbol. */
  0,	/* frob_file.  */
  0,	/* frob_file_before_adjust.  */
  0,	/* frob_file_before_fix.  */
  0,	/* frob_file_after_relocs.  */
  0,	/* s_get_size.  */
  0,	/* s_set_size.  */
  0,	/* s_get_align.  */
  0,	/* s_set_align.  */
  0,	/* s_get_other.  */
  0,	/* s_set_other.  */
  0,	/* s_get_desc.  */
  0,	/* s_set_desc.  */
  0,	/* s_get_type.  */
  0,	/* s_set_type.  */
  0,	/* copy_symbol_attributes.  */
  0,	/* process_stab.  */
  0,	/* separate_stab_sections.  */
  0,	/* init_stab_section.  */
  0,	/* sec_sym_ok_for_reloc. */
  wasm_pop_insert,
  0,	/* ecoff_set_ext.  */
  0,	/* read_begin_hook.  */
  0,	/* symbol_new_hook.  */
  0,	/* symbol_clone_hook.  */
  0	/* adjust_symtab.  */
};

#endif /* USE_EMULATIONS */

static const pseudo_typeS wasm_pseudo_table[] =
{
  {"sect", obj_wasm_section, 0},
  {"sect.s", obj_wasm_section, 0},
  {"section", obj_wasm_section, 0},
  {"section.s", obj_wasm_section, 0},
  {NULL, NULL, 0}
};
