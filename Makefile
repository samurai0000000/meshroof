# Makefile
#
# Copyright (C) 2025, Charles Chiou

MAKEFLAGS =	--no-print-dir

IDF_PATH :=	$(realpath esp-idf)
export IDF_PATH

# Make cannot source a script into its own process. If export.sh was not
# sourced (IDF_PYTHON_ENV_PATH unset), re-enter this Makefile from bash
# after sourcing it quietly.
ifeq ($(or $(IDF_PYTHON_ENV_PATH),$(IDF_EXPORTED)),)

.PHONY: _idf_export
_idf_export:
	@if [ ! -f "$(IDF_PATH)/export.sh" ]; then \
		echo "ESP-IDF export.sh not found at $(IDF_PATH)/export.sh" >&2; \
		exit 1; \
	fi
	@bash -c 'if ! . "$(IDF_PATH)/export.sh" >/dev/null 2>&1; then \
		echo "Failed to source $(IDF_PATH)/export.sh" >&2; \
		. "$(IDF_PATH)/export.sh"; \
		exit 1; \
	fi; \
	exec $(MAKE) IDF_EXPORTED=1 $(MAKECMDGOALS)'

ifeq ($(MAKECMDGOALS),)
.DEFAULT_GOAL :=	_idf_export
else
.PHONY: $(MAKECMDGOALS)
$(MAKECMDGOALS): _idf_export
	@:
endif

else

TARGETS +=	build/meshroof.bin

.PHONY: default clean distclean $(TARGETS)

default: $(TARGETS)

clean:
	@test -f build/Makefile && $(MAKE) -C build clean

distclean:
	rm -rf build/ sdkconfig

.PHONY: meshroof

meshroof: build/meshroof.bin

MESHROOF_TREE :=	\
	CMakeLists.txt version.h.in \
	$(wildcard *.cxx) $(wildcard *.hxx) \
	main \
	libmeshtastic

build/meshroof.bin: build/Makefile sdkconfig
	@if [ -f build/version.h ] && [ -n "`find -H $(MESHROOF_TREE) -type f \
	    \( -name '*.c' -o -name '*.cxx' -o -name '*.h' -o -name '*.hxx' \
	       -o -name 'CMakeLists.txt' -o -name 'version.h.in' \) \
	    -newer build/version.h -print -quit`" ]; then \
		rm -f build/version.h; \
	fi
	@$(MAKE) -C build

build/Makefile: CMakeLists.txt
	@mkdir -p build
	@cd build && cmake ..

.PHONY: release

release: build/Makefile
	@rm -f build/version.h
	@$(MAKE) -C build

sdkconfig: misc/sdkconfig
	@echo install misc/sdkconfig
	@cp -f $< $@

.PHONY: menuconfig

menuconfig: build/Makefile
	@$(MAKE) -C build $@

# Development & debug targets

ESPPORT ?=	$(shell misc/find_espressif_serial.sh)

.PHONY: flash

flash: build/Makefile
	@$(MAKE) -C build flash ESPPORT=$(ESPPORT)

.PHONY: reset

reset:
	@esptool.py --port $(ESPPORT) \
		--before default_reset --after hard_reset chip_id

endif
