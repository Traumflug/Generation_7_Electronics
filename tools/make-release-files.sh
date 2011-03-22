#!/bin/bash

while [ ! -d .git ]; do cd ..; done

BASE_DIR="${PWD}"
DOC_DIR="${BASE_DIR}/release documents"
cd "${BASE_DIR}"
mkdir -p "${DOC_DIR}"
rm -rf "${DOC_DIR}"/*

echo "Creating release tag ..."
RELEASE=$(git tag | tail -1)
touch "${DOC_DIR}/this is ${RELEASE}"
unset RELEASE

echo "Creating schematic PDFs ..."
for F in *.sch; do
  PS_DOC="${DOC_DIR}/${F%.sch} Schematic.ps"
  gschem -p -o "${PS_DOC}" -s "${BASE_DIR}/tools/print.scm" "${F}" >/dev/null
  cd "${DOC_DIR}"
  ps2pdf "${PS_DOC}"
  rm -f "${PS_DOC}"
  cd "${BASE_DIR}"
done

echo "Creating layout PDFs ..."
for F in *.pcb; do
  PS_DOC="${DOC_DIR}/${F%.pcb} Layout.ps"
  pcb -x ps --align-marks --outline --auto-mirror --media A4 \
    --psfade 0.6 --scale 1.0 --drill-copper --show-legend \
    --psfile "${PS_DOC}" "${F}" >/dev/null
  cd "${DOC_DIR}"
  ps2pdf "${PS_DOC}"
  rm -f "${PS_DOC}"
  cd "${BASE_DIR}"
done

echo "Creating layout PNGs ..."
for F in *.pcb; do
  PNG_DOC="${DOC_DIR}/${F%.pcb} Layout.png"
  pcb -x png --dpi 300 --only-visible --format PNG \
    --outfile "${PNG_DOC}" "${F}" >/dev/null
done

echo "Creating layout Gerbers ..."
for F in *.pcb; do
  GERBER_NAME="${F%.pcb}"
  GERBER_DIRNAME="${GERBER_NAME} Layout Gerbers"
  mkdir -p "${DOC_DIR}/${GERBER_DIRNAME}"
  pcb -x gerber --gerberfile "${DOC_DIR}/${GERBER_DIRNAME}/${GERBER_NAME}" \
    "${F}" >/dev/null
  cd "${DOC_DIR}"
  zip -rq "${GERBER_DIRNAME}.zip" "${GERBER_DIRNAME}"
  rm -rf "${GERBER_DIRNAME}"
  cd "${BASE_DIR}"
done

echo "Done."

