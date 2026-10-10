#define _GNU_SOURCE
#include <errno.h>
#include <time.h>
#include <x86intrin.h>

#include "xstdc/assert.h"
#include "xstdc/time.h"

XTimestamp xtimestamp(void) {
    struct timespec ts;
    const int status = clock_gettime(CLOCK_TAI, &ts);
    assert(status == 0);
    return (XTimestamp){ts.tv_sec, (u32)ts.tv_nsec};
}

u64 xtime_nanos(void) {
    struct timespec ts;
    const int status = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(status == 0);
    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}

u64 xtime_cycles(void) {
    unsigned int aux;
    return __rdtscp(&aux);
}

void xnanosleep(u64 nanos) {
    struct timespec ts;
    ts.tv_sec = nanos / 1000000000;
    ts.tv_nsec = nanos % 1000000000;
    while (nanosleep(&ts, &ts) != 0) {
        assert(errno == EINTR);
    }
}

static i64 calendar_floor(i64 a, i64 b) { return a / b - (a % b < 0); }
static bool calendar_leap(i64 year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
static i64 calendar_start(i64 year) {
    const i64 previous = year - 1;
    return 365 * (year - 1970) + calendar_floor(previous, 4) -
           calendar_floor(previous, 100) + calendar_floor(previous, 400) - 477;
}
static unsigned calendar_month_days(i64 year, unsigned month) {
    static const u8 lengths[] = {31, 28, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
    return lengths[month - 1] + (month == 2 && calendar_leap(year));
}
static bool calendar_valid(XDateTime dt) {
    return dt.month >= 1 && dt.month <= 12 && dt.day >= 1 &&
           dt.day <= calendar_month_days(dt.year, dt.month) && dt.hours < 24 &&
           dt.minutes < 60 && dt.seconds <= 60 && dt.nanos < 1000000000 &&
           (unsigned)dt.tz < XTZ_COUNT;
}
static i64 calendar_days(XDateTime dt) {
    static const u16 starts[] = {0,   31,  59,  90,  120, 151,
                                 181, 212, 243, 273, 304, 334};
    return calendar_start(dt.year) + starts[dt.month - 1] +
           (dt.month > 2 && calendar_leap(dt.year)) + dt.day - 1;
}
static void calendar_date(i64 days, XDateTime *dt) {
    assert(days >= calendar_start(-2147483647ll - 1) &&
           days < calendar_start(2147483648ll));
    i64 year = calendar_floor(days + 719528, 146097) * 400;
    year += (days - calendar_start(year)) / 366;
    while (calendar_start(year + 1) <= days)
        ++year;
    unsigned day = (unsigned)(days - calendar_start(year));
    unsigned month = 1;
    while (day >= calendar_month_days(year, month))
        day -= calendar_month_days(year, month++);
    dt->year = (i32)year;
    dt->month = month;
    dt->day = day + 1;
}
XDateTime xdate_from_posix(XPosixTimestamp ts) {
    assert(ts.nanos < 1000000000);
    XDateTime dt = {.tz = XTZ_UTC, .nanos = ts.nanos};
    const i64 days = calendar_floor(ts.seconds, 86400);
    // Remainder avoids overflowing days * 86400 at INT64_MIN.
    i64 seconds = ts.seconds % 86400;
    if (seconds < 0)
        seconds += 86400;
    calendar_date(days, &dt);
    dt.hours = seconds / 3600;
    dt.minutes = seconds / 60 % 60;
    dt.seconds = seconds % 60;
    return dt;
}
void xtimestamp_sub_dur(XTimestamp *ts, XDuration dur) {
    assert(ts && ts->nanos < 1000000000 && dur.nanos < 1000000000);
    const bool borrow = ts->nanos < dur.nanos;
    const i128 seconds = (i128)ts->seconds - dur.seconds - borrow;
    assert(seconds >= (-9223372036854775807ll - 1) &&
           seconds <= 9223372036854775807ll);
    ts->seconds = (i64)seconds;
    ts->nanos = ts->nanos + (borrow ? 1000000000u : 0) - dur.nanos;
}
void xdate_sub_days(XDateTime *dt, i32 days) {
    assert(dt && calendar_valid(*dt));
    calendar_date(calendar_days(*dt) - (i64)days, dt);
}
void xdate_sub_dur(XDateTime *dt, XDuration dur) {
    assert(dt);
    XTimestamp ts;
    const bool valid = xtimestamp_from_date(*dt, &ts);
    assert(valid);
    xtimestamp_sub_dur(&ts, dur);
    *dt = xdate_from_ts(ts, dt->tz);
}

#include "time_scales.inc"
#define NS_PER_SECOND 1000000000ll
#define LEAP_COUNT (sizeof(utc_leaps) / sizeof(*utc_leaps))
#define HISTORY_COUNT (sizeof(utc_history) / sizeof(*utc_history))

static i128 rounded_ratio(i128 numerator, i64 denominator) {
    return numerator >= 0 ? (numerator + denominator / 2) / denominator
                          : -((-numerator + denominator / 2) / denominator);
}
static XTimestamp split_nanos(i128 ns) {
    i128 seconds = ns / NS_PER_SECOND;
    i64 nanos = ns % NS_PER_SECOND;
    if (nanos < 0) {
        --seconds;
        nanos += NS_PER_SECOND;
    }
    assert(seconds >= (-9223372036854775807ll - 1) &&
           seconds <= 9223372036854775807ll);
    return (XTimestamp){(i64)seconds, (u32)nanos};
}
static i128 historical_tai(unsigned i, i128 utc_ns) {
    return utc_ns + utc_history[i].offset_ns +
           rounded_ratio((utc_ns - (i128)utc_history[i].start * NS_PER_SECOND) *
                             utc_history[i].rate,
                         NS_PER_SECOND);
}
static bool utc_to_tai(i64 utc, u32 nanos, bool leap, XTimestamp *out) {
    if (utc < utc_history[0].start)
        return false;
    if (utc < utc_leaps[0].start) {
        if (leap)
            return false;
        unsigned i = HISTORY_COUNT - 1;
        while (utc < utc_history[i].start)
            --i;
        *out =
            split_nanos(historical_tai(i, (i128)utc * NS_PER_SECOND + nanos));
        return true;
    }
    unsigned low = 0, high = LEAP_COUNT;
    while (low < high) {
        unsigned mid = (low + high) / 2;
        if (utc_leaps[mid].start <= utc)
            low = mid + 1;
        else
            high = mid;
    }
    if (leap && (low == LEAP_COUNT || utc_leaps[low].start != utc + 1 ||
                 utc_leaps[low].offset != utc_leaps[low - 1].offset + 1))
        return false;
    *out = (XTimestamp){utc + utc_leaps[low - 1].offset + leap, nanos};
    return true;
}
// Enumerate distinct offsets, then validate them against the actual UTC rules.
// This handles folds of any size without assuming a one-hour DST change.
static unsigned date_candidates(XDateTime dt, XTimestamp candidates[258]) {
    if (!calendar_valid(dt))
        return 0;
    const XTimezone *zone = xtimezone(dt.tz);
    i32 offsets[258];
    unsigned offsets_count = 0, count = 0;
    assert(zone->type_count <= 256);
    for (unsigned i = 0; i < zone->type_count + 2u; ++i) {
        i32 offset;
        if (i < zone->type_count)
            offset = zone->types[i].utc_off_s;
        else {
            if (!zone->future || !zone->future->valid)
                continue;
            if (i == zone->type_count + 1u && !zone->future->has_dst)
                continue;
            offset = i == zone->type_count ? zone->future->standard.utc_off_s
                                           : zone->future->daylight.utc_off_s;
        }
        bool seen = false;
        for (unsigned j = 0; j < offsets_count; ++j)
            seen |= offsets[j] == offset;
        if (seen)
            continue;
        offsets[offsets_count++] = offset;
        const i64 utc = calendar_days(dt) * 86400 + dt.hours * 3600 +
                        dt.minutes * 60 + (dt.seconds == 60 ? 59 : dt.seconds) -
                        offset;
        if (xtimezone_at_unix(dt.tz, utc).utc_off_s != offset)
            continue;
        XTimestamp ts;
        if (!utc_to_tai(utc, dt.nanos, dt.seconds == 60, &ts))
            continue;
        unsigned pos = count;
        while (pos && (candidates[pos - 1].seconds > ts.seconds ||
                       (candidates[pos - 1].seconds == ts.seconds &&
                        candidates[pos - 1].nanos > ts.nanos))) {
            candidates[pos] = candidates[pos - 1];
            --pos;
        }
        candidates[pos] = ts;
        ++count;
    }
    return count;
}
bool xtimestamp_from_date(XDateTime dt, XTimestamp *out) {
    assert(out);
    XTimestamp candidates[258];
    const unsigned count = date_candidates(dt, candidates);
    if (dt.index >= count)
        return false;
    *out = candidates[dt.index];
    return true;
}
XDateTime xdate_from_ts(XTimestamp ts, XTimezoneId tz) {
    assert(ts.nanos < NS_PER_SECOND);
    i64 utc;
    u32 nanos = ts.nanos;
    bool leap = false;
    if (ts.seconds < utc_leaps[0].start + utc_leaps[0].offset) {
        const i128 tai = (i128)ts.seconds * NS_PER_SECOND + ts.nanos;
        assert(tai >=
               historical_tai(0, (i128)utc_history[0].start * NS_PER_SECOND));
        i128 best_error = (i128)1 << 126, best = 0;
        // Fractional steps can overlap or skip civil instants. Choose the
        // closest representable date, preferring the later date on ties.
        for (unsigned i = 0; i < HISTORY_COUNT; ++i) {
            const i128 start = (i128)utc_history[i].start * NS_PER_SECOND;
            const i128 end =
                (i128)(i + 1 < HISTORY_COUNT ? utc_history[i + 1].start
                                             : utc_leaps[0].start) *
                NS_PER_SECOND;
            i128 candidate =
                start + rounded_ratio((tai - start - utc_history[i].offset_ns) *
                                          NS_PER_SECOND,
                                      NS_PER_SECOND + utc_history[i].rate);
            if (candidate < start)
                candidate = start;
            if (candidate >= end)
                candidate = end - 1;
            i128 error = historical_tai(i, candidate) - tai;
            if (error < 0)
                error = -error;
            if (error <= best_error) {
                best_error = error;
                best = candidate;
            }
        }
        const i128 boundary = (i128)utc_leaps[0].start * NS_PER_SECOND;
        if (boundary + (i128)utc_leaps[0].offset * NS_PER_SECOND - tai <=
            best_error)
            best = boundary;
        XTimestamp civil = split_nanos(best);
        utc = civil.seconds;
        nanos = civil.nanos;
    } else {
        unsigned low = 0, high = LEAP_COUNT;
        while (low < high) {
            unsigned mid = (low + high) / 2;
            if (utc_leaps[mid].start + utc_leaps[mid].offset <= ts.seconds)
                low = mid + 1;
            else
                high = mid;
        }
        leap = low < LEAP_COUNT &&
               ts.seconds == utc_leaps[low].start + utc_leaps[low].offset - 1;
        utc = ts.seconds - utc_leaps[low - 1].offset - leap;
    }
    const i32 offset = xtimezone_at_unix(tz, utc).utc_off_s;
    const i128 local = (i128)utc + offset;
    assert(local >= (-9223372036854775807ll - 1) &&
           local <= 9223372036854775807ll);
    XDateTime dt = xdate_from_posix((XPosixTimestamp){(i64)local, nanos});
    dt.tz = tz;
    // All bundled zones use whole-minute offsets at recorded leap seconds.
    assert(!leap || offset % 60 == 0);
    if (leap)
        dt.seconds = 60;
    XTimestamp candidates[258];
    const unsigned count = date_candidates(dt, candidates);
    assert(count);
    i128 nearest = (i128)1 << 126;
    for (unsigned i = 0; i < count; ++i) {
        i128 error =
            ((i128)candidates[i].seconds - ts.seconds) * NS_PER_SECOND +
            candidates[i].nanos - ts.nanos;
        if (error < 0)
            error = -error;
        if (error < nearest) {
            assert(i <= 255);
            dt.index = i;
            nearest = error;
        }
    }
    return dt;
}
