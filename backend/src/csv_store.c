#include <stdio.h>
#include <string.h>
#include "csv_store.h"

#define LINE_SIZE 2048

static const char PAPERS_HEADER[] = "paper_id,title,year,doi,openalex_id";
static const char AUTHORS_HEADER[] = "paper_id,author_id,author_name";
static const char CITATIONS_HEADER[] = "citing_paper_id,cited_paper_id";

static void write_csv_char(FILE *f, char c)
{
    if (c == '"')
        fputs("\"\"", f);
    else if (c == '\n' || c == '\r')
        fputc(' ', f);
    else
        fputc(c, f);
}

static void write_text_field(FILE *f, const char *text)
{
    fputc('"', f);
    for (; *text != '\0'; text++)
        write_csv_char(f, *text);
    fputc('"', f);
}

static FILE *open_csv(const char *path, const char *header)
{
    FILE *f = fopen(path, "a");

    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0)
        fprintf(f, "%s\n", header);
    return f;
}

static int build_id_prefix(const char *paper_id, char *prefix, size_t size)
{
    int written;

    if (paper_id == NULL || paper_id[0] == '\0' || strlen(paper_id) >= PAPER_ID_LEN)
        return 0;
    written = snprintf(prefix, size, "%s,", paper_id);
    return written > 0 && (size_t)written < size;
}

int paper_already_saved(const char *paper_id)
{
    char line[LINE_SIZE];
    char prefix[PAPER_ID_LEN + 1];
    int at_line_start = 1;
    FILE *f;

    if (!build_id_prefix(paper_id, prefix, sizeof(prefix)))
        return 0;

    f = fopen(PAPERS_CSV, "r");
    if (f == NULL)
        return 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        if (at_line_start && strncmp(line, prefix, strlen(prefix)) == 0) {
            fclose(f);
            return 1;
        }
        at_line_start = (strchr(line, '\n') != NULL);
    }
    fclose(f);
    return 0;
}

static int save_authors(const Paper *p)
{
    int i;
    FILE *f = open_csv(AUTHORS_CSV, AUTHORS_HEADER);

    if (f == NULL)
        return 0;
    for (i = 0; i < p->author_count; i++) {
        fprintf(f, "%s,%s,", p->paper_id, p->author_ids[i]);
        write_text_field(f, p->authors[i]);
        fputc('\n', f);
    }
    return fclose(f) == 0;
}

static int save_citations(const Paper *p)
{
    int i;
    FILE *f = open_csv(CITATIONS_CSV, CITATIONS_HEADER);

    if (f == NULL)
        return 0;
    if (p->reference_count == 0) {
        fprintf(f, "%s,\n", p->paper_id);
        return fclose(f) == 0;
    }
    for (i = 0; i < p->reference_count; i++)
        fprintf(f, "%s,%s\n", p->paper_id, p->references[i]);
    return fclose(f) == 0;
}

static int save_paper_row(const Paper *p)
{
    FILE *f = open_csv(PAPERS_CSV, PAPERS_HEADER);

    if (f == NULL)
        return 0;
    fprintf(f, "%s,", p->paper_id);
    write_text_field(f, p->title);
    fprintf(f, ",%d,%s,%s\n", p->year, p->doi, p->paper_id);
    return fclose(f) == 0;
}

static void copy_name(const char *text, char *name, size_t size)
{
    size_t length = strcspn(text, "\r\n");

    if (length >= 2 && text[0] == '"' && text[length - 1] == '"') {
        text++;
        length -= 2;
    }
    if (length >= size)
        length = size - 1;
    snprintf(name, size, "%.*s", (int)length, text);
}

static void add_author_from_line(const char *fields, Paper *out)
{
    char author_id[PAPER_ID_LEN];
    char name[AUTHOR_NAME_LEN];
    size_t id_length = strcspn(fields, ",");

    snprintf(author_id, sizeof(author_id), "%.*s", (int)id_length, fields);
    if (fields[id_length] == ',')
        copy_name(fields + id_length + 1, name, sizeof(name));
    else
        name[0] = '\0';
    paper_add_author(out, author_id, name);
}

int csv_load_authors(const char *paper_id, Paper *out)
{
    char line[LINE_SIZE];
    char prefix[PAPER_ID_LEN + 1];
    FILE *f;

    if (out == NULL || !build_id_prefix(paper_id, prefix, sizeof(prefix)))
        return 0;

    f = fopen(AUTHORS_CSV, "r");
    if (f == NULL)
        return 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, prefix, strlen(prefix)) == 0)
            add_author_from_line(line + strlen(prefix), out);
    }
    fclose(f);
    return 1;
}

static int extract_cited_id(const char *after_prefix, char *out)
{
    size_t len = strcspn(after_prefix, "\r\n");

    if (len == 0 || len >= PAPER_ID_LEN)
        return 0;
    snprintf(out, PAPER_ID_LEN, "%.*s", (int)len, after_prefix);
    return 1;
}

int csv_load_citations(const char *citing_id, char cited_ids[][PAPER_ID_LEN], int max)
{
    char line[LINE_SIZE];
    char prefix[PAPER_ID_LEN + 1];
    int count = 0;
    FILE *f;

    if (cited_ids == NULL || max <= 0 || !build_id_prefix(citing_id, prefix, sizeof(prefix)))
        return 0;

    f = fopen(CITATIONS_CSV, "r");
    if (f == NULL)
        return 0;

    while (count < max && fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, prefix, strlen(prefix)) == 0)
            count += extract_cited_id(line + strlen(prefix), cited_ids[count]);
    }
    fclose(f);
    return count;
}

int paper_has_citations_saved(const char *paper_id)
{
    char line[LINE_SIZE];
    char prefix[PAPER_ID_LEN + 1];
    FILE *f;

    if (!build_id_prefix(paper_id, prefix, sizeof(prefix)))
        return 0;

    f = fopen(CITATIONS_CSV, "r");
    if (f == NULL)
        return 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int csv_save_citations(const Paper *p)
{
    if (p == NULL || p->paper_id[0] == '\0')
        return CSV_ERR_FILE;
    if (paper_has_citations_saved(p->paper_id))
        return CSV_ALREADY_SAVED;
    return save_citations(p) ? CSV_SAVED : CSV_ERR_FILE;
}

int csv_save_paper(const Paper *p)
{
    if (paper_already_saved(p->paper_id))
        return CSV_ALREADY_SAVED;

    if (!save_authors(p) || !save_citations(p) || !save_paper_row(p))
        return CSV_ERR_FILE;
    return CSV_SAVED;
}
