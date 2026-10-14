#!/usr/bin/env bash
# Parses every example of the Pelago compiler repository (and its core library) with this grammar.
# Exits with a non-zero status when any file has an ERROR or MISSING node.
#
#   script/check-corpus.sh /path/to/pelago
#
# The tree-sitter binary is taken from $TREE_SITTER, then from ./node_modules/.bin, then from $PATH.
set -u

if [ $# -ne 1 ] || [ ! -d "$1" ]; then
  echo "usage: $0 <path to the pelago repository>" >&2
  exit 2
fi
repo=$1
here=$(cd "$(dirname "$0")/.." && pwd)

if [ -n "${TREE_SITTER:-}" ]; then
  ts=$TREE_SITTER
elif [ -x "$here/node_modules/.bin/tree-sitter" ]; then
  ts=$here/node_modules/.bin/tree-sitter
else
  ts=tree-sitter
fi

files=()
while IFS= read -r f; do files+=("$f"); done < <(find "$repo/examples" -name '*.pel' | sort)
if [ -f "$repo/compiler/core/core.pel" ]; then files+=("$repo/compiler/core/core.pel"); fi
if [ ${#files[@]} -eq 0 ]; then
  echo "no .pel files found under $repo" >&2
  exit 2
fi

cd "$here" || exit 2
bad=0
for f in "${files[@]}"; do
  out=$("$ts" parse -q "$f" 2>&1)
  status=$?
  # The CLI prints the position of the first ERROR or MISSING node on the summary line.
  if [ $status -ne 0 ] || printf '%s' "$out" | grep -qE '\((ERROR|MISSING)'; then
    bad=$((bad + 1))
    echo "FAIL  ${f#$repo/}"
    printf '%s\n' "$out" | grep -E '\((ERROR|MISSING)' | head -1 | sed 's/^/      /'
  else
    echo "ok    ${f#$repo/}"
  fi
done

total=${#files[@]}
echo "$((total - bad)) of $total files parse without ERROR or MISSING nodes"
[ $bad -eq 0 ]
