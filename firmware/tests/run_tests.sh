#!/bin/bash
# Host-side unit tests for the SeedJoy FFB engine + runtime (no hardware needed).
# These verify the effect math and the PID report/handshake logic — the parts
# that can be checked without a device. The USB descriptor still needs on-host
# validation (see docs/ffb-plan.md).
set -e
cd "$(dirname "$0")"

CXX="${CXX:-c++}"
FLAGS="-std=c++11 -Wall -DENABLE_FFB=1 -I../seedjoy"

echo "== FFB engine tests =="
$CXX $FLAGS ffb_engine_test.cpp ../seedjoy/ffb_engine.cpp -o /tmp/ffb_engine_test
/tmp/ffb_engine_test

echo
echo "== FFB runtime tests =="
$CXX $FLAGS ffb_runtime_test.cpp ../seedjoy/ffb_runtime.cpp ../seedjoy/ffb_engine.cpp -o /tmp/ffb_runtime_test
/tmp/ffb_runtime_test

echo
echo "== FFB descriptor validator =="
$CXX $FLAGS ffb_descriptor_test.cpp ../seedjoy/ffb_reports.cpp -o /tmp/ffb_descriptor_test
/tmp/ffb_descriptor_test

echo
echo "All host tests passed."
