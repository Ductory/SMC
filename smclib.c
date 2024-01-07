/**
 * @file smclib.c
 * @brief Library Support for Symbol Modifier for COFF (SMC).
 *
 * @author Dangfer
 * @date 2023-01-07
 * @version 1.0.0
 *
 * Note:
 *     This file is part of the SMC tool and is intended to be used in
 *     conjunction with the main application. It is not designed to be used
 *     independently.
 * 
 * License:
 *     This program is free software; you can redistribute it and/or modify it
 *     under the terms  of the GNU General Public License as published by the
 *     Free Software Foundation; either version 2 of the License, or (at your
 *     option) any later version.
 */


#include "smclib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>


/* Error handling */
jmp_buf _buf;

/**
 * @brief Prints an error message and performs a non-local jump.
 * 
 * @param fmt Error message format, analogous to printf.
 * @param ... Additional arguments for the format string.
 */
void __attribute__((noreturn))
error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);

	longjmp(_buf, 1);
}

/**
 * @brief Checks the validity of a pointer returned from memory allocation.
 *
 * @param ptr The pointer to be checked for NULL value.
 */
static inline void
check_ptr(void *ptr)
{
	if (!ptr)
		error("Memory allocation failed.");
}

/* File IO */
/**
 * @brief Opens a binary file, reads its contents into a buffer, and returns
 *        the file size.
 *
 * This function allocates memory for the content of the file and stores the
 * pointer in `buf`. The caller is responsible for freeing the allocated
 * memory.
 *
 * @param filename The name of the file to be opened.
 * @param buf      Pointer to the pointer of the buffer where the file
 *                 contents will be stored.
 * @return The size of the file in bytes.
 */
size_t
read_file(const char *filename, void **buf)
{
	if (!filename)
		error("Invalid filename.");
	FILE *fp = fopen(filename, "rb");
	if (!fp)
		error("Open file '%s' failed.", filename);

	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	rewind(fp);

	*buf = malloc(size);
	check_ptr(*buf);
	fread(*buf, size, 1, fp);

	fclose(fp);
	return size;
}


/* Dictionary */
/**
 * This dictionary is used within the context of smc (Symbol Modifier for COFF)
 * and is optimized for this use. The dictionary stores symbol names as keys
 * with their respective new symbols as values. If the value is NULL, the
 * original symbol name is to be used. If the value is not NULL, it indicates
 * that the symbol name should be modified to the value specified.
 * 
 * This dictionary does not support removing entries and hence does not contain
 * dummy entries.
 */

#define DICT_INIT_BITS 5
#define DICT_INIT_SIZE ((size_t)1 << DICT_INIT_BITS)

#define DICT_SIZE(dict) ((size_t)1 << (dict)->size_bits)
#define DICT_MASK(dict) (DICT_SIZE(dict) - 1)
#define NEXT_INDEX(dict,i) ((((i) * 5) + 1) & DICT_MASK(dict))
#define GET_ENTRY(dict,i) ((dict)->indices[i])
#define IS_ENTRY(ent,h,k) ((ent)->hash == h && strcmp((ent)->key, k) == 0)

#define IDX_EMPTY ((size_t)-1)

/**
 * @brief Creates and initializes a new dictionary object.
 *
 * @return A pointer to the newly created dictionary object.
 */
dict_t *
new_dict(void)
{
	dict_t *dict = malloc(sizeof(dict_t));
	check_ptr(dict);
	dict->size_bits = DICT_INIT_BITS;
	size_t size = DICT_SIZE(dict);
	dict->entries = malloc(size * sizeof(entry_t*));
	check_ptr(dict->entries);
	dict->indices = malloc(size * sizeof(size_t));
	check_ptr(dict->indices);
	dict->count = 0;
	memset(dict->indices, -1, size * sizeof(size_t));
	return dict;
}

/**
 * @brief Deletes a dictionary object and frees all associated memory.
 *
 * @param dict The dictionary object to delete.
 */
void
del_dict(dict_t *dict)
{
	// As the dictionary does not contain dummy entries, we just iterate over
	// the entries array.
	for (size_t e = 0; e < dict->count; ++e) {
		entry_t *entry = dict->entries[e];
		free((void*)entry->key);
		if (entry->key != entry->val) // The symbol name has been changed.
			free((void*)entry->val);
		free(entry);
	}
	free(dict->entries);
	free(dict->indices);
	free(dict);
}

/**
 * @brief Computes a hash value for the given key using shift operations.
 * 
 * @param key The key for which to compute the hash.
 * @return The computed hash value of the key.
 */
static hash_t
hash_key(key_t key)
{
	hash_t h = 1;
	const uint8_t *k = (typeof(k))key;
	while (*k)
		h += (h << 5) + (h >> 27) + *k++;
	return h;
}

/**
 * @brief Expands the storage capacity of the specified dictionary.
 *
 * @param dict The dictionary object to be expanded.
 */
static void
expand_dict(dict_t *dict)
{
	// Expand entries and indices arrays.
	size_t size = (size_t)1 << ++dict->size_bits;
	dict->entries = realloc(dict->entries, size * sizeof(entry_t*));
	check_ptr(dict->entries);
	dict->indices = realloc(dict->indices, size * sizeof(size_t));
	check_ptr(dict->indices);
	memset(dict->indices, -1, sizeof(size_t) * size);
	// Rehash all existing entries to the new indices array.
	for (size_t e = 0; e < dict->count; ++e) {
		hash_t hash = dict->entries[e]->hash;
		size_t i = hash & (size - 1);
		while (GET_ENTRY(dict, i) != IDX_EMPTY)
			i = NEXT_INDEX(dict, i);
		dict->indices[i] = e;
	}
}

/**
 * @brief Adds a key-value pair into the dictionary or updates an existing
 *        key's value.
 *
 * @param dict The dictionary to which the key-value pair should be added
 *             or updated.
 * @param key  The key for the new entry, representing the original symbol
 *             name.
 * @param val  The value for the entry, which should be NULL for new symbols.
 *             If the key already exists, `val` can be non-NULL to update the
 *             symbol's new name.
 *
 * @return true if the key-value pair was added or updated successfully; false
 *         if an error occurred.
 */
bool
dict_add(dict_t *dict, key_t key, val_t val)
{
	if (!dict)
		return false;
	hash_t hash = hash_key(key);
	size_t i = hash & DICT_MASK(dict), e;
	while ((e = GET_ENTRY(dict, i)) != IDX_EMPTY) {
		entry_t *entry = dict->entries[e];
		if (IS_ENTRY(entry, hash, key)) {
			if (entry->val) // Change symbol more than once.
				free((void*)entry->val);
			entry->val = strdup(val);
			check_ptr((void*)entry->val);
			return true;
		}
		i = NEXT_INDEX(dict, i);
	}
	if (val) // Cannot find the symbol.
		return false;
	entry_t *entry = malloc(sizeof(entry_t));
	check_ptr(entry);
	entry->hash = hash;
	entry->key = strdup(key);
	check_ptr((void*)entry->key);
	entry->val = NULL;
	dict->indices[i] = dict->count;
	dict->entries[dict->count++] = entry;
	if (dict->count > DICT_SIZE(dict) * 2 / 3)
		expand_dict(dict);
	return true;
}

/* Buffer */
/**
 * The buffer module provides a dynamic string buffer specifically designed to
 * facilitate fast and efficient string table construction where strings are
 * frequently appended to a growing buffer.
 */

#define BUF_INIT_SIZE 256

/**
 * @brief Creates and initializes a new buffer for string table construction.
 *
 * @return A pointer to the newly allocated buffer.
 */
buf_t *
new_buf(void)
{
	size_t size = BUF_INIT_SIZE;
	buf_t *buf  = malloc(sizeof(buf_t));
	check_ptr(buf);
	buf->size = size;
	buf->cnt  = 0;
	buf->buf  = malloc(size);
	check_ptr(buf->buf);
	return buf;
}

/**
 * @brief Deletes a buffer object.
 *
 * @param buf The buffer object to delete.
 */
void
del_buf(buf_t *buf)
{
	free(buf->buf);
	free(buf);
}

/**
 * @brief Concatenates a string to the end of the buffer, enlarging the buffer
 *        if necessary.
 *
 * @param buf The buffer to which the string will be concatenated.
 * @param s   The null-terminated string to be concatenated to the buffer.
 *
 * @return The offset in the buffer where the string was appended.
 */
size_t
buf_cat(buf_t *buf, const char *s)
{
	size_t len = strlen(s) + 1;
	while (buf->cnt + len > buf->size) // Enlarge buffer.
		buf->buf = realloc(buf->buf, buf->size <<= 1);
	memcpy(buf->buf + buf->cnt, s, len);
	size_t offset = buf->cnt;
	buf->cnt += len;
	return offset;
}