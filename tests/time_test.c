#define _GNU_SOURCE
#include "xstdc/assert.h"
#include "xstdc/mem.h"
#include "xstdc/print.h"
#include "xstdc/time.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static i64 utc(int year, int month, int day, int hour) {
    struct tm tm = {.tm_year = year - 1900,
                    .tm_mon = month - 1,
                    .tm_mday = day,
                    .tm_hour = hour};
    return (i64)timegm(&tm);
}
static void compare_zone(XTimezoneId id, i64 instant) {
    time_t t = (time_t)instant;
    struct tm civil;
    if (!localtime_r(&t, &civil))
        return; // libc's struct tm may overflow for TZif sentinels.
    XTimezoneOffset got = xtimezone_at_unix(id, instant);
    if (got.utc_off_s != civil.tm_gmtoff ||
        got.is_dst != (bool)civil.tm_isdst ||
        strcmp(got.abbreviation, civil.tm_zone)) {
        xprint_err("timezone mismatch %z at %i64: got %i32/%i32/%z expected "
                   "%i64/%i32/%z\n",
                   xtimezone(id)->name, &instant, &got.utc_off_s,
                   &(i32){got.is_dst}, got.abbreviation,
                   &(i64){civil.tm_gmtoff}, &civil.tm_isdst,
                   civil.tm_zone);
        assert(false);
    }
}
static void test_zones(void) {
    XTimezoneId id = XTZ_EUROPE_ZURICH;
    assert(!xtimezone_from_name(strl("Europe/NotAZone"), &id) &&
           id == XTZ_EUROPE_ZURICH);
    assert(!xtimezone_from_name(strl("europe/zurich"), &id));
    assert(!xtimezone_from_name((XStringVw){0}, &id));
    for (unsigned i = 0; i < XTZ_COUNT; ++i) {
        const XTimezone *zone = xtimezone((XTimezoneId)i);
        assert(xtimezone_from_name(xstrv_fromz(zone->name), &id) &&
               (unsigned)id == i);
        assert(zone->type_count && zone->types && zone->future);
        assert(setenv("TZ", zone->name, 1) == 0);
        tzset();
        compare_zone(id, -2208988800ll);
        compare_zone(id, 0);
        for (usize j = 0; j < zone->transition_count; ++j) {
            const XTimezoneTransition *transition = &zone->transitions[j];
            assert(transition->type_index < zone->type_count);
            assert(!j || zone->transitions[j - 1].unix_seconds <
                             transition->unix_seconds);
            compare_zone(id, transition->unix_seconds - 1);
            compare_zone(id, transition->unix_seconds);
            compare_zone(id, transition->unix_seconds + 1);
        }
        // Exercise future rules after TZif's explicit transitions end.
        const int years[] = {2050, 2100, 2400};
        for (unsigned y = 0; y < 3; ++y)
            for (int month = 1; month <= 12; ++month)
                for (int day = 1; day <= 28; day += 9)
                    compare_zone(id, utc(years[y], month, day, 12));
        const i64 start = utc(2050, 1, 1, 0);
        for (unsigned day = 0; day < 365; ++day)
            for (unsigned hour = 0; hour < 4; ++hour)
                compare_zone(id, start + (i64)day * 86400 + hour * 3600);
    }
    assert(xtimezone(XTZ_US_EASTERN)->transitions ==
           xtimezone(XTZ_AMERICA_NEW_YORK)->transitions);
    XTimezoneOffset winter =
        xtimezone_at_unix(XTZ_EUROPE_ZURICH, utc(2050, 1, 1, 0));
    XTimezoneOffset summer =
        xtimezone_at_unix(XTZ_EUROPE_ZURICH, utc(2050, 7, 1, 0));
    assert(winter.utc_off_s == 3600 && !winter.is_dst);
    assert(summer.utc_off_s == 7200 && summer.is_dst);
    XTimezoneOffset kathmandu =
        xtimezone_at_unix(XTZ_ASIA_KATHMANDU, utc(2050, 1, 1, 0));
    assert(kathmandu.utc_off_s == 20700);
}
static void test_clocks(void) {
    struct timespec before, after;
    assert(clock_gettime(CLOCK_MONOTONIC, &before) == 0);
    u64 n = xtime_nanos();
    assert(clock_gettime(CLOCK_MONOTONIC, &after) == 0);
    assert(n >= (u64)before.tv_sec * 1000000000 + (u64)before.tv_nsec);
    assert(n <= (u64)after.tv_sec * 1000000000 + (u64)after.tv_nsec);
    assert(clock_gettime(CLOCK_TAI, &before) == 0);
    XTimestamp ts = xtimestamp();
    assert(clock_gettime(CLOCK_TAI, &after) == 0);
    assert(ts.nanos < 1000000000);
    assert(ts.seconds > before.tv_sec ||
           (ts.seconds == before.tv_sec && ts.nanos >= before.tv_nsec));
    assert(ts.seconds < after.tv_sec ||
           (ts.seconds == after.tv_sec && ts.nanos <= after.tv_nsec));
    n = xtime_nanos();
    xnanosleep(1000000);
    assert(xtime_nanos() - n >= 1000000);
    xnanosleep(0);
}
static bool same_date(XDateTime a, XDateTime b) {
    return a.year == b.year && a.month == b.month && a.day == b.day &&
           a.hours == b.hours && a.minutes == b.minutes &&
           a.seconds == b.seconds && a.nanos == b.nanos && a.tz == b.tz &&
           a.index == b.index;
}
static void roundtrip(XDateTime date) {
    XTimestamp ts;
    assert(xtimestamp_from_date(date, &ts));
    assert(same_date(date, xdate_from_ts(ts, date.tz)));
}
static void test_date_transitions(void) {
    for (unsigned id = 0; id < XTZ_COUNT; ++id) {
        const XTimezone *zone = xtimezone((XTimezoneId)id);
        for (unsigned i = 0; i < zone->transition_count; ++i) {
            i64 t = zone->transitions[i].unix_seconds;
            if (t < utc(1972, 1, 1, 0) || t > utc(2500, 1, 1, 0))
                continue;
            for (int delta = -1; delta <= 1; ++delta) {
                XDateTime utc_date =
                    xdate_from_posix((XPosixTimestamp){t + delta, 123456789});
                XTimestamp ts, back;
                assert(xtimestamp_from_date(utc_date, &ts));
                XDateTime local = xdate_from_ts(ts, (XTimezoneId)id);
                assert(xtimestamp_from_date(local, &back));
                assert(ts.seconds == back.seconds && ts.nanos == back.nanos);
            }
        }
    }
}
static void test_calendar(void) {
    XTimezoneId id;
    const char slice[] = "Europe/Zurich!";
    assert(xtimezone_from_name((XStringVw){slice, 13}, &id));
    assert(id == XTZ_EUROPE_ZURICH);
    assert(!xtimezone_from_name((XStringVw){slice, 14}, &id));
    assert(!xtimezone_from_name(strl("UTC\0extra"), &id));
    assert(!xtimezone_from_name(strl(""), &id));
    XDateTime d = xdate_from_posix((XPosixTimestamp){-1, 999999999});
    assert(d.year == 1969 && d.month == 12 && d.day == 31 && d.hours == 23 &&
           d.minutes == 59 && d.seconds == 59 && d.nanos == 999999999);
    const int years[] = {-2000, -400, -1,   0,    1,    1600,
                         1900,  1960, 1970, 2000, 2100, 2400};
    for (unsigned i = 0; i < sizeof(years) / sizeof(*years); ++i)
        for (int month = 1; month <= 12; ++month)
            for (int day = 1; day <= 28; ++day) {
                d = xdate_from_posix(
                    (XPosixTimestamp){utc(years[i], month, day, 12), 42});
                assert(d.year == years[i] && d.month == month && d.day == day &&
                       d.hours == 12);
                if (years[i] >= 1970)
                    roundtrip(d);
            }
    d = (XDateTime){.year = 2024,
                    .month = 3,
                    .day = 31,
                    .hours = 2,
                    .minutes = 30,
                    .tz = XTZ_EUROPE_ZURICH};
    XTimestamp ts = {123, 456};
    assert(!xtimestamp_from_date(d, &ts) && ts.seconds == 123 &&
           ts.nanos == 456);
    d.month = 10;
    d.day = 27;
    XTimestamp first, second;
    assert(xtimestamp_from_date(d, &first));
    roundtrip(d);
    d.index = 1;
    assert(xtimestamp_from_date(d, &second));
    roundtrip(d);
    assert(second.seconds - first.seconds == 3600);
    d.index = 2;
    assert(!xtimestamp_from_date(d, &ts));
    d = (XDateTime){.year = 2024,
                    .month = 4,
                    .day = 7,
                    .hours = 1,
                    .minutes = 45,
                    .tz = XTZ_AUSTRALIA_LORD_HOWE};
    assert(xtimestamp_from_date(d, &first));
    roundtrip(d);
    d.index = 1;
    assert(xtimestamp_from_date(d, &second));
    roundtrip(d);
    assert(second.seconds - first.seconds == 1800);
    d = (XDateTime){.year = 2011,
                    .month = 12,
                    .day = 30,
                    .hours = 12,
                    .tz = XTZ_PACIFIC_APIA};
    assert(!xtimestamp_from_date(d, &ts));
    d = (XDateTime){.year = 2024, .month = 2, .day = 30};
    assert(!xtimestamp_from_date(d, &ts));
    d.day = 29;
    roundtrip(d);
    d.year = 2100;
    assert(!xtimestamp_from_date(d, &ts));
    d.year = 2000;
    roundtrip(d);
    d.seconds = 60;
    assert(!xtimestamp_from_date(d, &ts));
    d.seconds = 0;
    d.nanos = 1000000000;
    assert(!xtimestamp_from_date(d, &ts));
    d.nanos = 0;
    d.month = 0;
    assert(!xtimestamp_from_date(d, &ts));
    d.month = 1;
    d.tz = XTZ_COUNT;
    assert(!xtimestamp_from_date(d, &ts));
    // Calendar days and elapsed days differ across DST.
    d = (XDateTime){.year = 2024,
                    .month = 4,
                    .day = 1,
                    .hours = 2,
                    .minutes = 30,
                    .tz = XTZ_EUROPE_ZURICH};
    XDateTime elapsed = d;
    xdate_sub_days(&d, 1);
    assert(d.month == 3 && d.day == 31 && d.hours == 2 &&
           !xtimestamp_from_date(d, &ts));
    xdate_sub_dur(&elapsed, (XDuration){86400, 0});
    assert(elapsed.month == 3 && elapsed.day == 31 && elapsed.hours == 1);
    d = (XDateTime){.year = 2000, .month = 3, .day = 1};
    xdate_sub_days(&d, 1);
    assert(d.month == 2 && d.day == 29);
    xdate_sub_days(&d, -1);
    assert(d.month == 3 && d.day == 1);
    ts = (XTimestamp){0, 100};
    xtimestamp_sub_dur(&ts, (XDuration){0, 200});
    assert(ts.seconds == -1 && ts.nanos == 999999900);
    xtimestamp_sub_dur(&ts, (XDuration){-1, 999999900});
    assert(ts.seconds == 0 && ts.nanos == 0);
    ts = (XTimestamp){-9223372036854775807ll - 1, 0};
    xtimestamp_sub_dur(&ts, (XDuration){-9223372036854775807ll - 1, 0});
    assert(ts.seconds == 0 && ts.nanos == 0);
}
static void test_leaps(void) {
    const int dates[][2] = {
        {1972, 7}, {1973, 1}, {1974, 1}, {1975, 1}, {1976, 1}, {1977, 1},
        {1978, 1}, {1979, 1}, {1980, 1}, {1981, 7}, {1982, 7}, {1983, 7},
        {1985, 7}, {1988, 1}, {1990, 1}, {1991, 1}, {1992, 7}, {1993, 7},
        {1994, 7}, {1996, 1}, {1997, 7}, {1999, 1}, {2006, 1}, {2009, 1},
        {2012, 7}, {2015, 7}, {2017, 1}};
    for (unsigned i = 0; i < sizeof(dates) / sizeof(*dates); ++i) {
        i64 boundary = utc(dates[i][0], dates[i][1], 1, 0);
        XDateTime d =
            xdate_from_posix((XPosixTimestamp){boundary - 1, 987654321});
        XTimestamp ts;
        assert(xtimestamp_from_date(d, &ts));
        assert(ts.seconds == boundary - 1 + 10 + i);
        d.seconds = 60;
        roundtrip(d);
        assert(xtimestamp_from_date(d, &ts));
        assert(ts.seconds == boundary + 10 + i);
        for (unsigned zone = 0; zone < XTZ_COUNT; ++zone) {
            for (int delta = -1; delta <= 1; ++delta) {
                XTimestamp t = {ts.seconds + delta, ts.nanos}, back;
                XDateTime local = xdate_from_ts(t, (XTimezoneId)zone);
                assert(xtimestamp_from_date(local, &back));
                assert(t.seconds == back.seconds && t.nanos == back.nanos);
            }
        }
        xdate_sub_dur(&d, (XDuration){-1, 0});
        assert(d.day == 1 && d.hours == 0 && d.minutes == 0 && d.seconds == 0);
    }
    XDateTime d = xdate_from_posix((XPosixTimestamp){0, 0});
    XTimestamp ts;
    assert(xtimestamp_from_date(d, &ts));
    assert(ts.seconds == 8 && ts.nanos == 82000); // 1970 TAI-UTC = 8.000082 s.
    roundtrip(d);
    d = xdate_from_posix((XPosixTimestamp){utc(1961, 1, 1, 0), 0});
    assert(xtimestamp_from_date(d, &ts));
    assert(ts.seconds == utc(1961, 1, 1, 0) + 1 && ts.nanos == 422818000);
    roundtrip(d);
    d.year = 1959;
    assert(!xtimestamp_from_date(d, &ts));
    // The 1972 step leaves a small interval with no exact UTC date.
    const i64 start1972 = utc(1972, 1, 1, 0);
    ts = (XTimestamp){start1972 + 9, 950000000};
    d = xdate_from_ts(ts, XTZ_UTC);
    XTimestamp approximate;
    assert(xtimestamp_from_date(d, &approximate));
    assert(approximate.seconds == start1972 + 10 && approximate.nanos == 0);
    // At the August 1961 backward step the later UTC representation wins.
    d = xdate_from_posix((XPosixTimestamp){utc(1961, 8, 1, 0), 0});
    roundtrip(d);
    // Gregorian endpoints are independent of libc's narrower tm_year range.
    const i32 endpoints[] = {-2147483647 - 1, 2147483647};
    for (unsigned i = 0; i < 2; ++i) {
        XDateTime endpoint = {.year = endpoints[i], .month = 6, .day = 15};
        xdate_sub_days(&endpoint, 1);
        assert(endpoint.year == endpoints[i] && endpoint.month == 6 &&
               endpoint.day == 14);
        if (i)
            roundtrip(endpoint);
    }
    // Historical rate inversion stays within one nanosecond away from steps.
    for (int year = 1960; year < 1972; ++year)
        for (int month = 1; month <= 12; ++month) {
            d = xdate_from_posix(
                (XPosixTimestamp){utc(year, month, 15, 12), 123456789});
            roundtrip(d);
            assert(xtimestamp_from_date(d, &ts));
            ++ts.nanos;
            XDateTime back = xdate_from_ts(ts, XTZ_UTC);
            XTimestamp again;
            assert(xtimestamp_from_date(back, &again));
            i128 error = ((i128)again.seconds - ts.seconds) * 1000000000 +
                         again.nanos - ts.nanos;
            assert(error >= -1 && error <= 1);
        }
}
int main(void) {
    test_date_transitions();
    test_calendar();
    test_leaps();
    test_clocks();
    test_zones();
    xprint("%z\n", "time tests passed");
}
