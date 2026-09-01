# Code and build audit

## Audit boundary

Every originally tracked file was inspected: `README.md`, `DESIGN.md`, `IMPLEMENTATION.md`, `querier.c`, `makefile`, and `testing.sh`. No `libcs50`, `common`, crawler, indexer, fixture, or generated index is tracked, and the expected sibling dependency directories are absent. The component therefore cannot be compiled or executed here. Per the audit constraint, `querier.c` remains unchanged; proposed fixes below require verification in a complete TSE tree.

## `querier.c`

### Parsing and malformed operators

- Normal input is lowercased, split on whitespace, and rejects non-alphabetic/non-whitespace bytes. `ctype` calls correctly receive `unsigned char` values.
- Leading, trailing, and adjacent operators are rejected. Adjacent ordinary words correctly implement implicit AND.
- `char line[1024]` makes a longer physical line become multiple queries. Detect a missing newline and consume/reject the remainder, or use dynamic input.
- Accepted alphabetic characters depend on the active C locale; confirm that this matches the external index normalizer.

### NULL and allocation handling

- The index, token array, OR accumulator, ranking array, and URL-copy allocations are checked on their principal paths.
- `counters_new()` results in `evaluate_andsequence` are not checked before possible iteration or return.
- The defensive `evaluate_query` NULL path returns an unchecked `counters_new()` result.
- Allocation failures call `exit(2)` rather than unwinding the loaded index and live temporaries. Process teardown makes this practical for a CLI, but centralized error propagation would make cleanup testable.

### Counter operations

- AND correctly retains the minimum count for keys in the destination; a missing source key yields zero.
- OR correctly adds matching scores and inserts source-only keys, assuming the missing external `counters_set` API supports this contract.
- Mutation of counter values during iteration should be confirmed against the exact external `counters_iterate` contract.
- `old + count` can overflow signed `int` with sufficiently large or corrupt counts.
- Silent returns on NULL counter arguments can conceal an allocation or programming error.

### Ranking and comparator

- Only positive scores are collected and sorting is descending.
- `return db->score - da->score` can overflow signed `int`, causing undefined behavior. Use relational comparisons instead.
- Equal scores compare equal, so result order is unspecified. Add a document-ID tie-breaker if deterministic output is required.

### Files and URLs

- Files opened on visible normal/error paths are closed.
- `snprintf` results for `.crawler` and document paths are unchecked; truncation can target an unintended path.
- URL reads are capped at 1023 bytes and silently truncate a longer first line.
- Only `\n` is stripped, leaving `\r` on CRLF input.
- Missing/empty/unreadable page files all degrade to `(no-url)` without distinction.
- Opening the index once to validate and again to load is redundant and creates a small check/use race.

### Cleanup and exit behavior

- Normal query and EOF paths release results, tokens, URLs, ranking storage, and the index.
- A nonzero `index_load` status is reported but ignored; querying continues and `main` returns 0. A failed or partial load should normally clean up and exit nonzero.
- An internal NULL passed to `query_loop` also results in eventual success status.
- Malformed user queries appropriately remain nonfatal.

## `makefile`

- All dependency paths assume this directory is `<tse>/querier`; that layout is absent here.
- Linking both `../common/index.o` and `../common/common.a` may duplicate symbols if the archive already contains `index.o`. Inspect the missing archive before changing this.
- The component makefile compiles `../common/index.c` into a sibling directory. It should normally depend on an external library built by its owner. Its `clean` target does not remove that external object.
- `querier.o` omits included headers as prerequisites, so header changes may not rebuild it.
- `test` first requires a local binary, then `testing.sh` cleans and rebuilds the parent tree, making the local prerequisite redundant and unexpectedly removing parent artifacts.
- Recipes require Unix tools/Bash, and Valgrind/shared fixture paths are environment-specific.

Proposed complete-tree fix: establish whether `common.a` owns the index implementation, link each symbol once, build external libraries in their own makefiles, add header or generated dependency tracking, avoid parent-tree cleaning in the component test, and make fixture paths configurable.

## `testing.sh`

- The Bash-specific syntax matches its shebang, but it also assumes standard Unix utilities. It enables `set -e`, not `set -u` or `pipefail`.
- Course-specific fixture paths are assigned unconditionally, preventing simple `PDIR`/`IDX` environment overrides.
- `(cd .. && make clean && make)` prevents isolated use and cleans the whole parent tree.
- Argument cases inspect text but do not explicitly assert nonzero status. Query runs are also under `set +e`, so an unexpected failure can be masked if expected text exists.
- Smoke tests check only that a `Query:` line and separator exist; they do not verify exact documents, scores, precedence, implicit AND equivalence, ranking, or URL resolution.
- Missing cases include blanks, normalization, tabs, absent terms, operator combinations, ties, long input/URLs/paths, failed index loading, missing pages, allocation failures, and memory analysis.
- Temporary-directory creation, quoting, and trap cleanup are appropriately scoped. `# Bbasic queries` is only a comment typo.

Proposed complete-tree fix: accept fixture variables, avoid rebuilding the parent, capture every exit status, compare exact output from deterministic fixtures, and add overlong-input plus sanitizer/Valgrind tiers.

## Documentation contradictions corrected

- Earlier docs claimed a document-ID tie-breaker that the comparator does not implement.
- They described pagedir helpers, but source directly opens `.crawler` and numbered files.
- They implied unlimited line reads and complete URLs despite fixed 1024-byte buffers.
- They asserted leak-free behavior without reproducible memory testing.
- They described zero-score entries as dropped during intersection; source retains them until ranking filters positive scores.
- They presented general `make` commands without clearly stating the full course tree, external libraries, crawler data, and indexer output are required.
