#define _POSIX_C_SOURCE 200809L
#include "xstdc/assert.h"
#include "xstdc/file.h"
#include "xstdc/print.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void test_file(const char *path, const char *directory) {
    // No terminating NUL in this buffer: the byte after the view is '!'.
    char raw[512];
    usize length = __builtin_strlen(path);
    xmemcp(raw, (void *)path, length);
    raw[length] = '!';
    XStringVw name = {raw, length};
    XFile f = {.fd = -1};
    assert(!xfile_open(&f, name, XFILE_READ) && f.fd == -1);
    assert(xfile_open(&f, name, XFILE_CREATE | XFILE_READ | XFILE_WRITE));
    assert(fcntl(f.fd, F_GETFD) & FD_CLOEXEC);
    u64 size = 123, position;
    assert(xfile_size(&f, &size) && size == 0);
    assert(xfile_read_all(&f, 0));
    assert(xfile_read(&f, 0, 0) && xfile_write(&f, 0, 0) &&
           xfile_peek(&f, 0, 0));
    const char bytes[] = {'a', 0, 'c', 'd', 'e'};
    XFileWriter writer = xfile_writer(&f);
    assert(writer.base.vtable.write(&writer.base, bytes, sizeof bytes));
    assert(xfile_get_pos(&f, &position) && position == sizeof bytes);
    assert(xfile_size(&f, &size) && size == sizeof bytes);
    assert(xfile_get_pos(&f, &position) && position == sizeof bytes);
    assert(xfile_set_pos(&f, 1));
    XFileReader reader = xfile_reader(&f);
    char buffer[8] = {0};
    assert(reader.base.vtable->peek(&reader.base, buffer, 3));
    assert(xmemeq(buffer, (void *)(bytes + 1), 3));
    assert(xfile_get_pos(&f, &position) && position == 1);
    assert(!xfile_peek(&f, buffer, 5));
    assert(xfile_get_pos(&f, &position) && position == 1);
    assert(reader.base.vtable->read(&reader.base, buffer, 3));
    assert(xmemeq(buffer, (void *)(bytes + 1), 3));
    assert(xfile_get_pos(&f, &position) && position == 4);
    assert(!xfile_read(&f, buffer, 2) && buffer[0] == 'e');
    assert(xfile_get_pos(&f, &position) && position == 5);
    assert(xfile_read_all(&f, buffer));
    assert(xmemeq(buffer, (void *)bytes, sizeof bytes));
    assert(xfile_get_pos(&f, &position) && position == 5);
    assert(!xfile_set_pos(&f, (u64)-1));
    assert(xfile_get_pos(&f, &position) && position == 5);
    assert(xfile_free(&f) && f.fd == -1);
    assert(xfile_open(&f, name, XFILE_CREATE | XFILE_READ));
    assert(xfile_size(&f, &size) && size == 5); // CREATE does not truncate.
    assert(xfile_free(&f));
    assert(xfile_open(&f, name, XFILE_WRITE | XFILE_TRUNCATE));
    assert(xfile_size(&f, &size) && size == 0);
    assert(xfile_free(&f));
    assert(xfile_open(&f, xstrv_fromz(directory), XFILE_READ));
    assert(!xfile_read_all(&f, buffer));
    assert(xfile_free(&f));
}
static void test_buffering(const char *path) {
    XFile file;
    u64 value;
    struct stat st;
    char result[16];
    for (unsigned operation = 0; operation < 8; ++operation) {
        assert(xfile_open(&file, xstrv_fromz(path),
                          XFILE_READ | XFILE_WRITE | XFILE_TRUNCATE));
        char source[] = "abc";
        assert(xfile_write(&file, source, 3));
        source[0] = 'X'; // The buffer owns a copy, not a pointer to source.
        assert(fstat(file.fd, &st) == 0 && st.st_size == 0);
        switch (operation) {
        case 0:
            assert(xfile_flush(&file));
            break;
        case 1:
            assert(xfile_get_pos(&file, &value) && value == 3);
            break;
        case 2:
            assert(xfile_size(&file, &value) && value == 3);
            break;
        case 3:
            assert(xfile_set_pos(&file, 0));
            break;
        case 4:
            assert(!xfile_read(&file, result, 1));
            break;
        case 5:
            assert(!xfile_peek(&file, result, 1));
            break;
        case 6:
            assert(xfile_read_all(&file, result) && xmemeq(result, "abc", 3));
            break;
        case 7:
            assert(xfile_free(&file));
            break;
        }
        if (operation != 7) {
            assert(fstat(file.fd, &st) == 0 && st.st_size == 3);
            assert(pread(file.fd, result, 3, 0) == 3 &&
                   xmemeq(result, "abc", 3));
            assert(xfile_free(&file));
        } else {
            assert(xfile_open(&file, xstrv_fromz(path), XFILE_READ));
            assert(xfile_read_all(&file, result) && xmemeq(result, "abc", 3));
            assert(xfile_free(&file));
        }
    }
    // Lots of tiny writes must drain full buffers before an explicit flush.
    assert(xfile_open(&file, xstrv_fromz(path),
                      XFILE_WRITE | XFILE_READ | XFILE_TRUNCATE));
    for (unsigned i = 0; i < 65537; ++i) {
        const u8 byte = (u8)i;
        assert(xfile_write(&file, &byte, 1));
    }
    assert(fstat(file.fd, &st) == 0 && st.st_size > 0 && st.st_size < 65537);
    assert(xfile_flush(&file));
    assert(xfile_size(&file, &value) && value == 65537);
    u8 *all = xalloc(65537);
    assert(xfile_read_all(&file, all));
    for (unsigned i = 0; i < 65537; ++i)
        assert(all[i] == (u8)i);
    xfree(all, 65537);
    assert(xfile_free(&file));
#ifdef __linux__
    assert(xfile_open(&file, strl("/dev/full"), XFILE_WRITE));
    assert(xfile_write(&file, "pending", 7));
    assert(!xfile_flush(&file));
    value = 123;
    assert(!xfile_get_pos(&file, &value) && value == 123);
    // Replacing the failing descriptor lets us verify the pending bytes
    // survive.
    int replacement = open(path, O_WRONLY | O_TRUNC);
    assert(replacement >= 0 && dup2(replacement, file.fd) == file.fd);
    assert(close(replacement) == 0);
    assert(xfile_flush(&file));
    assert(xfile_free(&file));
    assert(xfile_open(&file, xstrv_fromz(path), XFILE_READ));
    assert(xfile_read_all(&file, result) && xmemeq(result, "pending", 7));
    assert(xfile_free(&file));
    assert(xfile_open(&file, strl("/dev/full"), XFILE_WRITE));
    assert(xfile_write(&file, "pending", 7));
    int fd = file.fd;
    assert(!xfile_free(&file) && file.fd == -1 && !file.write_buffer);
    assert(fcntl(fd, F_GETFD) == -1);
#endif
}
static void test_read_buffering(const char *path) {
    enum { COUNT = XFILE_BUFSIZE + 33 };
    u8 source[COUNT], result[COUNT];
    for (usize i = 0; i < COUNT; ++i)
        source[i] = (u8)i;
    XFile f;
    assert(xfile_open(&f, xstrv_fromz(path),
                      XFILE_READ | XFILE_WRITE | XFILE_TRUNCATE));
    assert(xfile_write_unbuffered(&f, source, COUNT));
    assert(!f.write_buffer && !f.read_buffer);
    assert(xfile_set_pos(&f, 0));
    assert(xfile_read(&f, result, 1) && result[0] == source[0]);
    assert(lseek(f.fd, 0, SEEK_CUR) == XFILE_BUFSIZE);
    u64 position;
    assert(xfile_get_pos(&f, &position) && position == 1);
    // Peek straddles cached data and a pread, without consuming either.
    assert(xfile_peek(&f, result, XFILE_BUFSIZE + 5));
    assert(xmemeq(result, source + 1, XFILE_BUFSIZE + 5));
    assert(xfile_get_pos(&f, &position) && position == 1);
    assert(lseek(f.fd, 0, SEEK_CUR) == XFILE_BUFSIZE);
    assert(xfile_read(&f, result, 1) && result[0] == source[1]);
    assert(lseek(f.fd, 0, SEEK_CUR) == XFILE_BUFSIZE);
    assert(!xfile_set_pos(&f, (u64)-1));
    assert(xfile_get_pos(&f, &position) && position == 2);
    assert(xfile_read_unbuffered(&f, result, 3));
    assert(xmemeq(result, source + 2, 3));
    assert(lseek(f.fd, 0, SEEK_CUR) == 5);
    // Switch from read-ahead to buffered write at the logical position.
    assert(xfile_read(&f, result, 1) && result[0] == source[5]);
    assert(xfile_write(&f, "XY", 2));
    assert(lseek(f.fd, 0, SEEK_CUR) == 6);
    assert(xfile_write_unbuffered(&f, "Z", 1));
    assert(lseek(f.fd, 0, SEEK_CUR) == 9);
    assert(pread(f.fd, result, 3, 6) == 3 && xmemeq(result, "XYZ", 3));
    assert(xfile_set_pos(&f, 10));
    assert(xfile_read(&f, result, 1) && result[0] == source[10]);
    assert(xfile_write_unbuffered(&f, "Q", 1));
    assert(lseek(f.fd, 0, SEEK_CUR) == 12);
    assert(xfile_read_unbuffered(&f, result, 1) && result[0] == source[12]);
    assert(xfile_set_pos(&f, COUNT - 2));
    assert(!xfile_read(&f, result, 3));
    assert(xfile_get_pos(&f, &position) && position == COUNT);
    assert(xfile_write(&f, "end", 3));
    assert(xfile_set_pos(&f, COUNT));
    assert(xfile_read_unbuffered(&f, result, 3) && xmemeq(result, "end", 3));
    assert(xfile_free(&f) && !f.read_buffer && !f.write_buffer);

    // A failed cache rewind on a pipe must preserve all unread data.
    int pipes[2];
    assert(pipe(pipes) == 0);
    assert(write(pipes[1], "abcdef", 6) == 6);
    assert(close(pipes[1]) == 0);
    f = (XFile){.mode = XFILE_READ, .fd = pipes[0]};
    assert(xfile_read(&f, result, 1) && result[0] == 'a');
    assert(!xfile_read_unbuffered(&f, result, 1));
    assert(xfile_read(&f, result, 5) && xmemeq(result, "bcdef", 5));
    assert(!xfile_read_unbuffered(&f, result, 1));
    assert(xfile_free(&f));
}
static void test_pipes(void) {
    enum { COUNT = 262144 };
    u8 *data = xalloc(COUNT);
    for (usize i = 0; i < COUNT; ++i)
        data[i] = (u8)i;
    for (unsigned direction = 0; direction < 2; ++direction) {
        int descriptors[2];
        assert(pipe(descriptors) == 0);
        pid_t child = fork();
        assert(child >= 0);
        if (!child) {
            alarm(15);
            close(descriptors[direction ? 1 : 0]);
            XFile file = {.mode = direction ? XFILE_READ : XFILE_WRITE,
                          .fd = descriptors[direction ? 0 : 1]};
            if (direction) {
                assert(xfile_read(&file, data, COUNT));
                for (usize i = 0; i < COUNT; ++i)
                    assert(data[i] == (u8)i);
            } else {
                for (usize i = 0; i < COUNT;) {
                    usize n = COUNT - i > 4093 ? 4093 : COUNT - i;
                    assert(xfile_write(&file, data + i, n));
                    i += n;
                }
            }
            assert(xfile_free(&file));
            _exit(0);
        }
        close(descriptors[direction ? 0 : 1]);
        XFile file = {.mode = direction ? XFILE_WRITE : XFILE_READ,
                      .fd = descriptors[direction ? 1 : 0]};
        if (direction) {
            assert(xfile_write(&file, data, COUNT));
        } else {
            u64 unchanged = 123;
            assert(!xfile_get_pos(&file, &unchanged) && unchanged == 123);
            assert(!xfile_peek(&file, data, 1));
            assert(!xfile_read_all(&file, data));
            assert(xfile_read(&file, data, COUNT));
            for (usize i = 0; i < COUNT; ++i)
                assert(data[i] == (u8)i);
            assert(!xfile_read(&file, data, 1));
        }
        assert(xfile_free(&file));
        int status;
        assert(waitpid(child, &status, 0) == child);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    xfree(data, COUNT);
}
static void test_print_writer(const char *path) {
    XFile file;
    assert(xfile_open(&file, xstrv_fromz(path),
                      XFILE_READ | XFILE_WRITE | XFILE_TRUNCATE));
    XFileWriter writer = xfile_writer(&file);
    u32 count = 42;
    bool enabled = true;
    assert(xprint_writer(&writer.base, "count=%u32 enabled=%b", &count,
                         &enabled) == 21);
    struct stat st;
    assert(fstat(file.fd, &st) == 0 &&
           st.st_size == 0); // Printing does not flush.
    char text[21];
    assert(xfile_read_all(&file, text));
    assert(xmemeq(text, "count=42 enabled=true", 21));
    assert(xfile_free(&file));
}
int main(void) {
    alarm(30);
    char directory[] = "/tmp/xstdc-file-test-XXXXXX";
    assert(mkdtemp(directory));
    char path[512];
    XStringVw name = xprint_buf(path, sizeof(path) - 1, "%z/data", directory);
    assert(name.p);
    path[name.len] = 0;
    test_file(path, directory);
    test_buffering(path);
    test_read_buffering(path);
    test_pipes();
    test_print_writer(path);
    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
    alarm(0);
    xprint("file tests passed\n");
}
