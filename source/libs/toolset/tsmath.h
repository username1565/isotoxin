#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

#ifdef _M_AMD64
#include <emmintrin.h>
#else
#include <xmmintrin.h>
#endif

namespace ts
{
	FORWARD_DECLARE_STRING(wchar, wstr_c);

	INLINE const float fastsqrt(const float x)
	{
		//	return x * inversesqrt(max(x, limits<float>::min()));//3x times faster than _mm_sqrt_ss with accuracy of 22 bits (but compiler optimize this not so well, and sqrtss works faster)
		float r;
		_mm_store_ss(&r, _mm_sqrt_ss(_mm_load_ss(&x)));
		return r;
	}


	struct f_rect_s
	{
		float x,y,width,height;
	};

#undef M_PI
#define M_PI        3.14159265358979323846
#define M_PI_MUL(x) float((x)*M_PI)

#define TO_RAD_K (M_PI/180.0)
#define TO_GRAD_K (180.0/M_PI)

#define GRAD2RAD(a) (float((a) * TO_RAD_K))
#define RAD2GRAD(a) (float((a) * TO_GRAD_K))

#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

	INLINE double    Pi()                    { return M_PI;          }
	INLINE double    TwoPi()                 { return M_PI * 2.0f;   }

    INLINE void sincos(const float angle, float & vsin, float & vcos)
    {
        float cis, sis;
        // not lame! cool optimisation
        // for best perfomance function compile flags: /Ogtpy

        _asm
        {
            fld angle
            fsincos
            fstp cis
            fstp sis
        }

        vsin = sis;
        vcos = cis;
    }

    INLINE void sincos(const float angle, double & vsin, double & vcos)
    {
        double cis, sis;
        // not lame! cool optimisation
        // for best perfomance function compile flags: /Ogtpy

        _asm
        {
            fld angle
            fsincos
            fstp cis
            fstp sis
        }

        vsin = sis;
        vcos = cis;
    }

    INLINE void sincos_r(const float angle, const float r, float & vsin, float & vcos)
    {
        float cis, sis;
        // not lame! cool optimisation
        // for best perfomance function compile flags: /Ogtpy

        _asm
        {
            fld angle
            fsincos
            fmul r
            fstp cis
            fmul r
            fstp sis
        }

        vsin = sis;
        vcos = cis;
    }

#include "tsvec.h"

#pragma warning ( push )
#pragma warning (disable: 4201) // nonstandard extension used : nameless struct/union
	class mat22
	{
	public:

		union
		{
			struct
			{
				float e11;
				float e12;
				float e21;
				float e22;
			};

			float e[2][2];
		};

		mat22(float _e11, float _e12, float _e21, float _e22) : e11(_e11), e12(_e12), e21(_e21), e22(_e22) {}
		mat22()	{}

		mat22(float angle)
		{
			float c = cos(angle);
			float s = sin(angle);

			e11 = c; e12 = s;
			e21 =-s; e22 = c;
		}

		float  operator()(int i, int j) const { return e[i][j]; }
		float& operator()(int i, int j)       { return e[i][j]; }


		const vec2& operator[](int i) const
		{
			return reinterpret_cast<const vec2&>(e[i][0]);
		}

		vec2& operator[](int i)
		{
			return reinterpret_cast<vec2&>(e[i][0]);
		}		

		static mat22 Identity()
		{
			static const mat22 T(1.0f, 0.0f, 0.0f, 1.0f);

			return T;
		}

		static mat22 Zer0()
		{
			static const mat22 T(0.0f, 0.0f, 0.0f, 0.0f);

			return T;
		}


		mat22 Tranpose() const
		{
			mat22 T;

			T.e11 = e11;
			T.e21 = e12;
			T.e12 = e21;
			T.e22 = e22;

			return T;
		}

		mat22 operator * (const mat22& M) const 
		{
			mat22 T;

			T.e11 = e11 * M.e11 + e12 * M.e21;
			T.e21 = e21 * M.e11 + e22 * M.e21;
			T.e12 = e11 * M.e12 + e12 * M.e22;
			T.e22 = e21 * M.e12 + e22 * M.e22;

			return T;
		}

		mat22 operator ^ (const mat22& M) const 
		{
			mat22 T;

			T.e11 = e11 * M.e11 + e12 * M.e12;
			T.e21 = e21 * M.e11 + e22 * M.e12;
			T.e12 = e11 * M.e21 + e12 * M.e22;
			T.e22 = e21 * M.e21 + e22 * M.e22;

			return T;
		}

		inline mat22 operator * ( float s ) const
		{
			mat22 T;

			T.e11 = e11 * s;
			T.e21 = e21 * s;
			T.e12 = e12 * s;
			T.e22 = e22 * s;

			return T;
		}

	};

#pragma warning ( pop )

    INLINE vec2 operator * (const vec2 &P, const mat22& M)
    {
        vec2 T;
        T.x = P.x * M.e11 + P.y * M.e12;
        T.y = P.x * M.e21 + P.y * M.e22;
        return T;
    }

    INLINE vec2 operator ^ (const vec2 &P, const mat22& M)
    {
        vec2 T;
        T.x = P.x * M.e11 + P.y * M.e21;
        T.y = P.x * M.e12 + P.y * M.e22;
        return T;
    }

    INLINE bool point_rite(const vec2 &s, const vec2 &e, const vec2 &p)
    {
        return (bool) (((e - s).cross(p - s)) <= 0.0f);
    }

	struct irect
	{
		ivec2 lt;
		ivec2 rb;

		irect() {}
		explicit irect(decltype(ivec2::x) s) :lt(s), rb(s) {}
		irect(const irect &ir):lt(ir.lt), rb(ir.rb) {}
		template<typename T1, typename T2> irect(const vec_t<T1,2> &lt_, const vec_t<T2,2> &rb_):lt(lt_.x,lt_.y), rb(rb_.x, rb_.y) {}
		irect(decltype(ivec2::x) x1, decltype(ivec2::y) y1, decltype(ivec2::x) x2, decltype(ivec2::y) y2) :lt(x1, y1), rb(x2, y2) {}

		auto width() const -> decltype(rb.x - lt.x) {return rb.x - lt.x;} 
		auto height() const -> decltype(rb.y - lt.y) {return rb.y - lt.y;};
		irect &setheight(int h) {rb.y = lt.y + h; return *this;}
		irect &setwidth(int w) {rb.x = lt.x + w; return *this;}

		ivec2 size() const {return ivec2( width(), height() );}

        ivec2 rt() const {return ivec2( rb.x, lt.y );}
        ivec2 lb() const {return ivec2( lt.x, rb.y );}

        ivec2 center() const { return ivec2((lt.x + rb.x) / 2, (lt.y + rb.y) / 2); }

        int area() const { return width() * height(); }
        bool zero_area() const {return lt.x >= rb.x || lt.y  >= rb.y; }

		bool inside(ivec2 p) const
		{
			return p.x >= lt.x && p.x < rb.x && p.y >= lt.y && p.y < rb.y;
		}

		irect & make_empty()
		{
			lt.x = maximum< decltype(lt.x) >::value;
			lt.y = maximum< decltype(lt.y) >::value;
			rb.x = minimum< decltype(rb.x) >::value;
			rb.y = minimum< decltype(rb.y) >::value;
            return *this;
		}

        int intersect_area(const irect &i);
		irect & intersect(const irect &i);
		irect & combine(const irect &i);

        ts::irect szrect() const { return ts::irect(ts::ivec2(0), size()); } // just size

        bool operator == (const irect &r) const {return lt == r.lt && rb == r.rb;}
        bool operator != (const irect &r) const {return lt != r.lt || rb != r.rb;}

        irect & operator +=(const ivec2& p)
        {
            lt += p;
            rb += p;
            return *this;
        }
        irect & operator -=(const ivec2& p)
        {
            lt -= p;
            rb -= p;
            return *this;
        }
	};
    INLINE irect operator+(const irect &r, const ivec2 &p) {return irect( r.lt + p, r.rb + p );}
    INLINE irect operator-(const irect &r, const ivec2 &p) {return irect( r.lt - p, r.rb - p );}

    template<typename STRTYPE> INLINE streamstr<STRTYPE> & operator<<(streamstr<STRTYPE> &dl, const irect &r)
    {
        dl.raw_append("rec[");
        dl << r.lt << "," << r.rb << "=" << r.size();
        return dl << "]";
    }


	struct doubleRect {
		double x0,x1,x2,x3,y0,y1,y2,y3;
	};

	INLINE void getUVfromRect(float & tx/*in - out*/, float & ty/*in - out*/, const doubleRect & rect/*out*/)
	{
		double B11 = (rect.x2 - rect.x3 - rect.x1 + rect.x0);
		double B12 = -(rect.y2 - rect.y3 - rect.y1 + rect.y0);
		double B13 = (rect.x3 - rect.x0) * (rect.y0 - rect.y1) - (rect.x0 - rect.x1) * (rect.y3 - rect.y0);
		double C11 = (rect.x3 - rect.x0);
		double C12 =  -(rect.y3 - rect.y0);
		//double B13 = C11 * (rect.y0 - rect.y1) + C12 * (rect.x0 - rect.x1);

		double B21 = (rect.x3 - rect.x0 - rect.x2 + rect.x1);
		double B22 = - (rect.y3 - rect.y0 - rect.y2 + rect.y1);
		double B23 = (rect.x0 - rect.x1) * (rect.y1 - rect.y2) - (rect.x1 - rect.x2) * (rect.y0 - rect.y1);
		double C21 = (rect.x0 - rect.x1);
		double C22 = - (rect.y0 - rect.y1);

		float xx = tx;
		float yy = ty;

		double dxu = xx - rect.x0;
		double dyu = yy - rect.y0;
		double dxv = xx - rect.x1;
		double dyv = yy - rect.y1;

		double B = B11 * dyu + B12 * dxu + B13;
		double C = C11 * dyu + C12 * dxu;

		tx = float (-C/B);

		B = B21 * dyv + B22 * dxv + B23;
		//B = B21 * dyv - B12 * dxv + B23;
		//B = - B11 * dyv - B12 * dxv + B23;
		C = C21 * dyv + C22 * dxv;

		ty = float (-C/B);
	}

typedef float   mat44[4][4];

#pragma warning(push)
#pragma warning(disable:4127)
template < typename T1, typename T2 > INLINE T1 CLAMP(T2 b)
{
	if (!is_signed<T2>::value) return b > maximum<T1>::value ? maximum<T1>::value : (T1)b;
	return b < 0 ? 0 : (b > maximum<T1>::value ? maximum<T1>::value : (T1)b);
}
#pragma warning(pop)

template < typename T1, typename T2, typename T3 >
INLINE T1 CLAMP ( const T1 & a, const T2 & vmin, const T3 & vmax )
{
    return (T1)(((a) > (vmax)) ? (vmax): (((a) < (vmin)) ? (vmin) : (a)));
}

template < typename T1, typename T2, typename T3 >
INLINE bool INSIDE ( const T1 & a, const T2 & vmin, const T3 & vmax )
{
    return (((a) >= (vmin)) && ((a) <= (vmax)));
}

template < typename T1, typename T2, typename T3, typename T4 >
INLINE bool NOTWITHIN ( const T1 & start, const T2 & end, const T3 & vmin, const T4 & vmax )
{
    return ((((start) < (vmin)) && ((end) < (vmin))) || (((start) > (vmax)) && ((end) > (vmax))));
}

template < typename T1, typename T2, typename T3, typename T4 >
INLINE T1 BETWEEN ( const T1 & a, const T2 & b, const T3 & dist, const T4 & pos )
{
    return ((a) + ((pos) * ((b) - (a))) / (dist));
}

template < typename T1 >
INLINE int SIGN ( const T1 & a )
{
    return (((a) == T1(0)) ? 0 : (((a) < T1(0)) ? -1 : 1));
}

INLINE int ROLLING(int v, int vmax)
{
    return  (vmax + (v % vmax)) % vmax;
}

INLINE float ROLL_FLOAT(float v, float vmax)
{
    return fmod((vmax + fmod(v, vmax)), vmax);
}

INLINE bool check_aspect_ratio(int width,int height, int x_ones, int y_ones) 
{
    bool ost = 0 == ((width % x_ones) + (height % y_ones));
    bool divs = (width / x_ones) == (height / y_ones);
    return ( ost && divs );
}

ivec2 TSCALL calc_best_coverage( int width,int height, int x_ones, int y_ones );

/*
INLINE int CeilPowerOfTwo(int x) 
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    ++x;
    return x;
}
*/

INLINE bool IsPowerOfTwo(const int x)    { return (x & (x - 1)) == 0 && x > 0; }

#define ISWAP(a,b) {a^=b^=a^=b;}
#define LERPFLOAT(a, b, t) ( (a) * (1.0f  - (t) ) + (t) * (b) )
#define LERPDOUBLE(a, b, t) ( double(a) * (1.0  - double(t) ) + double(t) * double(b) )
#define DOUBLESCALEADD(a, b, s0, s1) (a * s0 + b * s1)
#define HAS_ALL_BITS(code, bits) (((code) & (bits)) == (bits))
#define BIN__N(x) (x) | x>>3 | x>>6 | x>>9
#define BIN__B(x) (x) & 0xf | (x)>>12 & 0xf0
#define BIN8(v) (BIN__B(BIN__N(0x##v)))

#define BIN16(x1,x2) \
    ((BIN8(x1)<<8)+BIN8(x2))

#define BIN24(x1,x2,x3) \
    ((BIN8(x1)<<16)+(BIN8(x2)<<8)+BIN8(x3))

#define BIN32(x1,x2,x3,x4) \
    ((BIN8(x1)<<24)+(BIN8(x2)<<16)+(BIN8(x3)<<8)+BIN8(x4))

#define BIN64(x1,x2,x3,x4,x5,x6,x7,x8) \
    ((__int64(BIN32(x1,x2,x3,x4)) << 32) + __int64(BIN32(x5,x6,x7,x8)))

template<class T> T SQUARE(const T& value) { return value * value; }

double erf(double x);

INLINE uint32 GetThumbSize(const uint32 maxtexw, const uint32 maxw, const uint32 count)
{
    uint32 rez = maxtexw;
    while(rez * count > maxw) rez >>= 1;
    return rez;    
}

#define IEEE_FLT_MANTISSA_BITS	23
#define IEEE_FLT_EXPONENT_BITS	8
#define IEEE_FLT_EXPONENT_BIAS	127
#define IEEE_FLT_SIGN_BIT		31
INLINE int ilog2(float f) 
{
    return (((*reinterpret_cast<int *>(&f)) >> IEEE_FLT_MANTISSA_BITS) & ((1 << IEEE_FLT_EXPONENT_BITS) - 1)) - IEEE_FLT_EXPONENT_BIAS;
}

template <uint v> struct __LOG2_template { static uint __log2(); };
template <> struct __LOG2_template<SETBIT(0)> { static uint __log2() {return 0;}; };
template <> struct __LOG2_template<SETBIT(1)> { static uint __log2() {return 1;}; };
template <> struct __LOG2_template<SETBIT(2)> { static uint __log2() {return 2;}; };
template <> struct __LOG2_template<SETBIT(3)> { static uint __log2() {return 3;}; };
template <> struct __LOG2_template<SETBIT(4)> { static uint __log2() {return 4;}; };
template <> struct __LOG2_template<SETBIT(5)> { static uint __log2() {return 5;}; };
template <> struct __LOG2_template<SETBIT(6)> { static uint __log2() {return 6;}; };
template <> struct __LOG2_template<SETBIT(7)> { static uint __log2() {return 7;}; };
template <> struct __LOG2_template<SETBIT(8)> { static uint __log2() {return 8;}; };
template <> struct __LOG2_template<SETBIT(9)> { static uint __log2() {return 9;}; };
template <> struct __LOG2_template<SETBIT(10)> { static uint __log2() {return 10;}; };
template <> struct __LOG2_template<SETBIT(11)> { static uint __log2() {return 11;}; };
template <> struct __LOG2_template<SETBIT(12)> { static uint __log2() {return 12;}; };
template <> struct __LOG2_template<SETBIT(13)> { static uint __log2() {return 13;}; };
template <> struct __LOG2_template<SETBIT(14)> { static uint __log2() {return 14;}; };
template <> struct __LOG2_template<SETBIT(15)> { static uint __log2() {return 15;}; };
template <> struct __LOG2_template<SETBIT(16)> { static uint __log2() {return 16;}; };
template <> struct __LOG2_template<SETBIT(17)> { static uint __log2() {return 17;}; };
template <> struct __LOG2_template<SETBIT(18)> { static uint __log2() {return 18;}; };
template <> struct __LOG2_template<SETBIT(19)> { static uint __log2() {return 19;}; };
template <> struct __LOG2_template<SETBIT(20)> { static uint __log2() {return 20;}; };
template <> struct __LOG2_template<SETBIT(21)> { static uint __log2() {return 21;}; };
template <> struct __LOG2_template<SETBIT(22)> { static uint __log2() {return 22;}; };
template <> struct __LOG2_template<SETBIT(23)> { static uint __log2() {return 23;}; };
template <> struct __LOG2_template<SETBIT(24)> { static uint __log2() {return 24;}; };
template <> struct __LOG2_template<SETBIT(25)> { static uint __log2() {return 25;}; };
template <> struct __LOG2_template<SETBIT(26)> { static uint __log2() {return 26;}; };
template <> struct __LOG2_template<SETBIT(27)> { static uint __log2() {return 27;}; };
template <> struct __LOG2_template<SETBIT(28)> { static uint __log2() {return 28;}; };
template <> struct __LOG2_template<SETBIT(29)> { static uint __log2() {return 29;}; };
template <> struct __LOG2_template<SETBIT(30)> { static uint __log2() {return 30;}; };
template <> struct __LOG2_template<SETBIT(31)> { static uint __log2() {return 31;}; };



#define LOG2(x) __LOG2_template<x>::__log2()

// NVidia stuff
#define FP_ONE_BITS 0x3F800000
// r = 1/p
#define FP_INV(r,p)                                                          \
{                                                                            \
    int _i = 2 * FP_ONE_BITS - *(int *)&(p);                                 \
    r = *(float *)&_i;                                                       \
    r = r * (2.0f - (p) * r);                                                \
}

INLINE unsigned char FP_NORM_TO_BYTE2(float p)                                                 
{                                                                            
  float fpTmp = p + 1.0f;                                                      
  return (uint8)(((*(unsigned long*)&fpTmp) >> 15) & 0xFF);
}

INLINE unsigned char FP_NORM_TO_BYTE3(float p)     
{
  float ftmp = p + 12582912.0f;                                                      
  return (uint8)((*(unsigned long *)&ftmp) & 0xFF);
}

INLINE auint DetermineGreaterPowerOfTwo( auint val )
{
    uint num = 1;
    while (val > num)
    {
        num <<= 1;
    }

    return num;
}

INLINE int float_into_int( float x )
{
    return *(int *)&x;
}

INLINE float int_into_float( int x )
{
    return *(float *)&x;
}

INLINE aint TruncFloat( float x )
{
    return aint(x);
}
INLINE aint TruncDouble( double x )
{
    return aint(x);
}

#pragma warning (push)
#pragma warning (disable: 4035)

INLINE aint RoundToInt(float x) 
{
    //union {
    //    float f;
    //    int i;
    //} u = {x + 12582912.0f};		// 2^22+2^23

    //return (int)u.i - 0x4B400000;

    return _mm_cvtss_si32(_mm_load_ss(&x));
}

 INLINE int RoundToIntFullRange(double x) 
 {
     union {
         double f;
         int i[2];
     } u = {x + 6755399441055744.0f};		// 2^51+2^52
 
     return (int)u.i[0];
 }

#if 0
INLINE aint Float2Int( float x )
{
    //return (int)( x + ( (x >= 0) ? 0.5f : (-0.5f)) );
    //return floor( x + 0.5f );
    return RoundToInt( x );
}


INLINE aint Double2Int( double x )
{
    _asm 
    { 
        fld x
        push eax
        fistp dword ptr [esp]
        pop eax
    }
    //return (int)( x + ( (x >= 0) ? 0.5 : (-0.5)) );
    //return floor( x + 0.5f );
 
    //return RoundToIntFullRange( x );
 }
#endif
 
INLINE long int lround(double x)
{
#ifdef _M_AMD64
	return _mm_cvtsd_si32(_mm_load_sd(&x));
#else
	_asm
	{
		fld x
		push eax
		fistp dword ptr[esp]
		pop eax
	}
#endif
}

INLINE long int lround(float x)
{
	return _mm_cvtss_si32(_mm_load_ss(&x));//assume that current rounding mode is always correct (i.e. round to nearest)
}

//Fast floor and ceil ("Fast Rounding of Floating Point Numbers in C/C++ on Wintel Platform" - http://ldesoras.free.fr/doc/articles/rounding_en.pdf)
//valid input: |x| < 2^30, wrong results: lfloor(-1e-8) = lceil(1e-8) = 0
INLINE long int lfloor(float x) //http://www.masm32.com/board/index.php?topic=9515.0
{
	x = x + x - 0.5f;
	return _mm_cvtss_si32(_mm_load_ss(&x)) >> 1;
}
INLINE long int lceil(float x) //http://www.masm32.com/board/index.php?topic=9514.0
{
	x = -0.5f - (x + x);
	return -(_mm_cvtss_si32(_mm_load_ss(&x)) >> 1);
}


INLINE TSCOLOR LERPCOLOR(TSCOLOR c0, TSCOLOR c1, float t)
{
	TSCOLOR c = 0;

	c |= 0xFF & lround((0xFF & c0) + (int(0xFF & c1) - int(0xFF & c0)) * t);

    c0 >>= 8; c1 >>= 8;
	c |= (0xFF & lround((0xFF & c0) + (int(0xFF & c1) - int(0xFF & c0)) * t)) << 8;

    c0 >>= 8; c1 >>= 8;
	c |= (0xFF & lround((0xFF & c0) + (int(0xFF & c1) - int(0xFF & c0)) * t)) << 16;

    c0 >>= 8; c1 >>= 8;
	c |= (0xFF & lround((0xFF & c0) + (int(0xFF & c1) - int(0xFF & c0)) * t)) << 24;
    return c;
}

vec3 TSCALL RGB_to_HSL(const vec3& rgb);
vec3 TSCALL HSL_to_RGB(const vec3& c);

vec3 TSCALL RGB_to_HSV(const vec3& rgb);
vec3 TSCALL HSV_to_RGB(const vec3& c);

#pragma warning (pop)

#define SIN_TABLE_SIZE  256
#define FTOIBIAS        12582912.f

namespace ts_math
{
	struct sincostable
	{
		static float table[SIN_TABLE_SIZE];

		sincostable()
		{
			// init sincostable
			for( int i = 0; i < SIN_TABLE_SIZE; i++ )
			{
				table[i] = (float)sin(i * 2.0 * M_PI / SIN_TABLE_SIZE);
			}
		}

	};
}
INLINE float TableCos( float theta )
{
    union
    {
        int i;
        float f;
    } ftmp;

    // ideally, the following should compile down to: theta * constant + constant, changing any of these constants from defines sometimes fubars this.
    ftmp.f = theta * ( float )( SIN_TABLE_SIZE / ( 2.0f * M_PI ) ) + ( FTOIBIAS + ( SIN_TABLE_SIZE / 4 ) );
    return ts_math::sincostable::table[ ftmp.i & ( SIN_TABLE_SIZE - 1 ) ];
}

INLINE float TableSin( float theta )
{
    union
    {
        int i;
        float f;
    } ftmp;

    // ideally, the following should compile down to: theta * constant + constant
    ftmp.f = theta * ( float )( SIN_TABLE_SIZE / ( 2.0f * M_PI ) ) + FTOIBIAS;
    return ts_math::sincostable::table[ ftmp.i & ( SIN_TABLE_SIZE - 1 ) ];
}


template <class NUM> INLINE NUM AngleNorm(NUM a) // ����������� ����. ��������� �� -pi �� +pi
{
    //return fmod(a, NUM(2.0*M_PI));
    while(a>M_PI) a -= NUM(2.0*M_PI);
    while(a<=-M_PI) a += NUM(2.0*M_PI);
    return a;
}

template <class NUM> INLINE NUM AngleDist(NUM from,NUM to) // ��������� ����� ������. ��������� �� -pi �� +pi
{
    while (from < 0.0) from += NUM(2.0*M_PI);
    while (to < 0.0) to += NUM(2.0*M_PI);

    NUM r=to-from;

    if(from < M_PI)
    {
        if(r > M_PI) r -= NUM(2.0*M_PI);
    } else
    {
        if(r < -M_PI) r += NUM(2.0*M_PI);
    }

    return r;
}

INLINE bool point_line_side( const vec2 &p0, const vec2 &p1, const vec2 &p )
{
    vec2 dp = p1-p0;
    vec2 ddp = p-p0;
    return (ddp.x * dp.y - ddp.y * dp.x) >= 0;
}

INLINE auint isqr(aint x)
{
	return (auint)(x * x);
}

template<typename T> INLINE T isign(T x)
{
    typedef sztype<sizeof(T)>::type unsignedT;
	const unsigned bshift = (sizeof(T) * 8 - 1);
	return (T)((x >> bshift) | (unsignedT(-x) >> bshift));
}
 
INLINE int slround(int v1, int v2)
{
    int v3 = v1 / v2;
    if (v3 * v2 == v1)
    {
        return v1;
    }
    return v3 * v2 + v2;
}

#define NEG_FLOAT(x) (*((ts::uint32 *)&(x))) ^= 0x80000000;
#define MAKE_ABS_FLOAT(x) (*((ts::uint32 *)&(x))) &= 0x7FFFFFFF;
#define MAKE_ABS_DOUBLE(x) (*(((ts::uint32 *)&(x)) + 1)) &= 0x7FFFFFFF;
#define COPY_SIGN_FLOAT(to, from) { (*((ts::uint32 *)&(to))) = ((*((ts::uint32 *)&(to)))&0x7FFFFFFF) | ((*((ts::uint32 *)&(from))) & 0x80000000); }
#define SET_SIGN_FLOAT(to, sign) {*((ts::uint32 *)&(to)) = ((*((ts::uint32 *)&(to)))&0x7FFFFFFF) | ((ts::uint32(sign)) << 31);}
#define GET_SIGN_FLOAT(x) (((*((ts::uint32 *)&(x))) & 0x80000000) != 0)

template < typename T1 > INLINE T1 tabs(const T1 &x)
{
    return x >= 0 ? x : (-x);
}


template < typename T1, typename T2 > INLINE T1 absmin(const T1 & v1, const T2 & v2)
{
    return (tabs(v1) < tabs(v2)) ? v1 : v2;
}
template < typename T1, typename T2 > INLINE T1 absmax(const T1 & v1, const T2 & v2)
{
    return (tabs(v1) > tabs(v2)) ? v1 : v2;
}

/// Min, 2 arguments
template < typename T1, typename T2 > INLINE T1 tmin ( const T1 & v1, const T2 & v2 )
{
    return ( v1 < v2 ) ? v1 : v2;
}

template <> INLINE float tmin<float, float>(const float &x, const float &y)//������������ ������������� �������, �.�. ������ ������������� ������� � float ���������� ���, ��� �� ����, ����. ��� min(int, short)
{
    float r;
    _mm_store_ss(&r, _mm_min_ss(_mm_load_ss(&x), _mm_load_ss(&y)));
    return r;
}

/// Min, 3 arguments
template < typename T1, typename T2, typename T3 > INLINE T1 tmin ( const T1 & v1, const T2 & v2, const T3 & v3 )
{
    return tmin ( tmin ( v1, v2 ), v3 );
}

/// Min, 4 arguments
template < typename T1, typename T2, typename T3, typename T4 > INLINE T1 tmin ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4 )
{
    return tmin ( tmin ( v1, v2 ), tmin ( v3, v4 ) );
}

/// Min, 5 arguments
template < typename T1, typename T2, typename T3, typename T4, typename T5 > INLINE T1 tmin ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4, const T5 & v5 )
{
    return tmin ( tmin ( v1, v2 ), tmin ( v3, v4 ), v5 );
}

/// Min, 6 arguments
template < typename T1, typename T2, typename T3, typename T4, typename T5, typename T6 > INLINE T2 tmin ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4, const T5 & v5, const T6 & v6 )
{
    return tmin ( tmin ( v1, v2 ), tmin ( v3, v4 ), tmin ( v5, v6 ) );
}

template <typename T, int N> INLINE T tmin(const vec_t<T, N> &v)
{
    T r = v[0];
    for (int i = 1; i < N; i++) r = tmin(r, v[i]);
    return r;
}
template <typename T, int N> INLINE vec_t<T, N> tmin(const vec_t<T, N> &x, const vec_t<T, N> &y)
{
    vec_t<T, N> r;
    for (int i = 0; i < N; i++) r[i] = tmin(x[i], y[i]);
    return r;
}

/// Max, 2 arguments
template < typename T1, typename T2 > INLINE T1 tmax ( const T1 & v1, const T2 & v2 )
{
    return ( v1 > v2 ) ? v1 : v2;
}

template <> INLINE float tmax<float, float>(const float &x, const float &y)
{
    float r;
    _mm_store_ss(&r, _mm_max_ss(_mm_load_ss(&x), _mm_load_ss(&y)));
    return r;
}

/// Max, 3 arguments
template < typename T1, typename T2, typename T3 > INLINE T1 tmax ( const T1 & v1, const T2 & v2, const T3 & v3 )
{
    return tmax ( tmax (v1, v2), v3 );
}

/// Max, 4 arguments
template < typename T1, typename T2, typename T3, typename T4 > INLINE T1 tmax ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4 )
{
    return tmax ( tmax ( v1, v2 ), tmax ( v3, v4 ) );
}

/// Max, 5 arguments
template < typename T1, typename T2, typename T3, typename T4, typename T5 > INLINE T1 tmax ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4, const T5 & v5 )
{
    return tmax ( tmax ( v1, v2 ), tmax ( v3, v4 ), v5 );
}

/// Max, 6 arguments
template < typename T1, typename T2, typename T3, typename T4, typename T5, typename T6 > INLINE T1 tmax ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4, const T5 & v5, const T6 & v6 )
{
    return tmax ( tmax ( v1, v2 ), tmax ( v3, v4 ), tmax ( v5, v6 ) );
}

template <typename T, int N> INLINE T tmax(const vec_t<T, N> &v)
{
    T r = v[0];
    for (int i = 1; i < N; i++) r = tmax(r, v[i]);
    return r;
}

template <typename T, int N> INLINE const vec_t<T, N> tmax(const vec_t<T, N> &x, const vec_t<T, N> &y)
{
    vec_t<T, N> r;
    for (int i = 0; i < N; i++) r[i] = tmax(x[i], y[i]);
    return r;
}

INLINE int irect::intersect_area(const irect &i)
{
    if (lt.x >= i.rb.x) return 0;
    if (lt.y >= i.rb.y) return 0;
    if (rb.x <= i.lt.x) return 0;
    if (rb.y <= i.lt.y) return 0;

    return irect( tmax(lt.x, i.lt.x), tmax(lt.y, i.lt.y), tmin(rb.x, i.rb.x), tmin(rb.y, i.rb.y) ).area();
}

INLINE irect & irect::intersect(const irect &i)
{
	if (lt.x >= i.rb.x) return make_empty();
	if (lt.y >= i.rb.y) return make_empty();
	if (rb.x <= i.lt.x) return make_empty();
	if (rb.y <= i.lt.y) return make_empty();

	lt.x = tmax(lt.x, i.lt.x);
	lt.y = tmax(lt.y, i.lt.y);

	rb.x = tmin(rb.x, i.rb.x);
	rb.y = tmin(rb.y, i.rb.y);

	return *this;
}

INLINE irect & irect::combine(const irect &i)
{
	if (i.lt.x < lt.x) lt.x = i.lt.x;
	if (i.lt.y < lt.y) lt.y = i.lt.y;
	if (i.rb.x > rb.x) rb.x = i.rb.x;
	if (i.rb.y > rb.y) rb.y = i.rb.y;

	return *this;
}



template <typename V = float> class time_value_c
{
public:
    struct key_s
    {
        V       val;
        float   time;
    };
private:
    key_s *m_keys;
    int    m_keys_count;

    void destroy()
    {
        for (int i=0;i<m_keys_count;++i)
        {
            m_keys[i].~key_s();
        }
    }
    
public:
    time_value_c( const time_value_c &v ):m_keys(nullptr), m_keys_count(0)
    {
        memcpy( get_keys( v.get_keys_count() ), v.get_keys(), v.get_keys_count() * sizeof(key_s) );
    }
    time_value_c():m_keys(nullptr), m_keys_count(0) {}
    ~time_value_c()
    {
        destroy();
        if ( m_keys ) MM_FREE( m_keys );
    }

    time_value_c & operator=( const time_value_c &v )
    {
        memcpy( get_keys( v.get_keys_count() ), v.get_keys(), v.get_keys_count() * sizeof(key_s) );
        return *this;
    }


    int get_keys_count(void) const {return m_keys_count;};
    const key_s *get_keys(void) const {return m_keys;}
    key_s *get_keys( int count ) // discard all!!!
    {
        ASSERT( count >= 2 );
        destroy();
        if ( count != m_keys_count )
        {
            m_keys = (key_s *)MM_RESIZE(m_keys, sizeof(key_s) * count );
            m_keys_count = count;
        }
        return m_keys;
    }
   
    V get_linear( float t ) const
    {
        ASSERT (m_keys_count >= 2);
        const key_s *s = m_keys + 1;
        const key_s *e = m_keys + m_keys_count;
    
        if (t < m_keys->time) return m_keys->val;
    
        for (;s < e;)
        {
            const key_s *f = s - 1;
            if (f->time <= t && t <= s->time)
            {
                float dt = (s->time - f->time);
                if (dt <= 0) return s->val;
                return ( s->val - f->val ) * ((t - f->time) / dt) + f->val;
            }
            ++s;
        }
    
        return m_keys[m_keys_count-1].val;
    }
    bool  is_empty(void) const {return m_keys_count == 0;}

};

class time_float_c : public time_value_c<float>
{
    float m_low;
    float m_high;

public:
    time_float_c():m_low(0), m_high(1) {}
    ~time_float_c() {};

    float get_low(void) const {return m_low;}
    float get_high(void) const {return m_high;}
    void set_low(float low, bool correct = false);
    void set_high(float hi, bool correct = false);

    float get_linear( float t ) const
    {
        return time_value_c<float>::get_linear(t) * (m_high-m_low) + m_low;
    }
    float get_linear( float t, float amp /* ������������� �������� [0..1] */ ) const
    {
        return time_value_c<float>::get_linear(t) * amp * (m_high-m_low) + m_low;
    }

    void    init( const wsptr &val );
    wstr_c  initstr(void) const;

};

class time_float_fixed_c : public time_value_c<float>
{
public:
    time_float_fixed_c() {}
    ~time_float_fixed_c() {};

    float get_linear( float t ) const
    {
        return time_value_c<float>::get_linear(t);
    }
    float get_linear( float t, float amp /* ������������� �������� [0..1] */ ) const
    {
        return time_value_c<float>::get_linear(t) * amp;
    }

    void    init();
    void    init( const wsptr &val );
    wstr_c  initstr(void) const;

};

class time_color_c : public time_value_c< vec3 >
{

public:
    time_color_c() {}
    ~time_color_c() {};

    void    init( const wsptr &val );
    wstr_c  initstr(void) const;

};

} // namespace ts
