/*
 * author_hash.c
 *
 * Hash table with separate chaining for OpenAlex author IDs.
 *
 * Design:
 *   - AUTHOR_HASH_BUCKETS (61) prime buckets.
 *   - Hash function: djb2 variant (multiply-and-XOR) modulo bucket count.
 *   - Each bucket is a singly-linked list (AuthorNode chain).
 *   - Duplicate author IDs are detected before insertion via strcmp;
 *     no duplicates are stored.
 *   - Empty author IDs ("") are never inserted or matched.
 *
 * Cyclomatic complexity of every function: <= 5.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "author_hash.h"

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

/*
 * djb2-style hash: multiply by 31 and XOR each byte, then reduce
 * modulo AUTHOR_HASH_BUCKETS.  The result is always in [0, BUCKETS).
 * Operation 1: hash_author_id()
 */
unsigned int hash_author_id(const char *id)
{
    unsigned int h = 5381u;
    const unsigned char *p;

    if (id == NULL)
        return 0;

    for (p = (const unsigned char *)id; *p != '\0'; p++)
        h = h * 31u ^ (unsigned int)(*p);

    return h % (unsigned int)AUTHOR_HASH_BUCKETS;
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

void author_hash_init(AuthorHash *ht)
{
    int i;

    if (ht == NULL)
        return;

    for (i = 0; i < AUTHOR_HASH_BUCKETS; i++)
        ht->buckets[i] = NULL;
}

/*
 * hash_search (Operation 3)
 *
 * Compute the bucket index, then walk the chain comparing with strcmp.
 * Returns the matching node pointer, or NULL.
 * An empty author_id always returns NULL immediately.
 */
const AuthorNode *hash_search(const AuthorHash *ht,
                              const char *author_id)
{
    unsigned int idx;
    const AuthorNode *node;

    if (ht == NULL || author_id == NULL || author_id[0] == '\0' ||
        strlen(author_id) >= PAPER_ID_LEN)
        return NULL;

    idx  = hash_author_id(author_id);
    node = ht->buckets[idx];

    while (node != NULL) {
        if (strcmp(node->author_id, author_id) == 0)
            return node;
        node = node->next;
    }
    return NULL;
}

/* Backwards compatibility alias */
const AuthorNode *author_hash_lookup(const AuthorHash *ht,
                                     const char *author_id)
{
    return hash_search(ht, author_id);
}

/*
 * hash_insert (Operation 2)
 *
 * Silently ignores empty IDs and duplicates.
 * Allocates a new AuthorNode and prepends it to the bucket chain.
 * Returns 1 on success, 0 on failure.
 */
int hash_insert(AuthorHash *ht,
                const char *author_id,
                const char *author_name)
{
    unsigned int idx;
    AuthorNode *node;

    /* Reject NULL table, empty IDs, or oversized IDs */
    if (ht == NULL || author_id == NULL || author_id[0] == '\0' ||
        strlen(author_id) >= PAPER_ID_LEN)
        return 0;

    /* Reject duplicates */
    if (hash_search(ht, author_id) != NULL)
        return 1;   /* already present — not an error */

    node = (AuthorNode *)malloc(sizeof(AuthorNode));
    if (node == NULL) {
        fprintf(stderr, "hash_insert: malloc failed\n");
        return 0;
    }

    strncpy(node->author_id,   author_id,   PAPER_ID_LEN   - 1);
    strncpy(node->author_name, author_name ? author_name : "",
            AUTHOR_NAME_LEN - 1);
    node->author_id  [PAPER_ID_LEN   - 1] = '\0';
    node->author_name[AUTHOR_NAME_LEN - 1] = '\0';

    idx           = hash_author_id(author_id);
    node->next    = ht->buckets[idx];
    ht->buckets[idx] = node;

    return 1;
}

/* Backwards compatibility alias */
int author_hash_insert(AuthorHash *ht,
                       const char *author_id,
                       const char *author_name)
{
    return hash_insert(ht, author_id, author_name);
}

/*
 * author_hash_build
 *
 * Populates the table from a Paper's author_ids / authors arrays.
 * Empty author IDs in the paper are skipped.
 * Call author_hash_init() on the table before calling this function.
 */
void author_hash_build(AuthorHash *ht, const Paper *paper)
{
    int i;

    if (ht == NULL || paper == NULL)
        return;

    for (i = 0; i < paper->author_count && i < PAPER_MAX_AUTHORS; i++) {
        if (paper->author_ids[i][0] == '\0')
            continue;
        hash_insert(ht, paper->author_ids[i], paper->authors[i]);
    }
}

/*
 * hash_free (Operation 4)
 *
 * Walks every bucket chain and frees each node.
 * After this call, the bucket pointers are reset to NULL so the table
 * can be safely re-initialised.
 */
void hash_free(AuthorHash *ht)
{
    int i;
    AuthorNode *node;
    AuthorNode *next;

    if (ht == NULL)
        return;

    for (i = 0; i < AUTHOR_HASH_BUCKETS; i++) {
        node = ht->buckets[i];
        while (node != NULL) {
            next = node->next;
            free(node);
            node = next;
        }
        ht->buckets[i] = NULL;
    }
}

/* Backwards compatibility alias */
void author_hash_free(AuthorHash *ht)
{
    hash_free(ht);
}
