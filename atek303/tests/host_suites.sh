#!/bin/sh
# Les neuf suites du simulateur (CI et à la main) : construit le host, joue chaque suite.
# Une suite qui échoue sort en erreur et stoppe la release (test: du workflow release.yml).
set -e
cd "$(dirname "$0")/.."
sh build.sh --host
B=app/build/host
H=$(ls "$B"/*_host 2>/dev/null | head -n 1)   # le binaire suit le nom de fm1.toml (ass-id_host)
[ -n "$H" ] || { echo "host : pas de binaire *_host dans $B" >&2; exit 1; }
for t in basic clock controls editor fx options presets steps64 drums; do
    "$H" "app/tests/$t.txt" "$B"
done
echo "host : les neuf suites passent"
