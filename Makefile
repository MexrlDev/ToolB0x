NAME    := toolbox
CROSS   ?=
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
PYTHON  ?= python3

SRC_DIR := src
BUILD   := build

CFLAGS := -m64 -ffreestanding -fPIE -fno-stack-protector -fno-builtin \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -mno-red-zone -fno-omit-frame-pointer -mgeneral-regs-only \
          -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
          -I$(SRC_DIR)

LDFLAGS := -nostdlib -nostartfiles -nodefaultlibs -pie \
           -Wl,-T,$(CURDIR)/linker.ld -Wl,--build-id=none \
           -Wl,--no-undefined

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean hex check-tools

all: check-tools $(NAME).elf $(NAME).bin

check-tools:
	@command -v $(CC)      >/dev/null || { echo "ERROR: $(CC) not found";      exit 1; }
	@command -v $(OBJCOPY) >/dev/null || { echo "ERROR: $(OBJCOPY) not found"; exit 1; }
	@test -f linker.ld || { echo "ERROR: linker.ld is missing from repo root"; exit 1; }
	@test -d $(SRC_DIR) || { echo "ERROR: src/ directory is missing"; exit 1; }

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(NAME).elf: $(OBJS) linker.ld
	@echo "  LD  $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(NAME).bin: $(NAME).elf
	@echo "  BIN $@"
	@$(OBJCOPY) -O binary $< $@
	@echo "  -> $$(wc -c < $@) bytes"

hex: $(NAME).bin
	@echo "  HEX $(NAME).hex"
	@$(PYTHON) -c "import sys; sys.stdout.write(' '.join('%02X' % b for b in open('$(NAME).bin','rb').read()))" > $(NAME).hex
	@echo "  -> $$(wc -c < $(NAME).hex) chars"

clean:
	@rm -rf $(BUILD) $(NAME).elf $(NAME).bin $(NAME).hex
