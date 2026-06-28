ASM = nasm
CC = gcc
CFLAGS = -ffreestanding -m64 -mno-red-zone -nostdlib -Wall -Wextra -fno-pie -fno-pic -I/usr/include/efi -I/usr/include/efi/x86_64
CFLAGS_BOOT = -ffreestanding -fshort-wchar -mno-red-zone -DEFI_FUNCTION_WRAPPER -I/usr/include/efi -I/usr/include/efi/x86_64 -Wall
LD = ld
LD_FLAGS = -nostdlib -z max-page-size=0x1000
LD_FLAGS_BOOT = -nostdlib -shared -Wl,-znocombreloc -Wl,-Bsymbolic -Wl,-z,notext -Wl,-T,/usr/lib/elf_x86_64_efi.lds /usr/lib/crt0-efi-x86_64.o

all: BOOT Find Kernel.elf Image Run

# Find every C and ASMfile in Source folder
C_SOURCES := $(shell find Source -name "*.c" ! -name "Bootx64.c")
ASM_SOURCES := $(shell find Source -name "*.asm")

# Convert {Source}.c/asm into {Source}.o
OBJ_FILES := $(patsubst Source/%.c, Build/%.o, $(C_SOURCES))
OBJ_FILES += $(patsubst Source/%.asm, Build/%.o, $(ASM_SOURCES))

BOOT:
	$(CC) $(CFLAGS_BOOT) -c Source/Bootx64.c -o Build/Bootx64.o
	$(CC) $(LD_FLAGS_BOOT) Build/Bootx64.o -o Build/Bootx64.so -L/usr/lib -lgnuefi -lefi
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
        -j .rela -j .reloc --target=efi-app-x86_64 \
        Build/Bootx64.so Build/Bootx64.efi

Find:
	find Source -type d | sed 's|Source|Build|' | xargs mkdir -p
	rm -r Build/include

# Compilation rule - compile only when the C/ASM file has changed

Build/%.o: Source/%.c
	$(CC) $(CFLAGS) -c $< -o $@

Build/%.o: Source/%.asm
	$(ASM) -f elf64 $< -o $@

# Build Kernel.elf
Kernel.elf: $(OBJ_FILES)
	$(LD) $(LD_FLAGS) -T Source/Kernel_LinkerScript.ld $(OBJ_FILES) -o Build/Kernel.elf

# Write files to USB in UEFI format
USB:
	echo Whoops don't have anything for this just yet!

Image:
	# Mount NVMe image and copy files, then unmount
	sudo losetup -Pf Build/NVME.img
	mkdir -p /tmp/NVME
	sudo mount /dev/loop0p1 /tmp/NVME
	sudo cp Build/Bootx64.efi /tmp/NVME/EFI/Boot/
	sudo cp Build/Kernel.elf /tmp/NVME/
	sudo umount /tmp/NVME
	sudo losetup -d /dev/loop0

Run:
	qemu-system-x86_64 \
    -machine q35 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=Build/NVME.img,if=none,id=nvme0,format=raw \
    -device nvme,drive=nvme0,serial=RaffayNVMe

Commit:
	git remote set-url origin https://github.com/PotionSwirl/Brainrot26.git
	git add .
	git commit -m "$(MSG)"
	git push -u origin main
