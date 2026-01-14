

# **querier DESIGN.md**

*Riti Singh - Fall 2025*

## **Overview**

The Querier is the third module of the Tiny Search Engine (TSE).
Its job is to accept search queries from `stdin`, interpret them using the index produced by the Indexer, retrieve relevant documents, combine scores according to logical operators (“and”, “or”), rank results, and display them in a human-readable format.

The Querier **never crawls** or **builds an index**; instead it:

* loads an existing index file (via `index_load`)
* reads page files stored in the pageDirectory
* processes user commands until EOF
* resolves the query into a ranked list of documents
* prints results in a clean format

The program stops when EOF or ctrl-D is received.

---

## **Responsibilities**

The Querier’s tasks can be grouped into five major responsibilities:

1. **Input Validation**

   * Validate command-line parameters (`pageDirectory`, `indexFilename`)
   * Check that `pageDirectory` is a crawler-generated directory
   * Confirm that the index file loads correctly

2. **Query Parsing**

   * Normalize input (lowercase, trim whitespace)
   * Tokenize into words
   * Validate syntax:

     * no leading/trailing operators
     * "and"/"or" only between words
     * no repeated operators
   * Produce an array of terms representing:

     * AND sequences
     * OR combinations of AND sequences

3. **Query Evaluation**

   * For each AND-sequence:

     * intersect counters of each word
     * score = sum of minimum word counts per document
   * For OR-combinations:

     * union all AND-sequence results
     * score = sum of sub-results

4. **Ranking**

   * Collect all non-zero document scores
   * Convert to array of structs for sorting
   * Sort in decreasing score order

5. **Output Formatting**

   * Print “Query:” followed by normalized query
   * Print number of documents returned
   * For each result, print:

     ```
     score docID URL
     ```
   * If no documents match, print “No results found.”

---

## **Data Structures**

### **1. counters_t (from libcs50)**

Used for mapping:

```
docID → count
```

The Querier uses counters to:

* represent counts for each word
* compute AND (intersection) results
* compute OR (union) results

### **2. index_t**

The inverted index loaded from the indexer:

```
word → counters(docID → count)
```

Querier never edits it; it only queries it.

### **3. AND-sequence accumulator**

For each sequence separated by “or”, the Querier builds a temporary counters_t representing:

```
docID → score for that AND block
```

Often created by copying the first word’s counters, then intersecting with others.

### **4. Query evaluation helpers**

#### **two_counters (optional helper struct)**

If used, this structure allows two counters to be passed into a single callback:

```c
typedef struct two_counters {
    counters_t* c1;
    counters_t* c2;
} two_counters_t;
```

Useful for implementing:

* intersection
* union

#### **doc_t**

A document-score pair:

```c
typedef struct doc {
    int docID;
    int score;
} doc_t;
```

#### **all_docs**

A dynamic array of doc_t pointers, plus count:

```c
typedef struct all_docs {
    doc_t** docs;
    int ndocs;
} all_docs_t;
```

Used for sorting and printing results.

---

## **Functional Breakdown**

### **Main Workflow (`main`)**

1. Validate arguments
2. Load index file
3. Repeatedly prompt for a query
4. Process the query
5. Rank and print results
6. Cleanup and exit

### **Query Handling**

* `prompt()`
  Displays prompt only if input is from a terminal (`isatty()`).
  Reads line from stdin.

* `parse_query()`
  Tokenizes the line, validates logic, and returns an array of normalized words.

* `evaluate_query()`
  Implements Boolean logic:

  * break query into AND-blocks
  * evaluate each block
  * union all block results

* `evaluate_andsequence()`
  Performs the intersection across all words in an AND-block.

---

## **Intersection / Union Helper Logic**

### **Intersection (AND)**

For each docID:

```
score(docID) = min(counts for all words)
```

Implemented via:

* copy first counters
* for each next word:

  * intersect into result using callbacks

### **Union (OR)**

For each docID:

```
score(docID) = sum(scores from all AND-blocks)
```

---

## **Ranking and Printing**

The final counters_t from OR-combination:

* collect all non-zero entries
* convert to an array of doc_t
* sort descending by score (tiebreak by docID ascending)
* retrieve URL of each docID via pageDirectory

Print format:

```
Score: X  DocID: Y  URL: Z
```

---

## **External Modules Used**

* **libcs50**

  * `counters`
  * `hashtable`
  * `memory`

* **common**

  * `pagedir` (validate directories and fetch URLs)
  * `index` (load index)

---

## **Assumptions**

* The index file is well-formed (created by Indexer).
* The pageDirectory contains crawler-style files:

  * one file per docID
  * first line: URL
  * second line: depth
* Query operators are only:

  * `and`
  * `or`
* Words are alphabetic only after normalization.
* Upper/lowercase irrelevant; all is converted to lowercase.
* Index contains all possible words referenced by users.

---
