#include <stdio.h>
#include <string.h>
#include "paper.h"

static void copy_string(char *dest, const char *src, size_t size)
{
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
}

void paper_init(Paper *p, const char *id, const char *title, int year, const char *doi)
{
    copy_string(p->paper_id, id, PAPER_ID_LEN);
    copy_string(p->title, title, PAPER_TITLE_LEN);
    p->year = year;
    copy_string(p->doi, doi, PAPER_DOI_LEN);
    p->author_count = 0;
    p->reference_count = 0;
}

int paper_add_author(Paper *p, const char *author_id, const char *name)
{
    if (p->author_count >= PAPER_MAX_AUTHORS)
        return 0;
    copy_string(p->author_ids[p->author_count], author_id, PAPER_ID_LEN);
    copy_string(p->authors[p->author_count], name, AUTHOR_NAME_LEN);
    p->author_count++;
    return 1;
}

int paper_add_reference(Paper *p, const char *ref_id)
{
    if (p->reference_count >= PAPER_MAX_REFS)
        return 0;
    copy_string(p->references[p->reference_count], ref_id, PAPER_ID_LEN);
    p->reference_count++;
    return 1;
}

void paper_display(const Paper *p)
{
    int i;

    printf("Title    : %s\n", p->title);
    printf("Year     : %d\n", p->year);
    printf("OpenAlex : %s\n", p->paper_id);
    printf("DOI      : %s\n", p->doi);

    printf("Authors (%d):\n", p->author_count);
    for (i = 0; i < p->author_count; i++)
        printf("  %d. %s\n", i + 1, p->authors[i]);

    printf("Referenced works (%d):\n", p->reference_count);
    for (i = 0; i < p->reference_count; i++)
        printf("  %d. %s\n", i + 1, p->references[i]);
}
