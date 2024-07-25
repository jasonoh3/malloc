#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/catch.hpp"
#include "lib/mstats-utils.h"

// Testers 1-5
TEST_CASE("testers/mp0-gif - illinify gif", "[weight=20][part=5][suite=week2][timeout=30]") {
  system("make -s");
  system("make -s mp0-gif");

  system("cp tests/testers/mp0-gif/tay-small.gif tay-small.gif");

  system("./mstats ./mp0-gif evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  system("rm mstats_result.txt");
  REQUIRE(result->status == 1);

  REQUIRE( system("diff tay-small-illinify.gif tests/testers/mp0-gif/tay-small-illinify-expected.gif") == 0 );
  system("rm -f tay-small-illinify.gif");
}

TEST_CASE("tester1", "[weight=10][part=5][suite=week2][timeout=30]") {
  system("make -s");
  system("./mstats tests/testers_exe/tester1 evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  system("rm mstats_result.txt");
  REQUIRE(result->status == 1);
}

TEST_CASE("tester2", "[weight=10][part=5][suite=week2][timeout=30]") {
  system("make -s");
  system("./mstats tests/testers_exe/tester2 evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  system("rm mstats_result.txt");
  REQUIRE(result->status == 1);
}

TEST_CASE("tester3", "[weight=20][part=5][suite=week2][timeout=30]") {
  system("make -s");
  system("./mstats tests/testers_exe/tester3 evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  system("rm mstats_result.txt");
  REQUIRE(result->status == 1);
}

TEST_CASE("tester4", "[weight=20][part=5][suite=week2][timeout=30]") {
  system("make -s");
  system("./mstats tests/testers_exe/tester4 evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  system("rm mstats_result.txt");
  REQUIRE(result->status == 1);
}

TEST_CASE("tester5", "[weight=20][part=5][suite=week2][timeout=30]") {
  system("make -s");
  system("./mstats tests/testers_exe/tester5 evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  system("rm mstats_result.txt");
  REQUIRE(result->status == 1);
}
