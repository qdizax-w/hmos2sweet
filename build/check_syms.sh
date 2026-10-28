#!/bin/bash
# usage: check_syms.sh lib.so [extra libdirs...]  -> lists undefined symbols not provided by NEEDED libs
S=${SYSROOT:-$(dirname "$0")/work/sysroot}/usr/lib
f=$1; shift
dirs="$S $*"
need=$(readelf -d $f | grep NEEDED | sed 's/.*\[//;s/\]//')
defs=$(mktemp)
for n in $need; do for d in $dirs; do [ -f $d/$n ] && nm -D --defined-only $d/$n | awk '{print $NF}' | sed 's/@.*//' >> $defs && break; done; done
missing=0
for s in $(nm -D --undefined-only $f | awk '$1=="U"{print $2}' | sed 's/@.*//'); do grep -qxF "$s" $defs || { echo "MISSING $s"; missing=$((missing+1)); }; done
echo "$f: $missing missing; NEEDED: $(echo $need | tr '\n' ' ')"
rm $defs
