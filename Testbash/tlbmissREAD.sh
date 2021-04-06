#!bin/sh

NAME="RANDREAD"
RWMODE="randread"
SIZE=$1
PERFCMDLINE="/usr/src/linux-4.18.16/tools/perf/perf"

dd if=/dev/sda2 of=testfile bs=$SIZE count=1
$PERFCMDLINE stat -e dTLB-load-misses,dTLB-loads fio -filename=testfile -bs=1k -size=$SIZE -rw=$RWMODE -name=$NAME
rm testfile

dd if=/dev/sda2 of=testfile bs=$SIZE count=1
$PERFCMDLINE stat -e dTLB-load-misses,dTLB-loads fio -filename=testfile -bs=2k -size=$SIZE -rw=$RWMODE -name=$NAME;
rm testfile

dd if=/dev/sda2 of=testfile bs=$SIZE count=1
$PERFCMDLINE stat -e dTLB-load-misses,dTLB-loads fio -filename=testfile -bs=4k -size=$SIZE -rw=$RWMODE -name=$NAME;
rm testfile

dd if=/dev/sda2 of=testfile bs=$SIZE count=1
$PERFCMDLINE stat -e dTLB-load-misses,dTLB-loads fio -filename=testfile -bs=8k -size=$SIZE -rw=$RWMODE -name=$NAME;
rm testfile

dd if=/dev/sda2 of=testfile bs=$SIZE count=1
$PERFCMDLINE stat -e dTLB-load-misses,dTLB-loads fio -filename=testfile -bs=16k -size=$SIZE -rw=$RWMODE -name=$NAME;
rm testfile

dd if=/dev/sda2 of=testfile bs=$SIZE count=1
$PERFCMDLINE stat -e dTLB-load-misses,dTLB-loads fio -filename=testfile -bs=32k -size=$SIZE -rw=$RWMODE -name=$NAME;
rm testfile

dd if=/dev/sda2 of=testfile bs=$SIZE count=1
$PERFCMDLINE stat -e dTLB-load-misses,dTLB-loads fio -filename=testfile -bs=64k -size=$SIZE -rw=$RWMODE -name=$NAME;
rm testfile

dd if=/dev/sda2 of=testfile bs=$SIZE count=1
$PERFCMDLINE stat -e dTLB-load-misses,dTLB-loads fio -filename=testfile -bs=128k -size=$SIZE -rw=$RWMODE -name=$NAME;
rm testfile
