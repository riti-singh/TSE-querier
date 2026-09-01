# Querier design

## Scope and interface

The querier is the retrieval stage of Tiny Search Engine. It consumes, but does not create, a crawler page directory and an indexer-produced inverted index. It depends on external `libcs50` counters/memory APIs and the TSE `common/index` API.

```text
./querier pageDirectory indexFilename
```

`pageDirectory` must contain `.crawler`; `indexFilename` must be readable by the external index module. Queries are read from standard input until EOF. `Query?` appears only for terminal input.

## Query contract

Input is lowercased and split at whitespace. Only alphabetic characters and whitespace are accepted.

```text
query       := andSequence { "or" andSequence }*
andSequence := word { ["and"] word }*
```

Adjacent words are implicit AND, and AND has precedence over OR. An operator cannot be first or last, operators cannot be adjacent, and blank lines are ignored.

## Evaluation model

The index maps a word to `counters_t`, conceptually `docID → term frequency`. Each AND sequence copies the first term's counters and intersects subsequent terms, retaining the minimum count per document. A missing term makes every accumulated score zero. OR-separated group results are unioned by adding scores for matching document IDs.

## Ranking and output

Positive scores are collected into a `docscore_t` array and sorted descending with `qsort`. There is no tie-break rule. For each result, `pageDirectory/<docID>` is opened and its first line is used as the URL; failure produces `(no-url)`.

Output contains a normalized `Query:` line followed by a ranked list or `No documents match.`, then a separator.

## Ownership, cleanup, and errors

Token pointers refer into the current stack input buffer; only their pointer array is allocated. Evaluation counters and the ranking array are temporary. The index is owned by `main` until EOF. `get_url` returns an allocated caller-owned string.

Argument and major allocation failures exit nonzero. Malformed queries are nonfatal. The current implementation reports a nonzero `index_load` result but continues and ultimately returns success. Other robustness gaps are listed in [CODE_AUDIT.md](CODE_AUDIT.md).
