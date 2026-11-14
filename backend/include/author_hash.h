/*
 * author_hash.h
 *
 * Hash table with separate chaining for storing OpenAlex author IDs.
 * Used by citation_analysis.c to detect self-citations efficiently.
 *
 * One table is built from the citing paper's author IDs, then every cited
 * paper is checked against it.  Empty author IDs are never inserted or
 * matched.
 */

#ifndef AUTHOR_HASH_H
#define AUTHOR_HASH_H

#include "paper.h"   /* PAPER_ID_LEN, AUTHOR_NAME_LEN, PAPER_MAX_AUTHORS */

/* Number of buckets.  A prime slightly larger than PAPER_MAX_AUTHORS (50)
 * keeps the load factor low and spreads collisions well. */
#define AUTHOR_HASH_BUCKETS 61

/* One node in a chain inside a bucket. */
typedef struct AuthorNode {
    char author_id[PAPER_ID_LEN];
    char author_name[AUTHOR_NAME_LEN];
    struct AuthorNode *next;
} AuthorNode;

/* The hash table itself.  Declare as a local or static variable; always
 * initialise with author_hash_init() before use. */
typedef struct {
    AuthorNode *buckets[AUTHOR_HASH_BUCKETS];
} AuthorHash;

/* Initialise all bucket pointers to NULL.  Must be called before any
 * other function on this table. */
void author_hash_init(AuthorHash *ht);

/* 1. Calculate the hash index for an OpenAlex author ID. */
unsigned int hash_author_id(const char *id);

/* 2. Insert an author ID into the hash table. */
int hash_insert(AuthorHash *ht,
                const char *author_id,
                const char *author_name);

/* 3. Check whether an author ID exists in the hash table. */
const AuthorNode *hash_search(const AuthorHash *ht,
                              const char *author_id);

/* 4. Release allocated memory. */
void hash_free(AuthorHash *ht);

/* Insert a non-empty author_id/author_name pair into the table (alias for hash_insert).
 * Duplicate IDs are silently ignored (no double-counting).
 * Returns 1 on success, 0 if author_id is empty or malloc fails. */
int author_hash_insert(AuthorHash *ht,
                       const char *author_id,
                       const char *author_name);

/* Build the table from all valid (non-empty) author IDs of a Paper. */
void author_hash_build(AuthorHash *ht, const Paper *paper);

/* Look up an exact author_id string (alias for hash_search).
 * Returns a pointer to the matching AuthorNode, or NULL if not found.
 * Empty author_id always returns NULL. */
const AuthorNode *author_hash_lookup(const AuthorHash *ht,
                                     const char *author_id);

/* Free all dynamically allocated nodes (alias for hash_free). */
void author_hash_free(AuthorHash *ht);

#endif /* AUTHOR_HASH_H */
