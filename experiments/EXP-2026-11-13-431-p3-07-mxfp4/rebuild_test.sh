#!/bin/bash
# Rebuild the MXFP4 test DSO and fixture from the current Goose source.
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
bash "$F/experiments/EXP-2026-11-13-431-p3-07-mxfp4/build_test.sh" >/dev/null
echo REBUILT
