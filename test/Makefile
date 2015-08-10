QUIET?=@

INCLUDES = ../include

CFLAGS += -g -Wall 
CPPFLAGS += $(addprefix -I,$(INCLUDES))

TARGETS = \
	t_vmouse \
	t_ioctl \
	t_pageinfo \

all: $(TARGETS)

.PHONY: clean

clean:
	$(QUIET)rm -f $(TARGETS)
	$(QUIET)rm -f *.o *.d
