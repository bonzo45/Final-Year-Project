#include <cstring>
#include <iostream>

char xml_schema[] =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<executable>\n"
"  <category>Super-Resolution</category>\n"
"  <title>Reconstruction</title>\n"
"  <description><![CDATA[Combines a number of slice stacks into one super-resolution volume.]]></description>\n"
"  <version>0.0.1</version>\n"
"  <documentation-url>TODO</documentation-url>\n"
"  <license>TODO</license>\n"
"  <contributor>Bernhard Kainz, Sam Esgate, ..., (Imperial College London)</contributor>\n"
"  <parameters advanced=\"false\">\n"
"  <label>Input: Slice Stacks and Mask</label>\n"
"    <description>Slice Stacks to combine (required) and Mask (optional) which marks the region of interest in the slices.</description>\n"
"    <integer multiple=\"true\">\n"
"      <name>slices</name>\n"
"      <label>\"Slices\"</label>\n"
"      <default>required</default>\n"
"      <flag>i</flag>\n"
"      <channel>input</channel>\n"
"    </integer>\n"
"    <image>      \n"
"      <name>mask</name>\n"
"      <label>Mask</label>\n"
"      <default></default>\n"
"      <flag>m</flag>\n"
"      <channel>input</channel>\n"
"    </image>\n"
"  </parameters>\n"
"  <parameters advanced=\"false\">\n"
"    <label>Output</label>\n"
"    <description>Reconstructed Super-Resolution MRI Volume</description>\n"
"    <image>\n"
"      <name>output</name>\n"
"      <description>Super-Resolution Volume</description>\n"
"      <label>Super-Resolution Volume</label>\n"
"      <default>3T_GPUtest.nii</default>\n"
"      <flag>o</flag>\n"
"      <channel>output</channel>\n"
"    </image>\n"
"  </parameters>\n"
"</executable>\n"
;

int main(int argc, char** argv) {
  if (argc > 1) {
    if (strcmp(argv[1], "--xml") == 0){
      std::cout << xml_schema;
    }
  }

  return 0;
}