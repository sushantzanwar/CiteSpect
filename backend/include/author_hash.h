#ifndef AUTHOR_HASH_H
#define AUTHOR_HASH_H

#include "paper.h"

#define AUTHOR_HASH_BUCKETS 61

typedef struct AuthorNode {
    char author_id[PAPER_ID_LEN];
    char author_name[AUTHOR_NAME_LEN];
    struct AuthorNode *next;
} AuthorNode;

typedef struct {
    AuthorNode *buckets[AUTHOR_HASH_BUCKETS];
} AuthorHash;

void author_hash_init(AuthorHash *ht);

unsigned int hash_author_id(const char *id);

int hash_insert(AuthorHash *ht,
                const char *author_id,
                const char *author_name);

const AuthorNode *hash_search(const AuthorHash *ht,
                              const char *author_id);

void hash_free(AuthorHash *ht);

int author_hash_insert(AuthorHash *ht,
                       const char *author_id,
                       const char *author_name);

void author_hash_build(AuthorHash *ht, const Paper *paper);

const AuthorNode *author_hash_lookup(const AuthorHash *ht,
                                     const char *author_id);

void author_hash_free(AuthorHash *ht);

#endif
