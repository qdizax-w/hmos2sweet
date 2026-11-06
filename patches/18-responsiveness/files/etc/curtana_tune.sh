#!/system/bin/sh
# hmos2curtana: what Android's init.qcom.post_boot.sh does for atoll, and HarmonyOS never does.
# Values come from the stock curtana script, except where a comment says otherwise.
log() { echo "curtana-tune: $*" > /dev/kmsg; }
put() { # value file: a missing or refused knob is logged and skipped
  echo "$1" > "$2" 2>/dev/null || log "refused: $2 = $1"
}
CPU=/sys/devices/system/cpu
BOOST=/sys/module/cpu_boost/parameters

# core_ctl defaults to enabled with min_cpus=1, so it may isolate cores. Stock only keeps it
# on the silver cluster and with a floor of 4; UI latency matters more here, so it is off.
put 0 $CPU/cpu0/core_ctl/enable
put 0 $CPU/cpu6/core_ctl/enable

# big.LITTLE placement: move busy tasks to the A76 cores earlier than the 95/85 default
put 65 /proc/sys/kernel/sched_downmigrate
put 71 /proc/sys/kernel/sched_upmigrate
put 85 /proc/sys/kernel/sched_group_downmigrate
put 100 /proc/sys/kernel/sched_group_upmigrate
put 1 /proc/sys/kernel/sched_walt_rotate_big_tasks
put 740000 /proc/sys/kernel/sched_little_cluster_coloc_fmin_khz

put 1248000 $CPU/cpu0/cpufreq/schedutil/hispeed_freq
put 576000 $CPU/cpu0/cpufreq/scaling_min_freq
put 1267200 $CPU/cpu6/cpufreq/schedutil/hispeed_freq
put 652800 $CPU/cpu6/cpufreq/scaling_min_freq

# Touch boost. Android also has the perf HAL for this, HarmonyOS has only the kernel driver, so
# it is stronger than stock's 0:1324800 / 120 ms: both clusters, and 300 ms because the driver
# ignores input for 150 ms after each boost, which left gaps in a scroll.
# sched_boost_on_input moves the woken tasks to the big cores at once.
put "0:1516800 6:1651200" $BOOST/input_boost_freq
put 300 $BOOST/input_boost_ms
put 1 $BOOST/sched_boost_on_input

# schedtune is built in but HarmonyOS does not mount it: wake tasks on an idle CPU
if [ ! -d /dev/stune ]; then
  mkdir /dev/stune 2>/dev/null
  mount -t cgroup -o schedtune none /dev/stune 2>/dev/null || log "stune mount failed"
fi
put 1 /dev/stune/schedtune.prefer_idle

# The input path runs at nice 0 next to everything else; render_service is already at -20.
# renice -p acts on one thread, so walk /proc/<pid>/task.
for c in /proc/[0-9]*/comm; do
  read -r name < "$c" 2>/dev/null || continue
  case "$name" in
    multimodalinput|composer_host)
      d=${c%/comm}
      for t in "$d"/task/*; do renice -n -20 -p "${t##*/}" > /dev/null 2>&1; done
      log "$name ${d##*/}: nice -20"
      ;;
  esac
done

# State for the next get_logs: this is what the tuning has to be judged on
for f in $CPU/cpu0/core_ctl/enable $CPU/cpu6/core_ctl/enable $CPU/cpu0/cpufreq/scaling_governor \
         $CPU/cpu6/cpufreq/scaling_governor /sys/class/kgsl/kgsl-3d0/devfreq/governor \
         $BOOST/input_boost_freq; do
  read -r v < "$f" 2>/dev/null
  log "$f = $v"
done
for g in /sys/class/devfreq/*/governor; do
  read -r v < "$g" 2>/dev/null
  log "$g = $v"
done
while read -r k v _; do
  case "$k" in MemTotal:|MemAvailable:|SwapTotal:|SwapFree:) log "$k $v kB" ;; esac
done < /proc/meminfo
log done
