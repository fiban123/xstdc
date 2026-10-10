#include "xstdc/assert.h"
#include "xstdc/time.h"

#include "timezones_data.inc"

const XTimezone *xtimezone(XTimezoneId id) {
    assert((unsigned)id < XTZ_COUNT);
    return &zones[id];
}
static int name_compare(XStringVw a, const char *b) {
    usize i = 0;
    while (i < a.len && b[i] && a.p[i] == b[i])
        ++i;
    if (i == a.len)
        return b[i] ? -1 : 0;
    if (!b[i])
        return 1;
    return (unsigned char)a.p[i] - (unsigned char)b[i];
}
bool xtimezone_from_name(XStringVw name, XTimezoneId *out) {
    assert(out);
    if (!name.p || !name.len)
        return false;
    usize low = 0, high = XTZ_COUNT;
    while (low < high) {
        const usize mid = low + (high - low) / 2;
        const int order = name_compare(name, zones[zone_names[mid]].name);
        if (!order) {
            *out = zone_names[mid];
            return true;
        }
        if (order < 0)
            high = mid;
        else
            low = mid + 1;
    }
    return false;
}
static i64 floor_div(i64 a, i64 b) {
    i64 q = a / b;
    return q - (a % b < 0);
}
static bool leap(i64 year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
static i64 year_start(i64 year) {
    const i64 previous = year - 1;
    const i64 leaps = floor_div(previous, 4) - floor_div(previous, 100) +
                      floor_div(previous, 400);
    return 365 * (year - 1970) + leaps - 477;
}
static i64 year_of(i64 unix_seconds) {
    const i64 days = floor_div(unix_seconds, 86400);
    i64 year = floor_div(days + 719528, 146097) * 400;
    year += (days - year_start(year)) / 366;
    while (year_start(year + 1) <= days)
        ++year;
    assert(year >= (-2147483647ll - 1) && year <= 2147483647ll);
    return year;
}
static i64 rule_time(XTimezoneRule rule, i64 year, i32 standard, i32 before) {
    static const unsigned starts[] = {0,   31,  59,  90,  120, 151,
                                      181, 212, 243, 273, 304, 334};
    static const unsigned lengths[] = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31};
    const i64 jan = year_start(year);
    i64 day;
    if (rule.kind == XTZ_MONTH_WEEKDAY) {
        day = starts[rule.month - 1] + (rule.month > 2 && leap(year));
        const i64 absolute = jan + day + 4;
        const unsigned first =
            (unsigned)(absolute - floor_div(absolute, 7) * 7);
        unsigned date =
            1 + (rule.weekday + 7 - first) % 7 + (rule.week - 1) * 7;
        const unsigned last =
            lengths[rule.month - 1] + (rule.month == 2 && leap(year));
        if (date > last)
            date -= 7;
        day += date - 1;
    } else if (rule.kind == XTZ_JULIAN_NO_LEAP) {
        day = rule.day - 1 + (rule.day >= 60 && leap(year));
    } else
        day = rule.day;
    const i32 offset = rule.basis == XTZ_UTC_TIME   ? 0
                       : rule.basis == XTZ_STANDARD ? standard
                                                    : before;
    return (jan + day) * 86400 + rule.seconds - offset;
}
static XTimezoneOffset future_offset(const XTimezoneRules *rules, i64 when) {
    if (!rules->has_dst)
        return rules->standard;
    const i64 year = year_of(when);
    i64 latest = (-9223372036854775807ll - 1);
    bool dst = false;
    // Include adjacent years for southern seasons and transition times which
    // spill outside their nominal calendar day (or year).
    for (i64 y = year - 1; y <= year + 1; ++y) {
        const i64 start = rule_time(rules->start, y, rules->standard.utc_off_s,
                                    rules->standard.utc_off_s);
        const i64 end = rule_time(rules->end, y, rules->standard.utc_off_s,
                                  rules->daylight.utc_off_s);
        if (start <= when && start >= latest) {
            latest = start;
            dst = true;
        }
        if (end <= when && end > latest) {
            latest = end;
            dst = false;
        }
    }
    return dst ? rules->daylight : rules->standard;
}
XTimezoneOffset xtimezone_at_unix(XTimezoneId id, i64 unix_seconds) {
    const XTimezone *zone = xtimezone(id);
    if (zone->future->valid &&
        (!zone->transition_count ||
         unix_seconds >
             zone->transitions[zone->transition_count - 1].unix_seconds))
        return future_offset(zone->future, unix_seconds);
    usize low = 0, high = zone->transition_count;
    while (low < high) {
        const usize mid = low + (high - low) / 2;
        if (zone->transitions[mid].unix_seconds <= unix_seconds)
            low = mid + 1;
        else
            high = mid;
    }
    return zone->types[low ? zone->transitions[low - 1].type_index : 0];
}
