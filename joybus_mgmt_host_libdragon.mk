# libdragon host client. Include after n64.mk, then add $(JOYBUS_MGMT_HOST_LIBDRAGON_OBJS)
# to the elf's prerequisites.
JOYBUS_MGMT_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

N64_C_AND_CXX_FLAGS += -I$(JOYBUS_MGMT_DIR)/include

# The shared host interface, plus the libdragon transport
JOYBUS_MGMT_HOST_LIBDRAGON_OBJS := $(BUILD_DIR)/joybus_mgmt/host.o $(BUILD_DIR)/joybus_mgmt/libdragon.o

# A pattern rule, so this cannot become the app's default goal
$(BUILD_DIR)/joybus_mgmt/%.o: $(JOYBUS_MGMT_DIR)/src/host/%.c
	@mkdir -p $(dir $@)
	@echo "    [CC] $<"
	$(CC) -c $(CFLAGS) -o $@ $<

# A dep file holds an explicit rule, so including one would otherwise make its
# object the app's default goal, ahead of whatever the app defines below
JOYBUS_MGMT_GOAL := $(.DEFAULT_GOAL)
-include $(wildcard $(BUILD_DIR)/joybus_mgmt/*.d)
.DEFAULT_GOAL := $(JOYBUS_MGMT_GOAL)
