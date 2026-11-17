#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "author_hash.h"

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

void author_hash_init(AuthorHash *ht)
{
    int i;

    if (ht == NULL)
        return;

    for (i = 0; i < AUTHOR_HASH_BUCKETS; i++)
        ht->buckets[i] = NULL;
}

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

const AuthorNode *author_hash_lookup(const AuthorHash *ht,
                                     const char *author_id)
{
    return hash_search(ht, author_id);
}

int hash_insert(AuthorHash *ht,
                const char *author_id,
                const char *author_name)
{
    unsigned int idx;
    AuthorNode *node;

    if (ht == NULL || author_id == NULL || author_id[0] == '\0' ||
        strlen(author_id) >= PAPER_ID_LEN)
        return 0;

    if (hash_search(ht, author_id) != NULL)
        return 1;

    node = (AuthorNode *)malloc(sizeof(AuthorNode));
    if (node == NULL) {
        fprintf(stderr, "hash_insert: malloc failed\n");
        return 0;
    }

    strncpy(node->author_id,   author_id,   PAPER_ID_LEN   - 1);
    strncpy(node->author_name, author_name ? author_name : "",AUTHOR_NAME_LEN - 1);
    node->author_id  [PAPER_ID_LEN   - 1] = '\0';
    node->author_name[AUTHOR_NAME_LEN - 1] = '\0';

    idx           = hash_author_id(author_id);
    node->next    = ht->buckets[idx];
    ht->buckets[idx] = node;

    return 1;
}

int author_hash_insert(AuthorHash *ht,
                       const char *author_id,
                       const char *author_name)
{
    return hash_insert(ht, author_id, author_name);
}

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

void author_hash_free(AuthorHash *ht)
{
    hash_free(ht);
}
