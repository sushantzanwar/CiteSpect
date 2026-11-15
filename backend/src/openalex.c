#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "openalex.h"
#include "config.h"

#define RESPONSE_FILE "openalex_response.json"
#define API_BASE "https://api.openalex.org/works/"
#define API_WORKS_LIST "https://api.openalex.org/works"
#define API_FIELDS "?select=id,doi,title,publication_year,authorships,referenced_works"
#define ID_FIELDS "?select=id,doi,title,publication_year,authorships,referenced_works"
#define DOI_PREFIX "https://doi.org/"
#define URL_SIZE 512

static const char *skip_ws(const char *p)
{
    while (isspace((unsigned char)*p))
        p++;
    return p;
}

static const char *skip_separators(const char *p)
{
    while (isspace((unsigned char)*p) || *p == ',')
        p++;
    return p;
}

static int char_length(const char *p)
{
    return (*p == '\\' && p[1] != '\0') ? 2 : 1;
}

static const char *skip_string(const char *p)
{
    p++;
    while (*p != '\0' && *p != '"')
        p += char_length(p);
    return (*p == '"') ? p + 1 : p;
}

static int depth_change(char c)
{
    if (c == '{' || c == '[')
        return 1;
    if (c == '}' || c == ']')
        return -1;
    return 0;
}

static const char *skip_container(const char *p)
{
    int depth = 0;

    do {
        if (*p == '"') {
            p = skip_string(p);
            continue;
        }
        depth += depth_change(*p);
        p++;
    } while (depth > 0 && *p != '\0');
    return p;
}

static int is_value_end(char c)
{
    return c == ',' || c == '}' || c == ']' || isspace((unsigned char)c);
}

static const char *skip_literal(const char *p)
{
    while (*p != '\0' && !is_value_end(*p))
        p++;
    return p;
}

static const char *skip_value(const char *p)
{
    p = skip_ws(p);
    if (*p == '"')
        return skip_string(p);
    if (*p == '{' || *p == '[')
        return skip_container(p);
    return skip_literal(p);
}

static int utf8_encode(unsigned int cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
}

static int hex_value(char c)
{
    if (isdigit((unsigned char)c))
        return c - '0';
    if (isxdigit((unsigned char)c))
        return tolower((unsigned char)c) - 'a' + 10;
    return -1;
}

static int read_hex4(const char *p, unsigned int *cp)
{
    int i, digit;

    *cp = 0;
    for (i = 1; i <= 4; i++) {
        digit = hex_value(p[i]);
        if (digit < 0)
            return 0;
        *cp = *cp * 16 + (unsigned int)digit;
    }
    return 1;
}

static unsigned int replace_surrogate(unsigned int cp)
{
    return (cp >= 0xD800 && cp <= 0xDFFF) ? '?' : cp;
}

static char simple_escape(char c)
{
    if (c == 'n')
        return '\n';
    if (c == 't')
        return '\t';
    if (c == 'r')
        return '\r';
    return c;
}

static int read_escape(const char **pp, char *buf)
{
    const char *p = *pp + 1;
    unsigned int cp;
    int len = 1;

    if (*p == '\0') {
        *pp = p - 1;
        return 0;
    }
    if (*p == 'u' && read_hex4(p, &cp)) {
        len = utf8_encode(replace_surrogate(cp), buf);
        p += 4;
    } else {
        buf[0] = simple_escape(*p);
    }
    *pp = p;
    return len;
}

static void append_bytes(char *out, size_t *n, size_t size, const char *bytes, int len)
{
    if (*n + (size_t)len >= size)
        return;
    memcpy(out + *n, bytes, (size_t)len);
    *n += (size_t)len;
}

static int read_string(const char *p, char *out, size_t size)
{
    size_t n = 0;
    char buf[4];
    int len;

    p = skip_ws(p);
    if (*p != '"')
        return 0;

    for (p++; *p != '\0' && *p != '"'; p++) {
        if (*p == '\\') {
            len = read_escape(&p, buf);
        } else {
            buf[0] = *p;
            len = 1;
        }
        append_bytes(out, &n, size, buf, len);
    }
    if (*p != '"')
        return 0;
    out[n] = '\0';
    return 1;
}

static const char *read_key(const char *p, char *name, size_t size)
{
    if (*p != '"')
        return NULL;
    if (!read_string(p, name, size))
        return NULL;
    p = skip_ws(skip_string(p));
    if (*p != ':')
        return NULL;
    return skip_ws(p + 1);
}

static const char *find_key(const char *obj, const char *key)
{
    char name[64];
    const char *value;
    const char *p = skip_ws(obj);

    if (*p != '{')
        return NULL;
    p++;

    for (;;) {
        value = read_key(skip_separators(p), name, sizeof(name));
        if (value == NULL)
            return NULL;
        if (strcmp(name, key) == 0)
            return value;
        p = skip_value(value);
    }
}

static const char *array_start(const char *obj, const char *key)
{
    const char *value = find_key(obj, key);

    if (value == NULL || *value != '[')
        return NULL;
    return skip_ws(value + 1);
}

static int read_text(const char *value, char *out, size_t size)
{
    return value != NULL && read_string(value, out, size);
}

static const char *strip_doi_prefix(const char *doi)
{
    if (strncmp(doi, DOI_PREFIX, strlen(DOI_PREFIX)) == 0)
        return doi + strlen(DOI_PREFIX);
    if (strncmp(doi, "doi:", 4) == 0)
        return doi + 4;
    return doi;
}

static int is_safe_text(const char *s, const char *extra)
{
    if (s[0] == '\0')
        return 0;
    for (; *s != '\0'; s++) {
        if (!isalnum((unsigned char)*s) && strchr(extra, *s) == NULL)
            return 0;
    }
    return 1;
}

static int is_valid_work_id(const char *id)
{
    size_t length;

    if (id == NULL)
        return 0;
    length = strlen(id);
    if (id[0] != 'W' || length < 2 || length >= PAPER_ID_LEN)
        return 0;
    return strspn(id + 1, "0123456789") == length - 1;
}

static int build_command(char *command, size_t size, const char *url)
{
    const char *key = config_get_openalex_key();
    int written;

    if (key != NULL && !is_safe_text(key, "-_"))
        return 0;

    if (key != NULL)
        written = snprintf(command, size, "curl -s -f -L -o %s \"%s&api_key=%s\"",
                           RESPONSE_FILE, url, key);
    else
        written = snprintf(command, size, "curl -s -f -L -o %s \"%s\"",
                           RESPONSE_FILE, url);
    return written > 0 && (size_t)written < size;
}

static int download_work(const char *url)
{
    char command[1024];

    if (!build_command(command, sizeof(command), url))
        return OPENALEX_ERR_REQUEST;
    return (system(command) == 0) ? OPENALEX_OK : OPENALEX_ERR_REQUEST;
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;
    char *text;
    size_t got;

    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);

    text = (size > 0) ? malloc((size_t)size + 1) : NULL;
    if (text == NULL) {
        fclose(f);
        return NULL;
    }
    got = fread(text, 1, (size_t)size, f);
    text[got] = '\0';
    fclose(f);
    return text;
}

static int id_from_url(const char *url, char *out, size_t size)
{
    const char *slash;
    const char *id;

    if (url == NULL || out == NULL || size == 0)
        return 0;
    slash = strrchr(url, '/');
    id = (slash != NULL) ? slash + 1 : url;
    if (id[0] == '\0' || strlen(id) >= size)
        return 0;
    snprintf(out, size, "%s", id);
    return 1;
}

static int read_id(const char *value, char *id, size_t size)
{
    char url[128];

    if (!read_text(value, url, sizeof(url)))
        return 0;
    return id_from_url(url, id, size);
}

static int add_author_from_json(const char *authorship, Paper *p)
{
    const char *author = find_key(authorship, "author");
    char id[PAPER_ID_LEN] = "";
    char name[AUTHOR_NAME_LEN] = "";

    if (author == NULL)
        return 1;
    if (!read_id(find_key(author, "id"), id, sizeof(id)))
        return 1;
    if (!read_text(find_key(author, "display_name"), name, sizeof(name)))
        return 1;
    return paper_add_author(p, id, name);
}

static void parse_authors(const char *json, Paper *p)
{
    const char *item = array_start(json, "authorships");

    while (item != NULL && *item == '{') {
        if (!add_author_from_json(item, p)) {
            printf("Note: only the first %d authors are stored.\n", PAPER_MAX_AUTHORS);
            return;
        }
        item = skip_separators(skip_value(item));
    }
}

static void parse_references(const char *json, Paper *p)
{
    const char *item = array_start(json, "referenced_works");
    char id[PAPER_ID_LEN];

    while (item != NULL && *item == '"') {
        if (read_id(item, id, sizeof(id)) && is_valid_work_id(id) &&
            !paper_add_reference(p, id)) {
            printf("Note: only the first %d referenced works are stored.\n", PAPER_MAX_REFS);
            return;
        }
        item = skip_separators(skip_value(item));
    }
}

static void read_doi(const char *root, const char *input_doi, char *doi, size_t size)
{
    char url[256];

    snprintf(doi, size, "%s", (input_doi != NULL) ? input_doi : "");
    if (!read_text(find_key(root, "doi"), url, sizeof(url)))
        return;
    snprintf(doi, size, "%s", strip_doi_prefix(url));
}

int openalex_parse_work(const char *json, const char *input_doi, Paper *out)
{
    const char *root;
    const char *year_value;
    char id[PAPER_ID_LEN] = "";
    char title[PAPER_TITLE_LEN] = "(no title)";
    char doi[PAPER_DOI_LEN];

    if (json == NULL || out == NULL)
        return OPENALEX_ERR_PARSE;

    root = skip_ws(json);
    if (*root != '{' || !read_id(find_key(root, "id"), id, sizeof(id)) ||
        !is_valid_work_id(id))
        return OPENALEX_ERR_PARSE;

    read_text(find_key(root, "title"), title, sizeof(title));
    read_doi(root, input_doi, doi, sizeof(doi));
    year_value = find_key(root, "publication_year");

    paper_init(out, id, title, (year_value != NULL) ? atoi(year_value) : 0, doi);
    parse_authors(root, out);
    parse_references(root, out);
    return OPENALEX_OK;
}

static int fetch_url(const char *url, const char *fallback_doi, Paper *out)
{
    char *json;
    int code;

    code = download_work(url);
    if (code != OPENALEX_OK) {
        remove(RESPONSE_FILE);
        return code;
    }

    json = read_file(RESPONSE_FILE);
    remove(RESPONSE_FILE);
    if (json == NULL)
        return OPENALEX_ERR_READ;

    code = openalex_parse_work(json, fallback_doi, out);
    free(json);
    return code;
}

int openalex_fetch_by_doi(const char *doi, Paper *out)
{
    char url[URL_SIZE];

    if (doi == NULL || out == NULL)
        return OPENALEX_ERR_BAD_DOI;

    doi = strip_doi_prefix(doi);
    if (!is_safe_text(doi, "-._;()/:"))
        return OPENALEX_ERR_BAD_DOI;

    if (snprintf(url, sizeof(url), "%s%s%s%s", API_BASE, DOI_PREFIX, doi, API_FIELDS) >= (int)sizeof(url))
        return OPENALEX_ERR_BAD_DOI;

    return fetch_url(url, doi, out);
}

int openalex_fetch_by_id(const char *work_id, Paper *out)
{
    char url[URL_SIZE];

    if (out == NULL || !is_valid_work_id(work_id))
        return OPENALEX_ERR_BAD_ID;

    snprintf(url, sizeof(url), "%s%s%s", API_BASE, work_id, ID_FIELDS);
    return fetch_url(url, "", out);
}

static int parse_nonneg_int(const char *p, int *out)
{
    long val = 0;

    if (p == NULL || out == NULL)
        return 0;
    p = skip_ws(p);
    if (!isdigit((unsigned char)*p))
        return 0;
    while (isdigit((unsigned char)*p)) {
        int digit = *p - '0';
        if (val > (1000000000L - digit) / 10L)
            return 0;
        val = val * 10L + digit;
        p++;
    }
    if (!is_value_end(*p))
        return 0;
    *out = (int)val;
    return 1;
}

static int parse_citing_results_array(const char *results,
                                      char out_ids[][PAPER_ID_LEN],
                                      int current_count,
                                      int max_ids,
                                      int *page_items_parsed,
                                      int *truncated)
{
    const char *p = skip_ws(results + 1);
    int stored = 0;

    if (*p == ']')
        return OPENALEX_OK;

    while (*p != '\0') {
        const char *item_end;
        char id[PAPER_ID_LEN];

        if (*p != '{')
            return OPENALEX_ERR_PARSE;
        item_end = skip_container(p);
        if (item_end <= p || item_end[-1] != '}')
            return OPENALEX_ERR_PARSE;
        if (!read_id(find_key(p, "id"), id, sizeof(id)) || !is_valid_work_id(id))
            return OPENALEX_ERR_PARSE;

        (*page_items_parsed)++;
        if (current_count + stored < max_ids) {
            snprintf(out_ids[current_count + stored], PAPER_ID_LEN, "%s", id);
            stored++;
        } else {
            *truncated = 1;
        }

        p = skip_ws(item_end);
        if (*p == ']')
            return OPENALEX_OK;
        if (*p != ',')
            return OPENALEX_ERR_PARSE;
        p = skip_ws(p + 1);
        if (*p == ']' || *p == '\0')
            return OPENALEX_ERR_PARSE;
    }
    return OPENALEX_ERR_PARSE;
}

int openalex_parse_citing_page(const char *json,
                               char out_ids[][PAPER_ID_LEN],
                               int current_count,
                               int max_ids,
                               int *page_items_parsed,
                               int *meta_total_count,
                               int *truncated)
{
    const char *root;
    const char *end;
    const char *meta;
    const char *results;

    if (json == NULL || out_ids == NULL || current_count < 0 || max_ids <= 0 ||
        page_items_parsed == NULL || meta_total_count == NULL || truncated == NULL)
        return OPENALEX_ERR_PARSE;

    *page_items_parsed = 0;
    *meta_total_count = -1;
    *truncated = 0;

    root = skip_ws(json);
    if (*root != '{')
        return OPENALEX_ERR_PARSE;
    end = skip_container(root);
    if (end <= root || end[-1] != '}')
        return OPENALEX_ERR_PARSE;

    meta = find_key(root, "meta");
    if (meta == NULL || *meta != '{')
        return OPENALEX_ERR_PARSE;
    if (!parse_nonneg_int(find_key(meta, "count"), meta_total_count))
        return OPENALEX_ERR_PARSE;

    results = find_key(root, "results");
    if (results == NULL || *results != '[')
        return OPENALEX_ERR_PARSE;

    return parse_citing_results_array(results, out_ids, current_count, max_ids,
                                      page_items_parsed, truncated);
}

static void reset_citer_fetch_result(CiterFetchResult *out)
{
    out->count = 0;
    out->total_available = -1;
    out->is_complete = 0;
    out->is_truncated = 0;
    out->api_succeeded = 0;
}

static int apply_citing_page(const char *json,
                             int page,
                             int limit,
                             int per_page,
                             int *total_seen,
                             CiterFetchResult *out,
                             int *result_code)
{
    int page_items = 0;
    int meta_count = -1;
    int page_trunc = 0;
    int added;
    int code;

    code = openalex_parse_citing_page(json, out->ids, out->count, PAPER_MAX_REFS,
                                      &page_items, &meta_count, &page_trunc);
    if (code != OPENALEX_OK) {
        out->api_succeeded = 0;
        out->is_complete = 0;
        *result_code = code;
        return -1;
    }

    added = PAPER_MAX_REFS - out->count;
    if (page_items < added)
        added = page_items;
    if (added > 0)
        out->count += added;
    *total_seen += page_items;

    if (page == 1)
        out->total_available = meta_count;
    if (page_trunc)
        out->is_truncated = 1;

    if (out->total_available == 0 || *total_seen >= out->total_available) {
        out->api_succeeded = 1;
        out->is_complete = out->is_truncated ? 0 : 1;
        *result_code = OPENALEX_OK;
        return 0;
    }
    if (page_items == 0 || page_items < per_page ||
        out->count >= PAPER_MAX_REFS || page == limit) {
        out->api_succeeded = 1;
        out->is_truncated = 1;
        out->is_complete = 0;
        *result_code = OPENALEX_OK;
        return 0;
    }
    *result_code = OPENALEX_OK;
    return 1;
}

int openalex_collect_citing_pages(const char *const page_jsons[],
                                  int num_pages_provided,
                                  int max_pages_limit,
                                  int per_page,
                                  CiterFetchResult *out)
{
    int limit;
    int page;
    int total_seen = 0;
    int result_code = OPENALEX_OK;

    if (out == NULL)
        return OPENALEX_ERR_READ;
    reset_citer_fetch_result(out);
    if (per_page <= 0 || num_pages_provided < 0)
        return OPENALEX_ERR_READ;

    limit = (max_pages_limit > 0) ? max_pages_limit : OPENALEX_MAX_CITER_PAGES;
    for (page = 1; page <= limit; page++) {
        int step;

        if (page > num_pages_provided || page_jsons == NULL || page_jsons[page - 1] == NULL) {
            if (page == 1)
                return OPENALEX_ERR_READ;
            out->is_truncated = 1;
            out->is_complete = 0;
            out->api_succeeded = 1;
            break;
        }

        step = apply_citing_page(page_jsons[page - 1], page, limit, per_page,
                                 &total_seen, out, &result_code);
        if (step <= 0)
            return result_code;
    }
    return OPENALEX_OK;
}

int openalex_fetch_citing_ids(const char *work_id,
                              int max_pages,
                              CiterFetchResult *out)
{
    char url[URL_SIZE];
    int limit;
    int page;
    int total_seen = 0;
    int result_code = OPENALEX_OK;

    if (out == NULL)
        return OPENALEX_ERR_BAD_ID;
    reset_citer_fetch_result(out);
    if (!is_valid_work_id(work_id))
        return OPENALEX_ERR_BAD_ID;

    limit = (max_pages > 0) ? max_pages : OPENALEX_MAX_CITER_PAGES;
    for (page = 1; page <= limit; page++) {
        char *json;
        int code;
        int step;

        if (snprintf(url, sizeof(url), "%s?filter=cites:%s&select=id&per-page=%d&page=%d",
                     API_WORKS_LIST, work_id, OPENALEX_CITERS_PER_PAGE, page) >= (int)sizeof(url)) {
            if (page == 1)
                return OPENALEX_ERR_REQUEST;
            out->is_truncated = 1;
            out->is_complete = 0;
            return OPENALEX_ERR_REQUEST;
        }

        code = download_work(url);
        if (code != OPENALEX_OK) {
            remove(RESPONSE_FILE);
            if (page > 1) {
                out->is_truncated = 1;
                out->is_complete = 0;
            }
            return code;
        }

        json = read_file(RESPONSE_FILE);
        remove(RESPONSE_FILE);
        if (json == NULL) {
            if (page > 1) {
                out->is_truncated = 1;
                out->is_complete = 0;
            }
            return OPENALEX_ERR_READ;
        }

        step = apply_citing_page(json, page, limit, OPENALEX_CITERS_PER_PAGE,
                                 &total_seen, out, &result_code);
        free(json);
        if (step <= 0)
            return result_code;
    }
    return OPENALEX_OK;
}

const char *openalex_error_message(int code)
{
    static const char *const messages[] = {
        "OK",
        "Invalid DOI (empty or contains unsupported characters).",
        "Request failed: paper not found, no internet, or curl is missing.",
        "Could not read the OpenAlex response.",
        "Unexpected response format from OpenAlex.",
        "Invalid Work ID (it must look like W123456789)."
    };

    if (code < 0 || code >= (int)(sizeof(messages) / sizeof(messages[0])))
        return "Unknown error.";
    return messages[code];
}
