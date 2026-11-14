#ifndef PAPER_LIST_H
#define PAPER_LIST_H

#include "paper.h"

typedef struct PaperNode {
    Paper paper;
    struct PaperNode *next;
} PaperNode;

PaperNode *paper_list_insert(PaperNode *head, const Paper *paper);

PaperNode *paper_list_load(const char *filename);

PaperNode *paper_list_search(PaperNode *head, const char *paper_id);

int paper_list_count(const PaperNode *head);

void paper_list_display(const PaperNode *head);

void paper_list_free(PaperNode *head);

#endif
