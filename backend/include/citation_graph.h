#ifndef CITATION_GRAPH_H
#define CITATION_GRAPH_H

#include "paper.h"

typedef struct CitationEdge {
  char cited_paper_id[PAPER_ID_LEN];
  struct CitationEdge *next;
} CitationEdge;

typedef struct CitationGraph {
  char paper_id[PAPER_ID_LEN];
  CitationEdge *references;
  struct CitationGraph *next;
} CitationGraph;

CitationGraph *citation_graph_create(const char *paper_id);

int citation_graph_add_reference(CitationGraph *graph,
                                 const char *cited_paper_id);

int citation_graph_has_reference(const CitationGraph *graph,
                                 const char *target_id);

void citation_graph_display(const CitationGraph *graph);

void citation_graph_free(CitationGraph *graph);

#endif