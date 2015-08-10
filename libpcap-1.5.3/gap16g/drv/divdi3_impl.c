/*  Ripped from gcc-2.95.1/gcc/libgcc2.c  */

/* Copyright (C) 1989, 92-98, 1999 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* As a special exception, if you link this library with other files,
   some of which are compiled with GCC, to produce an executable,
   this library does not by itself cause the resulting executable
   to be covered by the GNU General Public License.
   This exception does not however invalidate any other reasons why
   the executable file might be covered by the GNU General Public License.  */

typedef unsigned int UQItype	__attribute__ ((mode (QI)));
typedef 	 int SItype	__attribute__ ((mode (SI)));
typedef unsigned int USItype	__attribute__ ((mode (SI)));
typedef		 int DItype	__attribute__ ((mode (DI)));
typedef unsigned int UDItype	__attribute__ ((mode (DI)));

typedef int word_type __attribute__ ((mode (__word__)));

/* Make sure that we don't accidentally use any normal C language built-in
   type names in the first part of this file.  Instead we want to use *only*
   the type names defined above.  The following macro definitions insure
   that if we *do* accidentally use some normal C language built-in type name,
   we will get a syntax error.  */

#define char bogus_type
#define short bogus_type
#define int bogus_type
#define long bogus_type
#define unsigned bogus_type
#define float bogus_type
#define double bogus_type

/* DIstructs are pairs of SItype values in the order determined by
   little/big ENDIAN.  */

#ifdef __i386__
  struct DIstruct {SItype low, high;};
#endif
#ifdef __powerpc__
  struct DIstruct {SItype high, low;};
#endif
#ifdef __s390__
  struct DIstruct {SItype high, low;};
#endif


/* We need this union to unpack/pack DImode values, since we don't have
   any arithmetic yet.  Incoming DImode parameters are stored into the
   `ll' field, and the unpacked result is read from the struct `s'.  */

typedef union
{
  struct DIstruct s;
  DItype ll;
} DIunion;


/*  From gcc-2.95.1/gcc/longlong.h  */

#ifndef SI_TYPE_SIZE
#define SI_TYPE_SIZE 32
#endif

#define __BITS4 (SI_TYPE_SIZE / 4)
#define __ll_B (1L << (SI_TYPE_SIZE / 2))
#define __ll_lowpart(t) ((USItype) (t) % __ll_B)
#define __ll_highpart(t) ((USItype) (t) / __ll_B)

#ifdef __i386__
#define sub_ddmmss(sh, sl, ah, al, bh, bl) \
  __asm__ ("subl %5,%1\n\tsbbl %3,%0"					\
	   : "=r" ((USItype) (sh)),					\
	     "=&r" ((USItype) (sl))					\
	   : "0" ((USItype) (ah)),					\
	     "g" ((USItype) (bh)),					\
	     "1" ((USItype) (al)),					\
	     "g" ((USItype) (bl)))
#define umul_ppmm(w1, w0, u, v) \
  __asm__ ("mull %3"							\
	   : "=a" ((USItype) (w0)),					\
	     "=d" ((USItype) (w1))					\
	   : "%0" ((USItype) (u)),					\
	     "rm" ((USItype) (v)))
#define udiv_qrnnd(q, r, n1, n0, d) \
  __asm__ ("divl %4"							\
	   : "=a" ((USItype) (q)),					\
	     "=d" ((USItype) (r))					\
	   : "0" ((USItype) (n0)),					\
	     "1" ((USItype) (n1)),					\
	     "rm" ((USItype) (d)))
#define count_leading_zeros(count, x) \
  do {									\
    USItype __cbtmp;							\
    __asm__ ("bsrl %1,%0"						\
	     : "=r" (__cbtmp) : "rm" ((USItype) (x)));			\
    (count) = __cbtmp ^ 31;						\
  } while (0)
#endif /* __i386__ */

#ifdef __powerpc__
#define sub_ddmmss(sh, sl, ah, al, bh, bl) \
  do {                                                                  \
    if (__builtin_constant_p (ah) && (ah) == 0)                         \
      __asm__ ("{sf%I3|subf%I3c} %1,%4,%3\n\t{sfze|subfze} %0,%2"       \
               : "=r" ((USItype) (sh)),                                 \
                 "=&r" ((USItype) (sl))                                 \
               : "r" ((USItype) (bh)),                                  \
                 "rI" ((USItype) (al)),                                 \
                 "r" ((USItype) (bl)));                                 \
    else if (__builtin_constant_p (ah) && (ah) ==~(USItype) 0)          \
      __asm__ ("{sf%I3|subf%I3c} %1,%4,%3\n\t{sfme|subfme} %0,%2"       \
               : "=r" ((USItype) (sh)),                                 \
                 "=&r" ((USItype) (sl))                                 \
               : "r" ((USItype) (bh)),                                  \
                 "rI" ((USItype) (al)),                                 \
                 "r" ((USItype) (bl)));                                 \
    else if (__builtin_constant_p (bh) && (bh) == 0)                    \
      __asm__ ("{sf%I3|subf%I3c} %1,%4,%3\n\t{ame|addme} %0,%2"         \
               : "=r" ((USItype) (sh)),                                 \
                 "=&r" ((USItype) (sl))                                 \
               : "r" ((USItype) (ah)),                                  \
                 "rI" ((USItype) (al)),                                 \
                 "r" ((USItype) (bl)));                                 \
    else if (__builtin_constant_p (bh) && (bh) ==~(USItype) 0)          \
      __asm__ ("{sf%I3|subf%I3c} %1,%4,%3\n\t{aze|addze} %0,%2"         \
               : "=r" ((USItype) (sh)),                                 \
                 "=&r" ((USItype) (sl))                                 \
               : "r" ((USItype) (ah)),                                  \
                 "rI" ((USItype) (al)),                                 \
                 "r" ((USItype) (bl)));                                 \
    else                                                                \
      __asm__ ("{sf%I4|subf%I4c} %1,%5,%4\n\t{sfe|subfe} %0,%3,%2"      \
               : "=r" ((USItype) (sh)),                                 \
                 "=&r" ((USItype) (sl))                                 \
               : "r" ((USItype) (ah)),                                  \
                 "r" ((USItype) (bh)),                                  \
                 "rI" ((USItype) (al)),                                 \
                 "r" ((USItype) (bl)));                                 \
  } while (0)
#define count_leading_zeros(count, x) \
  __asm__ ("{cntlz|cntlzw} %0,%1"                                       \
           : "=r" ((USItype) (count))                                   \
           : "r" ((USItype) (x)))
#define umul_ppmm(ph, pl, m0, m1) \
  do {                                                                  \
    USItype __m0 = (m0), __m1 = (m1);                                   \
    __asm__ ("mulhwu %0,%1,%2"                                          \
             : "=r" ((USItype) ph)                                      \
             : "%r" (__m0),                                             \
               "r" (__m1));                                             \
    (pl) = __m0 * __m1;                                                 \
  } while (0)
#endif /* __powerpc__ */

#ifdef __s390__
#define sub_ddmmss(sh, sl, ah, al, bh, bl) ({				\
  USItype __sh = (ah);							\
  USItype __sl = (al);							\
  __asm__ ("   slr  %1,%3\n"						\
           "   brc  3,0f\n"						\
           "   ahi  %0,-1\n"						\
           "0: slr  %0,%2"						\
           : "+&d" (__sh), "+d" (__sl)					\
           : "d" (bh), "d" (bl) : "cc" );				\
  (sh) = __sh;								\
  (sl) = __sl;								\
})
#define umul_ppmm(wh, wl, u, v) ({					\
  USItype __wh = (u);							\
  USItype __wl = (v);							\
  __asm__ ("   ltr  1,%0\n"						\
           "   mr   0,%1\n"						\
           "   jnm  0f\n"						\
           "   alr  0,%1\n"						\
           "0: ltr  %1,%1\n"						\
           "   jnm  1f\n"						\
           "   alr  0,%0\n"						\
           "1: lr   %0,0\n"						\
           "   lr   %1,1\n"						\
           : "+d" (__wh), "+d" (__wl)					\
           : : "0", "1", "cc" );					\
  (wh) = __wh;								\
  (wl) = __wl;								\
})
static const UQItype __clz_tab[] =
{
  0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
};
#define count_leading_zeros(count, x) \
  do {									\
    USItype __xr = (x);							\
    USItype __a;							\
									\
    if (SI_TYPE_SIZE <= 32)						\
      {									\
	__a = __xr < ((USItype)1<<2*__BITS4)				\
	  ? (__xr < ((USItype)1<<__BITS4) ? 0 : __BITS4)		\
	  : (__xr < ((USItype)1<<3*__BITS4) ?  2*__BITS4 : 3*__BITS4);	\
      }									\
    else								\
      {									\
	for (__a = SI_TYPE_SIZE - 8; __a > 0; __a -= 8)			\
	  if (((__xr >> __a) & 0xff) != 0)				\
	    break;							\
      }									\
									\
    (count) = SI_TYPE_SIZE - (__clz_tab[__xr >> __a] + __a);		\
  } while (0)
#endif

/* If this machine has no inline assembler, use C macros.  */

/* Define this unconditionally, so it can be used for debugging.  */
#define __udiv_qrnnd_c(q, r, n1, n0, d)  \
  do { \
    USItype __d1, __d0, __q1, __q0;                                     \
    USItype __r1, __r0, __m;                                            \
    __d1 = __ll_highpart (d);                                           \
    __d0 = __ll_lowpart (d);                                            \
                                                                        \
    __r1 = (n1) % __d1;                                                 \
    __q1 = (n1) / __d1;                                                 \
    __m = (USItype) __q1 * __d0;                                        \
    __r1 = __r1 * __ll_B | __ll_highpart (n0);                          \
    if (__r1 < __m)                                                     \
      {                                                                 \
        __q1--, __r1 += (d);                                            \
        if (__r1 >= (d)) /* i.e. we didn't get carry when adding to __r1 */\
          if (__r1 < __m)                                               \
            __q1--, __r1 += (d);                                        \
      }                                                                 \
    __r1 -= __m;                                                        \
                                                                        \
    __r0 = __r1 % __d1;                                                 \
    __q0 = __r1 / __d1;                                                 \
    __m = (USItype) __q0 * __d0;                                        \
    __r0 = __r0 * __ll_B | __ll_lowpart (n0);                           \
    if (__r0 < __m)                                                     \
      {                                                                 \
        __q0--, __r0 += (d);                                            \
        if (__r0 >= (d))                                                \
          if (__r0 < __m)                                               \
            __q0--, __r0 += (d);                                        \
      }                                                                 \
    __r0 -= __m;                                                        \
                                                                        \
    (q) = (USItype) __q1 * __ll_B | __q0;                               \
    (r) = __r0;                                                         \
  } while (0) 

/* If udiv_qrnnd was not defined for this processor, use __udiv_qrnnd_c.  */
#if !defined (udiv_qrnnd)
#define UDIV_NEEDS_NORMALIZATION 1
#define udiv_qrnnd __udiv_qrnnd_c
#endif

/*  End of gcc-2.95.1/gcc/longlong.h  */


static inline
DItype
__negdi2 (DItype u)
{
  DIunion w;
  DIunion uu;

  uu.ll = u;

  w.s.low = -uu.s.low;
  w.s.high = -uu.s.high - ((USItype) w.s.low > 0);

  return w.ll;
}


static inline
UDItype
__udivmoddi4 (UDItype n, UDItype d, UDItype *rp)
{
  DIunion ww;
  DIunion nn, dd;
  DIunion rr;
  USItype d0, d1, n0, n1, n2;
  USItype q0, q1;
  USItype b, bm;

  nn.ll = n;
  dd.ll = d;

  d0 = dd.s.low;
  d1 = dd.s.high;
  n0 = nn.s.low;
  n1 = nn.s.high;

#if !UDIV_NEEDS_NORMALIZATION
  if (d1 == 0)
    {
      if (d0 > n1)
	{
	  /* 0q = nn / 0D */

	  udiv_qrnnd (q0, n0, n1, n0, d0);
	  q1 = 0;

	  /* Remainder in n0.  */
	}
      else
	{
	  /* qq = NN / 0d */

	  if (d0 == 0)
	    d0 = 1 / d0;	/* Divide intentionally by zero.  */

	  udiv_qrnnd (q1, n1, 0, n1, d0);
	  udiv_qrnnd (q0, n0, n1, n0, d0);

	  /* Remainder in n0.  */
	}

      if (rp != 0)
	{
	  rr.s.low = n0;
	  rr.s.high = 0;
	  *rp = rr.ll;
	}
    }

#else /* UDIV_NEEDS_NORMALIZATION */

  if (d1 == 0)
    {
      if (d0 > n1)
	{
	  /* 0q = nn / 0D */

	  count_leading_zeros (bm, d0);

	  if (bm != 0)
	    {
	      /* Normalize, i.e. make the most significant bit of the
		 denominator set.  */

	      d0 = d0 << bm;
	      n1 = (n1 << bm) | (n0 >> (SI_TYPE_SIZE - bm));
	      n0 = n0 << bm;
	    }

	  udiv_qrnnd (q0, n0, n1, n0, d0);
	  q1 = 0;

	  /* Remainder in n0 >> bm.  */
	}
      else
	{
	  /* qq = NN / 0d */

	  if (d0 == 0)
	    d0 = 1 / d0;	/* Divide intentionally by zero.  */

	  count_leading_zeros (bm, d0);

	  if (bm == 0)
	    {
	      /* From (n1 >= d0) /\ (the most significant bit of d0 is set),
		 conclude (the most significant bit of n1 is set) /\ (the
		 leading quotient digit q1 = 1).

		 This special case is necessary, not an optimization.
		 (Shifts counts of SI_TYPE_SIZE are undefined.)  */

	      n1 -= d0;
	      q1 = 1;
	    }
	  else
	    {
	      /* Normalize.  */

	      b = SI_TYPE_SIZE - bm;

	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;

	      udiv_qrnnd (q1, n1, n2, n1, d0);
	    }

	  /* n1 != d0...  */

	  udiv_qrnnd (q0, n0, n1, n0, d0);

	  /* Remainder in n0 >> bm.  */
	}

      if (rp != 0)
	{
	  rr.s.low = n0 >> bm;
	  rr.s.high = 0;
	  *rp = rr.ll;
	}
    }
#endif /* UDIV_NEEDS_NORMALIZATION */

  else
    {
      if (d1 > n1)
	{
	  /* 00 = nn / DD */

	  q0 = 0;
	  q1 = 0;

	  /* Remainder in n1n0.  */
	  if (rp != 0)
	    {
	      rr.s.low = n0;
	      rr.s.high = n1;
	      *rp = rr.ll;
	    }
	}
      else
	{
	  /* 0q = NN / dd */

	  count_leading_zeros (bm, d1);
	  if (bm == 0)
	    {
	      /* From (n1 >= d1) /\ (the most significant bit of d1 is set),
		 conclude (the most significant bit of n1 is set) /\ (the
		 quotient digit q0 = 0 or 1).

		 This special case is necessary, not an optimization.  */

	      /* The condition on the next line takes advantage of that
		 n1 >= d1 (true due to program flow).  */
	      if (n1 > d1 || n0 >= d0)
		{
		  q0 = 1;
		  sub_ddmmss (n1, n0, n1, n0, d1, d0);
		}
	      else
		q0 = 0;

	      q1 = 0;

	      if (rp != 0)
		{
		  rr.s.low = n0;
		  rr.s.high = n1;
		  *rp = rr.ll;
		}
	    }
	  else
	    {
	      USItype m1, m0;
	      /* Normalize.  */

	      b = SI_TYPE_SIZE - bm;

	      d1 = (d1 << bm) | (d0 >> b);
	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;

	      udiv_qrnnd (q0, n1, n2, n1, d1);
	      umul_ppmm (m1, m0, q0, d0);

	      if (m1 > n1 || (m1 == n1 && m0 > n0))
		{
		  q0--;
		  sub_ddmmss (m1, m0, m1, m0, d1, d0);
		}

	      q1 = 0;

	      /* Remainder in (n1n0 - m1m0) >> bm.  */
	      if (rp != 0)
		{
		  sub_ddmmss (n1, n0, n1, n0, m1, m0);
		  rr.s.low = (n1 << b) | (n0 >> bm);
		  rr.s.high = n1 >> bm;
		  *rp = rr.ll;
		}
	    }
	}
    }

  ww.s.low = q0;
  ww.s.high = q1;
  return ww.ll;
}


DItype
__divdi3 (DItype u, DItype v)
{
  word_type c = 0;
  DIunion uu, vv;
  DItype w;

  uu.ll = u;
  vv.ll = v;

  if (uu.s.high < 0)
    c = ~c,
    uu.ll = __negdi2 (uu.ll);
  if (vv.s.high < 0)
    c = ~c,
    vv.ll = __negdi2 (vv.ll);

  w = __udivmoddi4 (uu.ll, vv.ll, (UDItype *) 0);
  if (c)
    w = __negdi2 (w);

  return w;
}


DItype
__moddi3 (DItype u, DItype v)
{
  word_type c = 0;
  DIunion uu, vv;
  DItype w;

  uu.ll = u;
  vv.ll = v;

  if (uu.s.high < 0)
    c = ~c,
    uu.ll = __negdi2 (uu.ll);
  if (vv.s.high < 0)
    vv.ll = __negdi2 (vv.ll);

  (void) __udivmoddi4 (uu.ll, vv.ll, &w);
  if (c)
    w = __negdi2 (w);

  return w;
}


UDItype
__umoddi3 (UDItype u, UDItype v)
{
  UDItype w;

  (void) __udivmoddi4 (u, v, &w);

  return w;
}


UDItype
__udivdi3 (UDItype n, UDItype d)
{
  return __udivmoddi4 (n, d, (UDItype *) 0);
}


