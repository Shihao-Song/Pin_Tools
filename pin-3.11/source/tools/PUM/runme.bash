#!/bin/bash

make

make -C test_apps/

../../../pin -t obj-intel64/normal_trace.so -t sample_traces/base.cpu_trace -- test_apps/test_app_base

../../../pin -t obj-intel64/pum_trace.so -t sample_traces/pum.cpu_trace -d sample_traces/pum.data -- test_apps/test_app_our

make clean

make clean -C test_apps/
