#!/bin/bash

# Do everything required for making a release of Generation 7 Electronics.
#
# Copyright (c) Markus "Traumflug" Hitter 2011

if [ "$1" = "" -o "$2" != "" ]; then
  echo "usage: $(basename $0) <release-number>"
  exit 1
fi

while [ ! -d .git ]; do cd ..; done

RELEASE="$1"
BASE_DIR="${PWD}"
DOC_DIR="${BASE_DIR}/release documents"

echo "Uncommitting old release ..."
git rm "${DOC_DIR}"/* || exit 1
echo "... done."

# Reset to a clean state.
rm -rf "${DOC_DIR}"
mkdir -p "${DOC_DIR}"

echo "Create Arduino IDE support ..."
cd "arduino support"
./make.sh
mv *.zip "${DOC_DIR}"
cd "${BASE_DIR}"
echo "... done."

echo "Creating release number file ..."
touch "${DOC_DIR}/this is release-${RELEASE}"
echo "... done."

echo "Creating schematic PDFs ..."
for F in *.sch; do
  PS_DOC="${DOC_DIR}/${F%.sch} Schematic.ps"
  gschem -p -o "${PS_DOC}" -s "${BASE_DIR}/tools/print.scm" "${F}" >/dev/null
  cd "${DOC_DIR}"
  ps2pdf "${PS_DOC}"
  rm -f "${PS_DOC}"
  cd "${BASE_DIR}"
done
echo "... done."

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
echo "... done."

echo "Creating layout PNGs ..."
for F in *.pcb; do
  PNG_DOC="${DOC_DIR}/${F%.pcb} Layout.png"
  pcb -x png --dpi 300 --only-visible --format PNG \
    --outfile "${PNG_DOC}" "${F}" >/dev/null
done
echo "... done."

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
echo "... done."

echo "Committing all the release files ..."
git add --force "${DOC_DIR}"
git commit -m "Make Release ${RELEASE}."
echo "... please enter release message - usually starts"
echo "    with \"New features: ...\" ..."
sleep 3
git tag -u 806F3A3E "release-${RELEASE}"
echo "... done."

echo "All done. Now push the result with \"git push --tags\"."


#  This script is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either
#  version 3 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.


