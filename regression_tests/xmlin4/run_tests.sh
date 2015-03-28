#!/bin/sh

echo -e "*************** Running text tests ..."

echo -e "\nTesting simple 'metx'"
./run_one_test.sh meta-metx

echo -e "\nTesting 'metx' import when namespace is not given, shoud fail"
MP4Box -add meta-metx-no-namespace.nhml -new meta-metx-no-namespace.mp4 

echo -e "\nTesting simple 'mett'"
./run_one_test.sh meta-mett

echo -e "\nTesting 'mett' when no mime type is provided, should default to text/plain"
./run_one_test.sh meta-mett-no-mime

echo -e "\nTesting 'mett' with an XML document"
./run_one_test.sh meta-mett-xml

echo -e "\nTesting 'mett' with an XML document and a header"
./run_one_test.sh meta-mett-xml-header

echo -e "\nTesting 'stpp' import"
./run_one_test.sh subt-stpp

echo -e "\nTesting 'stpp' import when namespace is not provided, should fail"
MP4Box -add subt-stpp-no-namespace.nhml -new subt-stpp-no-namespace.mp4

echo -e "\nTesting 'sbtt' import"
./run_one_test.sh subt-sbtt

echo -e "\nTesting 'sbtt' import without mime, default to text/plain"
./run_one_test.sh subt-sbtt-no-mime

echo -e "\nTesting simple 'stxt' import"
./run_one_test.sh text-stxt

echo -e "\nTesting 'stxt' import without mime, default to text/plain"
./run_one_test.sh text-stxt-no-mime

echo -e "\nTesting 'stxt' import with header"
./run_one_test.sh text-stxt-header

echo -e "\n**************** Testing SWF conversion as SVG and import as 'stxt' stream "
MP4Box -add anim.swf:fmt=svg -new text-stxt-svg.mp4
MP4Box -info text-stxt-svg.mp4
MP4Box -raw 1 text-stxt-svg.mp4
MP4Box -raws 1 text-stxt-svg.mp4

#MP4Box -mp4 anim.swf

echo -e "\n**************** Testing TTML import as 'stpp' stream "
MP4Box -add ebu-ttd_sample.ttml -new subt-stpp-ttml.mp4
MP4Box -info subt-stpp-ttml.mp4
MP4Box -raw 1 subt-stpp-ttml.mp4
MP4Box -raws 1 subt-stpp-ttml.mp4

echo -e "\n**************** Generating file with all text variants text-all.mp4 "
MP4Box -add meta-mett.mp4 -add meta-mett-xml.mp4 -add meta-mett-xml-header.mp4 -add meta-metx.mp4 -add subt-sbtt.mp4 -add subt-stpp.mp4 -add subt-stpp-ttml.mp4 -add text-stxt.mp4 -add text-stxt-header.mp4 -add text-stxt-svg.mp4 -new text-all.mp4