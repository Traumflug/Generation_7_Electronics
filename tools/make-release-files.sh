#!/bin/bash

while [ ! -d .git ]; do cd ..; done

BASE_DIR="${PWD}"
DOC_DIR="${BASE_DIR}/release documents"
cd "${BASE_DIR}"
mkdir -p "${DOC_DIR}"
rm -rf "${DOC_DIR}"/*

RELEASE=$(git tag | tail -1)
touch "${DOC_DIR}/this is ${RELEASE}"
unset RELEASE

for F in *.sch; do
  PS_DOC="${DOC_DIR}/${F%.sch} Schematic.ps"
  gschem -p -o "${PS_DOC}" -s "${BASE_DIR}/tools/print.scm" "$F"
  cd "${DOC_DIR}"
  ps2pdf "${PS_DOC}"
  rm -f "${PS_DOC}"
  cd "${BASE_DIR}"
done

