QUIET?=@

INCLUDES = ../include

CFLAGS += -g -Wall 
CPPFLAGS += $(addprefix -I,$(INCLUDES))

TARGETS = \
	t_vmouse \
	t_ioctl \
	t_pageinfo \
	t_netlink_userspace \
	t_nlmcast \
	t_nlucast

all: $(TARGETS)

.PHONY: clean

clean:
	$(QUIET)rm -f $(TARGETS)
	$(QUIET)rm -f *.o *.d *.tmp
