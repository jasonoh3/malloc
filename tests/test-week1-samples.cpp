#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/catch.hpp"
#include "lib/mstats-utils.h"

// ALLOCATION TESTS
TEST_CASE("00-simple - allocates memory and avoids errors", "[weight=1][part=1]") {
  system("make -s");
  system("./mstats tests/samples_exe/00-simple evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used > 10000);
  system("rm mstats_result.txt");
}

TEST_CASE("01-tutorial - allocates memory and avoid errors", "[weight=3][part=1]") {
  system("make -s");
  system("./mstats tests/samples_exe/01-tutorial evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used < 1700);
  REQUIRE(result->max_heap_used > 0);
  system("rm mstats_result.txt");
}

TEST_CASE("02-simple-reuse-of-free - reuses available free blocks", "[weight=3][part=1]") {
  system("make -s");
  system("./mstats tests/samples_exe/02-simple-reuse-of-free evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used < 0x60000);
  REQUIRE(result->max_heap_used > 0);
  system("rm mstats_result.txt");
}

TEST_CASE("03-partial-reuse-of-free - reuses available free blocks when possible", "[weight=3][part=1]") {
  system("make -s");
  system("./mstats tests/samples_exe/03-partial-reuse-of-free evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used < 0x95000);
  REQUIRE(result->max_heap_used > 0);
  system("rm mstats_result.txt");
}

TEST_CASE("04-calloc - calloc zeroes memory", "[weight=5][part=1]") {
  system("make -s");
  system("./mstats tests/samples_exe/04-calloc evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used > 0);
  system("rm mstats_result.txt");
}

TEST_CASE("05-realloc-as-free - realloc acts as free with size=0", "[weight=2][part=1]") {
  system("make -s");
  system("./mstats tests/samples_exe/05-realloc-as-free evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used > 0);
  system("rm mstats_result.txt");
}

TEST_CASE("06-realloc-smaller - realloc moves the contents when reassigning it", "[weight=3][part=1]") {
  system("make -s");
  system("./mstats tests/samples_exe/06-realloc-smaller evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used < 0x15000);
  system("rm mstats_result.txt");
}

TEST_CASE("07-realloc-moves - realloc moves the contents when reassigning it", "[weight=5][part=1]") {
  system("make -s");
  system("./mstats tests/samples_exe/07-realloc-moves evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used > 0x30000);
  system("rm mstats_result.txt");
}

// BLOCK SPLITTING - 25 points
TEST_CASE("01-tutorial - including blocking spliting", "[weight=25][part=2]") {
  system("make -s");
  system("./mstats tests/samples_exe/01-tutorial evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used < 1200);
  REQUIRE(result->max_heap_used > 0);
  system("rm mstats_result.txt");
}

// MEMORY COALESCING - 25 points
TEST_CASE("08-coalescing - Basic Coalescing", "[weight=10][part=3]") {
  system("make -s");
  system("./mstats tests/samples_exe/08-coalescing evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used < 0x55000);
  REQUIRE(result->max_heap_used > 0);
  system("rm mstats_result.txt");
}

TEST_CASE("09-coalescing-in-middle - Coalescing in the middle of a segment", "[weight=15][part=3]") {
  system("make -s");
  system("./mstats tests/samples_exe/09-coalescing-in-middle evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used < 0x95000);
  REQUIRE(result->max_heap_used > 0);
  system("rm mstats_result.txt");
}

// FREE LISTS - 25 points
TEST_CASE("10-free-lists - Free Lists", "[weight=25][part=4]") {
  system("make -s");
  system("./mstats tests/samples_exe/10-free-lists evaluate");
  mstats_result * result = read_mstats_result("mstats_result.txt");
  REQUIRE(result->status == 1);
  REQUIRE(result->max_heap_used < 100000 * 100 * 3);
  REQUIRE(result->max_heap_used > 0);
  REQUIRE(result->time_taken < 3);
  system("rm mstats_result.txt");
}
