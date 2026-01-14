#!/usr/bin/env bash
# testing.sh — tiny test runner for TSE querier

set -e

Q=./querier

# common shared paths?
PDIR=/cs50/shared/tse/output/letters-1
IDX=/cs50/shared/tse/output/letters-1.index
[[ -f "$PDIR/.crawler" ]] || PDIR="$HOME/cs50-dev/shared/tse/output/letters-1"
[[ -r "$IDX" ]] || IDX="$HOME/cs50-dev/shared/tse/output/letters-1.index"

# build everything from top level 
echo "== build =="
( cd .. && make clean >/dev/null && make >/dev/null )

[[ -x "$Q" ]] || { echo "querier not built"; exit 1; }
[[ -f "$PDIR/.crawler" ]] || { echo "can't find crawler dir: $PDIR"; exit 1; }
[[ -r "$IDX" ]] || { echo "can't read index: $IDX"; exit 1; }


TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# arg checking
echo "== arg checks =="
set +e
$Q                          > "$TMP/args0.out" 2>&1
$Q "$PDIR"                  > "$TMP/args1.out" 2>&1
$Q "$PDIR" "$IDX" extra     > "$TMP/argsextra.out" 2>&1
set -e
grep -E '^usage:' "$TMP/args0.out" >/dev/null
grep -E '^usage:' "$TMP/args1.out" >/dev/null
grep -E '^usage:' "$TMP/argsextra.out" >/dev/null

# bad paths (not a crawler dir; missing index) 
echo "== bad paths =="
mkdir -p "$TMP/notcrawler"
set +e
$Q "$TMP/notcrawler" "$IDX" > "$TMP/baddir.out" 2>&1
$Q "$PDIR" "$TMP/nope.idx"  > "$TMP/badidx.out" 2>&1
set -e
grep -i "not a crawler" "$TMP/baddir.out" >/dev/null
grep -i "cannot read index file" "$TMP/badidx.out" >/dev/null

# Bbasic queries (also checks prompt is suppressed when stdin redirected) 
echo "== basic queries =="
cat > "$TMP/q.txt" <<'EOF'
hello
hello and world
hello or world
hello and world or goodbye
EOF

$Q "$PDIR" "$IDX" < "$TMP/q.txt" > "$TMP/basic.out" 2>&1
grep '^Query:' "$TMP/basic.out" >/dev/null
grep -- '-----------------------------------------------' "$TMP/basic.out" >/dev/null
if grep -q "Query?" "$TMP/basic.out"; then
  echo "prompt should not appear when stdin is redirected"; exit 1
fi

# Syntax errors
echo "== syntax errs =="
cat > "$TMP/badq.txt" <<'EOF'
c@t
and dog
cat and
dog and or cat
EOF

set +e
$Q "$PDIR" "$IDX" < "$TMP/badq.txt" > "$TMP/syntax.out" 2>&1
set -e
grep -i "bad character" "$TMP/syntax.out" >/dev/null
grep -i "cannot be first" "$TMP/syntax.out" >/dev/null
grep -i "cannot be last" "$TMP/syntax.out" >/dev/null
grep -i "cannot be adjacent" "$TMP/syntax.out" >/dev/null
