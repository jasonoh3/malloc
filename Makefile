# Select version of g++
ifneq (, $(shell which g++-9))
CXX = g++-9
else
ifneq (, $(shell which g++-11))
CXX = g++-11
else
ifneq (, $(shell which g++))
CXX = g++
else
$(error No g++ compiler found.)
endif
endif
endif

# not using standard Makefile template because this makefile creates shared
# objects and weird stuff
CC = gcc
WARNINGS = -Wall -Wno-unused-result -Wno-unused-function #-Wextra -Werror -Wno-error=unused-parameter -Wmissing-declarations
CFLAGS_COMMON = $(WARNINGS) -std=c99 -D_GNU_SOURCE -lm $(CS340)
CFLAGS_RELEASE = $(CFLAGS_COMMON) -O3
CFLAGS_DEBUG = $(CFLAGS_COMMON) -O0 -g -DDEBUG
CFLAGS_CATCH = $(CFLAGS_COMMON) -std=c++11 -w -fpermissive
CFLAGS_SAMPLES = $(CFLAGS_COMMON) -w -fpermissive -Wunused-variable

OSX_SPECIFIC =

OS := $(shell uname)
ifeq ($(OS), Darwin)
OSX_SPECIFIC = -dynamiclib -flat_namespace
endif

all: programs sharedObjects samples testers

programs: mreplace mstats mstats-libc

sharedObjects: alloc.so lib/mstats-alloc.so lib/mstats-libc-alloc.so lib/osx-sbrk-mmap-wrapper.so

mp0-gif: tests/testers/mp0-gif/gif.c tests/testers/mp0-gif/main.c
	$(CC) $^ $(CFLAGS_DEBUG) -o $@


alloc.so: alloc.c
	$(CC) $^ $(CFLAGS_DEBUG) $(OSX_SPECIFIC) -o $@ -shared -fPIC -lm -ldl

lib/mstats-alloc.so: lib/mstats-alloc.c
	$(CC) $^ $(CFLAGS_DEBUG) $(OSX_SPECIFIC) -o $@ -shared -fPIC -lm -ldl

lib/mstats-libc-alloc.so: lib/mstats-alloc.c
	$(CC) $^ $(CFLAGS_DEBUG) $(OSX_SPECIFIC) -o $@ -shared -fPIC -lm -ldl -DUSE_LIBC_ALLOC


mreplace: mstats.c
	$(CC) $^ $(CFLAGS_DEBUG) -o $@ -ldl -lpthread

mstats: mstats.c
	$(CC) $< $(CFLAGS_RELEASE) -o $@ -ldl -lpthread -DSTATS_MODE

mstats-libc: mstats.c
	$(CC) $< $(CFLAGS_RELEASE) -o $@ -ldl -lpthread -DSTATS_MODE -DUSE_LIBC_ALLOC

lib/osx-sbrk-mmap-wrapper.so: lib/osx-sbrk-mmap-wrapper.c
	$(CC) $^ $(CFLAGS_DEBUG) -o $@ -shared -fPIC -lm


# testers compiled in debug mode to prevent compiler from optimizing away the
# behavior we are trying to test
TESTERS = $(patsubst %.c, %, $(wildcard tests/testers/*.c))
testers: $(TESTERS:tests/testers/%=tests/testers_exe/%)
tests/testers_exe/%: tests/testers/%.c
	@mkdir -p tests/testers_exe/
	$(CC) $^ $(CFLAGS_DEBUG) -o $@

# Compiling samples
SAMPLES = $(patsubst %.c, %, $(wildcard tests/samples/*.c))
samples: $(SAMPLES:tests/samples/%=tests/samples_exe/%)
tests/samples_exe/%: tests/samples/%.c
	@mkdir -p tests/samples_exe/
	$(CC) $^ $(CFLAGS_SAMPLES) -o $@


# Add target for tests
test: tests/test.o tests/lib/mstats-utils.so tests/test-week1-samples.cpp tests/test-week2-testers.cpp alloc.so
	$(CXX) $(CFLAGS_CATCH) tests/test.o tests/lib/mstats-utils.so tests/test-week1-samples.cpp tests/test-week2-testers.cpp -o $@

tests/lib/mstats-utils.so: tests/lib/mstats-utils.c
	$(CXX) $(CFLAGS_CATCH) $^ -c -o $@ -shared -fPIC -lm -ldl

tests/test.o: tests/test.cpp
	$(CXX) $(CFLAGS_CATCH) $^ -c -o $@

.PHONY : clean
clean:
	-rm -rf *.o alloc.so mreplace mstats mstats-libc testers_exe tests/testers_exe/ lib/*.so tests/samples_exe/ tests/test.o test mstats_result.txt tests/lib/*.so mp0-gif
