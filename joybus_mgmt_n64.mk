# libdragon host client. Include after n64.mk, then add $(JOYBUS_MGMT_N64_OBJS)
# to the elf's prerequisites.
JOYBUS_MGMT_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

N64_C_AND_CXX_FLAGS += -I$(JOYBUS_MGMT_DIR)/include

# The shared host interface, plus the libdragon transport
JOYBUS_MGMT_N64_OBJS := $(BUILD_DIR)/joybus_mgmt/host.o $(BUILD_DIR)/joybus_mgmt/n64.o

# A pattern rule, so this cannot become the app's default goal
$(BUILD_DIR)/joybus_mgmt/%.o: $(JOYBUS_MGMT_DIR)/src/host/%.c
	@mkdir -p $(dir $@)
	@echo "    [CC] $<"
	$(CC) -c $(CFLAGS) -o $@ $<

-include $(wildcard $(BUILD_DIR)/joybus_mgmt/*.d)
