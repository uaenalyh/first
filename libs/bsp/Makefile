#
# ACRN Hypervisor Static Library for Debug Features
#

FILE_PATH := $(dir $(MKFL_NAME))
SRCS += $(wildcard $(FILE_PATH)/*.c)
OBJS += $(patsubst %.c,$(HV_OBJDIR)/%.o,$(SRCS))

.PHONY: default
default: lib

lib: $(OBJS)
	$(AR) $(ARFLAGS) $(BSP_MOD) $(OBJS)

.PHONY: clean
clean:
	rm -f $(OBJS)

-include $(OBJS:.o=.d)

$(HV_OBJDIR)/%.o: %.c
	[ ! -e $@ ] && mkdir -p $(dir $@) && mkdir -p $(HV_MODDIR); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. -c $(CFLAGS) $(ARCH_CFLAGS) $< -o $@ -MMD -MT $@
