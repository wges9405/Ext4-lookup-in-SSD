#include "./nvme.h"
#include "./hash.c"
// #include "./read.h"

static uint8_t read_8(uint8_t *d) {
    uint16_t d1 = d[0];
    return d1;
}
static uint16_t read_16(uint8_t *d) {
    uint16_t d1 = d[1] << 8;
    uint16_t d2 = d[0];
    return d1 + d2;
}
static uint32_t read_32(uint8_t *d) {
    uint32_t d1 = d[3] << 24;
    uint32_t d2 = d[2] << 16;
    uint32_t d3 = d[1] << 8;
    uint32_t d4 = d[0];
    return d1 + d2 + d3 + d4;
}

static dx_node* read_hash_node(SsdDramBackend *mbe, FileSystem *fs, uint64_t block_idx) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;
    
    SuperBlock *sb = &fs->sb;
    // GroupDescriptor *gd = &fs->gd;
    uint64_t padding = block_idx * sb->size_block;
    dx_node *node = malloc(sizeof(dx_node));

    node->count = read_16(&mem[padding + 0x22]);
    
    node->entries = malloc(sizeof(dx_entry) * node->count);
    node->entries[0].hash  = 0;
    node->entries[0].block = read_32(&mem[padding + 0x24]);
    for (uint32_t i = 1, idx = 0x10; i < node->count; i++) {
        node->entries[i].hash  = read_32(&mem[padding + idx + 0x0]);
        node->entries[i].block = read_32(&mem[padding + idx + 0x4]);
        idx += 0x8;
    }

    return node;
}

static dx_root* read_hash_root(SsdDramBackend *mbe, FileSystem *fs, uint64_t block_idx) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;
    
    SuperBlock *sb = &fs->sb;
    // GroupDescriptor *gd = &fs->gd;
    uint64_t padding = block_idx * sb->size_block;
    dx_root *root = malloc(sizeof(dx_root));

    root->info.hash_version = read_8(&mem[padding + 0x1C]);
    root->info.indirect_levels = read_8(&mem[padding + 0x1E]);
    root->limit = read_16(&mem[padding + 0x20]);
    root->count = read_16(&mem[padding + 0x22]);

    root->entries = malloc(sizeof(dx_entry) * root->count);
    root->entries[0].hash  = 0;
    root->entries[0].block = read_32(&mem[padding + 0x24]);
    for (uint32_t i = 1, idx = 0x28; i < root->count; i++) {
        root->entries[i].hash  = read_32(&mem[padding + idx + 0x0]);
        root->entries[i].block = read_32(&mem[padding + idx + 0x4]);
        idx += 0x8;
    }

    return root;
}

static extent_hdr* read_extent_header(SsdDramBackend *mbe, uint64_t padding) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;
    extent_hdr *header = malloc(sizeof(extent_hdr));

    assert(read_16(&mem[padding]) == 0xF30A);
    header->eh_entries  = read_16(&mem[padding + 0x2]);
    header->eh_max      = read_16(&mem[padding + 0x4]);
    header->eh_depth    = read_16(&mem[padding + 0x6]);
/*
    printf("header eh_entries: %d\r\n", header->eh_entries);
    printf("header eh_max: %d\r\n", header->eh_max);
    printf("header eh_depth: %d\r\n", header->eh_depth);
    printf("-------\r\n");
*/
    return header;
}

static void read_extent_idx(SsdDramBackend *mbe, uint64_t padding, extent_idx *index) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;

    index->ei_block    = read_32(&mem[padding + 0x0]);
    index->ei_leaf_lo  = read_32(&mem[padding + 0x4]);
    index->ei_leaf_hi  = read_16(&mem[padding + 0x8]);
/*
    printf("index ei_block: %d\r\n", index->ei_block);
    printf("index ei_leaf_lo: %d\r\n", index->ei_leaf_lo);
    printf("index ei_leaf_hi: %ld\r\n", index->ei_leaf_hi);
*/
}

static void read_extent_(SsdDramBackend *mbe, uint64_t padding, extent_ *extent) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;

    extent->ee_block    = read_32(&mem[padding + 0x0]);
    extent->ee_len      = read_16(&mem[padding + 0x4]);
    extent->ee_leaf_hi  = read_16(&mem[padding + 0x6]);
    extent->ee_leaf_lo  = read_32(&mem[padding + 0x8]);
/*
    printf("extent ee_block: %d\r\n", extent->ee_block);
    printf("extent ee_len: %d\r\n", extent->ee_len);
    printf("extent ee_leaf_hi: %ld\r\n", extent->ee_leaf_hi);
    printf("extent ee_leaf_lo: %d\r\n", extent->ee_leaf_lo);
    //printf("-------\r\n");
*/
}

static extent_node *read_extent_data(SsdDramBackend *mbe, uint64_t padding) {
    // uint8_t *mem = (uint8_t *) mbe->logical_space;

    //printf("reading extent data\r\n");
    extent_node *node = malloc(sizeof(extent_node));
    node->header = NULL;
    node->header = read_extent_header(mbe, padding);
    
    if (node->header->eh_depth == 0) {
        node->leaf = true;
        node->index = NULL;
        node->extent = malloc(sizeof(extent_) * node->header->eh_entries);
        for (int idx = 0; idx < node->header->eh_entries; idx++)
            read_extent_(mbe, padding + 12 + idx * 12, &node->extent[idx]);
    } else {
        node->leaf = false;
        node->index = malloc(sizeof(extent_idx) * node->header->eh_entries);
        for (int idx = 0; idx < node->header->eh_entries; idx++)
            read_extent_idx(mbe, padding + 12 + idx * 12, &node->index[idx]);
        node->extent = NULL;
    }
    return node;
}

static Inode* read_inode(SsdDramBackend *mbe, FileSystem *fs, uint32_t inode_idx) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;

    SuperBlock *sb = &fs->sb;
    GroupDescriptor *gd = &fs->gd;
    Inode *inode = malloc(sizeof(Inode));

    //printf("reading inode: %d\r\n", inode_idx);
    uint64_t p_block = 0;
    uint32_t p_inode = 0, p_group = 0;
    while (p_group < sb->ttl_group_cnt) {
        if (inode_idx < p_group * sb->inodes_per_group) break;

        if (p_group > 0) p_inode = (inode_idx -1) % (p_group * sb->inodes_per_group);
        else p_inode = (inode_idx -1); 

        p_block = gd->inode_start_block_idx[p_group];
        p_group ++;
    }
    //printf("p_group: %d, p_inode: %d, p_block: %ld\r\n", p_group - 1, p_inode, p_block);
    //printf("-------\r\n");

    // uint64_t block_idx = gd->inode_start_block_idx[0];
    uint64_t padding = p_block * sb->size_block + p_inode * sb->size_inode;

    inode->mode                    = read_16(&mem[padding + 0x0]); assert(inode->mode | 0x4000);
    inode->size                    = read_32(&mem[padding + 0x4]);
    inode->last_access_time        = read_32(&mem[padding + 0x8]);
    inode->last_inode_change_time  = read_32(&mem[padding + 0xC]);
    inode->last_data_mod_time      = read_32(&mem[padding + 0x10]);
    inode->hard_link_cnt           = read_16(&mem[padding + 0x1A]);
    inode->block_cnt               = read_32(&mem[padding + 0x1C]);
    inode->flag                    = read_32(&mem[padding + 0x20]);
/*
    printf("mode: 0x%04x\r\n", rd->mode);
    printf("size: %d\r\n", rd->size);
    printf("last_access_time: 0x%08x\r\n", rd->last_access_time);
    printf("last_inode_change_time: 0x%08x\r\n", rd->last_inode_change_time);
    printf("last_data_mod_time: 0x%08x\r\n", rd->last_data_mod_time);
    printf("hard_link_cnt: %d\r\n", rd->hard_link_cnt);
    printf("block_cnt: %d\r\n", rd->block_cnt);
    printf("flag: 0x%08x\r\n", rd->flag);
*/
    if (inode->flag & 0x00080000) {
        //printf("EXT4_EXTENTS_FL\r\n");
        inode->extent_root = read_extent_data(mbe, padding + 0x28);
    }

    return inode;
}

static void read_group_descriptor(SsdDramBackend *mbe, FileSystem *fs) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;

    SuperBlock *sb = &fs->sb;
    GroupDescriptor *gd = &fs->gd;
    uint64_t block_idx = block_idx_gd;
    uint64_t padding = block_idx * sb->size_block;

    uint32_t bg_inode_table_lo;
    uint64_t bg_inode_table_hi;
    gd->inode_start_block_idx = malloc(sizeof(uint64_t) * sb->ttl_group_cnt);

    for (uint32_t idx = 0; idx < sb->ttl_group_cnt; idx++) {
        bg_inode_table_lo = read_32(&mem[padding + sb->size_group * idx + 0x8]);
        bg_inode_table_hi = read_32(&mem[padding + sb->size_group * idx + 0x28]);
        gd->inode_start_block_idx[idx] = (bg_inode_table_hi << 32) + bg_inode_table_lo;
        //printf("group %d inode start at %ld\r\n", idx, gd->inode_start_block_idx[idx]);
    }
    //printf("-------\r\n");
}

static void read_super_block(SsdDramBackend *mbe, FileSystem *fs) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;

    SuperBlock *sb = &fs->sb;
    // uint64_t block_idx = block_idx_sb;
    uint64_t padding = 0x400;

    sb->s_inodes_count      = read_32(&mem[padding + 0x0]);
    sb->s_blocks_count_lo   = read_32(&mem[padding + 0x4]);
    sb->s_log_block_size    = read_32(&mem[padding + 0x18]);
    sb->s_blocks_per_group  = read_32(&mem[padding + 0x20]);
    sb->s_inodes_per_group  = read_32(&mem[padding + 0x28]);
    sb->s_inode_size        = read_16(&mem[padding + 0x58]);
    sb->s_hash_seed[0]      = read_32(&mem[padding + 0xEC]);
    sb->s_hash_seed[1]      = read_32(&mem[padding + 0xF0]);
    sb->s_hash_seed[2]      = read_32(&mem[padding + 0xF4]);
    sb->s_hash_seed[3]      = read_32(&mem[padding + 0xF8]);
    sb->s_def_hash_version  = read_8( &mem[padding + 0xFC]);
    sb->s_blocks_count_hi   = read_32(&mem[padding + 0x150]);
    sb->s_flags             = read_32(&mem[padding + 0x160]);

/*
    printf("s_inodes_count: %d\r\n", sb->s_inodes_count);
    printf("s_blocks_count_lo: %d\r\n", sb->s_blocks_count_lo);
    printf("s_log_block_size: %d\r\n", sb->s_log_block_size);
    printf("s_inodes_per_group: %d\r\n", sb->s_inodes_per_group);
    printf("s_blocks_per_group: %d\r\n", sb->s_blocks_per_group);
    printf("s_inode_size: %d\r\n", sb->s_inode_size);
    printf("s_blocks_count_hi: %ld\r\n", sb->s_blocks_count_hi);
    printf("s_hash_seed[0]: %d\r\n", sb->s_hash_seed[0]);
    printf("s_hash_seed[1]: %d\r\n", sb->s_hash_seed[1]);
    printf("s_hash_seed[2]: %d\r\n", sb->s_hash_seed[2]);
    printf("s_hash_seed[3]: %d\r\n", sb->s_hash_seed[3]);
    printf("s_def_hash_version: %d\r\n", sb->s_def_hash_version);
    printf("s_flags: %d\r\n", sb->s_flags);
    printf("-------\r\n");
*/

    sb->ttl_inode_cnt       = sb->s_inodes_count;
    sb->ttl_block_cnt       = (sb->s_blocks_count_hi << 32) + sb->s_blocks_count_lo;
    sb->ttl_group_cnt       = sb->ttl_block_cnt / sb->s_blocks_per_group;
    sb->size_inode          = sb->s_inode_size;
    sb->size_block          = 1 << (10 + sb->s_log_block_size);
    sb->size_group          = size_gd;
    sb->inodes_per_group    = sb->s_inodes_per_group;
    sb->blocks_per_group    = sb->s_blocks_per_group;
    sb->inodes_per_block    = sb->size_block / sb->s_inode_size;
    sb->groups_per_block    = sb->size_block / sb->size_group;
    sb->s_hash_unsigned     = 0;
    if (sb->s_flags & 0x0002) sb->s_hash_unsigned = 3;
/*
    printf("ttl_inode_cnt: %d\r\n", sb->ttl_inode_cnt);
    printf("ttl_block_cnt: %ld\r\n", sb->ttl_block_cnt);
    printf("ttl_group_cnt: %d\r\n", sb->ttl_group_cnt);
    printf("size_inode: %d\r\n", sb->size_inode);
    printf("size_block: %d\r\n", sb->size_block);
    printf("size_group: %d\r\n", size_gd);
    printf("inodes_per_group: %d\r\n", sb->inodes_per_group);
    printf("blocks_per_group: %d\r\n", sb->blocks_per_group);
    printf("inodes_per_block: %d\r\n", sb->inodes_per_block);
    printf("groups_per_block: %d\r\n", sb->groups_per_block);
    printf("-------\r\n");
*/
}

static uint32_t get_file_hash(SsdDramBackend *mbe, FileSystem *fs, uint8_t hash_version, char *name, int32_t len) {
    SuperBlock *sb = &fs->sb;

    dx_hash_info *hinfo = malloc(sizeof(dx_hash_info));
    uint32_t hash;

    hinfo->hash_version = hash_version;
    if (hinfo->hash_version <= 2)
        hinfo->hash_version += sb->s_hash_unsigned;
    hinfo->seed = sb->s_hash_seed;
    int ret = calc_hash(mbe, fs, hinfo->hash_version, name, len, hinfo);
    assert(ret == 0);

    hash = hinfo->hash;
    free(hinfo);

    return hash;
}

static bool search_extent_tree(SsdDramBackend *mbe, FileSystem *fs, extent_node *node, uint32_t dir_idx, uint64_t *block_idx, uint16_t *block_len) {
    SuperBlock *sb = &fs->sb;

    //printf("Search dir block: %d\r\n", dir_idx);
    uint16_t entry_idx, extent_level = node->header->eh_depth;
    bool root = true;
    *block_idx = 0;
    *block_len = 0;

    // extent node
    entry_idx = 0;
    while(extent_level > 0) {
        while(entry_idx < node->header->eh_entries) {
            if (dir_idx < node->index->ei_block) break;

            *block_idx = (node->index->ei_leaf_hi << 32) + node->index->ei_leaf_lo;
            entry_idx ++;
        }
        //printf("curr extent level: %d, block: %ld\r\n", extent_level, *block_idx);

        if (root) root = !root;
        else free(node);
        assert(*block_idx != 0);

        uint64_t padding = *block_idx * sb->size_block;
        node = read_extent_data(mbe, padding);

        extent_level = node->header->eh_depth;
        entry_idx = 0;
    }
    //printf("curr extent level: %d, block: %ld\r\n", extent_level, *block_idx);

    // extent leaf
    assert(extent_level == 0);
    while(entry_idx < node->header->eh_entries) {
        if (dir_idx < node->extent[entry_idx].ee_block) break;

        *block_idx = (node->extent[entry_idx].ee_leaf_hi << 32) + node->extent[entry_idx].ee_leaf_lo;
        *block_len = node->extent[entry_idx].ee_len;
        entry_idx ++;
    }
    if (root) root = !root;
    else free(node);
    assert(*block_idx != 0);
    assert(*block_len != 0);

    //printf("Found dir block: %d, block: %ld\r\n", dir_idx, *block_idx);
    //printf("-------\r\n");
    return true;
}

static dir_entry* search_linear_directory(SsdDramBackend *mbe, FileSystem *fs, uint64_t block_idx, char *file_name) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;
    
    SuperBlock *sb = &fs->sb;
    // GroupDescriptor *gd = &fs->gd;
    bool found = false;
    uint64_t padding = block_idx * sb->size_block;
    dir_entry *entry = malloc(sizeof(dir_entry));

    int idx = 0x0;
    while(idx < sb->size_block) {
        entry->inode = read_32(&mem[padding + idx]);
        if (entry->inode == 0) break;
        idx += 0x4;
        entry->rec_len = read_16(&mem[padding + idx]);
        idx += 0x2;
        entry->name_len = read_8(&mem[padding + idx]);
        idx += 0x1;
        entry->type = read_8(&mem[padding + idx]);
        idx += 0x1;
        memset(entry->name, 0, EXT4_NAME_LEN);
        for (int i = 0; i < entry->name_len; i++) entry->name[i] = read_8(&mem[padding + idx + i]);
        //printf("inode: %d, rec_len: %d, name_len: %d, type: %d, name: %s\r\n", entry->inode, entry->rec_len, entry->name_len, entry->type, entry->name);

        if (strcmp(entry->name, file_name) == 0) {
            //printf("entry->name: %s, file_name: %s\r\n", entry->name, file_name);
            found = true;
            break;
        }

        if (entry->name_len % 4) idx += (entry->name_len / 4 + 1) * 4;
        else idx += (entry->name_len / 4) * 4;
    }
    if (found) return entry;
    else return NULL;
}

static dir_entry* search_hash_leaf(SsdDramBackend *mbe, FileSystem *fs, extent_node *extent_root, uint32_t dir_idx, char file_name[EXT4_NAME_LEN]) {
    // uint8_t *mem = (uint8_t *) mbe->logical_space;
    mbe->cnt++;

    uint64_t block_idx;
    uint16_t block_len;
    dir_entry *dest_file;
    
    // extent 
    search_extent_tree(mbe, fs, extent_root, dir_idx, &block_idx, &block_len);

    printf("curr hash level: leaf, dir block: %d, block: %ld\r\n", dir_idx, block_idx);
    printf("-------\r\n");
    
    // linear directory (dx leaf)
    while (block_len) {
        dest_file = search_linear_directory(mbe, fs, block_idx, file_name);
        if (dest_file != NULL) {
            printf("Found file '%s' !!! At file inode: %d\r\n", dest_file->name, dest_file->inode);
            break;
        }
        block_idx++;
        block_len--;
    }
    return dest_file;
}

static dir_entry* search_hash_node(SsdDramBackend *mbe, FileSystem *fs, extent_node *extent_root, uint32_t dir_idx, uint8_t dx_level, uint32_t file_hash, char file_name[EXT4_NAME_LEN]) {
    // uint8_t *mem = (uint8_t *) mbe->logical_space;
    mbe->cnt++;

    uint64_t block_idx;
    uint16_t block_len;
    // extent
    search_extent_tree(mbe, fs, extent_root, dir_idx, &block_idx, &block_len);
    // dx node
    dx_node *node = read_hash_node(mbe, fs, block_idx);
    uint32_t next_dir_idx = 0;

    printf("curr hash level: %d, dir block: %d\r\n", dx_level, dir_idx);
    printf("-------\r\n");

///* // search one file
    dx_entry *m;
    dx_entry *p = node->entries + 1;
    dx_entry *q = node->entries + node->count - 1;
    while (p <= q) {
        m = p + (q - p) / 2;
        if (m->hash > file_hash) q = m - 1;
        else p = m + 1;
    }
    p = p - 1;
    next_dir_idx = p->block;
    
    free(node->entries);
    free(node);

    if (dx_level > 0)
        return search_hash_node(mbe, fs, extent_root, next_dir_idx, dx_level - 1, file_hash, file_name);
    else 
        return search_hash_leaf(mbe, fs, extent_root, next_dir_idx, file_name);
//*/
/* // traverse all file
    uint16_t cnt = 0;
    while (cnt < node->count) {
        next_dir_idx = node->entries[cnt].block;
        if (dx_level > 0)
            search_hash_node(mbe, fs, extent_root, next_dir_idx, dx_level - 1, file_hash, file_name);
        else 
            search_hash_leaf(mbe, fs, extent_root, next_dir_idx, file_name);
        cnt++;
    }
*/
}

static dir_entry* search_hash_root(SsdDramBackend *mbe, FileSystem *fs, uint32_t inode_idx, char file_path[4096]) {
    // uint8_t *mem = (uint8_t *) mbe->logical_space;

    // SuperBlock *sb = &fs->sb;
    // GroupDescriptor *gd = &fs->gd;
    Inode *curr_inode;
    extent_node *extent_root;
    uint32_t next_inode = inode_idx;
    uint64_t block_idx;
    uint16_t block_len;
    dir_entry *dest_file = NULL;

    char *file_name = strtok(file_path, "/");
    while(file_name != NULL) {
        printf("Searching file: %s\r\n", file_name);
        printf("-------\r\n");

        curr_inode = read_inode(mbe, fs, next_inode);
        extent_root = curr_inode->extent_root;
        bool hash = curr_inode->flag & 0x00001000;  // hash tree flag
        if (hash) {
            printf("EXT4_INDEX_FL\r\n");
            printf("-------\r\n");

            // extent 
            search_extent_tree(mbe, fs, extent_root, 0, &block_idx, &block_len);
            // dx root
            dx_root *root = read_hash_root(mbe, fs, block_idx);
            uint32_t next_dir_idx = 0;

            uint32_t file_hash = get_file_hash(mbe, fs, root->info.hash_version, file_name, strlen(file_name));
            uint8_t dx_level = root->info.indirect_levels;

            printf("curr hash level: %d, dir block: %d\r\n", dx_level, 0);
            printf("-------\r\n");

///* search one file
            dx_entry *m;
            dx_entry *p = root->entries + 1;
            dx_entry *q = root->entries + root->count - 1;
            while (p <= q) {
                m = p + (q - p) / 2;
                if (m->hash > file_hash) q = m - 1;
                else p = m + 1;
            }
            p = p - 1;

            next_dir_idx = p->block;
            if (dx_level > 0)
                dest_file = search_hash_node(mbe, fs, extent_root, next_dir_idx, dx_level - 1, file_hash, file_name);
            else 
                dest_file = search_hash_leaf(mbe, fs, extent_root, next_dir_idx, file_name);

//*/
        } else {
            for(uint16_t cnt = 0 ; cnt < extent_root->header->eh_entries; cnt++) {
                dest_file = search_hash_leaf(mbe, fs, extent_root, cnt, file_name);
                if (dest_file != NULL) break;
            }
        }
        assert(dest_file != NULL);
        free(curr_inode);
        
        file_name = strtok(NULL, "/");
        if (file_name) {
            printf("Next file '%s'\r\n\n\n", file_name);
            next_inode = dest_file->inode;
            free(dest_file);
        }
        else printf("End ~~~\r\n\n\n");
    }
    printf("dest inode: %d\r\n", dest_file->inode);
    return dest_file;
}
/*
void read_regular_file(SsdDramBackend *mbe, FileSystem *fs, Inode* inode) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;

    SuperBlock *sb = &fs->sb;
    GroupDescriptor *gd = &fs->gd;
    for (uint16_t idx = 0; idx < inode->extent_root->header->eh_entries; idx++) {
        uint64_t block_idx;
        uint16_t block_len;
        search_extent_tree(mbe, fs, inode->extent_root, idx, &block_idx, &block_len);
        
        uint64_t padding = block_idx * sb->size_block; 
        
        uint8_t word;
        for (uint32_t i = 0x0; i <= 4096; i++) {
            word = read_8(&mem[padding + i]);
            printf("%c", word);
        }
        printf("\r\n");
    }
}

void search_hash_root_all(SsdDramBackend *mbe, FileSystem *fs, uint32_t inode_idx, char file_name[EXT4_NAME_LEN], dir_entry** dest_files, uint32_t *dest_file_num);
void search_linear_directory_all(SsdDramBackend *mbe, FileSystem *fs, uint64_t block_idx, char *file_name, dir_entry** dest_files, uint32_t *dest_file_num) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;
    
    SuperBlock *sb = &fs->sb;
    GroupDescriptor *gd = &fs->gd;
    bool found = false;
    uint64_t padding = block_idx * sb->size_block;
    dir_entry *entry = malloc(sizeof(dir_entry));

    int idx = 0x0;
    while(idx < sb->size_block) {
        entry->inode = read_32(&mem[padding + idx]);
        if (entry->inode == 0) break;
        idx += 0x4;
        entry->rec_len = read_16(&mem[padding + idx]);
        idx += 0x2;
        entry->name_len = read_8(&mem[padding + idx]);
        idx += 0x1;
        entry->type = read_8(&mem[padding + idx]);
        idx += 0x1;
        memset(entry->name, 0, EXT4_NAME_LEN);
        for (int i = 0; i < entry->name_len; i++) entry->name[i] = read_8(&mem[padding + idx + i]);
        //printf("inode: %d, rec_len: %d, name_len: %d, type: %d, name: %s", entry->inode, entry->rec_len, entry->name_len, entry->type, entry->name);

        if (strcmp(entry->name, file_name) == 0) {
            found = true;
            //break;
            uint32_t size = *dest_file_num;
            assert(dest_files != NULL);
            *dest_files = realloc(*dest_files, sizeof(dir_entry) * (size + 1));
            assert(dest_files != NULL);

            (*dest_files)[size].inode = entry->inode;
            (*dest_files)[size].rec_len = entry->rec_len;
            (*dest_files)[size].name_len = entry->name_len;
            (*dest_files)[size].type = entry->type;
            memcpy((*dest_files)[size].name, entry->name, sizeof(entry->name_len));
            
            *dest_file_num = size + 1;
        }

        if (entry->type == 0x2 ) { // Directory
            //printf("entry->name: %s, file_name: %s\r\n", entry->name, file_name);
            if (strcmp(entry->name, ".") != 0 && strcmp(entry->name, "..") != 0) {
                //printf("entry->name: %s\r\n", entry->name);
                search_hash_root_all(mbe, fs, entry->inode, file_name, dest_files, dest_file_num);
            }
        }
        //printf("\r\n");
        //if (found) break;

        if (entry->name_len % 4) idx += (entry->name_len / 4 + 1) * 4;
        else idx += (entry->name_len / 4) * 4;
    }
    //printf("-------\r\n");
}

void search_hash_leaf_all(SsdDramBackend *mbe, FileSystem *fs, extent_node *extent_root, uint32_t dir_idx, char file_name[EXT4_NAME_LEN], dir_entry** dest_files, uint32_t *dest_file_num) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;

    uint64_t block_idx;
    uint16_t block_len;
    dir_entry *file;
    
    // extent 
    search_extent_tree(mbe, fs, extent_root, dir_idx, &block_idx, &block_len);

    //printf("curr hash level: leaf, dir block: %d, block: %ld\r\n", dir_idx, block_idx);
    //printf("-------\r\n");
    
    // linear directory (dx leaf)
    while (block_len) {
        search_linear_directory_all(mbe, fs, block_idx, file_name, dest_files, dest_file_num);

        block_idx++;
        block_len--;
    }
}

void search_hash_node_all(SsdDramBackend *mbe, FileSystem *fs, extent_node *extent_root, uint32_t dir_idx, uint8_t dx_level, char file_name[EXT4_NAME_LEN], dir_entry** dest_files, uint32_t *dest_file_num) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;

    uint64_t block_idx;
    uint16_t block_len;
    // extent
    search_extent_tree(mbe, fs, extent_root, dir_idx, &block_idx, &block_len);
    // dx node
    dx_node *node = read_hash_node(mbe, fs, block_idx);
    uint32_t next_dir_idx = 0;

    //printf("curr hash level: %d, dir block: %d\r\n", dx_level, dir_idx);
    //printf("-------\r\n");

 // traverse all file
    uint16_t cnt = 0;
    while (cnt < node->count) {
        next_dir_idx = node->entries[cnt].block;
        if (dx_level > 0)
            search_hash_node_all(mbe, fs, extent_root, next_dir_idx, dx_level - 1, file_name, dest_files, dest_file_num);
        else 
            search_hash_leaf_all(mbe, fs, extent_root, next_dir_idx, file_name, dest_files, dest_file_num);
        cnt++;
    }
}

void search_hash_root_all(SsdDramBackend *mbe, FileSystem *fs, uint32_t inode_idx, char file_name[EXT4_NAME_LEN], dir_entry** dest_files, uint32_t *dest_file_num) {
    uint8_t *mem = (uint8_t *) mbe->logical_space;

    SuperBlock *sb = &fs->sb;
    GroupDescriptor *gd = &fs->gd;
    Inode *curr_inode;
    extent_node *extent_root;
    uint32_t next_inode = inode_idx;
    uint64_t block_idx;
    uint16_t block_len;


    curr_inode = read_inode(mbe, fs, next_inode);
    extent_root = curr_inode->extent_root;
    bool hash = curr_inode->flag & 0x00001000;  // hash tree flag

    //printf("Searching file: %s\r\n", file_name);
    //printf("-------\r\n");
    if (hash) {
        //printf("EXT4_INDEX_FL\r\n");
        //printf("-------\r\n");

        // extent 
        search_extent_tree(mbe, fs, extent_root, 0, &block_idx, &block_len);
        // dx root
        dx_root *root = read_hash_root(mbe, fs, block_idx);
        uint32_t next_dir_idx = 0;

        uint32_t file_hash = get_file_hash(mbe, fs, root->info.hash_version, file_name, strlen(file_name));
        uint8_t dx_level = root->info.indirect_levels;

        //printf("curr hash level: %d, dir block: %d\r\n", dx_level, 0);
        //printf("-------\r\n");
// traverse all file
        // uint16_t cnt = 0;
        // while (cnt < root->count) {
        //     if (dx_level > 0)
        //         search_hash_node_all(mbe, fs, curr_inode->extent_root, root->entries[cnt].block, dx_level - 1, file_name, dest_files, dest_file_num);
        //     else 
        //         search_hash_leaf_all(mbe, fs, curr_inode->extent_root, root->entries[cnt].block, file_name, dest_files, dest_file_num);
        //     cnt++;
        // }
//
    } else {
        for(uint16_t cnt = 0 ; cnt < extent_root->header->eh_entries; cnt++) {
            search_hash_leaf_all(mbe, fs, extent_root, cnt, file_name, dest_files, dest_file_num);
        }
    }
    //assert(dest_files != NULL);
}
*/
static uint32_t nvme_search(SsdDramBackend *mbe, char filepath[4096]) {
    FileSystem *fs = mbe->fs;
    mbe->cnt = 0;
    read_super_block(mbe, fs);
    read_group_descriptor(mbe, fs);

    dir_entry* dest_file = search_hash_root(mbe, fs, 2, filepath);
    if (dest_file != NULL) {
        printf("count: %ld\r\n", mbe->cnt);
        return dest_file->inode;
    } else {
        return 0;
    }
}