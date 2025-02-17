#ifdef KERNEL
#include <kernel.h>
#else
#include <runtime.h>
#endif
#include <pagecache.h>
#include <storage.h>
#include <tfs.h>

#define TFS_VERSION 0x00000004

typedef struct log *log;

typedef struct filesystem {
    id_heap storage;
    u64 size;
    heap h;
    int blocksize_order;
    int alignment_order;        /* in blocks */
    int page_order;
    u8 uuid[UUID_LEN];
    char label[VOLUME_LABEL_MAX_LEN];
    table files; // maps tuple to fsfile
    closure_type(log, void, tuple);
    heap dma;
    void *zero_page;
    block_io r;
    block_io w;
    block_flush flush;
    pagecache_volume pv;
    log tl;
    log temp_log;
    u64 next_extend_log_offset;
    u64 next_new_log_offset;
    tuple root;
} *filesystem;

typedef struct fsfile {
    rangemap extentmap;
    filesystem fs;
    pagecache_node cache_node;
    u64 length;
    tuple md;
    sg_io read;
    sg_io write;
    struct refcount refcount;
} *fsfile;

typedef struct extent {
    /* these are in block units */
    struct rmnode node;         /* must be first */
    u64 start_block;
    u64 allocated;
    tuple md;                   /* shortcut to extent meta */
    boolean uninited;
} *extent;

void ingest_extent(fsfile f, symbol foff, tuple value);

log log_create(heap h, filesystem fs, boolean initialize, status_handler sh);
boolean log_write(log tl, tuple t);
boolean log_write_eav(log tl, tuple e, symbol a, value v);
void log_flush(log tl, status_handler completion);
void log_destroy(log tl);
void flush(filesystem fs, status_handler);
u64 filesystem_allocate_storage(filesystem fs, u64 nblocks);
boolean filesystem_reserve_storage(filesystem fs, range storage_blocks);
boolean filesystem_free_storage(filesystem fs, range storage_blocks);
void filesystem_storage_op(filesystem fs, sg_list sg, merge m, range blocks, block_io op);
    
void filesystem_log_rebuild(filesystem fs, log new_tl, status_handler sh);
void filesystem_log_rebuild_done(filesystem fs, log new_tl);

boolean filesystem_reserve_log_space(filesystem fs, u64 *next_offset, u64 *offset, u64 size);

typedef closure_type(buffer_status, buffer, status);
fsfile allocate_fsfile(filesystem fs, tuple md);

#define filesystem_log_blocks(fs) (TFS_LOG_DEFAULT_EXTENSION_SIZE >> (fs)->blocksize_order)

static inline u64 bytes_from_sectors(filesystem fs, u64 sectors)
{
    return sectors << fs->blocksize_order;
}

static inline u64 sector_from_offset(filesystem fs, bytes b)
{
    return b >> fs->blocksize_order;
}
