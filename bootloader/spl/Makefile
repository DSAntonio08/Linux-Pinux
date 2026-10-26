CROSS = arm-none-eabi-
CFLAGS = -mcpu=cortex-a7 -marm -mfloat-abi=soft -ffreestanding -nostdlib -Os -Wall -Wextra -Iinclude -MMD -MP
	OBJ_FILES = build/start.o build/uart.o build/timer.o build/clock.o build/spl.o
OBJCPY_FLAGS = -O binary
HOSTFLAGS = -Wall -Wextra -g -fsanitize=address,undefined
.PHONY: all clean
.DELETE_ON_ERROR:

all: build/spl-boot.bin | build

clean:
	rm -rf build

build:
	mkdir -p build

build/%.o: src/%.c | build
	$(CROSS)gcc $(CFLAGS) -c $< -o $@

build/%.o: src/%.S | build
	$(CROSS)gcc $(CFLAGS) -c $< -o $@

build/spl.elf: $(OBJ_FILES) src/linker.ld | build
	$(CROSS)gcc $(CFLAGS) -T src/linker.ld -Wl,--orphan-handling=warn  $(OBJ_FILES) -o $@
	
build/spl.bin: build/spl.elf | build
	$(CROSS)objcopy $(OBJCPY_FLAGS) $< $@

../../tools/mkboot0: ../../tools/mkboot0.c
	cc $(HOSTFLAGS) $< -o $@

build/spl-boot.bin: build/spl.bin ../../tools/mkboot0 | build
	../../tools/mkboot0 $< $@

-include $(OBJ_FILES:.o=.d)