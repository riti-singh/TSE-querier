/*
 * querier.c - CS50 Tiny Search Engine 'querier' module
 *
 * The querier reads the index produced by the Indexer and the page files
 * produced by the Crawler, then interactively answers search queries
 * entered on stdin. It supports words and the operators "and" and "or",
 * where "and" has higher precedence than "or".
 *
 * Usage:
 *   ./querier pageDirectory indexFilename
 *
 * pageDirectory  - directory produced by crawler (contains .crawler and
 *                  files named 1,2,3,...)
 * indexFilename  - index file produced by indexer.
 *
 * Riti Singh, November 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>     // isatty
#include <limits.h>     // PATH_MAX

#include "counters.h"
#include "index.h"
#include "mem.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* fileno is POSIX, not in the C11 standard header, so declare it here. */
int fileno(FILE *stream);

/* local types */
/* docscore_t: pair of docID and its score for ranking. */
typedef struct docscore {
  int docID;
  int score;
} docscore_t;

/* two_counters_t: helper struct passed into counters_iterate. */
typedef struct two_counters {
  counters_t *a;
  counters_t *b;
} two_counters_t;

/* collect_arg_t: helper to build an array of docscore_t. */
typedef struct collect_arg {
  docscore_t *array;   // array into which we write
  int index;           // next free slot
} collect_arg_t;

/* function prototypes */
/* command-line handling */
static void parse_args(const int argc, char *argv[],
                       char **pageDirectory, char **indexFilename);

/* main loop helpers */
static void prompt(void);
static void query_loop(const char *pageDirectory, index_t *index);

/* parsing and syntax checking */
static bool tokenize_and_validate(char *line, char ***words_out,
                                  int *nwords_out);
static bool validate_tokens(char **words, const int nwords);
static bool is_operator(const char *word);

/* query evaluation */
static counters_t *evaluate_query(index_t *index, char **words,
                                  const int nwords);
static counters_t *evaluate_andsequence(index_t *index, char **words,
                                        const int nwords, const int start,
                                        int *end_out);

/* counters utilities */
static void counters_intersect(counters_t *dest, counters_t *src);
static void counters_union(counters_t *dest, counters_t *src);

/* iterate helpers for counters */
static void intersect_helper(void *arg, const int key, int count);
static void union_helper(void *arg, const int key, int count);
static void copy_helper(void *arg, const int key, int count);
static void zero_helper(void *arg, const int key, int count);

/* ranking and printing */
static void rank_and_print(counters_t *results, const char *pageDirectory);
static void count_nonzero(void *arg, const int key, int count);
static void collect_nonzero(void *arg, const int key, int count);
static int  cmp_docscore_desc(const void *a, const void *b);

/* reading URL from page files */
static char *get_url(const char *pageDirectory, const int docID);

/* main */
/* Parse arguments, load the index, and start the query loop. */
int
main(const int argc, char *argv[])
{
  char *pageDirectory = NULL;
  char *indexFilename = NULL;

  parse_args(argc, argv, &pageDirectory, &indexFilename);

  index_t *index = index_new(256);
  if (index == NULL) {
    fprintf(stderr, "querier: cannot allocate index\n");
    exit(2);
  }

  FILE *fp = fopen(indexFilename, "r");
  if (fp == NULL) {
    fprintf(stderr, "querier: cannot open index file '%s'\n", indexFilename);
    index_delete(index);
    exit(2);
  }
  int status = index_load(fp, index); 
  fclose(fp);

  if (status != 0) {
    fprintf(stderr, "querier: errors encountered while loading index file\n");
  }

  query_loop(pageDirectory, index);

  index_delete(index);
  return 0;
}

/* parse_args */
/* Parse and validate the command-line arguments.
 *
 * We expect:
 *   ./querier pageDirectory indexFilename
 *
 * We exit non-zero if:
 *   - wrong number of arguments
 *   - pageDirectory is not a crawler-produced directory
 *   - indexFilename is not readable
 */
static void
parse_args(const int argc, char *argv[],
           char **pageDirectory, char **indexFilename)
{
  if (argv == NULL || pageDirectory == NULL || indexFilename == NULL) {
    fprintf(stderr, "querier: parse_args got NULL parameter\n");
    exit(1);
  }

  if (argc != 3) {
    fprintf(stderr, "usage: %s pageDirectory indexFilename\n", argv[0]);
    exit(1);
  }

  *pageDirectory = argv[1];
  *indexFilename = argv[2];

  // validate pageDirectory by checking for pageDirectory/.crawler
  char crawlerPath[PATH_MAX];
  snprintf(crawlerPath, sizeof(crawlerPath), "%s/.crawler", *pageDirectory);
  FILE *cp = fopen(crawlerPath, "r");
  if (cp == NULL) {
    fprintf(stderr, "querier: '%s' is not a crawler directory\n",
            *pageDirectory);
    exit(1);
  }
  fclose(cp);

  // validate index file can be read
  FILE *ip = fopen(*indexFilename, "r");
  if (ip == NULL) {
    fprintf(stderr, "querier: cannot read index file '%s'\n", *indexFilename);
    exit(1);
  }
  fclose(ip);
}

/* prompt */
/* Print a prompt only if stdin is a terminal (interactive use). */
static void
prompt(void)
{
  if (isatty(fileno(stdin))) {
    printf("Query? ");
    fflush(stdout);
  }
}

/* query_loop */
/* Read one line at a time from stdin, clean and tokenize it,
 * validate the syntax, evaluate the query, and print ranked results.
 */
static void
query_loop(const char *pageDirectory, index_t *index)
{
  if (pageDirectory == NULL || index == NULL) {
    fprintf(stderr, "querier: query_loop got NULL parameter\n");
    return;
  }

  char line[1024];

  prompt();
  while (fgets(line, sizeof(line), stdin) != NULL) {

    char **words = NULL;
    int nwords = 0;

    if (!tokenize_and_validate(line, &words, &nwords)) {
      /* invalid query; error already printed */
      if (words != NULL) {
        mem_free(words);
      }
      prompt();
      continue;
    }

    if (nwords == 0) {
      /* blank line; nothing to do */
      mem_free(words);
      prompt();
      continue;
    }

    /* print cleaned query */
    printf("Query:");
    for (int i = 0; i < nwords; i++) {
      printf(" %s", words[i]);
    }
    printf("\n");

    counters_t *results = evaluate_query(index, words, nwords);
    rank_and_print(results, pageDirectory);

    counters_delete(results);
    mem_free(words);
    prompt();
  }

  printf("\n");
}

/* tokenize_and_validate */
/* Clean the input line, ensure only letters and spaces, split into tokens,
 * and check placement of operators.
 *
 * On success:
 *   - *words_out points to a malloc'ed array of nwords char*.
 *   - each char* points into the modified line buffer.
 *   - caller frees *words_out but not the individual strings.
 */
static bool
tokenize_and_validate(char *line, char ***words_out, int *nwords_out)
{
  if (line == NULL || words_out == NULL || nwords_out == NULL) {
    fprintf(stderr, "querier: tokenize_and_validate got NULL parameter\n");
    return false;
  }

  int len = strlen(line);

  /* first pass: lowercase and reject bad chars */
  for (int i = 0; i < len; i++) {
    unsigned char c = (unsigned char) line[i];
    if (isalpha(c)) {
      line[i] = (char) tolower(c);
    } else if (!isspace(c)) {
      fprintf(stderr, "Error: bad character '%c' in query\n", c);
      *words_out = NULL;
      *nwords_out = 0;
      return false;
    }
  }

  /* allocate worst-case number of words */
  char **words = mem_malloc(sizeof(char*) * (len/2 + 1));
  if (words == NULL) {
    fprintf(stderr, "querier: out of memory in tokenize_and_validate\n");
    exit(2);
  }

  int count = 0;
  int i = 0;
  while (i < len) {
    // Skip over any non-letter chars
    while (i < len && !isalpha((unsigned char)line[i])) {
      i++;
    }

    if (i >= len) {
      break;  // no more words
    }

    int start = i;

    // Consume letters
    while (i < len && isalpha((unsigned char)line[i])) {
      i++;
    }

    int end = i;
    line[end] = '\0';
    words[count] = &line[start];
    count++;
  }

  *words_out = words;
  *nwords_out = count;

  if (!validate_tokens(words, count)) {
    return false;
  }
  return true;
}

/* validate_tokens */
/* Check that the sequence of tokens follows the grammar:
 *  - first and last tokens are not operators
 *  - no two operators are adjacent
 */
static bool
validate_tokens(char **words, const int nwords)
{
  if (words == NULL) {
    fprintf(stderr, "querier: validate_tokens got NULL words\n");
    return false;
  }

  if (nwords == 0) {
    /* blank line is allowed */
    return true;
  }

  if (is_operator(words[0])) {
    fprintf(stderr, "Error: '%s' cannot be first\n", words[0]);
    return false;
  }
  if (is_operator(words[nwords-1])) {
    fprintf(stderr, "Error: '%s' cannot be last\n", words[nwords-1]);
    return false;
  }

  for (int i = 1; i < nwords; i++) {
    if (is_operator(words[i-1]) && is_operator(words[i])) {
      fprintf(stderr, "Error: '%s' and '%s' cannot be adjacent\n",
              words[i-1], words[i]);
      return false;
    }
  }
  return true;
}

/* is_operator */
/* Return true if word is exactly "and" or "or". */
static bool
is_operator(const char *word)
{
  if (word == NULL) {
    return false;
  }
  return (strcmp(word, "and") == 0 || strcmp(word, "or") == 0);
}

/* evaluate_query */
/* Evaluate a full query with AND precedence over OR.
 *
 * query ::= andsequence { "or" andsequence }*
 *
 * For each andsequence we compute intersection, then we union
 * all andsequence results together.
 */
static counters_t *
evaluate_query(index_t *index, char **words, const int nwords)
{
  if (index == NULL || words == NULL) {
    fprintf(stderr, "querier: evaluate_query got NULL parameter\n");
    return counters_new();
  }

  counters_t *orResult = counters_new();
  if (orResult == NULL) {
    fprintf(stderr, "querier: cannot allocate counters in evaluate_query\n");
    exit(2);
  }
  bool haveOrResult = false;

  int i = 0;
  while (i < nwords) {
    int end = 0;
    counters_t *andResult = evaluate_andsequence(index, words, nwords,
                                                 i, &end);

    if (!haveOrResult) {
      counters_delete(orResult);
      orResult = andResult;
      haveOrResult = true;
    } else {
      counters_union(orResult, andResult);
      counters_delete(andResult);
    }

    i = end;
    if (i < nwords && strcmp(words[i], "or") == 0) {
      i++;                    // skip the "or"
    }
  }

  if (!haveOrResult) {
    /* no words somehow; keep empty result */
  }
  return orResult;
}

/* evaluate_andsequence */
/* Evaluate a single andsequence starting at index "start".
 *
 * andsequence ::= word { ["and"] word }*
 * Stops at "or" or end of array.
 *
 * On return, *end_out is set to index of first token after this
 * andsequence (either an "or" or nwords).
 */
static counters_t *
evaluate_andsequence(index_t *index, char **words,
                     const int nwords, const int start, int *end_out)
{
  counters_t *result = NULL;
  int i = start;

  while (i < nwords && strcmp(words[i], "or") != 0) {
    if (strcmp(words[i], "and") == 0) {
      i++;   // skip explicit "and"
      continue;
    }

    /* words[i] is a real word. */
    counters_t *wordCtrs = index_find(index, words[i]);   // may be NULL

    if (result == NULL) {
      result = counters_new();
      if (wordCtrs != NULL) {
        counters_iterate(wordCtrs, result, copy_helper);
      }
    } else {
      if (wordCtrs == NULL) {
        /* intersect with empty set => everything becomes zero. */
        counters_iterate(result, result, zero_helper);
      } else {
        counters_intersect(result, wordCtrs);
      }
    }
    i++;
  }

  if (result == NULL) {
    result = counters_new();
  }

  if (end_out != NULL) {
    *end_out = i;
  }
  return result;
}

/* counters_intersect */
/* Modify dest in-place to become the intersection of dest and src.
 * For each docID in dest:
 *   newScore = min(dest[docID], src[docID])
 */
static void
counters_intersect(counters_t *dest, counters_t *src)
{
  if (dest == NULL || src == NULL) {
    return;
  }
  two_counters_t pair = { dest, src };
  counters_iterate(dest, &pair, intersect_helper);
}

/* counters_union */
/* Modify dest in-place to become the union of dest and src.
 * For each docID in src:
 *   dest[docID] += src[docID]
 */
static void
counters_union(counters_t *dest, counters_t *src)
{
  if (dest == NULL || src == NULL) {
    return;
  }
  counters_iterate(src, dest, union_helper);
}

/* intersect_helper */
/* Helper for counters_intersect. */
static void
intersect_helper(void *arg, const int key, int count)
{
  two_counters_t *pair = arg;
  int other = counters_get(pair->b, key);
  int newCount = (count < other) ? count : other;
  counters_set(pair->a, key, newCount);
}

/* union_helper */
/* Helper for counters_union. */
static void
union_helper(void *arg, const int key, int count)
{
  counters_t *dest = arg;
  int old = counters_get(dest, key);
  counters_set(dest, key, old + count);
}

/* copy_helper */
/* Helper used to copy all entries from one counters into another. */
static void
copy_helper(void *arg, const int key, int count)
{
  counters_t *dest = arg;
  counters_set(dest, key, count);
}

/* zero_helper */
/* Helper used to set all counts in a counters to zero. */
static void
zero_helper(void *arg, const int key, int count)
{
  (void) count;      // unused
  counters_t *dest = arg;
  counters_set(dest, key, 0);
}

/* rank_and_print */
/* Rank the results (counters) by score and print them.
 * If there are no matches, print "No documents match."
 */
static void
rank_and_print(counters_t *results, const char *pageDirectory)
{
  if (results == NULL || pageDirectory == NULL) {
    fprintf(stderr, "querier: rank_and_print got NULL parameter\n");
    return;
  }

  int n = 0;
  counters_iterate(results, &n, count_nonzero);

  if (n == 0) {
    printf("No documents match.\n");
    printf("-----------------------------------------------\n");
    return;
  }

  docscore_t *docs = mem_malloc(n * sizeof(docscore_t));
  if (docs == NULL) {
    fprintf(stderr, "querier: out of memory in rank_and_print\n");
    exit(2);
  }

  collect_arg_t arg = { docs, 0 };
  counters_iterate(results, &arg, collect_nonzero);

  qsort(docs, n, sizeof(docscore_t), cmp_docscore_desc);

  printf("Matches %d documents (ranked):\n", n);
  for (int i = 0; i < n; i++) {
    int id = docs[i].docID;
    int score = docs[i].score;
    char *url = get_url(pageDirectory, id);

    if (url == NULL) {
      printf("score %3d  doc %3d: (no-url)\n", score, id);
    } else {
      printf("score %3d  doc %3d: %s\n", score, id, url);
      mem_free(url);
    }
  }
  printf("-----------------------------------------------\n");

  mem_free(docs);
}

/* count_nonzero */
/* Count entries with non-zero score. */
static void
count_nonzero(void *arg, const int key, int count)
{
  (void) key;                 // unused
  if (count > 0) {
    int *n = arg;
    (*n)++;
  }
}

/* collect_nonzero */
/* Collect non-zero entries into an array of docscore_t. */
static void
collect_nonzero(void *arg, const int key, int count)
{
  if (count <= 0) {
    return;
  }
  collect_arg_t *ca = arg;
  ca->array[ca->index].docID = key;
  ca->array[ca->index].score = count;
  ca->index++;
}

/* cmp_docscore_desc */
/* qsort comparison: sort docscore_t by score, descending. */
static int
cmp_docscore_desc(const void *a, const void *b)
{
  const docscore_t *da = a;
  const docscore_t *db = b;
  return db->score - da->score;
}

/* get_url */
/* Given pageDirectory and docID, open the corresponding file and
 * return a newly-allocated string containing the URL (first line).
 * Caller must free the returned string with mem_free.
 * Returns NULL on error.
 */
static char *
get_url(const char *pageDirectory, const int docID)
{
  if (pageDirectory == NULL || docID <= 0) {
    return NULL;
  }

  char filename[PATH_MAX];
  snprintf(filename, sizeof(filename), "%s/%d", pageDirectory, docID);

  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    return NULL;
  }

  char buffer[1024];
  if (fgets(buffer, sizeof(buffer), fp) == NULL) {
    fclose(fp);
    return NULL;
  }
  fclose(fp);

  /* strip trailing newline */
  size_t len = strlen(buffer);
  if (len > 0 && buffer[len-1] == '\n') {
    buffer[len-1] = '\0';
  }

  char *url = mem_malloc(len + 1);
  if (url == NULL) {
    fprintf(stderr, "querier: out of memory in get_url\n");
    exit(2);
  }
  strcpy(url, buffer);
  return url;
}
