#include "test.h"

#include "../vr_options.h"

using namespace dxvk;


TEST_CASE(options_default_to_snap_turn) {
  VrOptions options;
  CHECK(!options.smoothTurn);
}


TEST_CASE(options_round_trip_through_the_file_format) {
  VrOptions written;
  written.smoothTurn = true;

  VrOptions read;
  vrParseOptions(vrFormatOptions(written), read);
  CHECK(read.smoothTurn);

  written.smoothTurn = false;
  read.smoothTurn = true;
  vrParseOptions(vrFormatOptions(written), read);
  CHECK(!read.smoothTurn);
}


TEST_CASE(options_parser_ignores_comments_sections_and_case) {
  VrOptions options;
  vrParseOptions("; comment\n# another\n[VR]\r\n  SMOOTHturn = 1 \r\nother=3\n", options);
  CHECK(options.smoothTurn);
}


TEST_CASE(options_parser_keeps_values_it_cannot_read) {
  VrOptions options;
  options.smoothTurn = true;

  vrParseOptions("", options);
  CHECK(options.smoothTurn);

  vrParseOptions("SmoothTurn=maybe\nSmoothTurn\n=1\n", options);
  CHECK(options.smoothTurn);

  vrParseOptions("SmoothTurn=0", options);
  CHECK(!options.smoothTurn);
}
