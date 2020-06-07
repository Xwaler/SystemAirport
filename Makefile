CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lpthread -lm

TARGET ?= SystemAirport
BUILD_DIR ?= ./build
SRC_DIRS ?= ./src
MKDIR_P ?= mkdir -p

SRCS := $(shell find $(SRC_DIRS) -name *.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

ifndef ECHO
HIT_TOTAL != ${MAKE} ${MAKECMDGOALS} --dry-run ECHO="HIT_MARK" | grep -c "HIT_MARK"
HIT_COUNT = $(eval HIT_N != expr ${HIT_N} + 1)${HIT_N}
ECHO = echo "[`expr ${HIT_COUNT} '*' 100 / ${HIT_TOTAL}`$(notdir %)]"
endif

all: $(TARGET) clean

$(TARGET): $(OBJS)
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@$(ECHO) $@

$(BUILD_DIR)/%.c.o: %.c
	@$(MKDIR_P) $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(ECHO) $@

.PHONY: clean

clean:
	@$(RM) -rf $(BUILD_DIR)

-include $(DEPS)
