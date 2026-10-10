#pragma once

#include "stream.h"
#include "string.h"
#include "types.h"

// primitive specifiers, everything is a pointer:
// %u64 %u128, %i32, %usz ... -> pointers to ints
// %f32 %f64 -> pointers to floats
// %s -> pointer to an XString(Vw)
// %c -> char
// %z -> nul term'd string
// %b -> pointer to one byte: zero prints false, nonzero prints true
// %% -> percent

// additional attributes:
// x -> print as hex (e.g. hex bytes for strings, or hex number for an int)
// lp -> leftpad
// rp -> rightpad
// t -> truncate
// e -> extend
// r -> round for floats
//

// example: print a float, rounded to 4 digits, in the center of a block of 3s,
// whose length is at least 60
//
// %f32(r=4,lp=3,rp=3,e=60)
//
// now lets just print it with padding on the right side.
//
// %f32(r=4,rp=3,e=60)
//
// Observe that when both lp and rp are
// defined, it puts it in the center. spaces in between attributes is allowed.
// The definition of some attributes may not be compatible with others. for such
// a collision, error. e.g. a %s with a rounding attribute.
// extend and truncate can be both defined, truncate just limits the lebgth of
// the result.

// All values are passed by pointer, including %c. %z takes the character
// pointer directly. Integer types: u/i8,16,32,64,128 and usz/isz.
// %s accepts either XString* or XStringVw*. NULL arguments are errors; a valid
// string/view whose p is NULL and len is zero formats as empty.
// x is a flag: lowercase hex, no prefix; signed integers retain a minus sign.
// Hex strings encode each byte as two digits. x is invalid for floats.
// %b supports padding and truncation; x and r are invalid for booleans.
// r=N means N fractional digits (0..INT_MAX); default float formatting is %g.
// Float conversion uses exact integer arithmetic, honoring the current
// floating-point rounding mode and locale decimal separator.
// lp/rp take one byte, optionally quoted (e.g. lp=' '); quoted escapes support
// \n, \r, \t, \\, \' and \". Both sides center, with odd padding on the right.
// Without lp/rp, e pads on the left with spaces. t limits the final padded
// field, keeping its leading bytes. Widths/limits are byte counts.
// Format/size errors return (usize)-1 or {NULL,0}; buffer errors may leave a
// partial prefix. Arguments and format must not overlap the output buffer.
// Results are NOT NUL terminated. xprint_buf returns only bytes that fit;
// use xprint_sz to obtain the full required size. NULL buffer requires len=0.
// Free xprint_str results with xfree(result.p, result.len).

// print to stdout
usize xprint(const char *zstr, ...);

// print to stderr
usize xprint_err(const char *zstr, ...);

// Print to writer; return bytes accepted or (usize)-1 on failure. Failure may
// leave partial output; does not flush or close the writer.
usize xprint_writer(XWriter *writer, const char *zstr, ...);

// do not print anywhere, just calculate size.
usize xprint_sz(const char *zstr, ...);

// print to a buffer, return a view of the result.
XStringVw xprint_buf(char *buf, usize len, const char *zstr, ...);

// print to a string, return it
XString xprint_str(const char *zstr, ...);
