// Debug consumers see only xmath declarations; libm stays in this translation
// unit.
#ifndef __OPTIMIZE__
// Natural logarithm from libm.
extern float logf(float x);
// Base-two logarithm from libm.
extern float log2f(float x);
// Base-ten logarithm from libm.
extern float log10f(float x);
// Exponential from libm.
extern float expf(float x);
// Sine in radians from libm.
extern float sinf(float x);
// Cosine in radians from libm.
extern float cosf(float x);
// Tangent in radians from libm.
extern float tanf(float x);
// Gamma function from libm.
extern float tgammaf(float x);
// Power from libm.
extern float powf(float x, float y);
// Real cube root from libm.
extern float cbrtf(float x);
// Natural logarithm from libm.
extern double log(double x);
// Base-two logarithm from libm.
extern double log2(double x);
// Base-ten logarithm from libm.
extern double log10(double x);
// Exponential from libm.
extern double exp(double x);
// Sine in radians from libm.
extern double sin(double x);
// Cosine in radians from libm.
extern double cos(double x);
// Tangent in radians from libm.
extern double tan(double x);
// Gamma function from libm.
extern double tgamma(double x);
// Power from libm.
extern double pow(double x, double y);
// Real cube root from libm.
extern double cbrt(double x);
// Hyperbolic sine from libm.
extern float sinhf(float x);
// Hyperbolic cosine from libm.
extern float coshf(float x);
// Hyperbolic tangent from libm.
extern float tanhf(float x);
// Inverse hyperbolic sine from libm.
extern float asinhf(float x);
// Inverse hyperbolic cosine from libm.
extern float acoshf(float x);
// Inverse hyperbolic tangent from libm.
extern float atanhf(float x);
// Euclidean length from libm, avoiding intermediate overflow/underflow.
extern float hypotf(float x, float y);
// Hyperbolic sine from libm.
extern double sinh(double x);
// Hyperbolic cosine from libm.
extern double cosh(double x);
// Hyperbolic tangent from libm.
extern double tanh(double x);
// Inverse hyperbolic sine from libm.
extern double asinh(double x);
// Inverse hyperbolic cosine from libm.
extern double acosh(double x);
// Inverse hyperbolic tangent from libm.
extern double atanh(double x);
// Euclidean length from libm, avoiding intermediate overflow/underflow.
extern double hypot(double x, double y);
// Extended-precision power from libm.
extern long double powl(long double x, long double y);
#endif

#define XMATH_IMPLEMENTATION
#define XMATH_EXTERN_INLINE
#include "xstdc/math.h"
