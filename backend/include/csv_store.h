#ifndef CSV_STORE_H
#define CSV_STORE_H

#include "paper.h"

#define PAPERS_CSV "data/papers.csv"
#define AUTHORS_CSV "data/authors.csv"
#define CITATIONS_CSV "data/citations.csv"

#define CSV_SAVED 0
#define CSV_ALREADY_SAVED 1
#define CSV_ERR_FILE 2

int csv_save_paper(const Paper *p);

int csv_save_citations(const Paper *p);

int paper_already_saved(const char *paper_id);

int csv_load_authors(const char *paper_id, Paper *out);

int csv_load_citations(const char *citing_id, char cited_ids[][PAPER_ID_LEN], int max);

int csv_load_citing_papers(const char *cited_id, char citing_ids[][PAPER_ID_LEN], int max);

int paper_has_citations_saved(const char *paper_id);

#endif
