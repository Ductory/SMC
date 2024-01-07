#ifndef _DICT_H
#define _DICT_H

#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

/* Error handling */
extern jmp_buf _buf;
void __attribute__((noreturn)) error(const char *fmt, ...);

/* File IO */
size_t read_file(const char *filename, void **buf);

/* Dictionary */
typedef size_t hash_t;
typedef const char *key_t;
typedef const char *val_t;
/**
 * @brief Represents an individual entry in a dictionary.
 */
struct entry_t {
	hash_t hash; ///< Hash value of the key.
	key_t  key;  ///< The actual key associated with the entry.
	val_t  val;  ///< The value associated with the key.
};
typedef struct entry_t entry_t;
/**
 * @brief Represents a hashtable-based dictionary.
 */
struct dict_t {
	entry_t **entries;   ///< Pointer to an array of entry pointers.
	size_t   *indices;   ///< Array of indices for the dictionary, used for efficient lookup.
	size_t    count;     ///< The number of entries currently in use.
	uint8_t   size_bits; ///< The base-2 logarithm of the size of the dictionary.
};
typedef struct dict_t dict_t;

dict_t *new_dict(void);
void del_dict(dict_t *dict);
val_t dict_query(dict_t *dict, key_t key);
bool dict_add(dict_t *dict, key_t key, val_t val);

/* Buffer */
/**
 * @brief Buffer utility for efficient string concatenation.
 */
typedef struct {
	size_t size; ///< Total size of the allocated buffer.
	size_t cnt;  ///< Current length of the content in the buffer.
	void  *buf;  ///< Pointer to the buffer's content.
} buf_t;

buf_t *new_buf(void);
void del_buf(buf_t *buf);
size_t buf_cat(buf_t *buf, const char *s);

#endif