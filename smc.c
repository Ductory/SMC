/**
 * @file smc.c
 * @brief Main part for Symbol Modifier for COFF (SMC).
 *
 * @author Dangfer
 * @date 2024-01-07
 * @version 1.0.0
 * 
 * Note:
 *     This tool is distributed in the hope that it will be useful, but WITHOUT
 *     ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *     FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * License:
 *     This program is free software; you can redistribute it and/or modify it
 *     under the terms of the GNU General Public License as published by the
 *     Free Software Foundation; either version 2 of the License, or (at your
 *     option) any later version.
 */


#include <stdio.h>
#include <windows.h>
#include "smclib.h"

static const char help[] =
	"Symbol Modifier for COFF (SMC)\n"
	"Usage: smc infile outfile old new [old new ...]\n"
	"where:\n"
	"  infile      is the name of the input COFF file.\n"
	"  outfile     is the name for the output COFF file with modified symbols.\n"
	"  old new     is a pair where 'old' is the original symbol name to be modified\n"
	"              and 'new' is the new symbol name.\n"
	"  @listfile   is an optional argument where 'listfile' is a file containing\n"
	"              multiple 'old new' pairs.\n";

/**
 * @brief Retrieve all symbol names from a COFF file symbol table and store
 *        them in a dictionary.
 *
 * This function allocates a dictionary object.
 * The caller is responsible for deleting the object.
 * 
 * @param symtab The symbol table of the COFF file.
 * @param nsym   Number of symbols in the symbol table.
 * @return Pointer to the dictionary containing the symbol names.
 */
static dict_t *
get_symbol_names(PIMAGE_SYMBOL symtab, size_t nsym)
{
	dict_t *dict = new_dict();
	// String table immediately follows symbol table.
	void *strtab = symtab + nsym;
	// Traverse symbol table.
	for (size_t i = 0; i < nsym; ++i) {
		PIMAGE_SYMBOL sym = &symtab[i];
		const char *s = sym->N.Name.Short ? sym->N.ShortName : strtab + sym->N.Name.Long;
		dict_add(dict, s, NULL);
		i += sym->NumberOfAuxSymbols;
	}
	return dict;
}

/**
 * @brief Modify a symbol name in the dictionary.
 *
 * @param dict Dictionary where the symbol names are stored.
 * @param old  Pointer to the string containing the old symbol name.
 * @param new  Pointer to the string containing the new symbol name.
 */
static inline void
change_symbol_name(dict_t *dict, const char *old, const char *new)
{
	if (!dict_add(dict, old, new))
		error("Cannot find symbol '%s'.", old);
}

int
main(int argc, char *argv[])
{
	if (argc < 3) {
		fputs(help, stderr);
		return 0;
	}
	// Set a Non-local jump.
	int code;
	if ((code = setjmp(_buf)))
		return code;
	// Read a COFF file and initialize essential information.
	void *file;
	read_file(argv[1], &file);
	PIMAGE_FILE_HEADER head   = file;
	PIMAGE_SYMBOL      symtab = file + head->PointerToSymbolTable;
	DWORD              nsym   = head->NumberOfSymbols;
	dict_t            *dict   = get_symbol_names(symtab, nsym);
	// Traverse all 'old new' pairs.
	for (int i = 3; i < argc;) {
		if (argv[i][0] == '@') { // listfile
			FILE *fp = fopen(argv[i] + 1, "r");
			char old[256], new[256];
			while (fscanf(fp, "%s%s", old, new) == 2)
				change_symbol_name(dict, old, new);
			fclose(fp);
			++i;
		} else {
			change_symbol_name(dict, argv[i], argv[i + 1]);
			i += 2;
		}
	}
	// Save changes to file.
	FILE *fp = fopen(argv[2], "wb");
	fwrite(file, head->PointerToSymbolTable, 1, fp);
	buf_t *buf = new_buf();
	buf->cnt = 4; // Skip string table length.
	PIMAGE_SYMBOL sym = symtab;
	for (size_t e = 0; e < dict->count; ++e) {
		entry_t *entry = dict->entries[e];
		const char *s = entry->val ? : entry->key;
		size_t len = strlen(s);
		if (len <= 8) {
			strncpy((char*)sym->N.ShortName, s, 8);
		} else {
			size_t offset = buf_cat(buf, s);
			sym->N.Name.Short = 0;
			sym->N.Name.Long  = offset;
		}
		sym += sym->NumberOfAuxSymbols + 1;
	}
	*(DWORD*)buf->buf = buf->cnt;
	fwrite(symtab, IMAGE_SIZEOF_SYMBOL, nsym, fp);
	fwrite(buf->buf, buf->cnt, 1, fp);
	fclose(fp);
}