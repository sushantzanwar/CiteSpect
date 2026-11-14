#ifndef CITATION_ANALYSIS_H
#define CITATION_ANALYSIS_H

#include "paper.h"

#define CITATION_SELF 0
#define CITATION_CROSS 1
#define CITATION_UNKNOWN 2

#define RECIPROCAL_CONFIRMED 0
#define RECIPROCAL_NOT 1
#define RECIPROCAL_UNKNOWN 2

int classify_citation(const Paper *citing, const Paper *cited, int shared_index[], int *shared_count);

int analyze_paper_citations(const char *citing_id);

#endif
