cmake_minimum_required(VERSION 3.15)
project(Bootloader C ASM_NASM)

enable_language(ASM_NASM)

# BIOS toolchain (x86_64-elf)
set(ELF_TOOLCHAIN_WINDOWS_URL "https://github.com/lordmilko/i686-elf-tools/releases/download/13.2.0/x86_64-elf-tools-windows.zip")
set(ELF_TOOLCHAIN_LINUX_URL "https://github.com/lordmilko/i686-elf-tools/releases/download/13.2.0/x86_64-elf-tools-linux.zip")
set(ELF_TOOLCHAIN_ZIP "${CMAKE_SOURCE_DIR}/x86_64-elf-tools.zip")
set(ELF_TOOLCHAIN_DIR "${CMAKE_SOURCE_DIR}/x86_64-elf-tools")
set(X86_64_ELF_BIN "${ELF_TOOLCHAIN_DIR}/bin/x86_64-elf-")

# UEFI toolchain (MinGW-w64)
set(MINGW_TOOLCHAIN_WINDOWS_URL "https://github.com/niXman/mingw-builds-binaries/releases/download/14.2.0-rt_v12-rev0/x86_64-14.2.0-release-win32-seh-ucrt-rt_v12-rev0.7z")
set(MINGW_TOOLCHAIN_LINUX_URL "https://github.com/niXman/mingw-builds-binaries/releases/download/14.2.0-rt_v12-rev0/x86_64-14.2.0-release-posix-seh-ucrt-rt_v12-rev0.7z")
set(MINGW_TOOLCHAIN_ZIP "${CMAKE_SOURCE_DIR}/mingw-w64.7z")
set(MINGW_TOOLCHAIN_DIR "${CMAKE_SOURCE_DIR}/mingw-w64")
set(MINGW_BIN "${MINGW_TOOLCHAIN_DIR}/mingw64/bin/x86_64-w64-mingw32-")

# --- Include directories ---
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- Sources ---
set(STAGE1_SRC ${CMAKE_SOURCE_DIR}/src/boot/boot1.asm)
set(STAGE2_SRC ${CMAKE_SOURCE_DIR}/src/boot/boot2.asm)
set(KERNEL_SRC ${CMAKE_SOURCE_DIR}/src/kernel/kernel.c)
set(VGA_SRC ${CMAKE_SOURCE_DIR}/src/kernel/vga.c)
set(MEM_SRC ${CMAKE_SOURCE_DIR}/src/kernel/mem.c)
set(PORT_SRC ${CMAKE_SOURCE_DIR}/src/kernel/port.c)
set(KBD_SRC ${CMAKE_SOURCE_DIR}/src/kernel/keyboard.c)
set(DISK_SRC ${CMAKE_SOURCE_DIR}/src/kernel/disk.c)
set(FAT32_SRC ${CMAKE_SOURCE_DIR}/src/kernel/fat32.c)
set(EFI_MAIN_SRC ${CMAKE_SOURCE_DIR}/src/boot/efi/efi_main.c)

# --- Outputs ---
set(STAGE1_BIN ${CMAKE_BINARY_DIR}/boot1.bin)
set(STAGE2_BIN ${CMAKE_BINARY_DIR}/boot2.bin)
set(STAGE2_OBJ ${CMAKE_BINARY_DIR}/boot2.o)
set(VGA_OBJ ${CMAKE_BINARY_DIR}/vga.o)
set(MEM_OBJ ${CMAKE_BINARY_DIR}/mem.o)
set(PORT_OBJ ${CMAKE_BINARY_DIR}/port.o)
set(KBD_OBJ ${CMAKE_BINARY_DIR}/keyboard.o)
set(DISK_OBJ ${CMAKE_BINARY_DIR}/disk.o)
set(FAT32_OBJ ${CMAKE_BINARY_DIR}/fat32.o)
set(KERNEL_OBJ ${CMAKE_BINARY_DIR}/kernel.o)
set(DISK_IMG ${CMAKE_BINARY_DIR}/disk.img)
set(EFI_MAIN_OBJ ${CMAKE_BINARY_DIR}/efi_main.o)
set(EFI_MAIN_SO ${CMAKE_BINARY_DIR}/efi_main.so)
set(EFI_MAIN_BIN ${CMAKE_BINARY_DIR}/BOOTX64.EFI)

# --- Build Stage1 ---
add_custom_command(
    OUTPUT ${STAGE1_BIN}
    COMMAND ${CMAKE_ASM_NASM_COMPILER} -f bin ${STAGE1_SRC} -o ${STAGE1_BIN}
    DEPENDS ${STAGE1_SRC}
    COMMENT "Assembling Stage 1 -> ${STAGE1_BIN}"
)

# --- Build Stage2 (64-bit elf) ---
add_custom_command(
    OUTPUT ${STAGE2_OBJ}
    COMMAND ${CMAKE_ASM_NASM_COMPILER} -f elf32 ${STAGE2_SRC} -o ${STAGE2_OBJ}
    DEPENDS ${STAGE2_SRC}
    COMMENT "Assembling Stage 2 -> ${STAGE2_OBJ}"
)

# --- Build kernel components ---
add_custom_command(
    OUTPUT ${KERNEL_OBJ}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -m32 -fno-pic -fno-builtin -fno-stack-protector -O0 -c ${KERNEL_SRC} -o ${KERNEL_OBJ} -I${CMAKE_SOURCE_DIR}/include
    DEPENDS ${KERNEL_SRC}
    COMMENT "Compiling kernel C -> ${KERNEL_OBJ}"
)

add_custom_command(
    OUTPUT ${VGA_OBJ}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -m32 -c ${VGA_SRC} -o ${VGA_OBJ} -O0 -fno-builtin -fno-stack-protector -fno-pic -I${CMAKE_SOURCE_DIR}/include
    DEPENDS ${VGA_SRC}
    COMMENT "Compiling VGA driver -> ${VGA_OBJ}"
)

add_custom_command(
    OUTPUT ${MEM_OBJ}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -m32 -c ${MEM_SRC} -o ${MEM_OBJ} -O0 -fno-builtin -fno-stack-protector -fno-pic -I${CMAKE_SOURCE_DIR}/include
    DEPENDS ${MEM_SRC}
    COMMENT "Compiling MEM -> ${MEM_OBJ}"
)

add_custom_command(
    OUTPUT ${PORT_OBJ}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -m32 -c ${PORT_SRC} -o ${PORT_OBJ} -O0 -fno-builtin -fno-stack-protector -fno-pic -I${CMAKE_SOURCE_DIR}/include
    DEPENDS ${PORT_SRC}
    COMMENT "Compiling PORT -> ${PORT_OBJ}"
)

add_custom_command(
    OUTPUT ${KBD_OBJ}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -m32 -c ${KBD_SRC} -o ${KBD_OBJ} -O0 -fno-builtin -fno-stack-protector -fno-pic -I${CMAKE_SOURCE_DIR}/include
    DEPENDS ${KBD_SRC}
    COMMENT "Compiling KBD -> ${KBD_OBJ}"
)

add_custom_command(
    OUTPUT ${DISK_OBJ}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -m32 -c ${DISK_SRC} -o ${DISK_OBJ} -O0 -fno-builtin -fno-stack-protector -fno-pic -I${CMAKE_SOURCE_DIR}/include
    DEPENDS ${DISK_SRC}
    COMMENT "Compiling DISK -> ${DISK_OBJ}"
)

add_custom_command(
    OUTPUT ${FAT32_OBJ}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -m32 -c ${FAT32_SRC} -o ${FAT32_OBJ} -O0 -fno-builtin -fno-stack-protector -fno-pic -I${CMAKE_SOURCE_DIR}/include
    DEPENDS ${FAT32_SRC}
    COMMENT "Compiling FAT32 -> ${FAT32_OBJ}"
)

# --- Link Stage2 into flat binary ---
add_custom_command(
    OUTPUT ${STAGE2_BIN}
    COMMAND ${X86_64_ELF_BIN}ld -m elf_i386 -T ${CMAKE_SOURCE_DIR}/linker.ld -nostdlib -o stage2.elf ${STAGE2_OBJ} ${KERNEL_OBJ} ${VGA_OBJ} ${MEM_OBJ} ${KBD_OBJ} ${PORT_OBJ} ${DISK_OBJ} ${FAT32_OBJ}
    COMMAND ${X86_64_ELF_BIN}objcopy -O binary stage2.elf ${STAGE2_BIN}
    DEPENDS ${STAGE2_OBJ} ${KERNEL_OBJ} ${VGA_OBJ} ${MEM_OBJ} ${KBD_OBJ} ${PORT_OBJ} ${DISK_OBJ} ${FAT32_OBJ} ${CMAKE_SOURCE_DIR}/linker.ld
    COMMENT "Linking Stage2 -> ${STAGE2_BIN}"
)

# --- Example Kernels ---
set(EX_KERNEL_SRC ${CMAKE_SOURCE_DIR}/examples/kernel/main.c)
set(EX_MEMTEST_SRC ${CMAKE_SOURCE_DIR}/examples/memtest/main.c)
set(EX_KERNEL_BIN ${CMAKE_BINARY_DIR}/KERNEL.BIN)
set(EX_MEMTEST_BIN ${CMAKE_BINARY_DIR}/MEMTEST.BIN)

add_custom_command(
    OUTPUT ${EX_KERNEL_BIN}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -m32 -fno-pic -fno-builtin -fno-stack-protector -O0 -c ${EX_KERNEL_SRC} -o ${CMAKE_BINARY_DIR}/ex_kernel.o
    COMMAND ${X86_64_ELF_BIN}ld -m elf_i386 -Ttext 0x100000 --oformat binary -o ${EX_KERNEL_BIN} ${CMAKE_BINARY_DIR}/ex_kernel.o
    DEPENDS ${EX_KERNEL_SRC}
    COMMENT "Building Example Kernel -> ${EX_KERNEL_BIN}"
)

add_custom_command(
    OUTPUT ${EX_MEMTEST_BIN}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -m32 -fno-pic -fno-builtin -fno-stack-protector -O0 -c ${EX_MEMTEST_SRC} -o ${CMAKE_BINARY_DIR}/ex_memtest.o
    COMMAND ${X86_64_ELF_BIN}ld -m elf_i386 -Ttext 0x100000 --oformat binary -o ${EX_MEMTEST_BIN} ${CMAKE_BINARY_DIR}/ex_memtest.o
    DEPENDS ${EX_MEMTEST_SRC}
    COMMENT "Building Memory Test Example -> ${EX_MEMTEST_BIN}"
)

# --- Example Kernels (64-bit) ---
set(EX_KERNEL64_SRC ${CMAKE_SOURCE_DIR}/examples/kernel/main64.c)

set(EX_MEMTEST64_SRC ${CMAKE_SOURCE_DIR}/examples/memtest/main64.c)

set(EX_KERNEL64_BIN ${CMAKE_BINARY_DIR}/KERN64.BIN)
set(EX_MEMTEST64_BIN ${CMAKE_BINARY_DIR}/TEST64.BIN)

add_custom_command(
    OUTPUT ${EX_KERNEL64_BIN}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -fno-pic -fno-builtin -fno-stack-protector -O0 -c ${EX_KERNEL64_SRC} -o ${CMAKE_BINARY_DIR}/ex_kernel64.o
    COMMAND ${X86_64_ELF_BIN}ld -nostdlib -Ttext 0x200000 --oformat binary -o ${EX_KERNEL64_BIN} ${CMAKE_BINARY_DIR}/ex_kernel64.o
    DEPENDS ${EX_KERNEL64_SRC}
    COMMENT "Building 64-bit Example Kernel -> ${EX_KERNEL64_BIN}"
)

add_custom_command(
    OUTPUT ${EX_MEMTEST64_BIN}
    COMMAND ${X86_64_ELF_BIN}gcc -ffreestanding -fno-pic -fno-builtin -fno-stack-protector -O0 -c ${EX_MEMTEST64_SRC} -o ${CMAKE_BINARY_DIR}/ex_memtest64.o
    COMMAND ${X86_64_ELF_BIN}ld -nostdlib -Ttext 0x200000 --oformat binary -o ${EX_MEMTEST64_BIN} ${CMAKE_BINARY_DIR}/ex_memtest64.o
    DEPENDS ${EX_MEMTEST64_SRC}
    COMMENT "Building 64-bit Memory Test Example -> ${EX_MEMTEST64_BIN}"
)

# --- Build UEFI Bootloader ---
set(SHARED_KERNEL_SOURCES
    ${KERNEL_SRC}
    ${VGA_SRC}
    ${MEM_SRC}
    ${KBD_SRC}
    ${FAT32_SRC}
)

add_custom_command(
    OUTPUT ${EFI_MAIN_BIN}
    COMMAND ${MINGW_BIN}gcc -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone -nostdlib -Wl,-dll -Wl,--subsystem,10 -e efi_main -o ${EFI_MAIN_BIN} ${EFI_MAIN_SRC} ${SHARED_KERNEL_SOURCES} -DUEFI_BUILD -I${CMAKE_SOURCE_DIR}/include
    DEPENDS ${EFI_MAIN_SRC} ${SHARED_KERNEL_SOURCES}
    COMMENT "Building UEFI executable -> ${EFI_MAIN_BIN}"
)

# --- Create disk image ---
add_custom_command(
    OUTPUT ${DISK_IMG}
    COMMAND python ${CMAKE_SOURCE_DIR}/scripts/create_disk.py ${DISK_IMG} ${STAGE1_BIN} ${STAGE2_BIN} ${CMAKE_SOURCE_DIR}/config/atlas.cfg KERNEL.BIN ${EX_KERNEL_BIN} MEMTEST.BIN ${EX_MEMTEST_BIN} KERN64.BIN ${EX_KERNEL64_BIN} TEST64.BIN ${EX_MEMTEST64_BIN} EFI/BOOT/BOOTX64.EFI ${EFI_MAIN_BIN}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS ${STAGE1_BIN} ${STAGE2_BIN} ${CMAKE_SOURCE_DIR}/scripts/create_disk.py ${CMAKE_SOURCE_DIR}/config/atlas.cfg ${EX_KERNEL_BIN} ${EX_MEMTEST_BIN} ${EX_KERNEL64_BIN} ${EX_MEMTEST64_BIN} ${EFI_MAIN_BIN}
    COMMENT "Building hybrid BIOS/UEFI bootable disk image -> ${DISK_IMG}"
)

# --- Download ELF toolchain (for BIOS) ---
if(NOT EXISTS ${ELF_TOOLCHAIN_DIR})
    message(STATUS "Downloading x86_64-elf toolchain...")
    if(WIN32)
        file(DOWNLOAD ${ELF_TOOLCHAIN_WINDOWS_URL} ${ELF_TOOLCHAIN_ZIP} SHOW_PROGRESS)
        execute_process(
            COMMAND powershell -Command "Expand-Archive -Path '${ELF_TOOLCHAIN_ZIP}' -DestinationPath '${ELF_TOOLCHAIN_DIR}' -Force"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    elseif(UNIX)
        file(DOWNLOAD ${ELF_TOOLCHAIN_LINUX_URL} ${ELF_TOOLCHAIN_ZIP} SHOW_PROGRESS)
        execute_process(
            COMMAND unzip -o ${ELF_TOOLCHAIN_ZIP} -d ${ELF_TOOLCHAIN_DIR}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    else()
        message(WARNING "Unknown host system: ${CMAKE_SYSTEM_NAME}")
    endif()
endif()

# --- Download MinGW toolchain (for UEFI) ---
if(NOT EXISTS ${MINGW_TOOLCHAIN_DIR})
    message(STATUS "Downloading MinGW-w64 toolchain for UEFI...")
    if(WIN32)
        file(DOWNLOAD ${MINGW_TOOLCHAIN_WINDOWS_URL} ${MINGW_TOOLCHAIN_ZIP} SHOW_PROGRESS)
        # Expand-Archive doesn't support .7z, so we need 7-Zip
        # Try to find 7z.exe in common locations
        if(EXISTS "C:/Program Files/7-Zip/7z.exe")
            set(SEVEN_ZIP "C:/Program Files/7-Zip/7z.exe")
        elseif(EXISTS "C:/Program Files (x86)/7-Zip/7z.exe")
            set(SEVEN_ZIP "C:/Program Files (x86)/7-Zip/7z.exe")
        else()
            # Hope it's in PATH
            set(SEVEN_ZIP "7z")
        endif()
        
        execute_process(
            COMMAND ${SEVEN_ZIP} x -o${MINGW_TOOLCHAIN_DIR} ${MINGW_TOOLCHAIN_ZIP} -y
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    elseif(UNIX)
        file(DOWNLOAD ${MINGW_TOOLCHAIN_LINUX_URL} ${MINGW_TOOLCHAIN_ZIP} SHOW_PROGRESS)
        execute_process(
            COMMAND 7z x -o${MINGW_TOOLCHAIN_DIR} ${MINGW_TOOLCHAIN_ZIP}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    else()
        message(WARNING "Unknown host system: ${CMAKE_SYSTEM_NAME}")
    endif()
endif()

# --- Run in QEMU ---
add_custom_target(run
    COMMAND qemu-system-x86_64 -drive format=raw,file=${DISK_IMG}
    DEPENDS ${DISK_IMG}
)

# --- Download OVMF (UEFI Firmware) ---
# Use a reliable source for OVMF.fd (EDK2 release)
set(OVMF_URL "https://github.com/retrage/edk2-nightly/raw/master/bin/RELEASEX64_OVMF.fd")
set(OVMF_DIR "${CMAKE_SOURCE_DIR}/OVMF.fd")

if(NOT EXISTS ${OVMF_DIR})
    message(STATUS "Downloading OVMF firmware for QEMU...")
    file(DOWNLOAD ${OVMF_URL} ${OVMF_DIR} SHOW_PROGRESS)
endif()

# --- Run in QEMU (UEFI) ---
add_custom_target(run-uefi
    COMMAND qemu-system-x86_64 -bios "${OVMF_DIR}" -drive format=raw,file=${DISK_IMG}
    DEPENDS ${DISK_IMG}
)
