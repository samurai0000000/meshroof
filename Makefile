# Makefile
#
# Copyright (C) 2025, Charles Chiou

MAKEFLAGS =	--no-print-dir

IDF_PATH :=	$(realpath esp-idf)
export IDF_PATH

TARGETS +=	build/meshroof.bin

.PHONY: default clean distclean $(TARGETS)

default: $(TARGETS)

clean:
	@test -f build/Makefile && $(MAKE) -C build clean

distclean:
	rm -rf build/

.PHONY: meshroof

meshroof: build/meshroof.bin

build/meshroof.bin: build/Makefile
	@$(MAKE) -C build

build/Makefile: CMakeLists.txt
	@mkdir -p build
	@cd build && cmake ..
