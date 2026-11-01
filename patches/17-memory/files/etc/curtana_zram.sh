#!/system/bin/sh
# hmos2curtana: compressed swap in RAM (zram, lz4) for the 4/6 GB Redmi Note 9S.
# HarmonyOS is tuned for 12 GB phones: without swap the 4 GB model runs with
# ~100 MB free and the UI stalls while the kernel drops and re-reads code pages.
Z=/sys/block/zram0
log() { echo "curtana-zram: $*" > /dev/kmsg; }
[ -d $Z ] || { log "no zram0 in this kernel"; exit 0; }
grep -q zram0 /proc/swaps && { log "already on"; exit 0; }
# 75% of RAM, at most 4 GB (in MB: the shell may only have 32-bit arithmetic; no awk on HarmonyOS)
MEM_KB=0
while read -r k v _; do [ "$k" = "MemTotal:" ] && MEM_KB=$v; done < /proc/meminfo
MB=$((MEM_KB / 1024 * 3 / 4))
[ $MB -gt 4096 ] && MB=4096
[ $MB -lt 256 ] && MB=1024
echo 1 > $Z/reset 2>/dev/null
echo lz4 > $Z/comp_algorithm 2>/dev/null
echo ${MB}M > $Z/disksize || { log "disksize ${MB}M refused"; exit 0; }
D=/dev/block/zram0
if [ ! -b $D ]; then
  MAJMIN=$(cat $Z/dev)
  mknod $D b ${MAJMIN%:*} ${MAJMIN#*:}
fi
/system/bin/mkswap $D > /dev/null 2>&1 || { log "mkswap failed"; exit 0; }
/system/bin/swapon $D || { log "swapon failed"; exit 0; }
echo 100 > /proc/sys/vm/swappiness
echo 0 > /proc/sys/vm/page-cluster
log "on: ${MB} MB lz4, swappiness 100"
