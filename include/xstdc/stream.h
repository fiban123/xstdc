#pragma once

#include "xstdc/mem.h"
#include "xstdc/types.h"

typedef struct XReader XReader;

typedef struct {
    bool (*read)(XReader *reader, void *dst, usize n);
    bool (*peek)(XReader *reader, void *dst, usize n);
} XReaderVTable;

struct XReader {
    const XReaderVTable *vtable;
};

// Read exactly n bytes through the reader; return false on failure.
static inline bool xreader_read(XReader *reader, void *dst, usize n);

// Read n bytes without advancing the reader; return false on failure.
static inline bool xreader_peek(XReader *reader, void *dst, usize n);

typedef struct XWriter XWriter;

typedef struct {
    bool (*write)(XWriter *writer, const void *src, usize n);
} XWriterVTable;

struct XWriter {
    // vtable is already pointer sized - copy it.
    XWriterVTable vtable;
};

// Write n bytes through the writer; return false on failure.
static inline bool xwriter_write(XWriter *writer, const void *src, usize n);

// Borrowed, fixed-size buffers. No allocation or growth. pos starts at zero.
// Operations either transfer all n bytes or return false without changing the
// buffer or position. Peek never advances. Overlapping source/destination is
// supported; zero-byte operations permit NULL buffers.
typedef struct {
    XReader base;
    const u8 *p;
    usize len, pos;
} XBufReader;

typedef struct {
    XWriter base;
    u8 *p;
    usize len, pos;
} XBufWriter;

// Copy n buffered bytes without advancing; return false if they do not fit.
static inline bool xbuf_reader_peek(XReader *reader, void *dst, usize n);

// Copy n buffered bytes and advance; leave position unchanged on failure.
static inline bool xbuf_reader_read(XReader *reader, void *dst, usize n);

// Copy n bytes into the buffer and advance; change nothing if they do not fit.
static inline bool xbuf_writer_write(XWriter *writer, const void *src,
                                     usize n);

// Create a reader borrowing len bytes, starting at position zero.
static inline XBufReader xbuf_reader(const void *p, usize len);

// Create a writer borrowing len writable bytes, starting at position zero.
static inline XBufWriter xbuf_writer(void *p, usize len);

// Definitions

static inline bool xreader_read(XReader *reader, void *dst, usize n) {
    return reader->vtable->read(reader, dst, n);
}

static inline bool xreader_peek(XReader *reader, void *dst, usize n) {
    return reader->vtable->peek(reader, dst, n);
}

static inline bool xwriter_write(XWriter *writer, const void *src, usize n) {
    return writer->vtable.write(writer, src, n);
}

static inline bool xbuf_reader_peek(XReader *reader, void *dst, usize n) {
    XBufReader *buf = (XBufReader *)reader;
    if (buf->pos > buf->len || n > buf->len - buf->pos ||
        (n && (!buf->p || !dst)))
        return false;
    if (n)
        xmemmv(dst, buf->p + buf->pos, n);
    return true;
}

static inline bool xbuf_reader_read(XReader *reader, void *dst, usize n) {
    if (!xbuf_reader_peek(reader, dst, n))
        return false;
    ((XBufReader *)reader)->pos += n;
    return true;
}

static inline bool xbuf_writer_write(XWriter *writer, const void *src,
                                     usize n) {
    XBufWriter *buf = (XBufWriter *)writer;
    if (buf->pos > buf->len || n > buf->len - buf->pos ||
        (n && (!buf->p || !src)))
        return false;
    if (n)
        xmemmv(buf->p + buf->pos, src, n);
    buf->pos += n;
    return true;
}

static inline XBufReader xbuf_reader(const void *p, usize len) {
    static const XReaderVTable vtable = {.read = xbuf_reader_read,
                                         .peek = xbuf_reader_peek};
    return (XBufReader){.base = {.vtable = &vtable}, .p = p, .len = len};
}

static inline XBufWriter xbuf_writer(void *p, usize len) {
    return (XBufWriter){
        .base = {.vtable = {.write = xbuf_writer_write}}, .p = p, .len = len};
}
