#ifndef PAPER_H
#define PAPER_H

#define PAPER_ID_LEN 32
#define PAPER_TITLE_LEN 512
#define PAPER_DOI_LEN 128
#define AUTHOR_NAME_LEN 100
#define PAPER_MAX_AUTHORS 50
#define PAPER_MAX_REFS 1000

typedef struct {
    char paper_id[PAPER_ID_LEN];
    char title[PAPER_TITLE_LEN];
    int year;
    char doi[PAPER_DOI_LEN];

    char author_ids[PAPER_MAX_AUTHORS][PAPER_ID_LEN];
    char authors[PAPER_MAX_AUTHORS][AUTHOR_NAME_LEN];
    int author_count;

    char references[PAPER_MAX_REFS][PAPER_ID_LEN];
    int reference_count;
} Paper;

void paper_init(Paper *p, const char *id, const char *title, int year, const char *doi);

int paper_add_author(Paper *p, const char *author_id, const char *name);

int paper_add_reference(Paper *p, const char *ref_id);

void paper_display(const Paper *p);

#endif
