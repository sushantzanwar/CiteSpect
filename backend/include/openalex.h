#ifndef OPENALEX_H
#define OPENALEX_H

#include "paper.h"

#define OPENALEX_OK 0
#define OPENALEX_ERR_BAD_DOI 1
#define OPENALEX_ERR_REQUEST 2
#define OPENALEX_ERR_READ 3
#define OPENALEX_ERR_PARSE 4
#define OPENALEX_ERR_BAD_ID 5

#define OPENALEX_CITERS_PER_PAGE 200
#define OPENALEX_MAX_CITER_PAGES 5

typedef struct {
    char ids[PAPER_MAX_REFS][PAPER_ID_LEN];
    int count;
    int total_available;
    int is_complete;
    int is_truncated;
    int api_succeeded;
} CiterFetchResult;

int openalex_parse_work(const char *json, const char *input_doi, Paper *out);

int openalex_parse_citing_page(const char *json,
                               char out_ids[][PAPER_ID_LEN],
                               int current_count,
                               int max_ids,
                               int *page_items_parsed,
                               int *meta_total_count,
                               int *truncated);

int openalex_collect_citing_pages(const char *const page_jsons[],
                                  int num_pages_provided,
                                  int max_pages_limit,
                                  int per_page,
                                  CiterFetchResult *out);

int openalex_fetch_by_doi(const char *doi, Paper *out);

int openalex_fetch_by_id(const char *work_id, Paper *out);

int openalex_fetch_citing_ids(const char *work_id,
                              int max_pages,
                              CiterFetchResult *out);

const char *openalex_error_message(int code);

#endif
