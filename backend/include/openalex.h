#ifndef OPENALEX_H
#define OPENALEX_H

#include "paper.h"

#define OPENALEX_OK 0
#define OPENALEX_ERR_BAD_DOI 1
#define OPENALEX_ERR_REQUEST 2
#define OPENALEX_ERR_READ 3
#define OPENALEX_ERR_PARSE 4
#define OPENALEX_ERR_BAD_ID 5

int openalex_fetch_by_doi(const char *doi, Paper *out);

int openalex_fetch_by_id(const char *work_id, Paper *out);

const char *openalex_error_message(int code);

#endif
