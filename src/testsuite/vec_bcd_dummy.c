/*
 * vec_bcd_dummy.c
 *
 *  Created on: Mar 1, 2018
 *      Author: sjmunroe
 */

#include <vec_common_ppc.h>
#include <vec_bcd_ppc.h>

vf64_t
test_pack_Decimal128 (_Decimal128 lval)
{
  return vec_pack_Decimal128 (lval);
}

vBCD_t
test_pack_Decimal128_cast (_Decimal128 lval)
{
  return (vBCD_t) vec_pack_Decimal128 (lval);
}

_Decimal128
test_unpack_Decimal128 (vf64_t lval)
{
  return vec_unpack_Decimal128 (lval);
}

_Decimal128
test_unpack_Decimal128_cast (vBCD_t lval)
{
  return vec_unpack_Decimal128 ((vf64_t) lval);
}

_Decimal128
test_vec_BCD2DFP (vBCD_t val)
{
  return vec_BCD2DFP (val);
}

vBCD_t
test_vec_DFP2BCD (_Decimal128 val)
{
  return vec_DFP2BCD (val);
}

vui8_t
test_vec_bcdctb100s (vui8_t a)
{
  return (vec_rdxct100b (a));
}

vBCD_t
test_vec_bcdadd (vBCD_t a, vBCD_t b)
{
  return vec_bcdadd (a, b);
}

vBCD_t
test_vec_bcdaddcsq (vBCD_t a, vBCD_t b)
{
  return vec_bcdaddcsq (a, b);
}

vBCD_t
test_vec_cbcdaddcsq (vBCD_t *c, vBCD_t a, vBCD_t b)
{
  return vec_cbcdaddcsq (c, a, b);
}

vBCD_t
test_vec_cbcdaddcsq_2 (vBCD_t *c, vBCD_t a, vBCD_t b)
{
  *c = vec_bcdaddcsq (a, b);
  return vec_bcdadd (a, b);
}

vBCD_t
test_vec_bcdaddesqm (vBCD_t a, vBCD_t b, vBCD_t c)
{
  return vec_bcdaddesqm (a, b, c);
}

vBCD_t
test_vec_bcdaddecsq (vBCD_t a, vBCD_t b, vBCD_t c)
{
  return vec_bcdaddecsq (a, b, c);
}

vBCD_t
test_vec_cbcdaddecsq (vBCD_t *co, vBCD_t a, vBCD_t b, vBCD_t c)
{
  return vec_cbcdaddecsq (co, a, b, c);
}

vBCD_t
test_vec_cbcdaddecsq_2 (vBCD_t *co, vBCD_t a, vBCD_t b, vBCD_t c)
{
  *co = vec_bcdaddecsq (a, b, c);
  return vec_bcdaddesqm (a, b, c);
}

vBCD_t
test_vec_bcdsub (vBCD_t a, vBCD_t b)
{
  return vec_bcdsub (a, b);
}

vBCD_t
test_vec_bcdsubcsq (vBCD_t a, vBCD_t b)
{
  return vec_bcdaddcsq (a, b);
}

vBCD_t
test_vec_bcdsubesqm (vBCD_t a, vBCD_t b, vBCD_t c)
{
  return vec_bcdsubesqm (a, b, c);
}

vBCD_t
test_vec_bcdsubecsq (vBCD_t a, vBCD_t b, vBCD_t c)
{
  return vec_bcdsubecsq (a, b, c);
}

vBCD_t
test_vec_cbcdsubecsq (vBCD_t *co, vBCD_t a, vBCD_t b, vBCD_t c)
{
  *co = vec_bcdsubecsq (a, b, c);
  return vec_bcdsubesqm (a, b, c);
}

vBCD_t
test_vec_bcdmulh (vBCD_t a, vBCD_t b)
{
  return vec_bcdmulh (a, b);
}

vBCD_t
test_vec_bcdmul (vBCD_t a, vBCD_t b)
{
  return vec_bcdmul (a, b);
}

vBCD_t
test_vec_cbcdmul (vBCD_t *p, vBCD_t a, vBCD_t b)
{
  return vec_cbcdmul (p, a, b);
}

vBCD_t
test_vec_cbcdmul_2 (vBCD_t *p, vBCD_t a, vBCD_t b)
{
  *p = vec_bcdmulh (a, b);
  return vec_bcdmul (a, b);
}

vBCD_t
test_vec_bcddiv (vBCD_t a, vBCD_t b)
{
  return vec_bcddiv (a, b);
}

vi128_t
test_vec_bcdctsq (vBCD_t a)
{
  return (vec_bcdctsq (a));
}

vui128_t
test_vec_bcdctuq (vBCD_t a)
{
  return (vec_bcdctuq (a));
}

vui64_t
test_vec_bcdctud (vBCD_t a)
{
  return (vec_bcdctud (a));
}

vui32_t
test_vec_bcdctuw (vBCD_t a)
{
  return (vec_bcdctuw (a));
}

vui16_t
test_vec_bcdctuh (vBCD_t a)
{
  return (vec_bcdctuh (a));
}

vui8_t
test_vec_bcdctub (vBCD_t a)
{
  return (vec_bcdctub (a));
}

vBCD_t
test_vec_bcdcpsgn (vBCD_t a, vBCD_t b)
{
  return vec_bcdcpsgn (a, b);
}

vBCD_t
test_vec_bcdsetsgn (vBCD_t a)
{
  return vec_bcdsetsgn (a);
}

vBCD_t
test_vec_bcdcfz (vui8_t vrb)
{
  return vec_bcdcfz (vrb);
}

vui8_t
test_vec_bcdctz (vBCD_t vrb)
{
  return vec_bcdctz (vrb);
}

vBCD_t
test_vec_bcds (vBCD_t vra, vi8_t vrb)
{
  return vec_bcds (vra, vrb);
}

vBCD_t
test_vec_bcdsr (vBCD_t vra, vi8_t vrb)
{
  return vec_bcdsr (vra, vrb);
}

vBCD_t
test_vec_bcdsrrqi (vBCD_t vra)
{
  return vec_bcdsrrqi (vra, 15);
}

vBCD_t
test_vec_bcdtrunc (vBCD_t vra, vui16_t vrb)
{
  return vec_bcdtrunc (vra, vrb);
}

vBCD_t
test_vec_bcdtruncqi (vBCD_t vra)
{
  return vec_bcdtruncqi (vra, 15);
}

vBCD_t
test_vec_bcdutrunc (vBCD_t vra, vui16_t vrb)
{
  return vec_bcdutrunc (vra, vrb);
}

vBCD_t
test_vec_bcdutruncqi (vBCD_t vra)
{
  return vec_bcdutruncqi (vra, 15);
}

vBCD_t
test_vec_bcdus (vBCD_t vra, vi8_t vrb)
{
  return vec_bcdus (vra, vrb);
}

vBCD_t
test_vec_bcdslqi_1 (vBCD_t vra, const int _N)
{
  return vec_bcdslqi (vra, 1);
}

vBCD_t
test_vec_bcdslqi_2 (vBCD_t vra, const int _N)
{
  return vec_bcdslqi (vra, 2);
}

vBCD_t
test_vec_bcdslqi_3 (vBCD_t vra, const int _N)
{
  return vec_bcdslqi (vra, 3);
}

vBCD_t
test_vec_bcdslqi_15 (vBCD_t vra, const int _N)
{
  return vec_bcdslqi (vra, 15);
}

vBCD_t
test_vec_bcdsrqi_1 (vBCD_t vra, const int _N)
{
  return vec_bcdsrqi (vra, 1);
}

vBCD_t
test_vec_bcdsrqi_2 (vBCD_t vra, const int _N)
{
  return vec_bcdsrqi (vra, 2);
}

vBCD_t
test_vec_bcdsrqi_3 (vBCD_t vra, const int _N)
{
  return vec_bcdsrqi (vra, 3);
}

vBCD_t
test_vec_bcdsrqi_15 (vBCD_t vra, const int _N)
{
  return vec_bcdsrqi (vra, 15);
}

vBCD_t
test_vec_bcdsruqi_1 (vBCD_t vra, const int _N)
{
  return vec_bcdsrqi (vra, 1);
}

vBCD_t
test_vec_bcdsruqi_2 (vBCD_t vra, const int _N)
{
  return vec_bcdsruqi (vra, 2);
}

vBCD_t
test_vec_bcdsruqi_3 (vBCD_t vra, const int _N)
{
  return vec_bcdsruqi (vra, 3);
}

vBCD_t
test_vec_bcdsruqi_15 (vBCD_t vra, const int _N)
{
  return vec_bcdsruqi (vra, 15);
}

int
test_vec_signgbit_bcdsq (vBCD_t a)
{
  return vec_signbit_bcdsq (a);
}

vb128_t
test_vec_setbool_bcdsq (vBCD_t a)
{
  return vec_setbool_bcdsq (a);
}

vb128_t
test_vec_setbool_bcdinv (vBCD_t a)
{
  return vec_setbool_bcdinv (a);
}

vbBCD_t
test_vec_bcdcmp_eqsq (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmp_eqsq (vra, vrb);
}

vbBCD_t
test_vec_bcdcmp_nesq (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmp_nesq (vra, vrb);
}

vbBCD_t
test_vec_bcdcmp_gtsq (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmp_gtsq (vra, vrb);
}

vbBCD_t
test_vec_bcdcmp_gesq (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmp_gesq (vra, vrb);
}

vbBCD_t
test_vec_bcdcmp_ltsq (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmp_ltsq (vra, vrb);
}

vbBCD_t
test_vec_bcdcmp_lesq (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmp_lesq (vra, vrb);
}

int
test_vec_bcdcmpeq (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmpeq (vra, vrb);
}

int
test_vec_bcdcmpne (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmpne (vra, vrb);
}

int
test_vec_bcdcmpgt (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmpgt (vra, vrb);
}

int
test_vec_bcdcmpge (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmpge (vra, vrb);
}

int
test_vec_bcdcmplt (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmplt (vra, vrb);
}

int
test_vec_bcdcmple (vBCD_t vra, vBCD_t vrb)
{
  return vec_bcdcmple (vra, vrb);
}

vui128_t
example_vec_bcdctuq (vui8_t vra)
{
  vui8_t d100;
  vui16_t d10k;
  vui32_t d100m;
  vui64_t d10e;
  vui128_t result;

  d100 = vec_rdxct100b ((vui8_t) vra);
  d10k = vec_rdxct10kh (d100);
  d100m = vec_rdxct100mw (d10k);
  d10e = vec_rdxct10E16d (d100m);
  result = vec_rdxct10e32q (d10e);

  return result;
}

void
example_vec_bcdctuq_loop (vui128_t *out, vui8_t* in, long cnt)
{
  vui8_t d100;
  vui16_t d10k;
  vui32_t d100m;
  vui64_t d10e;
//  vui128_t result;
  long i;

  for (i = 0; i < cnt; i++)
    {
      d100 = vec_rdxct100b (*in++);
      d10k = vec_rdxct10kh (d100);
      d100m = vec_rdxct100mw (d10k);
      d10e = vec_rdxct10E16d (d100m);
      *out++ = vec_rdxct10e32q (d10e);
    }
}

void
example_vec_cbcdecsq_loop (vBCD_t *cout, vBCD_t* out, vBCD_t* a, vBCD_t* b, long cnt)
{
  vBCD_t c, cn;
  long i;

  out[cnt-1] = vec_cbcdaddcsq (&c, a[cnt-1], b[cnt-1]);

  for (i = (cnt-2); i >= 0; i--)
    {
      out[i] = vec_cbcdaddecsq (&cn, a[i], b[i], c);
      c = cn;
    }
  *cout = cn;
}


void
example_bcdmul_2x2 (vBCD_t *__restrict__ mulu, vBCD_t m1h, vBCD_t m1l,
		    vBCD_t m2h, vBCD_t m2l)
{
  vBCD_t mc, mp, mq;
  vBCD_t mphh, mphl, mplh, mpll;
  mpll = vec_cbcdmul (&mplh, m1l, m2l);
  mp = vec_cbcdmul (&mphl, m1h, m2l);
  mplh = vec_bcdadd (mplh, mp);
  mc   = vec_bcdaddcsq (mplh, mp);
  mphl   = vec_bcdadd (mphl, mp);
  mp = vec_cbcdmul (&mc, m2h, m1l);
  mplh = vec_bcdadd (mplh, mp);
  mq   = vec_bcdaddcsq (mplh, mp);
  mphl = vec_bcdadd (mphl, mq);
  mc   = vec_bcdaddcsq (mplh, mq);
  mp = vec_cbcdmul (&mphh, m2h, m1h);
  mphl = vec_bcdadd (mphl, mp);
  mp   = vec_bcdaddcsq (mplh, mp);
  mphh = vec_bcdadd (mphh, mp);
  mphh = vec_bcdadd (mphh, mc);

  mulu[0] = mpll;
  mulu[1] = mplh;
  mulu[2] = mphl;
  mulu[3] = mphh;
}

#if (__GNUC__ > 4)
_Decimal128
test__builtin_ddedpdq ( _Decimal128 value)
{
  return __builtin_ddedpdq (2, value);
}

_Decimal128
test__builtin_denbcdq ( _Decimal128 value)
{
  return __builtin_denbcdq (1, value);
}

#ifdef _ARCH_PWR8
vi128_t
test__builtin_bcdadd (vi128_t vra, vi128_t vrb)
{
  return __builtin_bcdadd (vra, vrb, 0);
}

int
test__builtin_bcdadd_eq (vi128_t vra, vi128_t vrb)
{
  return __builtin_bcdadd_eq (vra, vrb, 0);
}

vi128_t
test__builtin_bcdsub (vi128_t vra, vi128_t vrb)
{
  return __builtin_bcdsub (vra, vrb, 0);
}

int
test__builtin_bcdsub_eq (vi128_t vra, vi128_t vrb)
{
  return __builtin_bcdsub_eq (vra, vrb, 0);
}

int
test__builtin_bcdsub_lt (vi128_t vra, vi128_t vrb)
{
  return __builtin_bcdsub_lt (vra, vrb, 0);
}

vi128_t
test__builtin_bcdmax (vi128_t vra, vi128_t vrb)
{
  vi128_t result = vra;
  if (__builtin_bcdsub_lt (vra, vrb, 0))
    result = vrb;

  return result;
}

vi128_t
test__vec_bcdaddcsq (vi128_t a, vi128_t b)
{
  vi128_t t, c;
  c = (vi128_t) _BCD_CONST_ZERO;
  t = __builtin_bcdadd (a, b, 0);
  if (__builtin_expect (__builtin_bcdadd_ov ( a, b, 0), 0))
    {
      c = (vi128_t) vec_bcdcpsgn (_BCD_CONST_PLUS_ONE, (vBCD_t) t);
    }
  return (c);
}

vi128_t
test__builtin_bcdabsd (vi128_t vra, vi128_t vrb)
{
  vi128_t result;
  if (__builtin_bcdsub_gt (vra, vrb, 0))
    result = __builtin_bcdsub (vra, vrb, 0);
  else
    result = __builtin_bcdsub (vrb, vra, 0);

  return result;
}

vi128_t
test__builtin_bcdabs (vi128_t vra)
{
  vi128_t result = vra;

  if (__builtin_bcdadd_lt (vra, (vi128_t) _BCD_CONST_ZERO, 0))
    result = __builtin_bcdsub ((vi128_t) _BCD_CONST_ZERO, vra, 0);

  return result;
}
#endif
#endif
