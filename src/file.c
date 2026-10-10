#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include "xstdc/file.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

struct XFileWriteBuffer {
    usize used;
    u8 bytes[XFILE_BUFSIZE];
};
struct XFileReadBuffer {
    usize pos, used;
    u8 bytes[XFILE_BUFSIZE];
};
static usize file_unread(const XFile *file) {
    return file->read_buffer ? file->read_buffer->used - file->read_buffer->pos
                             : 0;
}
static bool file_discard_read(XFile *file) {
    const usize unread = file_unread(file);
    if (unread) {
        off_t position;
        do {
            position = lseek(file->fd, -(off_t)unread, SEEK_CUR);
        } while (position < 0 && errno == EINTR);
        if (position < 0)
            return false;
    }
    if (file->read_buffer)
        file->read_buffer->used = file->read_buffer->pos = 0;
    return true;
}
// The pointed-to buffering state is mutable even through a const XFile, so
// get_pos can flush while retaining its existing public signature.
static bool file_flush(const XFile *file) {
    struct XFileWriteBuffer *buffer = file->write_buffer;
    if (!buffer)
        return true;
    usize sent = 0;
    while (sent < buffer->used) {
        const ssize_t n =
            write(file->fd, buffer->bytes + sent, buffer->used - sent);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            if (sent) {
                buffer->used -= sent;
                xmemmv(buffer->bytes, buffer->bytes + sent, buffer->used);
            }
            return false;
        }
        sent += (usize)n;
    }
    buffer->used = 0;
    return true;
}
bool xfile_flush(XFile *file) { return file_flush(file); }

bool xfile_open(XFile *out, XStringVw name, XFileMode mode) {
    const unsigned bits = (unsigned)mode;
    char *path = xalloc(name.len + 1);
    xmemcp(path, (void *)name.p, name.len);
    path[name.len] = 0;
    int flags = O_CLOEXEC;
    flags |= (bits & XFILE_WRITE) ? ((bits & XFILE_READ) ? O_RDWR : O_WRONLY)
                                  : O_RDONLY;
    if (bits & XFILE_CREATE)
        flags |= O_CREAT;
    if (bits & XFILE_TRUNCATE)
        flags |= O_TRUNC;
    int fd;
    do {
        fd = open(path, flags, 0666);
    } while (fd < 0 && errno == EINTR);
    xfree(path, name.len + 1);
    if (fd < 0)
        return false;
    *out = (XFile){.mode = mode, .fd = fd};
    return true;
}
bool xfile_free(XFile *file) {
    const bool flushed = file_flush(file);
    const int fd = file->fd;
    file->fd = -1;
    file->mode = 0;
    // Linux and BSD release the descriptor even on EINTR. Retrying there can
    // close a descriptor which another thread has already reused.
    int status;
#if defined(__hpux)
    do {
        status = close(fd);
    } while (status < 0 && errno == EINTR);
#else
    status = close(fd);
#endif
    if (file->write_buffer) {
        xfree(file->write_buffer, sizeof(*file->write_buffer));
        file->write_buffer = 0;
    }
    if (file->read_buffer) {
        xfree(file->read_buffer, sizeof(*file->read_buffer));
        file->read_buffer = 0;
    }
    return flushed && status == 0;
}
static usize file_chunk(usize n) {
    return n > (usize)SSIZE_MAX ? (usize)SSIZE_MAX : n;
}
bool xfile_read(XFile *file, void *dst, usize n) {
    if (!file_flush(file))
        return false;
    if (!n)
        return true;
    if (!file->read_buffer) {
        file->read_buffer = xalloc(sizeof(*file->read_buffer));
        file->read_buffer->used = file->read_buffer->pos = 0;
    }
    struct XFileReadBuffer *buffer = file->read_buffer;
    u8 *p = dst;
    while (n) {
        if (buffer->pos == buffer->used) {
            ssize_t got;
            do {
                got = read(file->fd, buffer->bytes, sizeof(buffer->bytes));
            } while (got < 0 && errno == EINTR);
            if (got <= 0)
                return false;
            buffer->pos = 0;
            buffer->used = (usize)got;
        }
        const usize available = buffer->used - buffer->pos;
        const usize count = n < available ? n : available;
        xmemcp(p, buffer->bytes + buffer->pos, count);
        buffer->pos += count;
        p += count;
        n -= count;
    }
    return true;
}
bool xfile_read_unbuffered(XFile *file, void *dst, usize n) {
    if (!file_flush(file))
        return false;
    if (n && !file_discard_read(file))
        return false;
    u8 *p = dst;
    while (n) {
        const ssize_t got = read(file->fd, p, file_chunk(n));
        if (got < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (!got)
            return false;
        p += got;
        n -= (usize)got;
    }
    return true;
}
bool xfile_write_unbuffered(XFile *file, const void *src, usize n) {
    if (!file_flush(file))
        return false;
    if (n && !file_discard_read(file))
        return false;
    const u8 *p = src;
    while (n) {
        const ssize_t sent = write(file->fd, p, file_chunk(n));
        if (sent < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (!sent)
            return false;
        p += sent;
        n -= (usize)sent;
    }
    return true;
}
bool xfile_write(XFile *file, const void *src, usize n) {
    if (!n)
        return true;
    if (!file_discard_read(file))
        return false;
    if (!file->write_buffer) {
        file->write_buffer = xalloc(sizeof(*file->write_buffer));
        file->write_buffer->used = 0;
    }
    struct XFileWriteBuffer *buffer = file->write_buffer;
    const u8 *p = src;
    while (n) {
        if (buffer->used == sizeof(buffer->bytes) && !file_flush(file))
            return false;
        const usize available = sizeof(buffer->bytes) - buffer->used;
        const usize count = n < available ? n : available;
        xmemcp(buffer->bytes + buffer->used, (void *)p, count);
        buffer->used += count;
        p += count;
        n -= count;
        if (buffer->used == sizeof(buffer->bytes) && !file_flush(file))
            return false;
    }
    return true;
}
bool xfile_get_pos(const XFile *file, u64 *out) {
    if (!file_flush(file))
        return false;
    off_t position;
    do {
        position = lseek(file->fd, 0, SEEK_CUR);
    } while (position < 0 && errno == EINTR);
    if (position < 0)
        return false;
    const usize unread = file_unread(file);
    *out = (u64)position - unread;
    return true;
}
bool xfile_set_pos(XFile *file, u64 pos) {
    if (!file_flush(file))
        return false;
    if (pos > INT64_MAX)
        return false;
    off_t position;
    do {
        position = lseek(file->fd, (off_t)pos, SEEK_SET);
    } while (position < 0 && errno == EINTR);
    if (position < 0)
        return false;
    if (file->read_buffer)
        file->read_buffer->used = file->read_buffer->pos = 0;
    return true;
}
bool xfile_peek(XFile *file, void *dst, usize n) {
    if (!file_flush(file))
        return false;
    if (!n)
        return true;
    u64 position;
    if (!xfile_get_pos(file, &position))
        return false;
    u8 *p = dst;
    const usize unread = file_unread(file);
    const usize cached = n < unread ? n : unread;
    if (cached) {
        xmemcp(p, file->read_buffer->bytes + file->read_buffer->pos, cached);
        p += cached;
        position += cached;
        n -= cached;
    }
    while (n) {
        if (position > INT64_MAX)
            return false;
        const ssize_t got = pread(file->fd, p, file_chunk(n), (off_t)position);
        if (got < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (!got)
            return false;
        p += got;
        n -= (usize)got;
        position += (u64)got;
    }
    return true;
}
bool xfile_size(XFile *file, u64 *out) {
    if (!file_flush(file))
        return false;
    struct stat st;
    int status;
    do {
        status = fstat(file->fd, &st);
    } while (status < 0 && errno == EINTR);
    if (status < 0)
        return false;
    if (!S_ISREG(st.st_mode))
        return false;
    if (st.st_size < 0)
        return false;
    *out = (u64)st.st_size;
    return true;
}
bool xfile_read_all(XFile *file, void *dst) {
    u64 size;
    if (!xfile_size(file, &size))
        return false;
    return xfile_set_pos(file, 0) && xfile_read(file, dst, (usize)size);
}
