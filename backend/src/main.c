#include "citation_analysis.h"
#include "citation_graph.h"
#include "config.h"
#include "csv_store.h"
#include "openalex.h"
#include "paper.h"
#include "paper_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_save_result(int code) {
  if (code == CSV_SAVED)
    printf("\nSaved to %s, %s and %s.\n", PAPERS_CSV, AUTHORS_CSV,
           CITATIONS_CSV);
  else if (code == CSV_ALREADY_SAVED)
    printf("\nThis paper is already in %s - nothing written.\n", PAPERS_CSV);
  else
    printf("\nError: could not write the CSV files (does the data/ folder "
           "exist?).\n");
}

static int read_line(const char *prompt, char *buffer, size_t size) {
  printf("%s", prompt);
  if (fgets(buffer, (int)size, stdin) == NULL)
    return 0;
  buffer[strcspn(buffer, "\r\n")] = '\0';
  return 1;
}

static void show_menu(void) {
  printf("\n========== CITESPECT ==========\n");
  printf("1. Display all papers\n");
  printf("2. Search paper by ID\n");
  printf("3. Add paper using DOI\n");
  printf("4. Analyze citations of a paper\n");
  printf("5. Exit\n");
  printf("6. Compare two papers (Bibliographic Coupling)\n");
}

static void search_paper(PaperNode *head) {
  char search_id[PAPER_ID_LEN];
  PaperNode *found;

  if (!read_line("Enter Paper ID to search: ", search_id, sizeof(search_id)))
    return;

  found = paper_list_search(head, search_id);
  if (found != NULL) {
    printf("\nPaper found:\n");
    printf("ID: %s\n", found->paper.paper_id);
    printf("Title: %s\n", found->paper.title);
    printf("Year: %d\n", found->paper.year);
  } else {
    printf("\nPaper not found.\n");
  }
}

static PaperNode *add_paper(PaperNode *head) {
  static Paper paper;
  char doi[PAPER_DOI_LEN];
  int code;

  if (!read_line("Enter a DOI (e.g. 10.1038/nature14539): ", doi, sizeof(doi)))
    return head;

  printf("\nSearching OpenAlex for DOI: %s\n\n", doi);
  code = openalex_fetch_by_doi(doi, &paper);
  if (code != OPENALEX_OK) {
    printf("Error: %s\n", openalex_error_message(code));
    return head;
  }
  CitationGraph *graph = citation_graph_create(paper.paper_id);
  for (int i = 0; i < paper.reference_count; i++) {
    citation_graph_add_reference(graph, paper.references[i]);
  }

  paper_display(&paper);
  citation_graph_display(graph);
  citation_graph_free(graph);
  code = csv_save_paper(&paper);
  print_save_result(code);
  if (code == CSV_SAVED)
    head = paper_list_insert(head, &paper);
  return head;
}

static void analyze_citations(void) {
  char citing_id[PAPER_ID_LEN];

  if (!read_line("Enter Paper ID to analyze: ", citing_id, sizeof(citing_id)))
    return;
  analyze_paper_citations(citing_id);
}

static void compare_coupling_menu(void) {
  char id_a[PAPER_ID_LEN];
  char id_b[PAPER_ID_LEN];

  if (!read_line("Enter Paper A ID: ", id_a, sizeof(id_a)))
    return;
  if (!read_line("Enter Paper B ID: ", id_b, sizeof(id_b)))
    return;
  compare_papers_coupling(id_a, id_b);
}

static void run_other_choice(int choice) {
  if (choice == 4)
    analyze_citations();
  else if (choice == 6)
    compare_coupling_menu();
  else if (choice != 5)
    printf("\nInvalid choice. Please enter 1-6.\n");
}

static PaperNode *run_choice(int choice, PaperNode *head) {
  if (choice == 1)
    paper_list_display(head);
  else if (choice == 2)
    search_paper(head);
  else if (choice == 3)
    head = add_paper(head);
  else
    run_other_choice(choice);
  return head;
}

static void print_field(const char *key, const char *value) {
  printf("%s=", key);
  for (; *value != '\0'; value++)
    putchar((*value == '\n' || *value == '\r') ? ' ' : *value);
  printf("\n");
}

static void print_result(const Paper *paper, const CitationGraph *graph,
                         int save_code) {
  const CitationEdge *edge;
  int i;

  printf("RESULT_START\n");
  print_field("PAPER_ID", paper->paper_id);
  print_field("TITLE", paper->title);
  printf("YEAR=%d\n", paper->year);
  print_field("DOI", paper->doi);
  printf("SAVE_STATUS=%d\n", save_code);
  printf("AUTHOR_COUNT=%d\n", paper->author_count);
  for (i = 0; i < paper->author_count; i++)
    print_field("AUTHOR", paper->authors[i]);
  printf("REFERENCE_COUNT=%d\n", paper->reference_count);
  for (i = 0; i < paper->reference_count; i++)
    print_field("REFERENCE", paper->references[i]);

  print_field("GRAPH_PAPER", graph->paper_id);
  for (edge = graph->references; edge != NULL; edge = edge->next)
    print_field("GRAPH_EDGE", edge->cited_paper_id);
  printf("RESULT_END\n");
}

static int run_doi_mode(const char *doi) {
  static Paper paper;
  CitationGraph *graph;
  int code = openalex_fetch_by_doi(doi, &paper);
  int i;

  if (code != OPENALEX_OK) {
    printf("ERROR=%s\n", openalex_error_message(code));
    return 1;
  }
  graph = citation_graph_create(paper.paper_id);
  if (graph == NULL) {
    printf("ERROR=Out of memory\n");
    return 1;
  }
  for (i = 0; i < paper.reference_count; i++)
    citation_graph_add_reference(graph, paper.references[i]);

  print_result(&paper, graph, csv_save_paper(&paper));
  citation_graph_free(graph);
  return 0;
}

static int run_menu(void) {
  PaperNode *paper_list;
  char line[16];
  int choice = 0;

  printf("=== CITESPECT - Research Paper Citation Analysis System ===\n\n");

  if (config_get_openalex_key() != NULL)
    printf("OpenAlex API key: found in %s\n", OPENALEX_KEY_ENV);
  else
    printf("OpenAlex API key: NOT set (requests will be anonymous and "
           "rate-limited)\n");

  paper_list = paper_list_load("data/papers.csv");
  if (paper_list == NULL)
    printf("No papers loaded from papers.csv (file missing or empty).\n");

  while (choice != 5) {
    printf("\nTotal papers: %d\n", paper_list_count(paper_list));
    show_menu();
    if (!read_line("Enter choice: ", line, sizeof(line)))
      break;
    choice = atoi(line);
    paper_list = run_choice(choice, paper_list);
  }

  paper_list_free(paper_list);
  printf("\nCITESPECT finished.\n");
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc >= 3 && strcmp(argv[1], "--doi") == 0)
    return run_doi_mode(argv[2]);
  if (argc >= 4 && strcmp(argv[1], "--coupling") == 0)
    return compare_papers_coupling(argv[2], argv[3]) ? 0 : 1;
  return run_menu();
}
