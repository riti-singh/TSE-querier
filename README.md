# TSE-querier


**Riti Singh, Fall 2025**
**Querier**

The **Querier** is the final component of the CS50 Tiny Search Engine (TSE).
It reads:

* the **page files** produced by the Crawler
* the **index file** produced by the Indexer

…then accepts user queries from **stdin**, evaluates them using the inverted index, and prints ranked search results.

The Querier does **not** crawl or index anything itself — it performs **search** on already-prepared data.

---

## **Usage**

From the root of the TSE project:

```bash
./querier/querier pageDirectory indexFilename
```

Where:

* `pageDirectory` is the directory created by the Crawler (containing numbered page files)
* `indexFilename` is the index file written by the Indexer

Example:

```bash
./querier/querier data/letters-1 letters.index
```

The program then prints:

```
Query?
```

You can type queries such as:

```
dartmouth
computer science
planet and earth
tse or project
```

Queries continue until **EOF** (Ctrl-D).

---

## **Implementation**

The querier is implemented in `querier.c` and follows the CS50 TSE specifications:

* **query parsing**: lowercasing, cleaning, splitting into words
* **validation**: checks for syntax errors such as consecutive operators or illegal characters
* **evaluation**:

  * `AND` groups are intersected using minimum document counts
  * `OR` groups are unioned by summing their scores
* **ranking**:

  * non-zero results are collected
  * sorted by descending document score
* **output**:

  * prints score, docID, and corresponding URL (first line of each page file)

The querier uses:

* `counters_t` for docID/count mappings
* `index_t` for the global inverted index
* helper functions such as `counters_intersect`, `counters_union`, and `rank_and_print`
* the `pagedir` module to read the correct page file for each docID

See `IMPLEMENTATION` for step-by-step details on logic and internal functions.

---

## **Assumptions**

* `pageDirectory` must contain files named with integer docIDs starting at 1 (created by Crawler).
* The index file must be in the format produced by the TSE Indexer.
* Words are normalized to lowercase alphabetic strings.
* Logical operators supported:

  * `and`
  * `or`
* The underlying index uses a hashtable of fixed slot size (200 by default).
* Extremely large datasets may exceed normal memory limits, but standard CS50 test cases will not.

---

## **Compilation**

To build the entire Tiny Search Engine (all modules):

```bash
make
```

To clean object files:

```bash
make clean
```

The querier binary will be created at:

```
querier/querier
```

Run it normally as shown above.

---

## **Testing**

Basic testing:

```bash
./crawler/crawler http://cs50tse.cs.dartmouth.edu/tse/letters/index.html data/letters-1 1
./indexer/indexer data/letters-1 letters.index
./querier/querier data/letters-1 letters.index
```


---

## **Files**

```
querier/
│── Makefile       — build rules for the querier
│── querier.c      — full implementation
│── README.md      — this file
```
