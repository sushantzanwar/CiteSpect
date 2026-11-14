#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "citation_graph.h"

CitationGraph *citation_graph_create(const char *paper_id) {
  CitationGraph *graph = malloc(sizeof(CitationGraph));

  if (graph == NULL) {
    return NULL;
  }

  strcpy(graph->paper_id, paper_id);
  graph->references = NULL;
  graph->next = NULL;

  return graph;
}

int citation_graph_add_reference(CitationGraph *graph,
                                 const char *cited_paper_id) {
  CitationEdge *new_edge;
  CitationEdge *current;

  if (graph == NULL || cited_paper_id == NULL) {
    return 0;
  }

  new_edge = malloc(sizeof(CitationEdge));

  if (new_edge == NULL) {
    return 0;
  }

  strcpy(new_edge->cited_paper_id, cited_paper_id);
  new_edge->next = NULL;

  if (graph->references == NULL) {
    graph->references = new_edge;
    return 1;
  }

  current = graph->references;

  while (current->next != NULL) {
    current = current->next;
  }

  current->next = new_edge;

  return 1;
}

int citation_graph_has_reference(const CitationGraph *graph,
                                 const char *target_id) {
  const CitationEdge *current;

  if (graph == NULL || target_id == NULL || target_id[0] == '\0') {
    return 0;
  }

  current = graph->references;
  while (current != NULL) {
    if (strcmp(current->cited_paper_id, target_id) == 0) {
      return 1;
    }
    current = current->next;
  }

  return 0;
}

void citation_graph_display(const CitationGraph *graph) {
  const CitationEdge *current;

  if (graph == NULL) {
    return;
  }

  printf("\n========== CITATION GRAPH ==========\n");
  printf("Paper: %s\n", graph->paper_id);

  current = graph->references;

  while (current != NULL) {
    printf("  -> %s\n", current->cited_paper_id);
    current = current->next;
  }
}

void citation_graph_free(CitationGraph *graph) {
  CitationEdge *current;

  if (graph == NULL) {
    return;
  }

  current = graph->references;

  while (current != NULL) {
    CitationEdge *next = current->next;
    free(current);
    current = next;
  }

  free(graph);
}