import os
import subprocess
import platform
import json
import sys
import ctypes
import time
from ctypes import wintypes

# =========================================================
# Admin / Root Check
# =========================================================
def is_admin():
    if platform.system() == "Windows":
        try:
            return ctypes.windll.shell32.IsUserAnAdmin()
        except Exception:
            return False
    else:
        return os.geteuid() == 0


def run_as_admin():
    if platform.system() == "Windows":
        if not is_admin():
            ctypes.windll.shell32.ShellExecuteW(
                None,
                "runas",
                sys.executable,
                " ".join(sys.argv),
                None,
                1
            )
            sys.exit(0)
    else:
        if not is_admin():
            print("Please run as root.")
            sys.exit(1)

# =========================================================
# Drive Enumeration
# =========================================================
def get_drives_windows():
    cmd = (
        'powershell -Command '
        '"Get-PhysicalDisk | '
        'Select DeviceId,FriendlyName,Size,BusType | '
        'ConvertTo-Json"'
    )

    try:
        output = subprocess.check_output(cmd, shell=True).decode('utf-8')
        data = json.loads(output)
        if isinstance(data, dict):
            data = [data]

        drives = []
        for d in data:
            if d.get("BusType") != "USB":
                continue
            drives.append({
                "id": d["DeviceId"],
                "name": d["FriendlyName"],
                "size": d["Size"],
                "path": f"\\\\.\\PhysicalDrive{d['DeviceId']}"
            })
        return drives
    except Exception as e:
        print(f"Drive enumeration failed: {e}")
        return []


def get_drives_linux():
    cmd = "lsblk -d -o NAME,MODEL,SIZE,TRAN -b -J"
    try:
        output = subprocess.check_output(cmd, shell=True).decode('utf-8')
        data = json.loads(output)
        return [
            {
                "id": d["name"],
                "name": d.get("model") or "Unknown",
                "size": int(d["size"]),
                "path": f"/dev/{d['name']}"
            }
            for d in data["blockdevices"]
            if d.get("tran") == "usb"
        ]
    except Exception as e:
        print(f"Drive enumeration failed: {e}")
        return []

# =========================================================
# Windows Volume Locking
# =========================================================
kernel32 = ctypes.WinDLL('kernel32.dll')  # Use WinDLL for __stdcall

# Define constants
INVALID_HANDLE_VALUE = -1  # Since wintypes.HANDLE(-1).value, but simplify
FSCTL_LOCK_VOLUME = 0x00090018
FSCTL_DISMOUNT_VOLUME = 0x00090020
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
FILE_SHARE_READ = 0x00000001
FILE_SHARE_WRITE = 0x00000002
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x80
FILE_BEGIN = 0

# Function prototypes
CreateFileW = kernel32.CreateFileW
CreateFileW.restype = wintypes.HANDLE
CreateFileW.argtypes = [
    wintypes.LPCWSTR,  # lpFileName
    wintypes.DWORD,    # dwDesiredAccess
    wintypes.DWORD,    # dwShareMode
    wintypes.LPVOID,   # lpSecurityAttributes
    wintypes.DWORD,    # dwCreationDisposition
    wintypes.DWORD,    # dwFlagsAndAttributes
    wintypes.HANDLE    # hTemplateFile
]

DeviceIoControl = kernel32.DeviceIoControl
DeviceIoControl.restype = wintypes.BOOL
DeviceIoControl.argtypes = [
    wintypes.HANDLE,   # hDevice
    wintypes.DWORD,    # dwIoControlCode
    wintypes.LPVOID,   # lpInBuffer
    wintypes.DWORD,    # nInBufferSize
    wintypes.LPVOID,   # lpOutBuffer
    wintypes.DWORD,    # nOutBufferSize
    wintypes.LPDWORD,  # lpBytesReturned
    wintypes.LPVOID    # lpOverlapped
]

WriteFile = kernel32.WriteFile
WriteFile.restype = wintypes.BOOL
WriteFile.argtypes = [
    wintypes.HANDLE,   # hFile
    wintypes.LPCVOID,  # lpBuffer
    wintypes.DWORD,    # nNumberOfBytesToWrite
    wintypes.LPDWORD,  # lpNumberOfBytesWritten
    wintypes.LPVOID    # lpOverlapped
]

ReadFile = kernel32.ReadFile
ReadFile.restype = wintypes.BOOL
ReadFile.argtypes = [
    wintypes.HANDLE,   # hFile
    wintypes.LPVOID,   # lpBuffer
    wintypes.DWORD,    # nNumberOfBytesToRead
    wintypes.LPDWORD,  # lpNumberOfBytesRead
    wintypes.LPVOID    # lpOverlapped
]

SetFilePointer = kernel32.SetFilePointer
SetFilePointer.restype = wintypes.LONG
SetFilePointer.argtypes = [
    wintypes.HANDLE,   # hFile
    wintypes.LONG,     # lDistanceToMove
    wintypes.PLONG,    # lpDistanceToMoveHigh
    wintypes.DWORD     # dwMoveMethod
]

FlushFileBuffers = kernel32.FlushFileBuffers
FlushFileBuffers.restype = wintypes.BOOL
FlushFileBuffers.argtypes = [wintypes.HANDLE]

CloseHandle = kernel32.CloseHandle
CloseHandle.restype = wintypes.BOOL
CloseHandle.argtypes = [wintypes.HANDLE]

GetLastError = kernel32.GetLastError
GetLastError.restype = wintypes.DWORD
GetLastError.argtypes = []


def lock_and_dismount_volume(letter):
    path = f"\\\\.\\{letter}:"

    handle = CreateFileW(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        None,
        OPEN_EXISTING,
        0,
        None
    )

    if handle == INVALID_HANDLE_VALUE:
        return None

    returned = wintypes.DWORD()

    if not DeviceIoControl(
        handle,
        FSCTL_LOCK_VOLUME,
        None, 0,
        None, 0,
        ctypes.byref(returned),
        None
    ):
        CloseHandle(handle)
        return None

    DeviceIoControl(
        handle,
        FSCTL_DISMOUNT_VOLUME,
        None, 0,
        None, 0,
        ctypes.byref(returned),
        None
    )

    FlushFileBuffers(handle)
    return handle


def lock_all_volumes_on_disk(disk_number):
    cmd = (
        f'powershell -Command '
        f'"Get-Partition -DiskNumber {disk_number} | '
        f'Get-Volume | Select -Expand DriveLetter"'
    )

    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    handles = []

    for line in result.stdout.splitlines():
        letter = line.strip()
        if not letter:
            continue

        h = lock_and_dismount_volume(letter)
        if not h:
            for old in handles:
                CloseHandle(old)
            raise RuntimeError(f"Failed to lock volume {letter}")

        handles.append(h)

    return handles

# =========================================================
# Raw Disk Writer (Windows)
# =========================================================
def burn_image_ctypes(image_path, drive_path, disk_number):
    try:
        volume_handles = lock_all_volumes_on_disk(disk_number)
    except RuntimeError as e:
        print(e)
        return False

    # Removed offlining step as it's not supported for removable media like USB drives

    handle = CreateFileW(
        drive_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        None,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        None
    )

    if handle == INVALID_HANDLE_VALUE:
        print(f"Failed to open disk (error {GetLastError()})")
        return False

    try:
        total = os.path.getsize(image_path)
        written_total = 0
        chunk_size = 1024 * 1024

        with open(image_path, "rb") as img:
            while True:
                data = img.read(chunk_size)
                if not data:
                    break

                buffer = ctypes.create_string_buffer(data)
                written = wintypes.DWORD()
                if not WriteFile(
                    handle,
                    buffer,
                    len(data),
                    ctypes.byref(written),
                    None
                ):
                    print(f"Write failed (error {GetLastError()})")
                    return False

                written_total += written.value
                percent = written_total * 100 / total
                print(f"\rProgress: {percent:6.2f}%", end="")
                sys.stdout.flush()

        FlushFileBuffers(handle)
        print("\nWrite complete.")

        # Verify boot signature
        SetFilePointer(handle, 0, None, FILE_BEGIN)
        buf = ctypes.create_string_buffer(512)
        read = wintypes.DWORD()

        ReadFile(handle, buf, 512, ctypes.byref(read), None)

        if buf.raw[510] == 0x55.to_bytes(1, 'big')[0] and buf.raw[511] == 0xAA.to_bytes(1, 'big')[0]:
            print("Boot signature verified.")
        else:
            print("Warning: boot signature mismatch.")

        return True

    finally:
        CloseHandle(handle)
        for h in volume_handles:
            CloseHandle(h)

        # Removed onlining step as offlining was skipped

# =========================================================
# Platform Dispatcher
# =========================================================
def burn_image(image_path, drive_path, disk_number):
    confirm = input(
        f"\nTYPE YES to overwrite {drive_path} with {image_path}: "
    )
    if confirm != "YES":
        print("Aborted.")
        return

    if platform.system() == "Windows":
        if burn_image_ctypes(image_path, drive_path, disk_number):
            print("Success.")
        else:
            print("Failed.")
    else:
        subprocess.run(
            f'dd if="{image_path}" of="{drive_path}" bs=4M conv=fsync status=progress',
            shell=True,
            check=True
        )

# =========================================================
# Main
# =========================================================
def find_disk_image():
    candidates = [
        "build/disk.img",
        "disk.img",
        os.path.join("..", "build", "disk.img")
    ]
    for p in candidates:
        if os.path.exists(p):
            return os.path.abspath(p)
    return None


def main():
    run_as_admin()

    image = find_disk_image()
    if not image:
        print("disk.img not found.")
        return

    if platform.system() == "Windows":
        drives = get_drives_windows()
    else:
        drives = get_drives_linux()

    if not drives:
        print("No removable drives found.")
        return

    for i, d in enumerate(drives):
        print(f"[{i}] {d['name']} ({d['size']/1024**3:.2f} GB) {d['path']}")

    idx = int(input("Select drive index: "))
    burn_image(image, drives[idx]["path"], drives[idx]["id"])


if __name__ == "__main__":
    main()