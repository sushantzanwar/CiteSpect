/*
 * test_openalex_parser.c
 *
 * Offline unit tests for the production OpenAlex JSON parser
 * (openalex_parse_work in backend/src/openalex.c).
 * Uses synthetic JSON strings only; makes zero network requests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openalex.h"
#include "paper.h"

static int passed = 0;
static int failed = 0;

static void check(const char *name, int condition)
{
    if (condition) {
        printf("[PASS] %s\n", name);
        passed++;
    } else {
        printf("[FAIL] %s\n", name);
        failed++;
    }
}

static void test_valid_response_and_extractions(void)
{
    const char *json =
        "{\n"
        "  \"meta\": {\"count\": 1, \"flags\": [true, false, null]},\n"
        "  \"id\": \"https://openalex.org/W2741809807\",\n"
        "  \"doi\": \"https://doi.org/10.1038/nature14539\",\n"
        "  \"title\": \"Deep Learning Review\",\n"
        "  \"publication_year\": 2015,\n"
        "  \"authorships\": [\n"
        "    {\"author_position\": \"first\", \"author\": {\"id\": \"https://openalex.org/A5023888391\", \"display_name\": \"Yann LeCun\"}},\n"
        "    {\"author_position\": \"last\",  \"author\": {\"id\": \"https://openalex.org/A5001234567\", \"display_name\": \"Geoffrey Hinton\"}}\n"
        "  ],\n"
        "  \"referenced_works\": [\n"
        "    \"https://openalex.org/W2100495367\",\n"
        "    \"https://openalex.org/W2194775991\"\n"
        "  ]\n"
        "}";
    Paper p;
    int rc = openalex_parse_work(json, "10.0000/fallback", &p);

    check("T1: Valid OpenAlex JSON parses with OPENALEX_OK",
          rc == OPENALEX_OK && p.year == 2015 &&
          strcmp(p.title, "Deep Learning Review") == 0 &&
          strcmp(p.doi, "10.1038/nature14539") == 0);

    check("T2: Paper ID extracted from OpenAlex URL",
          strcmp(p.paper_id, "W2741809807") == 0);

    check("T3: Author IDs and display names extracted accurately",
          p.author_count == 2 &&
          strcmp(p.author_ids[0], "A5023888391") == 0 &&
          strcmp(p.authors[0], "Yann LeCun") == 0 &&
          strcmp(p.author_ids[1], "A5001234567") == 0 &&
          strcmp(p.authors[1], "Geoffrey Hinton") == 0);

    check("T4: Reference Work IDs extracted from URLs",
          p.reference_count == 2 &&
          strcmp(p.references[0], "W2100495367") == 0 &&
          strcmp(p.references[1], "W2194775991") == 0);
}

static void test_missing_and_empty_fields(void)
{
    const char *missing_json = "{\"id\": \"https://openalex.org/W100\"}";
    const char *empty_arrays_json =
        "{"
        "\"id\": \"https://openalex.org/W200\","
        "\"title\": \"Empty Arrays\","
        "\"doi\": \"doi:10.1000/test\","
        "\"publication_year\": 2024,"
        "\"authorships\": [],"
        "\"referenced_works\": []"
        "}";
    Paper p1, p2;

    check("T5: Missing optional fields use safe defaults and fallback DOI",
          openalex_parse_work(missing_json, "10.5555/fallback", &p1) == OPENALEX_OK &&
          strcmp(p1.paper_id, "W100") == 0 &&
          strcmp(p1.title, "(no title)") == 0 &&
          strcmp(p1.doi, "10.5555/fallback") == 0 &&
          p1.year == 0 &&
          p1.author_count == 0 &&
          p1.reference_count == 0);

    check("T6: Empty authorships and referenced_works arrays yield 0 counts",
          openalex_parse_work(empty_arrays_json, "", &p2) == OPENALEX_OK &&
          strcmp(p2.paper_id, "W200") == 0 &&
          strcmp(p2.doi, "10.1000/test") == 0 &&
          p2.author_count == 0 &&
          p2.reference_count == 0);
}

static void test_malformed_null_and_oversized(void)
{
    const char *over_root_id =
        "{\"id\": \"https://openalex.org/W1234567890123456789012345678901\"}";
    const char *mixed_ids_json =
        "{"
        "\"id\": \"https://openalex.org/W300\","
        "\"authorships\": ["
        "  {\"author\": {\"id\": \"https://openalex.org/A1234567890123456789012345678901\", \"display_name\": \"Oversized\"}},"
        "  {\"author\": {\"id\": \"\", \"display_name\": \"EmptyId\"}},"
        "  {\"author\": {\"id\": \"https://openalex.org/A42\", \"display_name\": \"Valid Author\"}}"
        "],"
        "\"referenced_works\": ["
        "  \"https://openalex.org/W1234567890123456789012345678901\","
        "  \"https://openalex.org/NOT_A_WORK_ID\","
        "  \"https://openalex.org/W400\""
        "]"
        "}";
    Paper p;

    check("T7: Malformed JSON inputs rejected with OPENALEX_ERR_PARSE",
          openalex_parse_work("", "", &p) == OPENALEX_ERR_PARSE &&
          openalex_parse_work("[]", "", &p) == OPENALEX_ERR_PARSE &&
          openalex_parse_work("{\"title\": \"No ID\"}", "", &p) == OPENALEX_ERR_PARSE &&
          openalex_parse_work("{\"id\": \"https://openalex.org/BAD\"}", "", &p) == OPENALEX_ERR_PARSE &&
          openalex_parse_work("{\"id\": \"https://openalex.org/W123", "", &p) == OPENALEX_ERR_PARSE &&
          openalex_parse_work("{\"id\" \"https://openalex.org/W123\"}", "", &p) == OPENALEX_ERR_PARSE);

    check("T8: NULL pointers handled safely across openalex functions",
          openalex_parse_work(NULL, "10.1/x", &p) == OPENALEX_ERR_PARSE &&
          openalex_parse_work("{\"id\":\"https://openalex.org/W1\"}", NULL, NULL) == OPENALEX_ERR_PARSE &&
          openalex_parse_work("{\"id\":\"https://openalex.org/W1\"}", NULL, &p) == OPENALEX_OK &&
          openalex_fetch_by_doi(NULL, &p) == OPENALEX_ERR_BAD_DOI &&
          openalex_fetch_by_doi("10.1/x", NULL) == OPENALEX_ERR_BAD_DOI &&
          openalex_fetch_by_id("W123", NULL) == OPENALEX_ERR_BAD_ID);

    check("T9: Oversized/malformed IDs rejected without aborting valid entries",
          openalex_parse_work(over_root_id, "", &p) == OPENALEX_ERR_PARSE &&
          openalex_parse_work(mixed_ids_json, "", &p) == OPENALEX_OK &&
          p.author_count == 1 &&
          strcmp(p.author_ids[0], "A42") == 0 &&
          strcmp(p.authors[0], "Valid Author") == 0 &&
          p.reference_count == 1 &&
          strcmp(p.references[0], "W400") == 0);
}

static void test_author_and_reference_boundaries(void)
{
    static char json[65536];
    static Paper p;
    size_t pos = 0;
    int i;

    pos += (size_t)snprintf(json + pos, sizeof(json) - pos,
                            "{\"id\":\"https://openalex.org/W500\",\"authorships\":[");
    for (i = 0; i < PAPER_MAX_AUTHORS + 5; i++) {
        pos += (size_t)snprintf(json + pos, sizeof(json) - pos,
                                "%s{\"author\":{\"id\":\"https://openalex.org/A%d\",\"display_name\":\"Auth %d\"}}",
                                (i > 0) ? "," : "", i + 1, i + 1);
    }
    pos += (size_t)snprintf(json + pos, sizeof(json) - pos, "],\"referenced_works\":[");
    for (i = 0; i < PAPER_MAX_REFS + 5; i++) {
        pos += (size_t)snprintf(json + pos, sizeof(json) - pos,
                                "%s\"https://openalex.org/W%d\"",
                                (i > 0) ? "," : "", i + 1000);
    }
    snprintf(json + pos, sizeof(json) - pos, "]}");

    check("T10: Author count capped cleanly at PAPER_MAX_AUTHORS (50)",
          openalex_parse_work(json, "", &p) == OPENALEX_OK &&
          p.author_count == PAPER_MAX_AUTHORS &&
          strcmp(p.author_ids[0], "A1") == 0 &&
          strcmp(p.author_ids[PAPER_MAX_AUTHORS - 1], "A50") == 0);

    check("T11: Reference count capped cleanly at PAPER_MAX_REFS (1000)",
          p.reference_count == PAPER_MAX_REFS &&
          strcmp(p.references[0], "W1000") == 0 &&
          strcmp(p.references[PAPER_MAX_REFS - 1], "W1999") == 0);
}

static void test_escaped_characters(void)
{
    const char *json =
        "{"
        "\"id\": \"https://openalex.org/W600\","
        "\"title\": \"Quote \\\"Slash \\\\ Tab\\tNewline\\nCR\\r \\u0041 \\u00e9 \\u2014 \\ud800\","
        "\"authorships\": ["
        "  {\"author\": {\"id\": \"https://openalex.org/A600\", \"display_name\": \"Ren\\u00e9 \\\"R\\\"\"}}"
        "]"
        "}";
    Paper p;

    check("T12: Escaped JSON characters and UTF-8 \\uXXXX sequences decoded",
          openalex_parse_work(json, "", &p) == OPENALEX_OK &&
          strcmp(p.title, "Quote \"Slash \\ Tab\tNewline\nCR\r A \xC3\xA9 \xE2\x80\x94 ?") == 0 &&
          p.author_count == 1 &&
          strcmp(p.authors[0], "Ren\xC3\xA9 \"R\"") == 0);
}

int main(void)
{
    printf("==============================================\n");
    printf("  CITESPECT OPENALEX JSON PARSER UNIT TESTS   \n");
    printf("==============================================\n\n");

    test_valid_response_and_extractions();
    test_missing_and_empty_fields();
    test_malformed_null_and_oversized();
    test_author_and_reference_boundaries();
    test_escaped_characters();

    printf("\n----------------------------------------------\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("----------------------------------------------\n");

    return (failed == 0) ? 0 : 1;
}

