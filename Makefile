
# list each subdirectory to be built here
SUBDIRS = cm4 cm0plus

# for each entry in SUBDIRS, this creates a corresponding variable with the prefix "clean-"
# e.g. "cm4" -> "clean-cm4"
CLEANSUBDIRS = $(addprefix clean-, $(SUBDIRS))

# .PHONY targets will be run every time they are called.
# any special recipes you want to run by name should be a phony target.
.PHONY: all clean debug help $(SUBDIRS) $(CLEANSUBDIRS)

# the first recipe listed is the one that will be called by default if you type "make" with no arguments

# to make the target "all", everything in SUBDIRS must be up-to-date
all: $(SUBDIRS)

# to make each target in SUBDIRS, call "make -C" on each value in SUBDIRS
$(SUBDIRS):
	$(MAKE) -C $@

# whenever "make clean" is called, call each variable in CLEANSUBDIRS individually
clean: $(CLEANSUBDIRS)

# this recipe uses a static pattern that only applies to the list of targets in CLEANSUBDIRS
$(CLEANSUBDIRS): clean-%:
	$(MAKE) -C $* clean

debug: all
	./debug.sh

# recipe to print usage information
help:
	@echo "               make: calls 'make' and rebuilds source code in each subdirectory: $(SUBDIRS)"
	@echo "           make all: same as make with no arguments"
	@echo "         make clean: calls 'make clean' in each subdirectory: $(SUBDIRS)"
	@echo "  make clean-SUBDIR: only calls 'make clean' in the specified subdirectory"
	@echo "         make debug: rebuilds all source code, then calls 'debug.sh' to autostart debugging"
	@echo "          make help: displays this help message" 

