CROSS = arm-none-eabi-
CFLAGS = -mcpu=cortex-a7 -marm -mfloat-abi=soft -ffreestanding -nostdlib -Os -Wall -Wextra -Iinclude
OBJ_FILES = build/start.o build/uart.o
OBJCPY_FLAGS = -O binary
HOSTFLAGS = -Wall -Wextra -g -fsanitize=address,undefined

build/%.o: src/%.c
	$(CROSS)gcc $(CFLAGS) -c $< -o $@

build/%.o: src/%.S
	$(CROSS)gcc $(CFLAGS) -c $< -o $@

build/spl.elf: $(OBJ_FILES) src/linker.ld
	$(CROSS)gcc $(CFLAGS) -T src/linker.ld -Wl,--orphan-handling=warn  $(OBJ_FILES) -o $@
	
build/spl.bin: build/spl.elf
	$(CROSS)objcopy $(OBJCPY_FLAGS) $< $@

../../tools/mkboot0: ../../tools/mkboot0.c
	cc $(HOSTFLAGS) $< -o $@

build/spl-boot.bin: build/spl.bin ../../tools/mkboot0
	../../tools/mkboot0 $< $@
	