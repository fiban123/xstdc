#pragma once

#include "string.h"
#include "timezones.h"
#include "types.h"

// Offsets are seconds east of UTC. Descriptors and all pointed-to data are
// immutable and owned by the library. Abbreviations are display labels only.
typedef struct {
    i32 utc_off_s;
    bool is_dst;
    const char *abbreviation;
} XTimezoneOffset;

typedef struct {
    i64 unix_seconds; // UTC POSIX seconds, as used by IANA TZif (not TAI).
    u8 type_index;
} XTimezoneTransition;

typedef enum {
    XTZ_MONTH_WEEKDAY,
    XTZ_JULIAN_NO_LEAP,
    XTZ_DAY_OF_YEAR
} XTimezoneRuleKind;
typedef enum { XTZ_WALL, XTZ_STANDARD, XTZ_UTC_TIME } XTimezoneRuleBasis;
typedef struct {
    XTimezoneRuleKind kind;
    u8 month, week,
        weekday; // M rules: month 1..12, week 1..5 (5=last), Sunday=0.
    u16 day;     // J rules: 1..365, ignoring Feb 29; day-of-year rules: 0..365.
    i32 seconds; // Transition time relative to midnight; may be outside
                 // 0..86399.
    XTimezoneRuleBasis basis;
} XTimezoneRule;
typedef struct {
    bool valid, has_dst;
    XTimezoneOffset standard, daylight;
    XTimezoneRule start, end;
} XTimezoneRules;
typedef struct {
    const char *name; // IANA spelling, e.g. "Europe/Zurich".
    const XTimezoneTransition *transitions;
    u32 transition_count;
    const XTimezoneOffset *types;
    u16 type_count;
    const XTimezoneRules *future;
} XTimezone;

extern const char xtimezone_version[];
// Return the immutable timezone descriptor for id.
const XTimezone *xtimezone(XTimezoneId id);
// Case-sensitive lookup including aliases. On failure, out is unchanged.
bool xtimezone_from_name(XStringVw name, XTimezoneId *out);
// Look up the UTC offset at POSIX seconds; leap seconds are excluded. Future-
// rule evaluation requires a Gregorian year that fits i32.
XTimezoneOffset xtimezone_at_unix(XTimezoneId id, i64 unix_seconds);

// SI seconds since 1970-01-01T00:00:00 TAI.
// Continuous atomic time; not UTC or POSIX/Unix time.
// nanos is always 0..999999999.
typedef struct {
    i64 seconds;
    u32 nanos;
} XTimestamp;

// POSIX seconds since 1970-01-01T00:00:00 UTC, excluding leap seconds.
// An inserted leap second has no distinct representation in this type.
// nanos is always 0..999999999, including when seconds is negative.
typedef struct {
    i64 seconds;
    u32 nanos;
} XPosixTimestamp;

// Signed duration: seconds + nanos/1e9, with nanos in 0..999999999.
// For example, minus half a second is {-1, 500000000}.
typedef struct {
    i64 seconds;
    u32 nanos;
} XDuration;

// DateTime which maps uniquely to a TimeStamp, or is nonexistent.
// index specifies which occurrence it is. e.g. a XDateTime might occur twice,
// index 0 specifies it is the first occurrence, index 1 specifies it is the
// second.
typedef struct {
    i32 year;
    u8 month;
    u8 day;
    u8 hours;
    u8 minutes;
    u8 seconds;
    u32 nanos;
    XTimezoneId tz;
    u8 index;
} XDateTime;

// UTC before 1960 is unsupported; 1960..1971 uses rounded rate adjustments.
// Future dates assume the last recorded TAI-UTC offset until data is updated.

// Historical TAI-to-date rounding prefers the later date on a tie; fractional
// clock steps can cause larger errors than nanosecond rounding.

// Current timestamp using Linux CLOCK_TAI. Requires an OS clock configured with
// the correct TAI-UTC offset; clock failures assert.
XTimestamp xtimestamp(void);

// Convert a POSIX timestamp to a UTC calendar date (tz=XTZ_UTC, index=0).
// Result seconds is 0..59. Asserts on invalid nanos or an out-of-range year.
XDateTime xdate_from_posix(XPosixTimestamp ts);

// Convert an existing calendar date to TAI; return false without changing out
// on invalid input. Supports leap seconds and rounded historical UTC from 1960.
bool xtimestamp_from_date(XDateTime dt, XTimestamp *out);

// Subtract an elapsed duration; asserts on invalid nanos or i64 overflow.
void xtimestamp_sub_dur(XTimestamp *ts, XDuration dur);

// Convert TAI to a calendar date; exact from 1972, nearest UTC date before
// then. Asserts before UTC began in 1960 or outside the i32 calendar range.
XDateTime xdate_from_ts(XTimestamp ts, XTimezoneId tz);

// Calendar arithmetic: preserves time, timezone and occurrence index, even if
// the resulting local date is nonexistent. Asserts on invalid input/range.
void xdate_sub_days(XDateTime *dt, i32 days);
// Elapsed TAI arithmetic, including leap seconds. Input must exist; asserts if
// the input or resulting timestamp cannot be converted.
void xdate_sub_dur(XDateTime *dt, XDuration dur);

// Monotonic nanoseconds with an unspecified origin. Excludes system suspend.
u64 xtime_nanos(void);
// Raw TSC ticks; not necessarily current CPU core cycles.
u64 xtime_cycles(void);

// Sleeps for at least the requested interval, retrying signal interruptions.
void xnanosleep(u64 nanos);
