#include "xstdc/assert.h"
#include "xstdc/print.h"
#include "xstdc/stream.h"

int main(void) {
    const u8 data[] = {1, 0, 255, 4};
    u8 result[] = {9, 9, 9, 9, 9};
    XBufReader reader = xbuf_reader(data, sizeof data);
    XReader *r = &reader.base;
    assert(r->vtable->peek(r, result, 2));
    assert(reader.pos == 0 && result[0] == 1 && result[1] == 0);
    assert(r->vtable->read(r, result, 2));
    assert(reader.pos == 2);
    assert(!r->vtable->read(r, result, 3));
    assert(reader.pos == 2 && result[0] == 1 && result[2] == 9);
    assert(!r->vtable->peek(r, result, (usize)-1));
    assert(!r->vtable->read(r, 0, 1) && reader.pos == 2);
    assert(r->vtable->read(r, result + 2, 2));
    assert(reader.pos == 4 && xmemeq(result, data, 4) && result[4] == 9);
    assert(!r->vtable->read(r, result, 1));
    assert(r->vtable->read(r, 0, 0) && r->vtable->peek(r, 0, 0));

    u8 output[] = {9, 9, 9, 9, 9, 9};
    XBufWriter writer = xbuf_writer(output + 1, 4);
    XWriter *w = &writer.base;
    assert(w->vtable.write(w, data, 2));
    assert(writer.pos == 2);
    assert(!w->vtable.write(w, data, 3));
    assert(!w->vtable.write(w, data, (usize)-1));
    assert(!w->vtable.write(w, 0, 1));
    assert(writer.pos == 2 && output[3] == 9);
    assert(w->vtable.write(w, data + 2, 2));
    assert(writer.pos == 4 && xmemeq(output + 1, data, 4));
    assert(output[0] == 9 && output[5] == 9);
    assert(!w->vtable.write(w, data, 1));
    assert(w->vtable.write(w, 0, 0));

    XBufReader empty_reader = xbuf_reader(0, 0);
    XBufWriter empty_writer = xbuf_writer(0, 0);
    assert(xbuf_reader_read(&empty_reader.base, 0, 0));
    assert(xbuf_writer_write(&empty_writer.base, 0, 0));
    assert(!xbuf_reader_read(&empty_reader.base, result, 1));
    assert(!xbuf_writer_write(&empty_writer.base, data, 1));

    u8 overlap[] = {1, 2, 3, 4, 5};
    reader = xbuf_reader(overlap, 4);
    assert(xbuf_reader_read(&reader.base, overlap + 1, 4));
    const u8 expected[] = {1, 1, 2, 3, 4};
    assert(xmemeq(overlap, expected, 5));
    writer = xbuf_writer(overlap, 4);
    assert(xbuf_writer_write(&writer.base, overlap + 1, 4));
    const u8 restored[] = {1, 2, 3, 4, 4};
    assert(xmemeq(overlap, restored, 5));
    xprint("stream tests passed\n");
}
