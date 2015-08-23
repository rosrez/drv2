QUIET?=@

INCLUDES = ../include

CFLAGS += -g -Wall 
CPPFLAGS += $(addprefix -I,$(INCLUDES))

TARGETS = \
	t_mmap \
	t_polld \
	t_task_struct

all: $(TARGETS)

.PHONY: clean

clean:
	$(QUIET)rm -f $(TARGETS)
	$(QUIET)rm -f *.o *.d
