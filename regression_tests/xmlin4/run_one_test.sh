#!/bin/sh

echo -e "*************** Running basic test for "$1

echo -e "\nTesting import"
MP4Box -add $1.nhml -new $1.mp4

echo -e "\nTesting info dump"
MP4Box -info $1.mp4

echo -e "\nTesting entire track extraction"
MP4Box -raw 1 $1.mp4

echo -e "\nTesting sample by sample extraction"
MP4Box -raws 1 $1.mp4

echo -e "\nTesting nhml export"
MP4Box -nhml 1 $1.mp4

echo -e "\nTesting nhml re-import"
MP4Box -add $1_track1.nhml -new $1_track1.mp4
