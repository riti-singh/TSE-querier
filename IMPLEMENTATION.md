# Querier implementation notes

This document describes `querier.c`. External API details cannot be verified because the required `libcs50` and TSE `common` sources are not included.

## Startup

`main` calls `parse_args`, creates a 256-slot index, opens the serialized index, calls `index_load`, and enters `query_loop`. Directory validation directly opens `<pageDirectory>/.crawler`; no pagedir helper is used. The index file is opened once for validation and again for loading. A nonzero load status is printed but does not stop execution.

## Parsing

`query_loop` uses `fgets` with a 1024-byte buffer. `tokenize_and_validate` lowercases letters, rejects other non-whitespace bytes, allocates a token-pointer array, NUL-terminates tokens in the input buffer, and validates operator placement. Adjacent ordinary words mean implicit AND. A physical line longer than 1023 bytes is evaluated in chunks because a missing newline is not detected.

## Evaluation

`evaluate_query` splits at `or`. `evaluate_andsequence` skips explicit `and`, copies counters for its first word, and intersects later words. Intersection retains the smaller score; an absent term zeroes accumulated scores. OR union adds group scores for the same document ID. Thus `a b or c and d` means `(a AND b) OR (c AND d)`.

## Ranking and URL lookup

`rank_and_print` counts positive entries, allocates a contiguous `docscore_t` array, collects entries, and sorts by descending score. Its subtraction-based comparator has no tie-breaker. `get_url` formats `<pageDirectory>/<docID>`, reads at most 1023 URL bytes from the first line, strips `\n`, and returns an allocated copy.

## Cleanup

Normal paths release per-query counters, token arrays, URL strings, the ranking array, and finally the index. Some counter allocations and formatted-path truncation cases are unchecked. See [CODE_AUDIT.md](CODE_AUDIT.md).
