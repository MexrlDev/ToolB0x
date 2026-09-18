NAME    := toolbox
CROSS   ?= 
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

SRC_DIR := src
BUILD   := build

CFLAGS := -m64 -ffreestanding -fPIE -fno-stack-protector -fno-builtin \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -mno-red-zone -fno-omit-frame-pointer -mgeneral-regs-only \
          -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
          -I$(SRC_DIR)

LDFLAGS := -nostdlib -nostartfiles -nodefaultlibs -pie \
           -Wl,-T,linker.ld -Wl,--build-id=none -Wl,-z,notext \
           -Wl,--no-undefined

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean hex

all: $(NAME).elf $(NAME).bin

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(NAME).elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(NAME).bin: $(NAME).elf
	$(OBJCOPY) -O binary $< $@
	@echo "bin: $$(wc -c < $@) bytes"

hex: $(NAME).bin
	@python3 -c "import sys; print(' '.join('%02X' % b for b in open('$(NAME).bin','rb').read()))" > $(NAME).hex
	@echo "hex: $$(wc -c < $(NAME).hex) chars"

clean:
	rm -rf $(BUILD) $(NAME).elf $(NAME).bin $(NAME).hex
