#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "paper_list.h"

#define LINE_SIZE 2048

PaperNode *paper_list_insert(PaperNode *head, const Paper *paper)
{
    PaperNode *node = malloc(sizeof(PaperNode));
    PaperNode *last;

    if (node == NULL)
        return head;
    node->paper = *paper;
    node->next = NULL;

    if (head == NULL)
        return node;
    last = head;
    while (last->next != NULL)
        last = last->next;
    last->next = node;
    return head;
}

PaperNode *paper_list_search(PaperNode *head, const char *paper_id)
{
    PaperNode *current = head;

    while (current != NULL) {
        if (strcmp(current->paper.paper_id, paper_id) == 0) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

int paper_list_count(const PaperNode *head)
{
    int count = 0;
    const PaperNode *current = head;

    while (current != NULL) {
        count++;
        current = current->next;
    }

    return count;
}

static const char *read_field(const char *p, char *out, size_t size)
{
    size_t len = strcspn(p, ",\r\n");

    if (len >= size)
        len = size - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return p + strcspn(p, ",\r\n");
}

static int is_closing_quote(const char *p)
{
    return p[0] == '"' && p[1] != '"';
}

static const char *read_title(const char *p, char *title, size_t size)
{
    size_t n = 0;

    while (*p != '\0' && !is_closing_quote(p)) {
        if (*p == '"')
            p++;
        if (n < size - 1)
            title[n++] = *p;
        p++;
    }
    title[n] = '\0';
    return p;
}

static int parse_line(const char *line, Paper *paper)
{
    char id[PAPER_ID_LEN];
    char title[PAPER_TITLE_LEN];
    char year[16];
    char doi[PAPER_DOI_LEN];
    const char *p = read_field(line, id, sizeof(id));

    if (p[0] != ',' || p[1] != '"')
        return 0;
    p = read_title(p + 2, title, sizeof(title));
    if (strncmp(p, "\",", 2) != 0)
        return 0;
    p = read_field(p + 2, year, sizeof(year));
    if (*p != ',')
        return 0;
    read_field(p + 1, doi, sizeof(doi));

    paper_init(paper, id, title, atoi(year), doi);
    return 1;
}

PaperNode *paper_list_load(const char *filename)
{
    char line[LINE_SIZE];
    Paper paper;
    PaperNode *head = NULL;
    FILE *f = fopen(filename, "r");

    if (f == NULL)
        return NULL;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (parse_line(line, &paper))
            head = paper_list_insert(head, &paper);
    }
    fclose(f);
    return head;
}

void paper_list_display(const PaperNode *head)
{
    int count = 0;

    for (; head != NULL; head = head->next) {
        count++;
        printf("%d. %s | %s | %d | %s\n", count, head->paper.paper_id,
               head->paper.title, head->paper.year, head->paper.doi);
    }
    printf("Total papers: %d\n", count);
}

void paper_list_free(PaperNode *head)
{
    PaperNode *next;

    while (head != NULL) {
        next = head->next;
        free(head);
        head = next;
    }
}
