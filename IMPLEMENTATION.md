

# **IMPLEMENTATION.md**

# **Tiny Search Engine — Querier Module**

**Riti Singh —  Fall 2025**

This document explains in detail how the **Querier** is implemented, how it interacts with the Crawler and Indexer outputs, and how search queries are parsed, validated, processed, and evaluated.
It supplements `README.md` by describing the internal logic and functions used inside `querier.c`.

---

# **1. Program Overview**

The querier takes two inputs:

1. **pageDirectory** — directory created by the Crawler
2. **indexFilename** — file created by the Indexer

The querier:

* Loads the **inverted index** into memory (`index_t`)
* Repeatedly prompts the user for a **search query**
* Normalizes, validates, and parses the query
* Evaluates the query using **logical AND and OR**
* Ranks documents by relevance score
* Prints matching documents in descending order

It terminates when the user sends **EOF** (Ctrl-D).

---

# **2. Data Structures Used**

### **index_t (from common/index.c)**

Represents the inverted index:
`word → counters_t (docID → count)`

### **counters_t (from libcs50/counters.c)**

Associates each document ID with its term frequency for a given word.

### **Helper structs inside querier**

During ranking, the querier builds small result structs:

```c
typedef struct {
    int docID;
    int score;
} docscore_t;
```

These are stored in arrays and sorted with `qsort()`.

---

# **3. Initialization Phase**

### **validate command-line arguments**

* check `argc == 3`
* verify `pageDirectory` is a Crawler-produced directory (via `pagedir_validate()`)
* verify `indexFilename` is readable

### **load the index**

```c
FILE* fp = fopen(indexFilename, "r");
index_load(fp, index);
fclose(fp);
```

The index is now ready for querying.

---

# **4. Query Processing Loop**

The querier displays a prompt only if stdin is a terminal:

```c
if (isatty(fileno(stdin)))
    printf("Query? ");
```

Each loop iteration:

1. Read a line using `fgets()`
2. Strip whitespace
3. Normalize (lowercase, alphabetical-only words)
4. Parse into tokens
5. Validate query syntax
6. Evaluate the query
7. Rank and print results
8. Loop until EOF

---

# **5. Query Parsing**

### **Tokenizing**

The input string is split into space-separated tokens:

* alphabetic words → stored as query terms
* `and` and `or` → treated as operators

Example:

```
computer science and engineering or dartmouth
```

Becomes:

```
["computer", "science", "and", "engineering", "or", "dartmouth"]
```

### **Validation Rules**

The querier checks for:

* no leading operators
* no trailing operators
* no two consecutive operators
* only alphabetic words
* normalization of uppercase → lowercase

Invalid queries produce an error message and skip evaluation.

---

# **6. Evaluating a Query**

Queries are evaluated in two logical stages:

---

## **6.1 Handling AND sequences**

A sequence like:

```
apple and banana and orange
```

is processed left-to-right using **intersection**:

1. Start with counters for "apple"
2. Intersect with counters for "banana"
3. Intersect with counters for "orange"

Intersection rule:
`count(docID) = MIN(count1, count2)`

If a doc is missing from any word, its score becomes 0 and is dropped.

This is implemented with helper functions such as:

* `counters_intersect()`
* `intersect_helper()`
* iterative calls to `counters_get()` / `counters_set()`

---

## **6.2 Handling OR sequences**

OR combines entire AND-sequences.

Example:

```
apple orange or pear grape
```

Breaks into:

* Group A: `apple orange`
* Group B: `pear grape`

Evaluation:

1. Evaluate group A → `Aresult`
2. Evaluate group B → `Bresult`
3. Combine them:

`count(docID) = Aresult(docID) + Bresult(docID)`

This is processed by:

* `counters_union()`
* `union_helper()`

---

## **6.3 Example full evaluation step**

Query:

```
tse and project or cs50
```

→ AND groups:

* Group 1: `tse project`
* Group 2: `cs50`

→ OR union:

`finalCount = union( intersect(tse, project), counters(cs50) )`

---

# **7. Ranking Results**

Once the final counter set is built:

1. Count how many non-zero results exist
   (`count_nonzero()` helper)
2. Allocate an array of that size
3. Collect each result into the array
   (`collect_nonzero()` helper)
4. Sort the array using `qsort()` and a comparison function:

```c
static int cmp_docscore_desc(const void* a, const void* b)
{
    const docscore_t* da = a;
    const docscore_t* db = b;
    return db->score - da->score;   // descending
}
```

5. Print results in order:

```
score  docID  URL
```

To fetch the URL, querier opens:

```
pageDirectory/<docID>
```

and reads the first line.

---

# **8. Cleanup / Memory Management**

Before exit:

* free the query string buffer
* free all intermediate counters sets
* free the `index_t` by calling `index_delete()`

Memory management follows CS50 guidelines: no leaks, no dangling pointers.

---

# **9. Modules Used**

* **libcs50**

  * `counters.h`
  * `file.h`
  * `mem.h`
* **common**

  * `pagedir.h`
  * `index.h`
* **querier**

  * `querier.c`
  * `Makefile`

---

# **10. Error Handling**

The querier handles these gracefully:

* invalid directory → exit with message
* unreadable index file → exit
* malformed queries → print message, continue loop
* missing words in index → treat as empty counters
* empty final result set → print nothing but continue

---

# **11. Limitations**

* Only supports `and` / `or` operators
* Only alphabetical words
* Requires Crawler-style page files
* Requires Indexer-style index file
* Hashtable size and performance depend on underlying index

---


