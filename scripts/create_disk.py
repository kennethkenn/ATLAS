import struct
import os
import sys

def create_fat32_image(image_path, boot1_path, boot2_path, config_path, additional_files=None):
    if additional_files is None:
        additional_files = []

    # Parameters matches boot1.asm BPB
    bytes_per_sector = 512
    reserved_sectors = 32
    fat_count = 2
    sectors_per_fat = 0x400
    total_sectors = 0x20000 # 64MB
    
    # Read core files
    with open(boot1_path, 'rb') as f:
        boot1 = f.read()
    with open(boot2_path, 'rb') as f:
        boot2 = f.read()
    
    # Prepare image
    image = bytearray(total_sectors * bytes_per_sector)
    
    # 1. Write Boot Sector (VBR)
    image[0:len(boot1)] = boot1

    def write_fsinfo(sector_idx):
        offset = sector_idx * 512
        struct.pack_into('<L', image, offset + 0, 0x41615252)
        struct.pack_into('<L', image, offset + 484, 0x61417272)
        struct.pack_into('<L', image, offset + 488, 0xFFFFFFFF) # Free cluster count
        struct.pack_into('<L', image, offset + 492, 0xFFFFFFFF) # Next free cluster
        struct.pack_into('<L', image, offset + 508, 0xAA550000)

    # 1.1 FSINFO (Sector 1) and Backup FSINFO (Sector 7)
    write_fsinfo(1)
    write_fsinfo(7)

    # 1.2 Backup Boot Sector (Sector 6)
    image[6*512 : 6*512 + 512] = image[0:512]
    
    # 2. Write Stage 2 (Reserved Sectors, starting at Sector 8)
    # This avoids overlap with sectors 1, 6, 7
    image[8*512 : 8*512 + len(boot2)] = boot2
    
    # 3. Initialize FATs
    fat_start = reserved_sectors * bytes_per_sector
    fat_size_bytes = sectors_per_fat * bytes_per_sector
    
    def set_fat_entry(cluster, value):
        offset = fat_start + (cluster * 4)
        struct.pack_into('<L', image, offset, value & 0x0FFFFFFF)
        # Mirror to second FAT
        struct.pack_into('<L', image, offset + fat_size_bytes, value & 0x0FFFFFFF)

    # Reserved entries 0 and 1
    # 0x0FFFFFF8 and 0x0FFFFFFF
    set_fat_entry(0, 0x0FFFFFF8)
    set_fat_entry(1, 0x0FFFFFFF)
    
    # Cluster 2 = Root Dir
    set_fat_entry(2, 0x0FFFFFFF)
    
    root_lba = reserved_sectors + (fat_count * sectors_per_fat)
    root_offset = root_lba * bytes_per_sector
    
    # 4. Create Volume Label Entry in Root Dir
    # ATLAS BOOT (11 chars) + attr (0x08)
    label_entry = struct.pack('<11sBBBHHHHHHHL',
        b"ATLAS BOOT ", 0x08, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    )
    image[root_offset : root_offset + 32] = label_entry
    dir_entry_offset = root_offset + 32

    # Files to add: ATLAS.CFG + additional_files
    files_to_process = [("ATLAS.CFG", config_path)] + additional_files
    
    next_cluster = 3
    
    # Simple directory tracking: {path: cluster}
    dirs = {"": 2} # Root is cluster 2

    def format_name(name, is_dir=False):
        if is_dir:
            return name.upper()[:11].ljust(11).encode('ascii')
        parts = name.split('.')
        base = parts[0].upper()[:8].ljust(8)
        ext = parts[1].upper()[:3].ljust(3) if len(parts) > 1 else "   "
        return (base + ext).encode('ascii')

    def get_or_create_dir(path):
        if path in dirs:
            return dirs[path]
        
        # Split into parent and current
        parts = path.rsplit('/', 1)
        parent_path = parts[0] if len(parts) > 1 else ""
        current_name = parts[-1]
        
        parent_cluster = get_or_create_dir(parent_path)
        
        # Allocate cluster for this directory
        nonlocal next_cluster
        curr_cluster = next_cluster
        next_cluster += 1
        
        # Mark in FAT
        set_fat_entry(curr_cluster, 0x0FFFFFFF)
        
        # Initialize the new directory with . and .. entries
        curr_lba = root_lba + (curr_cluster - 2)
        curr_offset = curr_lba * bytes_per_sector
        
        # Entry: .
        dot_entry = struct.pack('<11sBBBHHHHHHHL',
            b".          ", 0x10, 0, 0, 0, 0, 0,
            (curr_cluster >> 16) & 0xFFFF, 0, 0, curr_cluster & 0xFFFF, 0
        )
        image[curr_offset : curr_offset + 32] = dot_entry
        
        # Entry: ..
        # If parent is root (2), dotdot should point to 0 according to FAT spec
        dotdot_cluster = 0 if parent_cluster == 2 else parent_cluster
        dotdot_entry = struct.pack('<11sBBBHHHHHHHL',
            b"..         ", 0x10, 0, 0, 0, 0, 0,
            (dotdot_cluster >> 16) & 0xFFFF, 0, 0, dotdot_cluster & 0xFFFF, 0
        )
        image[curr_offset + 32 : curr_offset + 64] = dotdot_entry
        
        # Add entry to parent directory
        parent_lba = root_lba + (parent_cluster - 2)
        parent_offset = parent_lba * bytes_per_sector
        
        # Find empty slot in parent (simple search)
        found = False
        for i in range(0, bytes_per_sector, 32):
            if image[parent_offset + i] == 0 or image[parent_offset + i] == 0xE5:
                entry = struct.pack('<11sBBBHHHHHHHL',
                    format_name(current_name, True), 0x10, 0, 0, 0, 0, 0,
                    (curr_cluster >> 16) & 0xFFFF, 0, 0, curr_cluster & 0xFFFF, 0
                )
                image[parent_offset + i : parent_offset + i + 32] = entry
                found = True
                break
        
        if not found:
            print(f"Warning: No space in parent directory {parent_path} for {current_name}")
            
        dirs[path] = curr_cluster
        return curr_cluster

    for filename, source_path in files_to_process:
        with open(source_path, 'rb') as f:
            content = f.read()
        
        file_size = len(content)
        num_clusters = (file_size + bytes_per_sector - 1) // bytes_per_sector
        if num_clusters == 0: num_clusters = 1
        
        # Handle path
        parts = filename.replace('\\', '/').rsplit('/', 1)
        dir_path = parts[0] if len(parts) > 1 else ""
        base_name = parts[-1]
        
        target_dir_cluster = get_or_create_dir(dir_path)
        start_cluster = next_cluster
        
        # Write content 
        for i in range(num_clusters):
            cluster_lba = root_lba + (next_cluster - 2)
            cluster_offset = cluster_lba * bytes_per_sector
            chunk = content[i*bytes_per_sector : (i+1)*bytes_per_sector]
            image[cluster_offset : cluster_offset + len(chunk)] = chunk
            
            if i == num_clusters - 1:
                set_fat_entry(next_cluster, 0x0FFFFFFF)
            else:
                set_fat_entry(next_cluster, next_cluster + 1)
            next_cluster += 1
            
        # Create Directory Entry in target_dir
        dir_lba = root_lba + (target_dir_cluster - 2)
        dir_offset = dir_lba * bytes_per_sector
        
        found = False
        # Skip volume label if root
        start_search = 32 if target_dir_cluster == 2 else 0
        for i in range(start_search, bytes_per_sector, 32):
            if image[dir_offset + i] == 0 or image[dir_offset + i] == 0xE5:
                entry = struct.pack('<11sBBBHHHHHHHL', 
                    format_name(base_name), 0x20, 0, 0, 0, 0, 0,
                    (start_cluster >> 16) & 0xFFFF, 0, 0, start_cluster & 0xFFFF, file_size
                )
                image[dir_offset + i : dir_offset + 32 + i] = entry
                found = True
                break
        if not found:
            print(f"Warning: No space in directory {dir_path} for {base_name}")

    # Write to file
    with open(image_path, 'wb') as f:
        f.write(image)
    print(f"Created {image_path} with {len(files_to_process)} files.")

if __name__ == "__main__":
    if len(sys.argv) < 5:
        print("Usage: create_disk.py <output> <boot1> <boot2> <config> [file1_name file1_path ...]")
        sys.exit(1)
    
    image_out = sys.argv[1]
    b1 = sys.argv[2]
    b2 = sys.argv[3]
    cfg = sys.argv[4]
    
    others = []
    for i in range(5, len(sys.argv), 2):
        if i+1 < len(sys.argv):
            others.append((sys.argv[i], sys.argv[i+1]))
            
    create_fat32_image(image_out, b1, b2, cfg, others)
