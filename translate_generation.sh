#!/bin/bash
# this file is used to auto-generate .qm file from .ts file.
# author: shibowen at linuxdeepin.com
set -e

for ts in translations/*.ts
do
    printf '\nprocess %s\n' "$ts"
    /usr/lib/qt6/bin/lrelease "${ts}"
done
