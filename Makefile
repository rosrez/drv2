EXCLDIRS='^.$$'

SUBDIRS=$(shell find . -maxdepth 1 -type d | grep -v -E $(EXCLDIRS)) 

# The default is to build modules

all:
	@for dir in $(SUBDIRS); do \
	  if [ ! -f $$dir/Makefile ]; then \
	    continue; \
	  fi; \
	  echo "Building $$dir"; \
	  $(MAKE) -C $$dir; \
	done

clean:
	@for dir in $(SUBDIRS); do \
	  if [ ! -f $$dir/Makefile ]; then \
	    continue; \
	  fi; \
	  echo "Cleaning $$dir"; \
	  $(MAKE) -C $$dir clean; \
	done

.PHONY: all clean 
