#include "xstdc/linked_list.h"
#include "xstdc/print.h"
#include "xstdc/rng.h"

static void check(XLinkedList *single, XDoublyLinkedList *double_list,
                  const u8 *model, usize count, usize width) {
    assert(single->sz == count && double_list->sz == count);
    assert((single->head == 0) == (count == 0));
    assert((single->tail == 0) == (count == 0));
    assert((double_list->head == 0) == (count == 0));
    assert((double_list->tail == 0) == (count == 0));
    XLinkedListIter a;
    XDoublyLinkedListIter b;
    xllit_init(&a, single);
    xdllit_init_head(&b, double_list);
    assert(!xdllit_backward(&b));
    for (usize i = 0; i < count; ++i) {
        void *pa = xllit_forward(&a), *pb = xdllit_forward(&b);
        assert(pa && pb && !((usize)pa % 8) && !((usize)pb % 8));
        assert(xmemeq(pa, model + i * width, width));
        assert(xmemeq(pb, model + i * width, width));
        // Switching direction returns the same element across the cursor.
        assert(xdllit_backward(&b) == pb);
        assert(xdllit_forward(&b) == pb);
        if (i + 1 == count) {
            assert(pa == single->tail->data && !single->tail->next);
            assert(pb == double_list->tail->data && !double_list->tail->next);
        }
    }
    assert(!xllit_forward(&a) && !xllit_forward(&a));
    assert(!xdllit_forward(&b) && !xdllit_forward(&b));
    for (usize i = count; i; --i)
        assert(xmemeq(xdllit_backward(&b), model + (i - 1) * width, width));
    assert(!xdllit_backward(&b));
    xdllit_init_tail(&b, double_list);
    assert(!xdllit_forward(&b));
    for (usize i = count; i; --i)
        assert(xmemeq(xdllit_backward(&b), model + (i - 1) * width, width));
    assert(!xdllit_backward(&b));
    if (count)
        assert(!double_list->head->prev);
}

static void test_model(usize width) {
    XLinkedList a;
    XDoublyLinkedList b;
    xllist_init(&a, width);
    xdllist_init(&b, width);
    u8 model[96 * 129], value[129];
    usize count = 0;
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, width);
    check(&a, &b, model, count, width);
    for (usize step = 0; step < 1500; ++step) {
        u64 r = xrng_xoshiro256pp_u64(&rng);
        if (count < 96 && (!count || (r & 1))) {
            usize pos = (r >> 8) % (count + 1);
            for (usize i = 0; i < width; ++i)
                value[i] = (u8)(r + i);
            if (!pos) {
                xllist_prepend(&a, value);
                if (count && (r & 2))
                    xdllist_insertb4(&b, b.head->data, value);
                else
                    xdllist_prepend(&b, value);
            } else if (pos == count && (r & 2)) {
                xllist_append(&a, value);
                xdllist_append(&b, value);
            } else {
                XLinkedListNode *prev_a = a.head;
                XDoublyLinkedListNode *prev_b = b.head;
                for (usize i = 1; i < pos; ++i) {
                    prev_a = prev_a->next;
                    prev_b = prev_b->next;
                }
                xllist_insertafter(&a, prev_a->data, value);
                if (prev_b->next && (r & 2))
                    xdllist_insertb4(&b, prev_b->next->data, value);
                else
                    xdllist_insertafter(&b, prev_b->data, value);
            }
            xmemmv(model + (pos + 1) * width, model + pos * width,
                   (count - pos) * width);
            xmemcp(model + pos * width, value, width);
            ++count;
        } else {
            usize pos = (r >> 8) % count;
            if (!pos) {
                xllist_pop_front(&a);
                if (r & 2)
                    xdllist_pop_front(&b);
                else
                    xdllist_remove(&b, b.head->data);
            } else {
                XLinkedListNode *prev_a = a.head;
                XDoublyLinkedListNode *prev_b = b.head;
                for (usize i = 1; i < pos; ++i) {
                    prev_a = prev_a->next;
                    prev_b = prev_b->next;
                }
                xllist_removeafter(&a, prev_a->data);
                xdllist_remove(&b, prev_b->next->data);
            }
            --count;
            xmemmv(model + pos * width, model + (pos + 1) * width,
                   (count - pos) * width);
        }
        check(&a, &b, model, count, width);
    }
    xllist_free(&a);
    xdllist_free(&b);
    xllist_free(&a);
    xdllist_free(&b);
    assert(a.elem_sz == width && b.elem_sz == width);
    check(&a, &b, model, 0, width);
    xllist_append(&a, value);
    xdllist_append(&b, value);
    check(&a, &b, value, 1, width);
    xllist_pop_front(&a);
    xdllist_pop_front(&b);
    check(&a, &b, model, 0, width);
    xllist_free(&a);
    xdllist_free(&b);
}

static void test_stable_pointers(void) {
    XLinkedList a;
    XDoublyLinkedList b;
    xllist_init(&a, sizeof(u64));
    xdllist_init(&b, sizeof(u64));
    u64 value = 42;
    xllist_append(&a, &value);
    xdllist_append(&b, &value);
    u64 *pa = (u64 *)a.head->data, *pb = (u64 *)b.head->data;
    xllist_prepend(&a, pa);
    xdllist_prepend(&b, pb);
    xllist_append(&a, pa);
    xdllist_append(&b, pb);
    xllist_insertafter(&a, pa, pa);
    xdllist_insertafter(&b, pb, pb);
    xllist_removeafter(&a, pa);
    xdllist_remove(&b, b.head->next->next->data);
    xllist_pop_front(&a);
    xdllist_pop_front(&b);
    assert(a.head->data == (u8 *)pa && b.head->data == (u8 *)pb);
    assert(*pa == 42 && *pb == 42);
    // Remove the tail, then verify a fresh append repairs both link directions.
    xllist_removeafter(&a, pa);
    xdllist_remove(&b, b.tail->data);
    xllist_append(&a, pa);
    xdllist_append(&b, pb);
    u64 expected[] = {42, 42};
    check(&a, &b, (const u8 *)expected, 2, sizeof(u64));
    xllist_free(&a);
    xdllist_free(&b);
}

int main(void) {
    const usize widths[] = {1, 3, 8, 129};
    for (usize i = 0; i < sizeof widths / sizeof *widths; ++i)
        test_model(widths[i]);
    test_stable_pointers();
    xprint("linked list tests passed\n");
}
