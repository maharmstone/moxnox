// SPDX-License-Identifier: MIT
#pragma once

#define HUGE_VAL (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())
#define HUGE_VALL (__builtin_huge_vall())
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))

#define FP_NAN 0x0100
#define FP_NORMAL 0x0400
#define FP_INFINITE (FP_NAN | FP_NORMAL)
#define FP_ZERO 0x4000
#define FP_SUBNORMAL (FP_NORMAL | FP_ZERO)

#define FP_ILOGB0 ((int)0x80000000)
#define FP_ILOGBNAN ((int)0x7fffffff)

#define isfinite(x) (__builtin_isfinite(x))
#define isinf(x) (__builtin_isinf_sign(x) != 0)
#define isnan(x) (__builtin_isnan(x))

#define isgreater(x, y) (__builtin_isgreater(x, y))
#define isgreaterequal(x, y) (__builtin_isgreaterequal(x, y))
#define isless(x, y) (__builtin_isless(x, y))
#define islessequal(x, y) (__builtin_islessequal(x, y))
#define islessgreater(x, y) (__builtin_islessgreater(x, y))
#define isunordered(x, y) (__builtin_isunordered(x, y))

#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
#define M_LN2 0.69314718055994530942
#define M_SQRT2 1.41421356237309504880
#define M_LOG2E 1.4426950408889634074

typedef float float_t;
typedef double double_t;

#define fpclassify(x) (__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x))
#define isnormal(x) (__builtin_isnormal(x))
#define signbit(x) (__builtin_signbit(x))

#ifdef __cplusplus
extern "C" {
#endif

double acos(double x);
double asin(double x);
double atan(double x);
double atan2(double y, double x);
double cos(double x);
double sin(double x);
double tan(double x);
double acosh(double x);
double asinh(double x);
double atanh(double x);
double cosh(double x);
double sinh(double x);
double tanh(double x);
double exp(double x);
double exp2(double x);
double expm1(double x);
double frexp(double value, int* exp);
double ldexp(double x, int exp);
double log(double x);
double log10(double x);
double log1p(double x);
double log2(double x);
double modf(double value, double* iptr);
double scalbn(double x, int n);
double cbrt(double x);
double fabs(double x);
double hypot(double x, double y);
double pow(double x, double y);
double sqrt(double x);
double erf(double x);
double erfc(double x);
double ceil(double x);
double floor(double x);
double round(double x);
double fmod(double x, double y);
double copysign(double x, double y);
double nextafter(double x, double y);
double fma(double x, double y, double z);
double lgamma(double x);
double fdim(double x, double y);
double fmax(double x, double y);
double fmin(double x, double y);
int ilogb(double x);
long long llrint(double x);
long long llround(double x);
double logb(double x);
long lrint(double x);
long lround(double x);
double nan(const char* tagp);
double nearbyint(double x);
double nexttoward(double x, long double y);
double remainder(double x, double y);
double remquo(double x, double y, int* quo);
double rint(double x);
double scalbln(double x, long n);
double tgamma(double x);
double trunc(double x);

float acosf(float x);
float asinf(float x);
float atanf(float x);
float atan2f(float y, float x);
float cosf(float x);
float sinf(float x);
float tanf(float x);
float acoshf(float x);
float asinhf(float x);
float atanhf(float x);
float coshf(float x);
float sinhf(float x);
float tanhf(float x);
float expf(float x);
float exp2f(float x);
float expm1f(float x);
float frexpf(float value, int* exp);
float ldexpf(float x, int exp);
float logf(float x);
float log10f(float x);
float log1pf(float x);
float log2f(float x);
float modff(float value, float* iptr);
float scalbnf(float x, int n);
float cbrtf(float x);
float fabsf(float x);
float hypotf(float x, float y);
float powf(float x, float y);
float sqrtf(float x);
float erff(float x);
float erfcf(float x);
float ceilf(float x);
float floorf(float x);
float roundf(float x);
float fmodf(float x, float y);
float copysignf(float x, float y);
float nextafterf(float x, float y);
float fmaf(float x, float y, float z);
float lgammaf(float x);
float fdimf(float x, float y);
float fmaxf(float x, float y);
float fminf(float x, float y);
int ilogbf(float x);
long long llrintf(float x);
long long llroundf(float x);
float logbf(float x);
long lrintf(float x);
long lroundf(float x);
float nanf(const char* tagp);
float nearbyintf(float x);
float nexttowardf(float x, long double y);
float remainderf(float x, float y);
float remquof(float x, float y, int* quo);
float rintf(float x);
float scalblnf(float x, long n);
float tgammaf(float x);
float truncf(float x);

long double acoshl(long double x);
long double asinhl(long double x);
long double atanhl(long double x);
long double exp2l(long double x);
long double expm1l(long double x);
long double log1pl(long double x);
long double log2l(long double x);
long double scalbnl(long double x, int n);
long double cbrtl(long double x);
long double hypotl(long double x, long double y);
long double erfl(long double x);
long double erfcl(long double x);
long double roundl(long double x);
long double copysignl(long double x, long double y);
long double nextafterl(long double x, long double y);
long double fmal(long double x, long double y, long double z);
long double lgammal(long double x);
long double fdiml(long double x, long double y);
long double fmaxl(long double x, long double y);
long double fminl(long double x, long double y);
int ilogbl(long double x);
long long llrintl(long double x);
long long llroundl(long double x);
long double logbl(long double x);
long lrintl(long double x);
long lroundl(long double x);
long double nanl(const char* tagp);
long double nearbyintl(long double x);
long double nexttowardl(long double x, long double y);
long double remainderl(long double x, long double y);
long double remquol(long double x, long double y, int* quo);
long double rintl(long double x);
long double scalblnl(long double x, long n);
long double tgammal(long double x);
long double truncl(long double x);
long double acosl(long double x);
long double asinl(long double x);
long double atan2l(long double y, long double x);
long double atanl(long double x);
long double ceill(long double x);
long double coshl(long double x);
long double cosl(long double x);
long double expl(long double x);
long double fabsl(long double x);
long double floorl(long double x);
long double fmodl(long double x, long double y);
long double frexpl(long double value, int* exp);
long double ldexpl(long double x, int exp);
long double log10l(long double x);
long double logl(long double x);
long double modfl(long double value, long double* iptr);
long double powl(long double x, long double y);
long double sinhl(long double x);
long double sinl(long double x);
long double sqrtl(long double x);
long double tanhl(long double x);
long double tanl(long double x);
double j0(double x);
double j1(double x);
double jn(int n, double x);
double y0(double x);
double y1(double x);
double yn(int n, double x);

#ifdef __cplusplus
}
#endif
