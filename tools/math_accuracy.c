// MPFR accuracy survey; see tools/math_accuracy.md for methodology and usage.
#include "xstdc/math.h"
#include "xstdc/print.h"
#include "xstdc/rng.h"
#include <fenv.h>
#include <float.h>
#include <gnu/libc-version.h>
#include <math.h>
#include <mpfr.h>
#include <stdlib.h>

typedef enum {
    SQRT,
    LN,
    LOG2,
    LOG10,
    LOGN,
    EXP,
    SIN,
    COS,
    TAN,
    GAMMA,
    POW,
    CBRT,
    NTHROOT,
    MOD,
    OP_COUNT
} Op;
static const char *names[] = {"sqrt", "ln",   "log2",    "log10", "logn",
                              "exp",  "sin",  "cos",     "tan",   "gamma",
                              "pow",  "cbrt", "nthroot", "mod"};
typedef struct {
    u64 count, exact, special_bad, zero_bad, tolerance_bad;
    double max_ulp, max_relative, x, y, actual, reference;
    double bad_x, bad_y, bad_actual, bad_reference;
} Stats;
static mpfr_t a, b, low, high, t0, t1, t2, t3, scratch, error;
static XRngXoshiro256ppState rng;
static u64 unresolved, escalations;

static bool same(double x, double y) {
    return (isnan(x) && isnan(y)) ||
           (x == y && (x != 0 || !!signbit(x) == !!signbit(y)));
}

static double evaluate(Op op, bool single, bool custom, double x, double y) {
    if (single) {
        float f = x, g = y;
        if (custom) {
            switch (op) {
            case SQRT:
                return f32sqrt(f);
            case LN:
                return f32ln(f);
            case LOG2:
                return f32log2(f);
            case LOG10:
                return f32log10(f);
            case LOGN:
                return f32logn(f, g);
            case EXP:
                return f32exp(f);
            case SIN:
                return f32sin(f);
            case COS:
                return f32cos(f);
            case TAN:
                return f32tan(f);
            case GAMMA:
                return f32gamma(f);
            case POW:
                return f32pow(f, g);
            case CBRT:
                return f32cbrt(f);
            case NTHROOT:
                return f32nthroot(f, g);
            case MOD:
                return f32mod(f, g);
            default:
                abort();
            }
        }
        switch (op) {
        case SQRT:
            return sqrtf(f);
        case LN:
            return logf(f);
        case LOG2:
            return log2f(f);
        case LOG10:
            return log10f(f);
        case LOGN:
            return logf(f) / logf(g);
        case EXP:
            return expf(f);
        case SIN:
            return sinf(f);
        case COS:
            return cosf(f);
        case TAN:
            return tanf(f);
        case GAMMA:
            return tgammaf(f);
        case POW:
            return powf(f, g);
        case CBRT:
            return cbrtf(f);
        case MOD:
            return fmodf(f, g);
        default:
            break;
        }
    } else {
        if (custom) {
            switch (op) {
            case SQRT:
                return f64sqrt(x);
            case LN:
                return f64ln(x);
            case LOG2:
                return f64log2(x);
            case LOG10:
                return f64log10(x);
            case LOGN:
                return f64logn(x, y);
            case EXP:
                return f64exp(x);
            case SIN:
                return f64sin(x);
            case COS:
                return f64cos(x);
            case TAN:
                return f64tan(x);
            case GAMMA:
                return f64gamma(x);
            case POW:
                return f64pow(x, y);
            case CBRT:
                return f64cbrt(x);
            case NTHROOT:
                return f64nthroot(x, y);
            case MOD:
                return f64mod(x, y);
            default:
                abort();
            }
        }
        switch (op) {
        case SQRT:
            return sqrt(x);
        case LN:
            return log(x);
        case LOG2:
            return log2(x);
        case LOG10:
            return log10(x);
        case LOGN:
            return log(x) / log(y);
        case EXP:
            return exp(x);
        case SIN:
            return sin(x);
        case COS:
            return cos(x);
        case TAN:
            return tan(x);
        case GAMMA:
            return tgamma(x);
        case POW:
            return pow(x, y);
        case CBRT:
            return cbrt(x);
        case MOD:
            return fmod(x, y);
        default:
            break;
        }
    }
    // No libm nthroot: use pow(|x|, 1/n), restoring the odd-root sign.
    if (!isfinite(y) || y == 0 || (x < 0 && fmod(fabs(y), 2) != 1))
        return NAN;
    bool negative = signbit(x) && fmod(fabs(y), 2) == 1;
    double r =
        single ? powf(fabsf((float)x), 1.0f / (float)y) : pow(fabs(x), 1.0 / y);
    return negative ? -r : r;
}

// Bound log(a)/log(b) using all four interval endpoints.
static void logn_bounds(void) {
    mpfr_log(t0, a, MPFR_RNDD);
    mpfr_log(t1, a, MPFR_RNDU);
    mpfr_log(t2, b, MPFR_RNDD);
    mpfr_log(t3, b, MPFR_RNDU);
    if (mpfr_nan_p(t0) || mpfr_nan_p(t2)) {
        mpfr_set_nan(low);
        mpfr_set_nan(high);
        return;
    }
    mpfr_srcptr num[] = {t0, t1}, den[] = {t2, t3};
    mpfr_set_inf(low, 1);
    mpfr_set_inf(high, -1);
    for (unsigned i = 0; i < 2; ++i)
        for (unsigned j = 0; j < 2; ++j) {
            mpfr_div(scratch, num[i], den[j], MPFR_RNDD);
            if (mpfr_less_p(scratch, low))
                mpfr_set(low, scratch, MPFR_RNDD);
            mpfr_div(scratch, num[i], den[j], MPFR_RNDU);
            if (mpfr_greater_p(scratch, high))
                mpfr_set(high, scratch, MPFR_RNDU);
        }
}

static void bounds(Op op, double x, double y) {
    mpfr_set_d(a, x, MPFR_RNDN);
    mpfr_set_d(b, y, MPFR_RNDN);
    if (op == LOGN) {
        logn_bounds();
        return;
    }
    if (op == NTHROOT) {
        if (!isfinite(y) || y == 0 || (x < 0 && fmod(fabs(y), 2) != 1)) {
            mpfr_set_nan(low);
            mpfr_set_nan(high);
            return;
        }
        // xmath explicitly defines degree two as sqrt, including signed zero.
        if (y == 2) {
            mpfr_sqrt(low, a, MPFR_RNDD);
            mpfr_sqrt(high, a, MPFR_RNDU);
            return;
        }
        if (y == trunc(y) && fabs(y) < 1000000) {
            mpfr_rootn_si(low, a, (long)y, MPFR_RNDD);
            mpfr_rootn_si(high, a, (long)y, MPFR_RNDU);
            return;
        }
        mpfr_abs(a, a, MPFR_RNDN);
        mpfr_ui_div(t0, 1, b, MPFR_RNDD);
        mpfr_ui_div(t1, 1, b, MPFR_RNDU);
        bool increasing = mpfr_cmp_ui(a, 1) >= 0;
        mpfr_pow(low, a, increasing ? t0 : t1, MPFR_RNDD);
        mpfr_pow(high, a, increasing ? t1 : t0, MPFR_RNDU);
        return; // Negative bases with fractional degrees were rejected above.
    }
    for (unsigned i = 0; i < 2; ++i) {
        mpfr_ptr r = i ? high : low;
        mpfr_rnd_t round = i ? MPFR_RNDU : MPFR_RNDD;
        switch (op) {
        case SQRT:
            mpfr_sqrt(r, a, round);
            break;
        case LN:
            mpfr_log(r, a, round);
            break;
        case LOG2:
            mpfr_log2(r, a, round);
            break;
        case LOG10:
            mpfr_log10(r, a, round);
            break;
        case EXP:
            mpfr_exp(r, a, round);
            break;
        case SIN:
            mpfr_sin(r, a, round);
            break;
        case COS:
            mpfr_cos(r, a, round);
            break;
        case TAN:
            mpfr_tan(r, a, round);
            break;
        case GAMMA:
            mpfr_gamma(r, a, round);
            break;
        case POW:
            mpfr_pow(r, a, b, round);
            break;
        case CBRT:
            mpfr_cbrt(r, a, round);
            break;
        case MOD:
            mpfr_fmod(r, a, b, round);
            break;
        default:
            abort();
        }
    }
}

static void precision(mpfr_prec_t p) {
    mpfr_ptr vars[] = {a, b, low, high, t0, t1, t2, t3, scratch, error};
    for (unsigned i = 0; i < sizeof vars / sizeof *vars; ++i)
        mpfr_set_prec(vars[i], p);
}

static void observe(Stats *s, double actual, double ref, double x, double y,
                    bool single, Op op) {
    ++s->count;
    s->exact += same(actual, ref);
    if (isnan(ref) || isinf(ref) || !isfinite(actual)) {
        if (!same(actual, ref)) {
            if (!s->special_bad) {
                s->bad_x = x;
                s->bad_y = y;
                s->bad_actual = actual;
                s->bad_reference = ref;
            }
            ++s->special_bad;
        }
        return;
    }
    s->zero_bad += actual == 0 && ref == 0 && !same(actual, ref);
    // Upper bound on absolute error against the precise interval, not rounded
    // ref.
    mpfr_set_d(scratch, actual, MPFR_RNDN);
    mpfr_sub(t0, scratch, low, MPFR_RNDA);
    mpfr_abs(t0, t0, MPFR_RNDN);
    mpfr_sub(t1, scratch, high, MPFR_RNDA);
    mpfr_abs(t1, t1, MPFR_RNDN);
    mpfr_max(error, t0, t1, MPFR_RNDU);
    int exponent = ref == 0 ? (single ? -149 : -1074)
                            : ilogb(fabs(ref)) - (single ? 23 : 52);
    if (exponent < (single ? -149 : -1074))
        exponent = single ? -149 : -1074;
    mpfr_div_2si(t0, error, exponent, MPFR_RNDU);
    double ulp = mpfr_get_d(t0, MPFR_RNDU);
    if (ulp > s->max_ulp || s->count == 1) {
        s->max_ulp = ulp;
        s->x = x;
        s->y = y;
        s->actual = actual;
        s->reference = ref;
    }
    mpfr_abs(t0, high, MPFR_RNDN);
    if (!mpfr_zero_p(t0)) {
        mpfr_div(t1, error, t0, MPFR_RNDU);
        double relative = mpfr_get_d(t1, MPFR_RNDU);
        if (relative > s->max_relative)
            s->max_relative = relative;
    }
    bool absolute = op == LN || op == LOG2 || op == LOG10 || op == LOGN ||
                    op == SIN || op == COS || op == TAN;
    if (absolute)
        mpfr_add_ui(t0, t0, 1, MPFR_RNDU);
    mpfr_mul_d(t0, t0, single ? 2e-5 : 2e-12, MPFR_RNDU);
    s->tolerance_bad += mpfr_greater_p(error, t0);
}

static void sample(Stats stats[2], Op op, bool single, double x, double y) {
    if (single) {
        x = (float)x;
        y = (float)y;
    }
    double ref = 0;
    bool certified = false;
    for (mpfr_prec_t p = 256; p <= 4096; p *= 2) {
        precision(p);
        bounds(op, x, y);
        double l =
            single ? mpfr_get_flt(low, MPFR_RNDN) : mpfr_get_d(low, MPFR_RNDN);
        double h = single ? mpfr_get_flt(high, MPFR_RNDN)
                          : mpfr_get_d(high, MPFR_RNDN);
        if (same(l, h)) {
            ref = l;
            certified = true;
            escalations += p > 256;
            break;
        }
    }
    if (!certified) {
        ++unresolved;
        xprint_err("unresolved %z(%f64, %f64)\n", names[op], &x, &y);
        return;
    }
    for (unsigned custom = 0; custom < 2; ++custom)
        observe(&stats[custom], evaluate(op, single, custom, x, y), ref, x, y,
                single, op);
}

static double uniform(double lo, double hi) {
    return xrng_f64range(xrng_xoshiro256pp_u64(&rng), lo, hi);
}
static double positive(bool single, bool subnormal) {
    int emin = single ? (subnormal ? -149 : -126) : (subnormal ? -1074 : -1022);
    int emax = single ? 127 : 1023;
    int e = emin + xrng_xoshiro256pp_u64(&rng) % (emax - emin + 1);
    double x = ldexp(uniform(1, 2), e);
    if (single)
        x = (float)x;
    return isfinite(x) ? x : (single ? FLT_MAX : DBL_MAX);
}

static void random_inputs(Op op, bool single, bool stress, double *x,
                          double *y) {
    *y = 2;
    switch (op) {
    case SQRT:
    case LN:
    case LOG2:
    case LOG10:
    case LOGN:
        *x = positive(single, stress);
        if (op == LOGN)
            *y = stress ? 1 + uniform(single ? 0x1p-23 : 0x1p-52, .1)
                        : uniform(1.25, 16);
        break;
    case EXP:
        *x = uniform(single ? (stress ? -110 : -80) : (stress ? -750 : -700),
                     single ? (stress ? 95 : 80) : (stress ? 720 : 700));
        break;
    case SIN:
    case COS:
    case TAN:
        do {
            *x = stress ? positive(single, true) : uniform(-1e6, 1e6);
            if (single)
                *x = (float)*x;
        } while (!stress && op == TAN && fabs(cos(*x)) < .01);
        if (stress && (xrng_xoshiro256pp_u64(&rng) & 1))
            *x = -*x;
        break;
    case GAMMA:
        *x = stress ? uniform(single ? -34 : -170, single ? 36 : 172)
                    : uniform(.25, 20);
        break;
    case POW:
        *x = stress ? positive(single, true) : uniform(.125, 16);
        *y = uniform(stress ? -64 : -8, stress ? 64 : 8);
        break;
    case CBRT:
    case NTHROOT:
        *x = positive(single, true);
        *y = (stress && (xrng_xoshiro256pp_u64(&rng) & 1))
                 ? uniform(.125, 64)
                 : 1 + 2 * (xrng_xoshiro256pp_u64(&rng) % 16);
        if (*y == trunc(*y) && fmod(*y, 2) == 1 &&
            (xrng_xoshiro256pp_u64(&rng) & 1))
            *x = -*x;
        if (xrng_xoshiro256pp_u64(&rng) & 1)
            *y = -*y;
        break;
    case MOD:
        *x = positive(single, true);
        *y = positive(single, true);
        if (xrng_xoshiro256pp_u64(&rng) & 1)
            *x = -*x;
        if (xrng_xoshiro256pp_u64(&rng) & 1)
            *y = -*y;
        break;
    default:
        abort();
    }
}

static void boundaries(Stats stats[2], Op op, bool single, bool stress) {
    if (stress && op == NTHROOT) {
        const double bases[] = {single ? nextafterf(1, 0) : nextafter(1, 0),
                                1,
                                single ? nextafterf(1, 2) : nextafter(1, 2),
                                single ? 0x1p-149 : 0x1p-1074,
                                single ? FLT_MIN : DBL_MIN,
                                single ? FLT_MAX : DBL_MAX,
                                .5,
                                2,
                                0x1p-100,
                                0x1p100};
        const double degrees[] = {single ? 0x1p-149 : 0x1p-1074,
                                  single ? FLT_MIN : DBL_MIN,
                                  single ? FLT_MAX : DBL_MAX,
                                  single ? 0x1p-23 : 0x1p-52,
                                  .001,
                                  .01,
                                  .1,
                                  .9,
                                  .95,
                                  .98,
                                  1.01,
                                  1.25,
                                  4,
                                  7.1,
                                  31};
        for (usize i = 0; i < sizeof bases / sizeof *bases; ++i)
            for (usize j = 0; j < sizeof degrees / sizeof *degrees; ++j) {
                sample(stats, op, single, bases[i], degrees[j]);
                sample(stats, op, single, bases[i], -degrees[j]);
            }
    }
    const double edges[] = {0.,
                            -0.,
                            1.,
                            -1.,
                            2.,
                            -2.,
                            .125,
                            16.,
                            .25,
                            20.,
                            single ? FLT_MIN : DBL_MIN,
                            single ? FLT_MAX : DBL_MAX,
                            single ? 0x1p-149 : 0x1p-1074,
                            INFINITY,
                            -INFINITY,
                            NAN};
    if (stress) {
        for (usize i = 0; i < sizeof edges / sizeof *edges; ++i) {
            if (op == LOGN)
                sample(stats, op, single, edges[i] > 0 ? edges[i] : 1, 2);
            else if (op == POW || op == NTHROOT || op == MOD) {
                const double ys[] = {-3, -2, -1, 0,        .5,        1,
                                     2,  3,  8,  INFINITY, -INFINITY, NAN};
                for (usize j = 0; j < sizeof ys / sizeof *ys; ++j)
                    sample(stats, op, single, edges[i], ys[j]);
            } else
                sample(stats, op, single, edges[i], 2);
        }
    }
    // Adjacent representable inputs around reduction/table boundaries and
    // zeros.
    for (int i = -128; i <= 128; ++i) {
        double center = 1 + (double)i / 128, y = 2;
        if (op == SIN || op == COS || op == TAN)
            center = i * 0x1.921fb54442d18p+0;
        else if (op == EXP)
            center = i * 0x1.62e42fefa39efp-1;
        else if (op == GAMMA)
            center = stress ? i / 4. : .25 + (i + 128) * (19.75 / 256);
        else if (op == CBRT || op == NTHROOT) {
            center = (double)i * i * i;
            y = 3;
        } else if (op == MOD) {
            center = i;
            y = 3;
        }
        double large = (636619 - (i + 128)) * 0x1.921fb54442d18p+0;
        if (single) {
            center = (float)center;
            large = (float)large;
        }
        double points[] = {
            single ? nextafterf(center, -INFINITY)
                   : nextafter(center, -INFINITY),
            center,
            single ? nextafterf(center, INFINITY) : nextafter(center, INFINITY),
            single ? nextafterf(large, -INFINITY) : nextafter(large, -INFINITY),
            large,
            single ? nextafterf(large, INFINITY) : nextafter(large, INFINITY)};
        const unsigned point_count =
            op == SIN || op == COS || op == TAN ? 6 : 3;
        for (unsigned j = 0; j < point_count; ++j) {
            double x = points[j];
            if (!stress) {
                if ((op == SQRT || op == LN || op == LOG2 || op == LOG10 ||
                     op == LOGN) &&
                    x < (single ? FLT_MIN : DBL_MIN))
                    continue;
                if (op == POW && (x < .125 || x > 16))
                    continue;
                if (op == GAMMA && (x < .25 || x > 20))
                    continue;
                if (op == TAN && fabs(cos(x)) < .01)
                    continue;
                if (op == EXP && fabs(x) > (single ? 80 : 700))
                    continue;
            }
            sample(stats, op, single, x, y);
        }
    }
}

int main(int argc, char **argv) {
    u64 samples = argc > 1 ? strtoull(argv[1], 0, 10) : 10000;
    if (!samples || fesetround(FE_TONEAREST))
        return 1;
    mpfr_inits2(256, a, b, low, high, t0, t1, t2, t3, scratch, error,
                (mpfr_ptr)0);
    xprint(
        "# MPFR %z; GMP %z; glibc %z; compiler %z; random samples/row %u64\n",
        mpfr_get_version(), gmp_version, gnu_get_libc_version(), __VERSION__,
        &samples);
    xprint(
        "profile\ttype\tfunction\tbackend\tcount\texact\tmax_ulp\tmax_"
        "relative\tspecial_bad\tzero_bad\ttolerance_bad\tworst_x\tworst_"
        "y\tactual\treference\tbad_x\tbad_y\tbad_actual\tbad_reference\tworst_"
        "x_bits\tworst_y_bits\tactual_bits\treference_bits\tbad_x_bits\tbad_y_"
        "bits\tbad_actual_bits\tbad_reference_bits\n");
    for (unsigned stress = 0; stress < 2; ++stress)
        for (unsigned single = 0; single < 2; ++single)
            for (Op op = 0; op < OP_COUNT; ++op) {
                Stats stats[2] = {0};
                xrng_xoshiro256pp_seed(&rng, 0xacca1234 + op + single * 100);
                for (u64 i = 0; i < samples; ++i) {
                    double x, y;
                    random_inputs(op, single, stress, &x, &y);
                    sample(stats, op, single, x, y);
                }
                boundaries(stats, op, single, stress);
                for (unsigned custom = 0; custom < 2; ++custom) {
                    Stats *s = &stats[custom];
                    xprint("%z\t%z\t%z\t%z\t%u64\t%u64\t%f64\t%f64\t%u64\t%"
                           "u64\t%u64\t%f64(r=17)\t%f64(r=17)\t%f64\t%f64\t%"
                           "f64\t%f64\t%f64\t%f64",
                           stress ? "stress" : "documented",
                           single ? "f32" : "f64", names[op],
                           custom
                               ? "xmath"
                               : (op == LOGN || op == NTHROOT ? "libm-composed"
                                                              : "libm"),
                           &s->count, &s->exact, &s->max_ulp, &s->max_relative,
                           &s->special_bad, &s->zero_bad, &s->tolerance_bad,
                           &s->x, &s->y, &s->actual, &s->reference, &s->bad_x,
                           &s->bad_y, &s->bad_actual, &s->bad_reference);
                    const double values[] = {
                        s->x,     s->y,     s->actual,     s->reference,
                        s->bad_x, s->bad_y, s->bad_actual, s->bad_reference};
                    for (unsigned i = 0; i < 8; ++i) {
                        u64 bits;
                        xmemcp(&bits, (void *)&values[i], 8);
                        xprint("\t%u64(x)", &bits);
                    }
                    xprint("\n");
                }
            }
    xprint("# unresolved %u64; samples requiring >256 bits %u64\n", &unresolved,
           &escalations);
    mpfr_clears(a, b, low, high, t0, t1, t2, t3, scratch, error, (mpfr_ptr)0);
    mpfr_free_cache();
    return unresolved ? 1 : 0;
}
