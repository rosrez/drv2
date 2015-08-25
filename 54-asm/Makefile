CFLAGS += -g -Wall 
ASFLAGS += -g -Wall

TARGETS = \
	syscall_64 \
	syscall_32 \
	syscall_cat \
	showarg \
	asm-inline

all: $(TARGETS)
	@echo "Done."

.PHONY: clean

clean:
	@rm -f $(TARGETS)
	@rm -f *.o
	@echo "Done cleaning."

syscall_64 syscall_32 syscall_cat: LDFLAGS += -nostdlib

syscall_32: LDFLAGS += -m32
