#pragma once

#include "xstdc/stream.h"
#include "xstdc/string.h"
#include "xstdc/types.h"

// At least READ or WRITE is required; TRUNCATE requires WRITE.
// CREATE preserves an existing file unless TRUNCATE is also specified.
typedef enum : u8 {
    XFILE_CREATE = 1 << 0,
    XFILE_TRUNCATE = 1 << 1,
    XFILE_WRITE = 1 << 2,
    XFILE_READ = 1 << 3,
} XFileMode;

enum { XFILE_BUFSIZE = 4096 };

// Owns its descriptor. Do not copy as another owner. Synchronize concurrent
// operations when they depend on the shared file position. fd=-1 means closed.
typedef struct {
    XFileMode mode;
    i32 fd;
    struct XFileWriteBuffer *write_buffer; // Private, allocated on first write.
    struct XFileReadBuffer *read_buffer;   // Private, allocated on first read.
} XFile;

// Open a non-NUL-terminated path; return false without changing out on failure.
// New files use 0666 filtered by umask; descriptors are close-on-exec.
extern bool xfile_open(XFile *out, XStringVw name, XFileMode mode);
// Flush and close, invalidating file even on error; return false if either
// fails. Discard pending data and release buffers, but do not free file itself.
extern bool xfile_free(XFile *file);

// Read exactly n bytes, caching read-ahead data. Return false on error or EOF;
// partial progress is possible. Position queries report the logical position.
extern bool xfile_read(XFile *file, void *dst, usize n);
// Flush writes and rewind unread cached bytes, then read exactly n bytes via
// OS. Return false on error or EOF; partial progress is possible.
extern bool xfile_read_unbuffered(XFile *file, void *dst, usize n);
// Flush writes and rewind unread cached bytes, then write n bytes via OS.
// Return false on error; partial progress is possible.
extern bool xfile_write_unbuffered(XFile *file, const void *src, usize n);
// Buffer n bytes, flushing when full; success means accepted, not persisted.
// Non-write operations flush first; failure may leave partial progress.
extern bool xfile_write(XFile *file, const void *src, usize n);
// Flush pending writes; retain unwritten bytes after an error for a later
// retry.
extern bool xfile_flush(XFile *file);
// Flush writes, then read n bytes without advancing, even on failure. Requires
// a seekable file; concurrent content changes are not prevented.
extern bool xfile_peek(XFile *file, void *dst, usize n);

// Read a regular file from its start; dst must fit the entire current size. The
// caller must prevent file growth between sizing dst and reading.
extern bool xfile_read_all(XFile *file, void *dst);
// Regular-file size in bytes, without changing position. out unchanged on
// error.
extern bool xfile_size(XFile *file, u64 *out);
// Successful explicit seeks discard the read cache (including seeks to the
// current position, which can refresh data changed externally).
extern bool xfile_set_pos(XFile *file, u64 pos);
// Return the logical byte position in out; leave out unchanged on error.
extern bool xfile_get_pos(const XFile *file, u64 *out);

// Adapters borrow file; it must remain open and outlive them.
typedef struct {
    XWriter base;
    XFile *file;
} XFileWriter;
typedef struct {
    XReader base;
    XFile *file;
} XFileReader;

// Write through the borrowed file using its write buffer.
static inline bool xfile_writer_write(XWriter *writer, const void *src,
                                      usize n);
// Read through the borrowed file using its read buffer.
static inline bool xfile_reader_read(XReader *reader, void *dst, usize n);
// Peek through the borrowed file without advancing its position.
static inline bool xfile_reader_peek(XReader *reader, void *dst, usize n);
// Create a writer borrowing file; file must remain open and outlive the writer.
static inline XFileWriter xfile_writer(XFile *file);
// Create a reader borrowing file; file must remain open and outlive the reader.
static inline XFileReader xfile_reader(XFile *file);

// Definitions

static inline bool xfile_writer_write(XWriter *writer, const void *src,
                                      usize n) {
    return xfile_write(((XFileWriter *)writer)->file, src, n);
}

static inline bool xfile_reader_read(XReader *reader, void *dst, usize n) {
    return xfile_read(((XFileReader *)reader)->file, dst, n);
}

static inline bool xfile_reader_peek(XReader *reader, void *dst, usize n) {
    return xfile_peek(((XFileReader *)reader)->file, dst, n);
}

static inline XFileWriter xfile_writer(XFile *file) {
    return (XFileWriter){.base = {.vtable = {.write = xfile_writer_write}},
                         .file = file};
}

static inline XFileReader xfile_reader(XFile *file) {
    static const XReaderVTable vtable = {.read = xfile_reader_read,
                                         .peek = xfile_reader_peek};
    return (XFileReader){.base = {.vtable = &vtable}, .file = file};
}
