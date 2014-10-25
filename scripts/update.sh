#!/bin/bash

set -o errexit
set -o nounset

LANGUAGES="de_de"

for lang in $LANGUAGES ; do 

wget -m -nd http://static.tiptoi.com/db/tiptoi_$lang.xml
cat tiptoi_$lang.xml | ./download.pl | bash
#cat tiptoi_$lang.xml | ./download.pl

done
