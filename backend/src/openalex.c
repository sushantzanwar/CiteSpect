#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "openalex.h"
#include "config.h"

#define RESPONSE_FILE "openalex_response.json"
#define API_BASE "https://api.openalex.org/works/"
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
    size_t length = strlen(id);

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

static void id_from_url(const char *url, char *out, size_t size)
{
    const char *slash = strrchr(url, '/');
    const char *id = (slash != NULL) ? slash + 1 : url;

    strncpy(out, id, size - 1);
    out[size - 1] = '\0';
}

static int read_id(const char *value, char *id, size_t size)
{
    char url[128];

    if (!read_text(value, url, sizeof(url)))
        return 0;
    id_from_url(url, id, size);
    return 1;
}

static int add_author_from_json(const char *authorship, Paper *p)
{
    const char *author = find_key(authorship, "author");
    char id[PAPER_ID_LEN] = "";
    char name[AUTHOR_NAME_LEN] = "";

    if (author == NULL)
        return 1;
    read_id(find_key(author, "id"), id, sizeof(id));
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
        if (read_id(item, id, sizeof(id)) && !paper_add_reference(p, id)) {
            printf("Note: only the first %d referenced works are stored.\n", PAPER_MAX_REFS);
            return;
        }
        item = skip_separators(skip_value(item));
    }
}

static void read_doi(const char *root, const char *input_doi, char *doi, size_t size)
{
    char url[256];

    strncpy(doi, input_doi, size - 1);
    doi[size - 1] = '\0';
    if (!read_text(find_key(root, "doi"), url, sizeof(url)))
        return;
    strncpy(doi, strip_doi_prefix(url), size - 1);
    doi[size - 1] = '\0';
}

static int parse_work(const char *json, const char *input_doi, Paper *out)
{
    const char *root = skip_ws(json);
    const char *year_value;
    char id[PAPER_ID_LEN] = "";
    char title[PAPER_TITLE_LEN] = "(no title)";
    char doi[PAPER_DOI_LEN];

    if (*root != '{' || !read_id(find_key(root, "id"), id, sizeof(id)))
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

    code = parse_work(json, fallback_doi, out);
    free(json);
    return code;
}

int openalex_fetch_by_doi(const char *doi, Paper *out)
{
    char url[URL_SIZE];

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

    if (!is_valid_work_id(work_id))
        return OPENALEX_ERR_BAD_ID;

    snprintf(url, sizeof(url), "%s%s%s", API_BASE, work_id, ID_FIELDS);
    return fetch_url(url, "", out);
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
