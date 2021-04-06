#!bin/sh

NAME="RANDWRITE"
SIZE=$1
PERFCMDLINE="/usr/src/linux-4.18.16/tools/perf/perf"

$PERFCMDLINE stat -e cache-misses,cache-references fio -filename=testfile -bs=1k -size=$SIZE -rw=randwrite -name=$NAME;
rm testfile

$PERFCMDLINE stat -e cache-misses,cache-references fio -filename=testfile -bs=2k -size=$SIZE -rw=randwrite -name=$NAME;
rm testfile

$PERFCMDLINE stat -e cache-misses,cache-references fio -filename=testfile -bs=4k -size=$SIZE -rw=randwrite -name=$NAME;
rm testfile

$PERFCMDLINE stat -e cache-misses,cache-references fio -filename=testfile -bs=8k -size=$SIZE -rw=randwrite -name=$NAME;
rm testfile

$PERFCMDLINE stat -e cache-misses,cache-references fio -filename=testfile -bs=16k -size=$SIZE -rw=randwrite -name=$NAME;
rm testfile

$PERFCMDLINE stat -e cache-misses,cache-references fio -filename=testfile -bs=32k -size=$SIZE -rw=randwrite -name=$NAME;
rm testfile

$PERFCMDLINE stat -e cache-misses,cache-references fio -filename=testfile -bs=64k -size=$SIZE -rw=randwrite -name=$NAME;
rm testfile

$PERFCMDLINE stat -e cache-misses,cache-references fio -filename=testfile -bs=128k -size=$SIZE -rw=randwrite -name=$NAME;
rm testfile
