/*
 Copyright (c) [2017-2018] IBM Corporation.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 vec_f128_ppc.h

 Contributors:
      IBM Corporation, Steven Munroe
      Created on: Apr 11, 2016
 */

/*!
 * \file  vec_f128_ppc.h
 * \brief Header package containing a collection of 128-bit SIMD
 * operations over Quad-Precision floating point elements.
 *
 * PowerISA 3.0B added Quad-Precision floating point type and
 * operations to the Vector-Scalar Extension (VSX) facility.
 * The first hardware implementation is available in POWER9.
 *
 * PowerISA 3.1 added new min/max/compare Quad-Precision operations.
 * Also added new quadword (128-bit) integer operations including
 * converts between quadword integer and Quad-Precision floating point.
 * The first hardware implementation is available in POWER10.
 *
 * While all Quad-Precision operations are on 128-bit vector registers,
 * they are defined as scalars in the PowerISA. The OpenPOWER ABI also
 * treats the __float128 type as scalar that just happens to use vector
 * registers for parameter passing and operations.
 * As such no operations using __float128 (_Float128, or __ieee128) as
 * parameter or return value are defined as vector built-ins in the ABI
 * or <altivec.h>.
 *
 * \note GCC 8.2 does document some built-ins, using the <I>scalar</I>
 * prefix (scalar_extract_exp, scalar_extract_sig,
 * scalar_test_data_class), that do accept the __ieee128 type.
 * This work seems to be incomplete as scalar_exp_cmp_* for the
 * __ieee128 type are not present.
 * GCC 7.3 defines vector and scalar forms of the extract/insert_exp
 * for float and double but not for __ieee128.
 * These built-ins are not defined in GCC 6.4. See
 * <a href="https://gcc.gnu.org/onlinedocs/">compiler documentation</a>.
 * These are useful operations and can be implemented in a few
 * vector logical instructions for earlier machines. So it seems
 * reasonable to add these to pveclib for both vector and scalar forms.
 *
 * Quad-Precision is not supported in hardware until POWER9. However
 * the compiler and runtime supports the __float128 type and arithmetic
 * operations via soft-float emulation for earlier processors.
 * The soft-float implementation follows the ABI and passes __float128
 * parameters and return values in vector registers.
 *
 * \note The performance of the libgcc soft-float (KF Mode) runtime for
 * __float128 varies between releases of GCC. Some GCC versions
 * introduced a bug that miss-compiled the transfer of __float128
 * parameters to GPR pairs as required for generic soft-float
 * implementation. For POWER8 this results in a significant performance
 * hit.
 *
 * The PowerISA 3.0 also defines a number of useful quad-precision
 * operations using the "round-to-odd" override. This is useful when
 * the results of quad-precision arithmetic must be rounded to a
 * shorter precision while avoiding double rounding. Recent GCC
 * compilers support these operations as built-ins for the POWER9
 * target, but they not supported by the C language or GCC runtime
 * library. This means that round-to-odd is not easily available to
 * libraries that need to support IEEE-128 on POWER8. Again it may be
 * reasonable to add these to pveclib.
 *
 * \note See
 * <a href="https://www.exploringbinary.com/gcc-avoids-double-rounding-errors-with-round-to-odd/">
 * GCC Avoids Double Rounding Errors With Round-To-Odd</a>
 *
 * Another issue is the performance of GCC soft-float runtime for
 * IEEE-128 (KF mode). There seem to be a number of issues with code
 * generation for transfers from __float128 to 64-bit integer GPRs.
 * This is required to match the ABI (vector) parameters to the
 * soft-float runtime using integer scalars. For POWER8 targets
 * the GCC compiler generates store vector followed by two load
 * doubleword instructions. This generates high frequencies of
 * load-hit-store rejects at runtime.
 * It also looks like there is significant instruction latency
 * associated with the XER carry bit required for extended (128-bit)
 * integer arithmetic.
 * \note The first issue (parameter store reload) is a compiler bug.
 * The second issue is an attribute of the hardware implementation
 * and associate trade-offs.
 * Both of these issues can be avoided by providing a soft-float
 * implementation for __float128 using VSX vector 128-bit arithmetic and
 * logical operations.
 *
 * For the QP operations that have libgcc implementations and we have
 * corresponding PVECLIB implementations we can do direct
 * performance comparisons. So far micro-benchmarks show a significant
 * performance gain for the PVECLIB vector implementations vs the
 * GCC KF mode runtime.
 *
 * | Power8 QP  | cmpqp |cvtdpqp|cvtqpdp|cvtuqqp|cvtqpuq| mulqp | addqp |
 * |-----------:|:-----:|:------:|:-----:|:-----:|:-----:|:-----:|:------|
 * |\%improvement| 22.4 |  60.7  | 46.2* |  28.9 |  72.4 |  1.8* | 10.1* |
 *
 * \note These micro-benchmarks and others are included in
 * src/testsuite. Specifically files vec_perf_f128.c and
 * vec_f128_dummy.c. Make check will compile the appropriate files
 * and link them into the pveclib_perf executable.
 * Items marked with "*" compare the pveclib round-to-odd
 * implementation
 * to the soft-float default rounding mode. The libgcc soft-float
 * runtime does not implement round-to-odd.
 *
 * There are number of __float128 operations that should generate a
 * single instruction for POWER9 and few (less than 10) instructions
 * for POWER8.
 * This includes all of the __float128 classification functions
 * (isnormal/subnormal/finite/inf/nan/zero).
 * Unfortunately for POWER8 the compilers will generate calls to the
 * GCC runtime (__unordkf2, __gekf2, ...) for these functions.
 * In many cases the code size generated for the runtime calls
 * far exceeds any in-line VSX code PVECLIB will generate.
 *
 * So it is not unreasonable for this header to provide vector forms
 * of the __float128 classification functions
 * (isnormal/subnormal/finite/inf/nan/zero). It is little additional
 * effort to include the
 * sign bit manipulation operations (copysign, abs, nabs, and neg).
 *
 * These functions can be implemented directly using (one or more) POWER9
 * instructions, or a few vector logical and integer compare
 * instructions for POWER7/8. Each is comfortably small enough to be
 * in-lined and inherently faster than the equivalent POSIX or compiler
 * built-in runtime functions. Performing these operations in-line and
 * directly in vector registers (VRs) avoids call/return and VR <-> GPR
 * transfer overhead.
 * It also seems reasonable to provide Quad-Precision extract/insert
 * exponent/significand and compare exponent operations for POWER7/8.
 *
 * The PVECLIB implementations for quad-precision arithmetic and
 * conversion operations are large enough
 * that most applications will want to call a library.
 * PVECLIB will build and release the appropriate CPU tuned libraries.
 * This will follow the general design used for multiple
 * quadword integer multiply functions (vec_int512_ppc.h).
 *
 * \note At this time, PVECLIB does not intend to replace existing
 * GCC/libm IEEE-128 runtime APIs and will maintain it own unique
 * name-space. However if the maintainers of these projects want to
 * leverage PVECLIB they are allowed under the terms of the
 * <a href="http://www.apache.org/licenses/LICENSE-2.0">
 * Apache License, Version 2.0.</a>
 *
 * These PVECLIB operations should be useful for applications using
 * Quad-Precision while needing to still support POWER8 but also
 * build for POWER9/10.
 * An important goal is to allow applications and libraries to safely
 * substitute PVECLIB operations for C language and math.h __float128
 * operators and functions as point optimizations.
 * The largest gains will be seen for builds targeting POWER8 without
 * degrading performance when targeting POWER9/10.
 * They should also be useful and improve performance of soft-float
 * implementations of Quad-Precision math library functions.
 *
 * \note The compiler disables associated <altivec.h> built-ins if the
 * <B>mcpu</B> target does not enable the specific instruction.
 * For example if you compile with <B>-mcpu=power8</B>, Quad-Precision
 * floating-point built-ins operations useful
 * for floating point classification are not defined.
 * This header provides the appropriate substitutions,
 * will generate the minimum code, appropriate for the target,
 * and produce correct results.
 *
 * \note Most ppc64le compilers will default to <B>-mcpu=</B><I>power8</I>
 * if  <B>-mcpu</B> is not specified.
 *
 * This header covers operations that are any of the following:
 *
 * - Implemented in hardware instructions in newer processors,
 * but useful to programmers on slightly older processors
 * (even if the equivalent function requires more instructions).
 * - Defined in the OpenPOWER ABI but <I>not</I> yet defined in
 * <altivec.h> provided by available compilers in common use.
 * Examples include scalar_test_neg, scalar_test_data_class, etc.
 * - Defined for POWER9 (as built-ins) but not supported in the
 * soft-float runtime implementation provided for POWER8.
 * Examples include the arithmetic/conversion operations supporting
 * the <I>round-to-odd</I> override.
 * - Providing vector float tests for special conditions
 * without generating extraneous floating-point exceptions.
 * This is important for implementing __float128 forms of ISO C99 Math
 * functions. Examples include vector isnan, isinf, etc.
 * - Commonly used operations, not covered by the ABI or
 * <altivec.h>, and require multiple instructions or
 * are not obvious.
 *
 * \section f128_softfloat_0_0 Vector implementation of Quad-Precision Soft-float
 *
 * The discussion above raises a interesting question. If we can
 * provide useful implementations of Quad-Precision; classification,
 * extract/insert, and compare exponent operations, why not continue
 * with Quad-Precision compare, convert to/from integer, and
 * arithmetic operations?
 *
 * This raises the stakes in complexity and size of implementation.
 * Providing a vector soft-float implementation equivalent to the
 * GCC run-time libgcc <B>__addkf3/__divkf3/__mulkf3/__subkf3</B>
 * would be a substantial effort. The IEEE standard is exacting
 * about rounding and exception handling. Comparisons require special
 * handling of; signed zero, infinities, and NaNs.
 * Even float/integer conversions require correct rounding and return
 * special values for overflow.
 * Also it is not clear how such an effort would be accepted.
 *
 * The good news is PVECLIB already provides a strong quadword integer
 * operations set. Integer Add, subtract, and multiply are covered with
 * the usual compare/shift/rotate operations (See vec_int128_ppc.h
 * and vec_int64_ppc.h).
 * The weak spot is general quadword integer divide. Until recently,
 * integer divide has not been part of the vector ISA.
 * But the introduction of Vector Divide Signed/Unsigned Quadword in
 * POWER10 raises the priority of vector integer divide for PVECLIB.
 *
 * For now we propose a phased approach, starting with enablers and
 * infrastructure, building up layers, starting simple and adding
 * complexity.
 *
 * - Basic enablers; classification, extract/insert exponent,
 * compare exponent.
 * - Quad-Precision comparison operators.
 *   - Initially ignore special cases and exceptions
 *   - Add Signed Zero, Infinity. and NaN special cases
 *   - Exceptions (FPSCR) when someone asks
 * - Quad-Precision from/to integer word/doubleword/quadword.
 *   - Cases that don't require rounding (i.e truncate and DW to QP).
 *   - Cases that require rounding
 *     - Round to odd.
 *     - Round to Nearest/Even
 *     - Others if asked
 * - Quad-Precision arithmetic
 *   - Add/Sub/Mul
 *     - Round-to-Odd first
 *   - Fused Multiply-Add
 *     - Round-to-Odd first
 *   - Divide
 *     - Round-to-Odd first
 *     - Will need vec_int128_ppc.h implementation of vec_divuq()
 *   - Signed Zero, Infinity. and NaN special cases
 *   - Other rounding modes
 *   - Exceptions (FPSCR) when someone asks
 *
 * The intent is that such PVECLIB operations can be mixed in with or
 * substituted for C Language _FLoat128 expressions or functions.
 * The in-lined operations should have performance advantages over
 * equivalent library functions on both POWER8/9.
 *
 * This is a big list. It is TBD how far I will get given  my current
 * limited resources.
 *
 * \note We are focusing on POWER8 here because the implementation gets
 * a lot harder for POWER7 and earlier. POWER7 is missing:
 * - Vector Quadword integer add/sub with carry extend.
 * - Vector Doubleword integer arithmetic, compares.
 * and count-leading zeros.
 * - Vector Word integer multiply.
 * - Direct transfer between VRs and GPRs.
 *
 * \subsection f128_softfloat_0_0_0 Quad-Precision data class and exponent access for POWER8
 *
 * Most math library functions need to test the data class (normal,
 * infinity, NaN, etc) and or range of input values. This usually involves
 * separating the sign, exponent, and significand out from
 * __float128 values, and comparing one or more of these parts,
 * to special integer values.
 *
 * PowerISA 3.0B (POWER9) provides instructions for these in addition
 * to a comprehensive set of arithmetic and compare instructions.
 * These operations are also useful for the soft-float implementation
 * of __float128 for POWER8 and earlier. The OpenPOWER ABI specifies
 * __float128 parameters are in VRs and are immediately accessible to
 * VMX/VSR instructions. This is important as the cost of transferring
 * values between VRs and GPRs is quite high on POWER8 and even higher
 * for POWER7 and earlier (which requires store to temporaries and
 * reload).
 *
 * Fortunately these operations only require logical (and/or/xor),
 * shift and integer compare operations to implement. These are
 * available as vector intrinsics or provides by PVECLIB
 * (see vec_int128_ppc.h).
 *
 * The operations in this group include:
 * - Altivec like predicates;
 * vec_all_isfinitef128(),
 * vec_all_isinff128(),
 * vec_all_isnanf128(),
 * vec_all_isnormalf128(),
 * vec_all_issubnormalf128(),
 * vec_all_iszerof128(),
 * vec_signbitf128().
 * - Vector boolean predicates;
 * vec_isfinitef128(),
 * vec_isinff128(),
 * vec_isnanf128(),
 * vec_isnormalf128(),
 * vec_issubnormalf128(),
 * vec_iszerof128(),
 * vec_setb_qp().
 * - Sign bit manipulation;
 * vec_absf128(),
 * vec_nabsf128(),
 * vec_negf128(),
 * vec_copysignf128().
 * - Data manipulation;
 * vec_xsiexpqp(),
 * vec_xsxexpqp(), vec_xxxexpqpp(),
 * vec_xsxsigqp().
 * - Exponent Compare;
 * vec_cmpqp_exp_eq(),
 * vec_cmpqp_exp_gt(),
 * vec_cmpqp_exp_lt(),
 * vec_cmpqp_exp_unordered().
 *
 * For example the data class test isnan:
 * \code
static inline vb128_t
vec_isnanf128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && \
  defined (__FLOAT128__) && (__GNUC__ > 7)
  vui32_t result = CONST_VINT128_W (0, 0, 0, 0);

  if (scalar_test_data_class (f128, 0x40))
    result = CONST_VINT128_W (-1, -1, -1, -1);

  return (vb128_t)result;
#else
  vui32_t tmp;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  tmp = vec_andc_bin128_2_vui32t (f128, signmask);
  return vec_cmpgtuq ((vui128_t)tmp , (vui128_t)expmask);
#endif
}
 * \endcode
 * Which has implementations for POWER9 (and later) and POWER8
 * (and earlier).
 *
 * For POWER9 it generates:
 * \code
     xststdcqp cr0,v2,64
     bne     .+12
     xxspltib vs34,255
     b       .+8
     xxspltib vs34,0
 * \endcode
 * Which uses the intrinsic scalar_test_data_class() to generate the
 * VSX Scalar Test Data Class Quad-Precision instruction
 * with "data class mask" of <I>class.NaN</I> to set the condition code.
 * If the condition is <I>match</I>, load the 128-bit bool value of all
 * 1's (true). Otherwise load all 0's (false).
 *
 *
 * For POWER8 it generates
 * \code
     addis   r9,r2,@ha.rodata.cst16+0x30
     vspltisw v1,-1
     vspltisw v12,0
     addi    r9,r9,@l.rodata.cst16+0x30
     vslw    v1,v1,v1
     lvx     v0,0,r9
     vsldoi  v1,v1,v12,12

     xxlandc vs33,vs34,vs33
     vsubcuq v0,v0,v1
     xxspltw vs32,vs32,3
     vcmpequw v2,v0,v12
 * \endcode
 * The first 7 instructions above, load the constant vectors needed
 * by the logic. These constants only need to be generated once per
 * function and can be shared across operations.
 *
 * In the C code we use a special transfer function combined with
 * logical AND complement (vec_andc_bin128_2_vui32t()).
 * This is required because while __float128 values are held in VRs,
 * the compiler considers them to be scalars and will not
 * allow simple casts to (any) vector type. So the PVECLIB
 * implementation provides <I>xfer</I> function using a union to
 * transfer the __float128 value to a vector type.
 * In most case this logical transfer simply serves to make the
 * compiler happy and does not need to generate any code.
 * In this case the
 * <I>xfer</I> function combines the transfer with a vector and
 * complement to mask off the sign bit.
 *
 * Then compare the masked result as a 128-bit integer value greater
 * than infinity (expmask). Here we use the vec_cmpgtuq() operation
 * from vec_int128_ppc.h. For POWER8, vec_cmpgtuq() generates the
 * Vector Subtract and Write Carry Unsigned Quadword instruction for
 * 128-bit unsigned compares. A '0' carry indicates greater than.
 * The next two instructions (from vec_setb_ncq())
 * convert the carry bit to the required
 * 128-bit bool value.
 *
 * While the POWER8 sequence requires more instructions (including the
 * const vector set up) than POWER9, it is not significantly larger.
 * And as mentioned above, the set-up code can be optimized across
 * operations sharing the same constants. The code
 * (less the setup) is only 10 cycles for POWER8 vs 6 for POWER9.
 * Also the code is not any larger than the function call overhead for
 * the libgcc runtime equivalent <B>__unordkf2</B>. And is much faster
 * then the generic soft-float implementation.
 *
 * Another example, Scalar Extract Exponent Quad-Precision:
 *
 * \code
static inline vui64_t
vec_xsxexpqp (__binary128 f128)
{
  vui64_t result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  __asm__(
      "xsxexpqp %0,%1"
      : "=v" (result)
      : "v" (f128)
      : );

#else
  vui32_t tmp;
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0)

  tmp = vec_and_bin128_2_vui32t (f128, expmask);
  result = (vui64_t) vec_sld (tmp, tmp, 10);
#endif
  return result;
}
 * \endcode
 * Which has implementations for POWER9 (and later) and POWER8
 * (and earlier).
 *
 * For POWER9 it generates the
 * VSX Scalar Extract Exponent Quad-Precision instruction.
 * \code
      xsxexpqp v2,v2
 * \endcode
 *
 * \note Would like to use the intrinsic scalar_extract_exp() here but
 * this is not available until GCC 11 (or later).
 * Also GCC defines these scalar built-ins to return integer scalar
 * values in GPRs.
 * This would defeat the purpose of an all vector implementation.
 *
 * For POWER8 we generate
 * \code
     addis   r9,r2,.rodata.cst16+0xc0@ha
     addi    r9,r9,.rodata.cst16+0xc0@l
     lvx     v13,0,r9

     xxland  vs34,vs34,vs45
     vsldoi  v2,v2,v2,10
 * \endcode
 * The first 3 instructions above load the constant vector needed
 * by the logic. This constant only needs to be generated once per
 * function and can be shared across operations.
 *
 * Again we use a special transfer function combined with
 * logical AND (vec_and_bin128_2_vui32t()) to transfer the__float128
 * to a vector type and mask off all bits except for the 15-bit
 * exponent. Then we rotate the exponent logically right 48-bit to
 * right justify the exponent in vector doubleword 0.
 * This matches the results of the xsxexpqp instruction.
 *
 * \subsection f128_softfloat_IRRN_0_0 Intermediate results and Rounding for Quad-Precision
 *
 * The IEEE-128 floating-point storage (external) format fits neatly in
 * 128-bits. But this compact format needs to be expanded internally
 * during QP operations. The sign and exponent are normally manipulated
 * separately from the significand. And for finite values the
 * Leading-bit (implied but not included in the storage format) must
 * be restored to take part in arithmetic/rounding/normalization
 * operations.
 *
 * \note The <I>Leading</I>, <I>Hidden</I>,  and <I>Implicit</I> bits
 * are different names for the most significant bit of the significand.
 * This bit is Hidden or Implicit only for the external or storage
 * format of floating-point numbers. But it needs to be explicitly
 * represented in the internal <I>Intermediate Results (<B>IR</B>)</I>.
 * See Also: PowerISA 3.0
 * 7.3.3 VSX Floating-Point Execution Models.
 *
 * For a soft-float implementation of IEEE-128 on POWER8 we want to
 * extract these components into 128-bit vector registers and operate
 * on them using vector instructions. This allows direct use of 128-bit
 * arithmetic/shift/rotate operations (see vec_int128_ppc.h), while
 * avoiding expensive transfers between VRs and GPRs.
 *
 * To extract the sign-bit we can either AND with a 128-bit mask or use
 * a set-bool operation (vec_setb_qp() or vec_setb_sq()).
 * The masked sign-bit can be ORed with the final IEEE-128 vector
 * result to set the appropriate sign.
 * The 128-bit vector bool can be used with vec_sel()
 * (vec_self128(), vec_selsq(), vec_seluq()) to select
 * results based on the sign-bit while avoiding branch logic.
 *
 * We use vec_xsxexpqp() to extract the 15-bit exponent into a
 * vector doubleword integer element. The biased exponent is returned
 * in the high doubleword (matching the POWER9 instruction).
 * Depending on the operation, the exponent (or derived values)
 * may need to be transfered/replicated to the low doubleword element.
 * This is easily accomplished using vec_splatd(). Operations
 * requiring two Quad-precision operands can combine the extracted
 * exponents into a single vector doubleword using vec_mrgahd().
 *
 * \note Operations requiring two Quad-precision operands have an
 * opportunity to vectorize the exponent extraction and comparisons
 * (all/any) for finite vs NaN/Infinity.
 * See vec_xxxexpqpp().
 *
 * We use vec_xsxsigqp() to extract the 113-bit significand into a
 * vector quadword integer. This operation restores the leading-bit
 * for normal (not NaN, Infinity, denormal or zero) values.
 * The significand is returned right-justified in the quadword.
 *
 * At the end of the operation we can use vec_or() and vec_xsiexpqp()
 * to combine these (sign, exponent, and significand) components into
 * a IEEE-128 result.
 *
 * \subsubsection f128_softfloat_IRRN_0_1 Representing Intermediate results for Quad-Precision
 *
 * Internal IEEE floating-point operations will need/generate
 * additional bits to support normalization and rounding.
 * The PowerISA describes a
 * <B>VSX Execution Model for IEEE Operations</B>
 * \sa PowerISA 3.0B, Book I, Section 7.3.3
 * VSX Floating-Point Execution Models.
 *
 * IEEE quad-precision execution model
 *  |  |   | 0 |1 ----------------------------- 112|   |   |   |
 *  |:-|:-:|:-:|:---------------------------------:|:-:|:-:|--:|
 *  | S| C | L |             FRACTION              | G | R | X |
 *  | - Sign bit |||||||
 *  | - Carry bit |||||||
 *  | - Leading bit, also called the implicit or hidden bit |||||||
 *  | - Fraction (112-bits) |||||||
 *  | - Guard bit |||||||
 *  | - Round bit |||||||
 *  | - (X) AKA Sticky bit, logical OR of remaining bits |||||||
 *
 * This model is a guide for processor design and soft-float
 * implementors. This is also described as the
 * <I>Intermediate result Representation (<B>IR</B>)</I>.
 * As such the implementation may arrange these bits into
 * different registers as dictated by design and performance.
 *
 * The GRX bits extend the low order bits of the fraction and are
 * required for rounding. Basically these bits encode how <I>near</I>
 * the intermediate result is to a representable result.
 * The GR bits are required for post-normalization of the result
 * and participate in shifts during normalization. For right shifts,
 * bits shift out of the R-bit are logically ORed into the X-bit.
 * For left shifts, 0 bits shifted into the R-bit
 * (the X-bit is ignored).
 *
 * As mentioned before, it is convenient to keep the sign-bit in a
 * separate vector quadword.
 * This not an extension of the significand but is
 * needed to select results for arithmetic and some rounding modes.
 * The remaining (C through X) bits can be represented in a vector
 * quadword register or a vector register pair.
 *
 * \paragraph f128_softfloat_IRRN_0_1_1 IR for conversion
 * For example integer to QP
 * conversions can be represented in a vector quadword by left
 * justifying the magnitude before normalization and rounding.
 * For example from vec_xscvuqqp():
 * \code
    { // We need to produce a normal QP, so we treat the integer like a
      // denormal, then normalize it.
      // Start with the quad exponent bias + 127 then subtract the count of
      // leading '0's. The 128-bit significand can have 0-127 leading '0's.
      vui64_t q_expm = (vui64_t) CONST_VINT64_DW (0, (0x3fff + 127));
      vui64_t i64_clz = (vui64_t) vec_clzq (q_sig);
      q_sig = vec_slq (q_sig, (vui128_t) i64_clz);
      q_exp = vec_subudm (q_expm, i64_clz);
      // This is the part that might require rounding.
      // The Significand (including the L-bit) is right justified in
      // in the high-order 113-bits of q_sig.
      // The guard, round, and sticky (GRX) bits are in the low-order
      // 15 bits.
      ...
    }
 * \endcode
 *
 * See \ref f128_softfloat_IRRN_0_2_1
 * and \ref f128_softfloat_IRRN_0_2_2.
 *
 * The <I>Round to Nearest Even</I> case may increment the significand
 * and that may generate a carry from the <B>L-bit</B>. One option is
 * to use vec_addcuq() to capture the carry. For example:
 * \code
      ...
      // We add 0x3fff to GRX-bits which may carry into low order sig-bit
      // This may result in a carry out from the L-bit into C-bit.
      q_carry = vec_addcuq (q_sig, (vui128_t) RXmask);
      q_sig = vec_adduqm (q_sig, (vui128_t) RXmask);
      // Generate a bool mask from the carry to use in the vsel
      qcmask = vec_setb_cyq (q_carry);
      // Two cases; 1) We did carry so shift (double) left 112 bits
      q_sigc = vec_sldqi (q_carry, q_sig, 112);
      // 2) no carry so shift right 15 bits
      q_sig = vec_srqi ((vui128_t) q_sig, 15);
      // Select which based on carry
      q_sig = (vui128_t) vec_sel ((vui32_t) q_sig, (vui32_t) q_sigc, (vui32_t) qcmask);
      // Increment the exponent based on the carry
      q_exp = vec_addudm (q_exp, (vui64_t) q_carry);
 * \endcode
 * In this case having the carry as a separate vector simplifies
 * adjusting the exponent.
 *
 * \paragraph f128_softfloat_IRRN_0_1_2 IR for addition
 * Quad-precision addition and subtraction is a case where right
 * justifying the <B>IR</B> is helpful.
 * For addition/subtraction the <B>IR</B> only needs 117-bits which
 * can be accommodated in a single 128-vector.
 * Significands (which includes the leading/implicit bit) can be
 * converted to <B>IR</B> form by shifting left 3-bits.
 * This still leaves room on the left for the carry-bit.
 * For example:
 * \code
      // Precondition the significands before add so the GRX bits
      // are in the least significant 3 bit.
      a_sig = vec_slqi (a_sig, 3);
      b_sig = vec_slqi (b_sig, 3);
 * \endcode
 * In this case we need to insure that any right shifts of the
 * <B>IR</B> collect any bits shifted-away into the X-bit.
 * For example:
 * \code
      // At this point we can assume that:
      // 1) The magnitude (vfa) >= magnitude (vfb)
      // 1a) Exponents (a_exp, b_exp) in the range E_min -> E_max
      // 1b) The GRX-bits of a_sig/b_sig are still 0b000.
      // 2) a_exp >= b_exp
      // 2a) If a_exp == b_exp then a_sig >= b_sig
      // 2b) If a_exp > b_exp then
      //     shift (b_sig) right by (a_exp - b_exp) bits
      //     any bits shifted out of b_sig are ORed into the X-bit
      if (vec_cmpud_all_lt (b_exp, a_exp))
	{
	  vui64_t d_exp, l_exp;
	  vui128_t t_sig;
	  const vui64_t exp_128 = (vui64_t) CONST_VINT64_DW( 128, 128 );

	  d_exp = vec_subudm (a_exp, b_exp);
	  if (vec_cmpud_all_lt (d_exp, exp_128))
	    {
	      l_exp = vec_subudm (exp_128, d_exp);
	      t_sig = vec_srq (b_sig, (vui128_t) d_exp);
	      x_bits = vec_slq (b_sig, (vui128_t) l_exp);
	    }
	  else
	    {
	      x_bits = b_sig;
	      t_sig = (vui128_t) q_zero;
	    }
          // Collect any bits shifted-away, into a single carry bit
	  p_odd = vec_addcuq (x_bits, (vui128_t) q_ones);
	  // OR this into the X-bit
	  b_sig = (vui128_t) vec_or ((vui32_t) t_sig, (vui32_t) p_odd);
	}
 * \endcode
 * In this case, condition-1 means that, only b_sig needs right
 * shifting before significand addition.
 * The a_sig can be used directly as it had previously been
 * left shifted 3-bits where the GRX-bits were set to 0b000.
 *
 * A simpler case occurs when addition generates a carry. Here we
 * need to shift right 1-bit while preserving any nonzero X-bit.
 * For example:
 * \code
 *    // Check for carry and adjust
      if (vec_cmpuq_all_gt (s_sig, (vui128_t) sigov))
	{
	  p_odd = (vui128_t) vec_and ((vui32_t) s_sig, xbitmask);
	  s_sig = vec_srqi (s_sig, 1);
	  s_sig = (vui128_t) vec_or ((vui32_t) s_sig, (vui32_t) p_odd);
	  q_exp = vec_addudm (q_exp, q_one);
	}
 * \endcode
 * These two sequences preserve the X-bit going into the rounding stage.
 * See \ref f128_softfloat_IRRN_0_2_1
 * and \ref f128_softfloat_IRRN_0_2_6.
 *
 * \paragraph f128_softfloat_IRRN_0_1_3 IR for multiply
 * Quad-precision Multiply and Multiply-Add require quadword register
 * pairs to hold the <B>IR</B> product. The product of two 113-bit
 * significands requires 226-bits. This includes the product C-/L-bits
 * but we will need at least 3 additional bits for GRX.
 *
 * We can use operations from
 * vec_int128_ppc.h to produce the double quadword product.
 * By pre-adjusting the inputs before the
 * multiply we can align the split between the high 114-bits
 * (right justified) and low 112-bits (left justified)
 * of the product to align with the high and low quadword registers.
 * For example:
 * \code
      // Pre-align the multiply inputs so that the product is split
      // with the CL-Fraction-bits in high_sig, and GRX-bits in low_sig.
      a_sig = vec_slqi (a_sig, 8);
      b_sig = vec_slqi (b_sig, 8);
      low_sig = vec_muludq (&high_sig, a_sig, b_sig);
 * \endcode
 * The high_sig includes the C-/L-bits and high-order 112-bits of
 * product fraction.
 * The low_sig includes the low order 112-bits of the fraction
 * followed by 16-bits of zero.
 *
 * Most of the low-order fraction bits contribute to the X-bit.
 * So we can normally delay collecting x-bits until the rounding stage.
 * Only the most extreme denormals will result in a right shift large
 * enough to require intervention to preserve bits that would otherwise
 * be shifted away.
 *
 * After normalization the high-order bits of low_sig become the
 * GRX-bits. Actually we can treat the low order 126-bits as
 * uncollected x-bits. We use this to simplify the rounding process.
 * For this case we can manipulate low_sig to generate a carry
 * for rounding up/down/odd and then add/subtract/or this carry
 * to high_sig to produce the rounded product significand.
 * See \ref f128_softfloat_IRRN_0_2_1
 * and \ref f128_softfloat_IRRN_0_2_6.
 *
 * \subsubsection f128_softfloat_IRRN_0_2 Rounding for Quad-Precision
 *
 * The PowerISA support 6 rounding modes for Quad-Precision
 *
 * - Round to Nearest Even
 * - Round towards Zero
 * - Round towards +Infinity
 * - Round towards -Infinity
 * - Round to Nearest Away
 * - Round to Odd
 *
 * \note See
 * <a href="https://www.exploringbinary.com/gcc-avoids-double-rounding-errors-with-round-to-odd/">
*  GCC Avoids Double Rounding Errors With Round-To-Odd</a>
 *
 * The first four modes are encoded in the <B>FPSCR<sub>RN</sub></B>
 * rounding mode bits. The last two are encoded in instructions as
 * instruction local overrides.
 * The VSX Scalar Round to Quad-Precision Integer instruction can
 * override the RN and encode any of the six rounding modes.
 *
 * The rounding mode results are defined in terms of the intermediate
 * result (IR), and how close it is to the <I>representable result</I>,
 * based on the GRX-bits. The IR is either; exact,
 * closer to the next lower (NL) representable result,
 * Midway between, or
 * closer to the next Higher (NH) representable result,
 *
 *  | G | R | X | interpretation |
 *  |:-:|:-:|:-:|:----------------------------------|
 *  | 0 | 0 | 0 | IR is exact |
 *  | 0 | 0 | 1 | IR is closer to NL |
 *  | 0 | 1 | 0 | IR is closer to NL |
 *  | 0 | 1 | 1 | IR is closer to NL |
 *  | 1 | 0 | 0 | IR is midway between NL and NH |
 *  | 1 | 0 | 1 | IR is closer to NH |
 *  | 1 | 1 | 0 | IR is closer to NH |
 *  | 1 | 1 | 1 | IR is closer to NH |
 *
 * Next lower is effectively truncating the IR (setting GRX = 0b000),
 * while next higher will increment the significand by one.
 *
 * - Round to Nearest Even
 *  - If exact chose IR
 *  - Otherwise if IR is closer to NL, choose NL
 *  - Otherwise if IR is closer to NH, choose NH
 *  - Otherwise if IR in midway, choose whichever makes the result even.
 * - Round towards Zero
 *  - If exact chose IR
 *  - Otherwise, choose NL
 * - Round towards +Infinity
 *  - If exact chose IR
 *  - Otherwise if positive, choose NH
 *  - Otherwise if negative, choose NL
 * - Round towards -Infinity
 *  - If exact chose IR
 *  - Otherwise if positive, choose NL
 *  - Otherwise if negative, choose NH
 * - Round to Nearest Away
 *  - If exact chose IR
 *  - Otherwise if G = 0, choose NL
 *  - Otherwise if G = 1, choose NH
 * - Round to Odd
 *  - If exact chose IR
 *  - Otherwise, choose NL, and if G=1,  or R=1, or X=1,
 *  set the least significant bit to 1.
 *
 * \note The coding examples that follow are incomplete.
 * More examples will be provided are more operation are completed
 * and tested.
 *
 * The exact coding for rounding modes depends on how the <B>IR</B> is
 * represented in vector registers and this may differ by operation.
 * Conversions, addition, and subtraction tend to use a single
 * vector register where the GRX resides in the low-order bits.
 * While Multiplies generate double quadword results and so use
 * vector register pairs. Here the CL and Fraction bits are right
 * justified in a high_sig vector. While the low-order fraction/GRX
 * bits are left justified in a low_sig vector.
 *
 * \paragraph f128_softfloat_IRRN_0_2_1 Examples for Round to Nearest Even
 * Example for Convert to Quad-precision:
 * \code
      ...
      // We add 0x3fff to GRX-bits which may carry into low order sig-bit
      // This may result in a carry out from the L-bit into C-bit.
      q_carry = vec_addcuq (q_sig, (vui128_t) RXmask);
      q_sig = vec_adduqm (q_sig, (vui128_t) RXmask);
      // Generate a bool mask from the carry to use in the vsel
      qcmask = vec_setb_cyq (q_carry);
      // Two cases; 1) We did carry so shift (double) left 112 bits
      q_sigc = vec_sldqi (q_carry, q_sig, 112);
      // 2) no carry so shift right 15 bits
      q_sig = vec_srqi ((vui128_t) q_sig, 15);
      // Select which based on carry
      q_sig = (vui128_t) vec_sel ((vui32_t) q_sig, (vui32_t) q_sigc, (vui32_t) qcmask);
      // Increment the exponent based on the carry
      q_exp = vec_addudm (q_exp, (vui64_t) q_carry);
 * \endcode
 * This code runs about 16 instructions.
 *
 * Example for Add/Subtract Quad-precision:
 * \code
  // Round to nearest even from low_sig bits
  // For "round to Nearest, ties to even".
  // GRX = 0b001 - 0b011; truncate
  // GRX = 0b100 and bit-127 is odd; round up, otherwise truncate
  // GRX = 0b100 - 0b111; round up

  const vui32_t rmask = CONST_VINT128_W(0, 0, 0, 3);
  vui128_t p_rnd;
  // We can simplify by copying the low order fraction bit (p_odd)
  // and ADD it to the bit-X. This forces a tie into rounding up
  // if the current fraction is odd, making it even.
  // This simplifies to (s_sig + rmask + p_odd)
  // Rounding will generate a carry into the low order fraction bit
  // if and only if GRX > 0b100 or (GRX == 0b100) && (bit-127 == 1)

  p_tmp = vec_srqi (s_sig, 3);
  p_odd = (vui128_t) vec_and ((vui32_t) p_tmp, onemask);
  s_sig = vec_addecuq (s_sig, (vui128_t) rmask, p_odd);
  q_sig = vec_srqi (s_sig, 3);
 * \endcode
 * Adding 3 plus the fraction odd bit to thex GRX-bits generates
 * a carry into the low-order fraction bit for rounding.
 * In this case we use the extend form of add quadword to
 * effect a 3 way add.
 * After rounding convert the <B>IR</B> into a significand
 * by shifting right 3 bits.
 *
 * Example for Multiply Quad-precision:
 * \code
  const vui32_t rmask = CONST_VINT128_W(0x7fffffff, -1, -1, -1);
  vui128_t p_rnd;
  // We can simplify by copying the low order bit (p_odd) of high_sig
  // and OR it with the bit-X. This forces a tie into rounding up
  // if the current fraction is odd, making it even.
  // This simplifies to the carry from (low_sig + rmask + p_odd)
  // Rounding will generate a carry into p_rnd.
  // if and only if GRX > 0b100 or (GRX == 0b100) && (bit-127 == 1)
  p_odd = (vui128_t) vec_and ((vui32_t) high_sig, onemask);
  p_rnd = vec_addecuq (low_sig, (vui128_t) rmask, p_odd);
  q_sig =  vec_adduqm (high_sig, p_rnd);
 * \endcode
 * Here we take advantage of <I>uncollected x-bits</I> in low_sig.
 * Until we add the rmask and generate the carry, we can OR/ADD
 * p_odd to any bit in low_sig except the Guard or Round bits.
 * In this case we use the extend/carry form of add quadword to
 * effect a 3 way add and generate the carry/round bit.
 *
 * \note In the specific examples above the <B>and</B> with the
 * <I>onemask</I> is not required before use as the VRC operand of
 * vec_addecuq(). The instruction internally uses only bit[127]
 * for the carry-in on the extended quadword add.
 *
 * \paragraph f128_softfloat_IRRN_0_2_2 Examples for Round toward Zero
 * Example for Convert Quadword to Quad-precision:
 * \code
      // Simplest case, shift right 15 bits
      q_sig = vec_srqi ((vui128_t) q_sig, 15);
 * \endcode
 * This code runs about 3 instructions.
 *
 * \paragraph f128_softfloat_IRRN_0_2_6 Examples for Round to Odd
 * Example for Convert to Quad-precision.
 * \code
      const vui32_t RXmask = CONST_VINT128_W ( 0, 0, 0, 0x7fff);
      vui32_t q_odd;
      // For "Round to Odd".
      // If if G=1, or R=1, or X=1, Set least significant bit to 1.
      // Isolate GRX bit then add the mask.
      q_odd = vec_and ((vui32_t) q_siq, RXmask);
      // The add will generate a carry into bit 112, for non-zero GRX
      q_odd = vec_add (q_odd, RXmask);
      // Or this into bit 112 of the q_sig.
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_odd);
      q_sig = vec_srqi ((vui128_t) q_sig, 15);
 * \endcode
 * This code runs about 6 instructions to load the mask and round=odd.
 *
 * Example for Add/Subtract Quad-precision:
 * \code
      // Round to odd from low order GRX-bits
      p_tmp = vec_slqi (s_sig, 125);
      p_odd = vec_addcuq (p_tmp, (vui128_t) q_ones);
      q_sig = vec_srqi (s_sig, 3);
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, (vui32_t) p_odd);
 * \endcode
 *
 * Examples for Multiply Quad-precision.
 * For this case we can manipulate low_sig to generate a carry
 * for rounding up/down/odd and then add/subtract/or this carry
 * to high_sig to produce the rounded product significand.
 * For example round-to-odd:
 * \code
      // Round to odd from low_sig bits
      p_odd = vec_addcuq (low_sig, (vui128_t) q_ones);
      q_sig = (vui128_t)  vec_or ((vui32_t) high_sig, (vui32_t) p_odd);
 * \endcode
 *
 * \subsection f128_softfloat_0_0_1 Quad-Precision compares for POWER8
 *
 * IEEE floating-point compare is a bit more complicated than binary
 * integer compare operations. The two main complications are;
 * Not-a-Number (NaN) which IEEE insists are <I>unordered</I>,
 * and signed 0.0 where IEEE insists that -0.0 is equal to +0.0.
 * If you ignore the NaN and signed 0.0 cases you can treat
 * floating-point values as signed magnitude binary integers, and
 * use integer compares and boolean logic.
 * Which looks like this:
 *
 * - a \=<sup>f</sup> b \=\= (a \=<sup>s</sup> b)
 * - a \<<sup>f</sup> b \=\=  (a \>\=<sup>s</sup> 0 \& a \<<sup>s</sup> b)
 * \| (a \<<sup>s</sup> 0 \& a \><sup>u</sup> b)
 * - a \<\=<sup>f</sup> b \=\=  (a \>\=<sup>s</sup> 0  \& a \<\=<sup>s</sup> b)
 * \| (a \<<sup>s</sup> 0 \& a \>\=<sup>u</sup> b)
 *
 * Where;
 * \=<sup>f</sup>, \<<sup>f</sup>, and \<\=<sup>f</sup> are the desired
 * floating-point compares,
 * \=<sup>s</sup>, \<<sup>s</sup>, \<\=<sup>s</sup>, \><sup>s</sup>
 * and \>\=<sup>s</sup>, are signed integer compares,
 * and \=<sup>u</sup>, \><sup>u</sup>, and \>\=<sup>u</sup> are unsigned
 * integer compares.
 *
 * \see "Hacker's Delight, 2nd Edition,"
 * Henry S. Warren, Jr, Addison Wesley, 2013.
 * Chapter 17, Floating-point,
 * Section 17-3 Comparing Floating-Point Numbers Using Integer
 * Operations.
 *
 * One key implication of this is that we will need signed and unsigned
 * 128-bit compare operations. Instructions for 128-bit integer
 * compares was added for PowerISA 3.1 (POWER10) but we also need to
 * support POWER8/9. The good news is that PowerISA 2.07B (POWER8)
 * includes Vector Add/Subtract Modulo/Carry/Extend Quadword
 * instructions.
 * Specifically Vector Subtract & write Carry Unsigned Quadword
 * can implement all the unsigned ordered (<. <=, >, >=) compares by
 * manipulating the comparand order and evaluating the carry for
 * 0 or 1.
 *
 * POWER8 also includes vector doubleword integer compare instructions.
 * And the Vector Compare Equal To Unsigned Doubleword instruction can
 * be used with a few additional logical operations to implement 128-bit
 * equal and not equal operations.
 * These operations are already provided by vec_int128_ppc.h.
 *
 * Some additional observations:
 * - The unsigned compare equal can be used for unsigned or signed integers.
 * - (a \>\=<sup>s</sup> 0) \=\= ~(a \<<sup>s</sup> 0).
 *   - So we only need one compare boolean and the binary NOT.
 *   - ((a \>\=<sup>s</sup> 0) \& a \<<sup>s</sup> b)
 *   \| (~(a \>\=<sup>s</sup> 0) \& a \><sup>u</sup> b).
 *   - Now this starts to look like a vector select operation.
 *   - (src1 \& ~mask) \| (src2 \& mask)
 * - (a \>\=<sup>s</sup> 0) is a special case that only depends on
 * the sign-bit.
 *   - A unsigned compare can be used with a slight change,
 *   - Propagating the sign-bit across the (quad)word generates the
 *   same boolean. This is the vec_setb_sq() operation. The __float128
 *   variant is vec_setb_qp()
 *
 * \note The examples that follow, use vector __int128 parameters instead
 * of __binary128 to avoid the hassles of cast incompatibility between
 * scalar __binary128's and vector types. The actual implementations use
 * the xfer functions.
 *
 * \code
vb128_t
test_cmpltf128_v1c (vi128_t vfa128, vi128_t vfb128)
{
  vb128_t altb, agtb;
  vb128_t signbool;
  vb128_t result;

  // Replace (vfa >= 0) with (vfa < 0) == vec_setb_qp (vfa)
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t splat = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  signbool = (vb128_t) vec_sra (splat, shift);

  altb = vec_cmpltsq (vfa128, vfb128);
  agtb = vec_cmpgtuq ((vui128_t) vfa128, (vui128_t) vfb128);
  result = (vb128_t) vec_sel ((vui32_t)altb, (vui32_t)agtb, (vui32_t)signbool);
  return result;
}
 * \endcode
 *
 * Now we can tackle the pesky signed 0.0 case. The simplest method is to
 * add another term that test for either a or b is -0.0. This simplifies
 * to just logical a OR b and unsigned compare to -0.0.
 * Which looks like this:
 *
 * - a \=<sup>f</sup> b \=\= (a \=<sup>s</sup> b)
 * \| ((a \| b) \=\= 0x80000000...0)
 * - a \<<sup>f</sup> b \=\=  (a \>\=<sup>s</sup> 0 \& a \<<sup>s</sup> b)
 * \| ((a \<<sup>s</sup> 0 \& a \><sup>u</sup> b)
 * \& ((a \| b) \!\= 0x80000000...0))
 * - a \<\=<sup>f</sup> b \=\=  (a \>\=<sup>s</sup> 0  \& a \<\=<sup>s</sup> b)
 * \| ((a \<<sup>s</sup> 0 \& a \>\=<sup>u</sup> b)
 * \| ((a \| b) \=\= 0x80000000...0))
 *
 * Again we can replace signed compares (a >= 0) and (a < 0) with a
 * single vec_setb_qp() and simplify the boolean logic by using
 * vec_sel().
 * For the  ((a \| b) \!\= 0x80000000...0) term we can save an
 * instruction by replacing vec_cmpneuq() with vec_cmpequq() and
 * replacing the AND operation with AND compliment.
 * \code
vb128_t
test_cmpltf128_v2c (vi128_t vfa128, vi128_t vfb128)
{
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vb128_t altb, agtb, nesm;
  vui32_t or_ab;
  vb128_t signbool;
  vb128_t result;

  // Replace (vfa >= 0) with (vfa < 0) == vec_setb_qp (vfa)
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t splat = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  signbool = (vb128_t) vec_sra (splat, shift);

  altb = vec_cmpltsq (vfa128, vfb128);
  agtb = vec_cmpgtuq ((vui128_t) vfa128, (vui128_t) vfb128);

  or_ab = vec_or ((vui32_t) vfa128, (vui32_t) vfb128 );
  // For ne compare eq and and compliment
  nesm = vec_cmpequq ((vui128_t) or_ab, (vui128_t) signmask);
  agtb = (vb128_t) vec_andc ((vui32_t) agtb, (vui32_t) nesm);
  // select altb for 0's and agtb for 1's
  return (vb128_t) vec_sel ((vui32_t)altb, (vui32_t)agtb, (vui32_t)signbool);
}
 * \endcode
 * This sequence runs 27 instructions when you include the constant loads.
 *
 * An alternative compare method converts both floating-point values
 * in a way that a single (unsigned) integer compare can be used.
 *
 * \code
 // for each comparand
  if (n >= 0)
    n = n + 0x80000000;
  else
    n = -n;
 // Use unsigned integer comparison
 * \endcode
 * An interesting feature of this method is that +0.0 becomes
 * (0x00000000 + 0x80000000 = 0x80000000) and -0.0 becomes
 * (0x80000000 - 0x80000000 = 0x80000000) which effectively
 * converts any -0.0 into +0.0 for comparison. Signed 0.0 solved.
 *
 * Another optimization converts (n \= n + 0x80000000) to
 * (n \= n XOR 0x80000000). Gives the same result and for POWER8
 * a vec_xor() is 2 cycles latency vs 4 cycles for _vec_adduqm().
 *
 * \code
vb128_t
test_cmpltf128_v3d (vui128_t vfa128, vui128_t vfb128)
{
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  const vui8_t shift = vec_splat_u8 (7);

  vb128_t result;
  vb128_t age0, bge0;
  vui128_t vra, vrap, vran;
  vui128_t vrb, vrbp, vrbn;
  vui8_t splta, spltb;

  // signbool = vec_setb_qp;
  splta = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  age0 = (vb128_t) vec_sra (splta, shift);

  vrap = (vui128_t) vec_xor ((vui32_t) vfa128, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vfa128);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  spltb = vec_splat ((vui8_t) vfb128, VEC_BYTE_H);
  bge0 = (vb128_t) vec_sra (spltb, shift);

  vrbp = (vui128_t) vec_xor ((vui32_t) vfb128, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vfb128);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  return vec_cmpltuq (vra, vrb);
}
 * \endcode
 * This sequence runs (approximately) 20 instructions when you include
 * loading the required constants.
 * It also manages to use only splat-immediate forms to load
 * constants and so does not need to establish the TOC pointer nor
 * any address calculations to access constants via load instructions.
 *
 * The next IEEE issue is detecting NaNs and returning <I>unordered</I>
 * status. Adding the following code to a compare operation insures
 * that if either comparand is NaN; false (unordered) is returned for
 * compares (eq, lt, gt).
 *
 * \code
  if (vec_all_isnanf128 (vfa) || vec_all_isnanf128 (vfb))
    return (vb128_t) vec_splat_u32 (0);
 * \endcode
 * The pair of vec_all_isnanf128() operations add significant overhead
 * both in code size (39 instructions) and cycles. This form should
 * only be used if is required for correct results and
 * has not been tested by prior logic in this code path.
 * \sa The combined operation vec_all_isunorderedf128().
 *
 * \note At this point we are not trying to comply with PowerISA by
 * setting any FPSCR bits associated with Quad-Precision compare.
 * If such is required, VXSNAN and/or VXVC can be set
 * using the Move To FPSCR Bit 1 (mtfsb1) instruction.
 *
 * \subsection f128_softfloat_0_0_2 Quad-Precision converts for POWER8
 *
 * IEEE floating-point conversions are also a bit complicated.
 * Dealing with Not-a-Number (NaN), Infinities and subnormal is
 * part of it.
 * But the conversion may also require normalization and rounding
 * depending on element size and types involved.
 * Some examples:
 * - Double precision floats and long long integers can be represented
 * exactly in Quad precision float. But:
 * - Down conversions (to doubleword) from Quad-Precision may require
 *  rounding/truncation.
 *  - Conversions to integer that overflow are given special values.
 * - Conversions between QP and quadword integer may also require
 *  rounding/truncation.
 *   - 128-bit integer values may not fit into the QPs 113-bit
 *   significand.
 *
 * For PowerISA 3.0 (POWER9) includes full hardware instruction
 * support for Quad-Precision, Including:
 * - Conversions between Quad-Precison and Double floating-point
 * (xscvdpqp, xscvqpdp[o]).
 * - Conversions between Quad-Precison and doubleword and word integer
 * (xscvqpsdz, xscvqpswz, xscvqpudz, xscvqpuwz, xscvsdqp, xscvudqp).
 *
 * PowerISA 3.1 (POWER10) includes:
 * - Conversions between Quad-Precison and quadword integer
 * (xscvqpsqz, xscvqpuqz, xscvsqqp, xscvuqqp).
 *
 * For POWER8 (and earlier) we need to do a little more work
 * The general plan for conversion starts by disassembling the input
 * value into its parts and analyze.
 * For signed integer values disassemble usually means sign and
 * unsigned magnitude.
 * Analysis might be a range check or counting leading zeros.
 * For floating point values this is usually sign, exponent, and
 * significand.
 * Analysis usually means determining the data class (NaN, infinity,
 * normal, subnormal, zero) as each requires special handling in the
 * conversion.
 *
 * Conversion involves adjusting the <I>parts</I> as needed to match
 * the type of the result. This is normally only adds and shifts.
 * Finally we need to reassemble the parts based on the result type.
 * For integers this normally just converting the unsigned magnitude
 * to a signed '2's complement value based on the sign of the input.
 * For floating-point this requires merging the sign bit with the
 * (adjusted) significand and merging that with the (adjusted)
 * exponent.
 *
 * The good news is that all of the required operations are already
 * available in <B>altivec.h</B> or PVECLIB.
 *
 * \subsubsection f128_softfloat_0_0_2_0 Convert Double-Precision to Quad-Precision
 *
 * This is one of the simpler conversions as the conversion is always
 * exact (no rounding/truncation is required, and no overflow is
 * possible). The process starts with disassembling the
 * double-precision value.
 * \code
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);

  f64[VEC_DW_L] = 0.0; // clear the right most element to zero.
  // Extract the exponent, significand, and sign bit.
  d_exp = vec_xvxexpdp (f64);
  d_sig = vec_xvxsigdp (f64);
  q_sign = vec_and ((vui32_t) f64, signmask);
 * \endcode
 * We insure that the low-order doubleword of the vector f64 is zeroed.
 * This is necessary for then we normalize the 128-bit significand for
 * the quad-precision result.
 * The operations vec_xvxexpdp() and vec_xvxsigdp() are provided by
 * vec_f64_ppc.h supporting both the POWER9 instruction and equivalent
 * implementation for POWER8.
 * And finally we extract the sign-bit. We can't use the copysign()
 * here due to the difference in type.
 *
 * Now we analyze the data class of the double-precision input.
 * \code
  if (vec_all_isfinitef64 (f64))
    { // Not NaN or Inf
      if (vec_all_isnormalf64 (vec_splat (f64, VEC_DW_H)))
	{
	// ... adjust exponent and expand significand
	}
      else
	{ // Must be zero or subnormal
	  if (vec_all_iszerof64 (f64))
	    {
	      // ... copy zero exponent and significand
	    }
	  else
	    { // Must be subnormal
	      // ... normalize signifcand for QP and adjust exponent
	    }
	}
    }
  else
    { // isinf or isnan.
      // ... set exponent to QP max and expand significand
    }
 * \endcode
 * This code is arranged with an eye to the most common cases and
 * specifics of the conversion required by each data class.
 * The operations vec_all_isfinitef64(), vec_all_isnormalf64() and
 * vec_all_iszerof64() are provided by
 * vec_f64_ppc.h supporting both the POWER9 instruction and equivalent
 * implementation for POWER8.
 *
 * The normal case requires shifting the significand and adjusting the exponent.
 * \code
  const vui64_t exp_delta = (vui64_t) CONST_VINT64_DW ( (0x3fff - 0x3ff), 0 );
          ...
	  q_sig = vec_srqi ((vui128_t) d_sig, 4);
	  q_exp = vec_addudm (d_exp, exp_delta);
 * \endcode
 * The double significand has the fraction bits starting a bit-12 and
 * the implied '1' in bit-11. For quad-precision we need to shift this
 * right 4-bits to align the fraction to start in bit-16.
 * We need a quadword shift as the significand will now extend into
 * the high order bits of the second (low order) doubleword.
 * To adjust the exponent we need to convert the double biased exponent
 * (1 to 2046) into unbiased (-1022 to +1023) by subtracting the
 * exponent bias (+1023 or 0x3ff) value. Then we can add the
 * quad-precision exponent bias (+16383 or 0x3fff) to compute the
 * final exponent.
 * We can combine the bias difference into a single constant
 * (0x3fff - 0x3ff) and only need a single add at runtime.
 *
 * The operations vec_srqi() is provided by vec_int128_ppc.h and
 * vec_addudm() is provided by vec_int64_ppc.h.
 * We use PVECLIB operations here to insure that this code is safe to
 * use with older compilers and pre-POWER8 processors.
 *
 * The zero case requires setting the quad-precision significand and
 * exponent to zero.
 * \code
	      q_sig = (vui128_t) d_sig;
	      q_exp = (vui64_t) d_exp;
 * \endcode
 * We know that the double significand and exponent are zero, so just
 * assign them to the quad-precision parts. The sign bit will applied
 * later with the common insert exponent code.
 *
 * The subnormal case is a bit more complicated.
 * The tricky part is while the double-precision value is subnormal
 * the equivalent quad-precision value is not.
 * So we need to normalize the significand and compute a new exponent.
 * \code
    // Need to adjust the quad exponent by the f64 denormal exponent
    // (-1023) knowing that the f64 sig will have at least 12 leading '0's
    vui64_t q_denorm = (vui64_t) CONST_VINT64_DW ( (16383 - (1023 -12)), 0 );
    vui64_t f64_clz;
    f64_clz = vec_clzd (d_sig);
    d_sig = vec_vsld (d_sig, f64_clz);
    q_exp = vec_subudm (q_denorm, f64_clz);
    q_sig = vec_srqi ((vui128_t) d_sig, 15);
 * \endcode
 * We use a doubleword count leading zeros (ctz) and shift left to
 * normalize the significand so that the first '1'-bit moves to bit-0.
 * Then we compute the quad-precision exponent by subtracting the ctz
 * value from a constant (16383 - (1023 -12)).
 * This represents the quad-precision exponent bias, minus the
 * double-precsion exponent bias, minus the minimum leading zero count
 * for the double-precision subnormal significand.
 *
 * The NaN/Infinity case requires shifting the significand and setting
 * the exponent to quad-precision max.
 * \code
      q_sig = vec_srqi ((vui128_t) d_sig, 4);
      q_exp = (vui64_t) CONST_VINT64_DW (0x7fff, 0);
 * \endcode
 * We need this shift as NaN has a non-zero significand and it might
 * be nonzero in one of the low order bits.
 * Separating out the infinity case (where the significand is zero)
 * is not worth the extra (isnan) test to avoid the shift.
 *
 * Now that all the parts are converted to quad-precision, we are ready
 * to reassemble the QP result.
 * \code
  // Copy Sign-bit to QP significand before insert.
  q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
  // Insert exponent into significand to complete conversion to QP
  result = vec_xsiexpqp (q_sig, q_exp);
 * \endcode
 *
 * Putting this all together we get something like:
 * \code
  vui64_t d_exp, d_sig, q_exp;
  vui128_t q_sig;
  vui32_t q_sign;
  const vui64_t exp_delta = (vui64_t) CONST_VINT64_DW ( (0x3fff - 0x3ff), 0 );
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);

  f64[VEC_DW_L] = 0.0; // clear the right most element to zero.
  // Extract the exponent, significand, and sign bit.
  d_exp = vec_xvxexpdp (f64);
  d_sig = vec_xvxsigdp (f64);
  q_sign = vec_and ((vui32_t) f64, signmask);
  if (vec_all_isfinitef64 (f64))
    { // Not NaN or Inf
      if (vec_all_isnormalf64 (vec_splat (f64, VEC_DW_H)))
	{
	  q_sig = vec_srqi ((vui128_t) d_sig, 4);
	  q_exp = vec_addudm (d_exp, exp_delta);
	}
      else
	{ // zero or subnormal
	  if (vec_all_iszerof64 (f64))
	    {
	      q_sig = (vui128_t) d_sig;
	      q_exp = (vui64_t) d_exp;
	    }
	  else
	    { // Must be subnormal
	      vui64_t q_denorm = (vui64_t) CONST_VINT64_DW ( (0x3fff - 1023 -12), 0 );
	      vui64_t f64_clz;
	      f64_clz = vec_clzd (d_sig);
	      d_sig = vec_vsld (d_sig, f64_clz);
	      q_exp = vec_subudm (q_denorm, f64_clz);
	      q_sig = vec_srqi ((vui128_t) d_sig, 15);
	    }
	}
    }
  else
    { // isinf or isnan.
      q_sig = vec_srqi ((vui128_t) d_sig, 4);
      q_exp = (vui64_t) CONST_VINT64_DW (0x7fff, 0);
    }
  // Copy Sign-bit to QP significand before insert.
  q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
  // Insert exponent into significand to complete conversion to QP
  result = vec_xsiexpqp (q_sig, q_exp);
 * \endcode
 *
 * At this stage we have a functionally correct implementation and now
 * we can look for opportunities for optimization.
 * One issue is the generated code is fairly large
 * (~436 bytes and ~100 instructions).
 * For POWER8 the data class predicates (vec_all_isfinitef64, etc)
 * each require one or more vector constant loads and bit mask
 * operations before the associated vector compares.
 * Also the extract significand operation requires the equivalent of
 * isnormal (with two vector compares) as preparation for conditionally
 * restoring the implied (hidden) bit.
 *
 * By testing the extracted (exponent and significand) parts directly
 * we can simplify the compare logic and eliminate
 * some (redundant) vector constant loads. For example:
 * \code
  const vui64_t d_naninf = (vui64_t) CONST_VINT64_DW ( 0x7ff, 0 );
  const vui64_t d_denorm = (vui64_t) CONST_VINT64_DW ( 0, 0 );
  // ...
  // The extract sig operation has already tested for finite/subnormal.
  // So avoid testing isfinite/issubnormal again by simply testing
  // the extracted exponent.
  if (__builtin_expect (!vec_cmpud_all_eq (d_exp, d_naninf), 1))
    { // Not Nan or Inf
      if (__builtin_expect (!vec_cmpud_all_eq (d_exp, d_denorm), 1))
	{
	// ... adjust exponent and expand significand
	}
      else
	{ // Must be zero or subnormal
	  if (vec_cmpud_all_eq (d_sig, d_denorm))
	    {
	      // ... copy zero exponent and significand
	    }
	  else
	    { // Must be subnormal
	      // ... normalize signifcand for QP and adjust exponent
	    }
	}
    }
  else
    { // isinf or isnan.
      // ... set exponent to QP max and expand significand
    }
 * \endcode
 * The implementation based on this logic is smaller
 * (~300 bytes and ~75 instructions).
 * Performance results TBD.
 *
 * \subsubsection f128_softfloat_0_0_2_1 Convert Doubleword integer to Quad-Precision
 * Converting binary integers to floating point is simpler as there
 * are fewer data classes to deal with. Basically zero and non-zero
 * numbers (no signed 0s, infinities or NaNs).
 * Also the conversion from 64-bit integers to 128-bit floating-point
 * is always exact (there is no rounding).
 *
 * Unsigned doubleword is the simplest case. We only need to test for
 * binary zero. If zero just return a QP +0.0 constant. Otherwise
 * we can treat the binary magnitude as a denormalized number and
 * normalize it. The binary zero test and processing looks like this:
 * \code
  int64[VEC_DW_L] = 0UL; // clear the right most element to zero.
  // Quick test for 0UL as this case requires a special exponent.
  d_sig = int64;
  if (vec_cmpud_all_eq (int64, d_zero))
    { // Zero sign, exponent and significand.
      result = vec_xfer_vui64t_2_bin128 (d_zero);
    }
  else ...
 * \endcode
 * For the non-zero case we assume the binary point follows the unit
 * bit (bit-63) of the 64-bit magnitude.
 * Then we use count leading zeros to find the first significant bit.
 * This count is used to normalize/shift (left justify) the
 * magnitude and adjust the QP exponent to reflect the binary
 * point following the unit (original doubleword bit 63) bit.
 * So far we are using only doubleword data and instructions.
 * \code
    { // We need to produce a normalized QP, so we treat the integer
      // like a denormal, then normalize it.
      // Start with the quad exponent bias + 63 then subtract the count of
      // leading '0's. The 64-bit sig can have 0-63 leading '0's.
      vui64_t q_expm = (vui64_t) CONST_VINT64_DW ((0x3fff + 63), 0 );
      vui64_t i64_clz = vec_clzd (int64);
      d_sig = vec_vsld (int64, i64_clz);
      q_exp = vec_subudm (q_expm, i64_clz);
      q_sig = vec_srqi ((vui128_t) d_sig, 15);
      result = vec_xsiexpqp (q_sig, q_exp);
    }
 * \endcode
 * The high order bit (after normalization) will become the
 * <I>implicit</I> (hidden) bit in QP format. So we shift the quadword
 * right 15-bits to become the QP significand.
 * This shift includes the low order 64-bits we zeroed out early on
 * and zeros out the sign-bit as a bonus.
 * Finally we use vec_xsiexpqp() to merge the adjusted exponent and
 * significand.
 *
 * The signed doubleword conversion is bit more complicated.
 * We deal with zero case in the same way. Otherwise
 * we need to separate the signed doubleword into a sign-bit and
 * unsigned 64-bit magnitude. Which looks something like this:
 * \code
  const vui64_t d_zero = (vui64_t) CONST_VINT64_DW ( 0, 0 );
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  ...
      // Convert 2s complement to signed magnitude form.
      q_sign = vec_and ((vui32_t) int64, signmask);
      d_neg  = vec_subudm (d_zero, (vui64_t)int64);
      d_sign = (vui64_t) vec_cmpequd ((vui64_t) q_sign, (vui64_t) signmask);
      // Select the original int64 if positive otherwise the negated value.
      d_sig = (vui64_t) vec_sel ((vui32_t) int64, (vui32_t) d_neg, (vui32_t) d_sign);
 * \endcode
 * The normalization process is basically the same as unsigned but we
 * merge the sign-bit into the significant before inserting the
 * exponent.
 * \code
      // Count leading zeros and normalize.
      i64_clz = vec_clzd (d_sig);
      d_sig = vec_vsld (d_sig, i64_clz);
      q_exp = vec_subudm (q_expm, i64_clz);
      q_sig = vec_srqi ((vui128_t) d_sig, 15);
      // Copy Sign-bit to QP significand before insert.
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
      // Insert exponent into significand to complete conversion to QP
      result = vec_xsiexpqp (q_sig, q_exp);
 * \endcode
 *
 * \subsubsection f128_softfloat_0_0_2_2 Convert Quad-Precision to Quadword integer
 *
 * Convertions between quad-precision and quadword integers is
 * complicated by the fact that the QP significand is only 113-bits
 * while the quadword integer magnitude can be 127/128 bits.
 * It may not be possible to represent the quadword magnitude
 * exactly.
 * Conversions from quad-precision float to integer may have
 * nonzero fractions which require rounding/truncation.
 *
 * For POWER9 we have the
 * <B>VSX Scalar Convert with round to zero Quad-Precision to
 * Signed/Unsigned Doubleword
 * <I>(xscvqpsdz/xscvqpudz)</I></B> instructions.
 * For POWER10 we have the
 * <B>VSX Scalar Convert with round to zero Quad-Precision to
 * Signed/Unsigned Quadword
 * <I>(xscvqpsqz/xscvqpuqz)</I></B> instructions.
 * Conversion using other rounding modes require using
 * <B>VSX Scalar Round to Quad-Precision Integer <I>(xsrqpi)</I></B>
 * instruction.
 *
 * \note The <I>xsrqpi</I> instruction
 * allows for overriding the rounding mode as an immediate operand.
 * So a two instruction sequence can implement any of the four
 * <B>FPSCR<sub>RN</sub></B> rounding modes plus the fifth
 * (Round to Nearest Away) mode specific to floating point integer
 * instructions.
 *
 * For this example we will look at Convert with Round to Zero
 * Quad-Precision to Unsigned Quadword.
 * The POWER10 operation can be implemented as a single xscvqpuqz
 * instruction. For example:
 * \code
static inline vui128_t
vec_xscvqpuqz (__binary128 f128)
{
  vui128_t result;
#if defined (_ARCH_PWR10)  && (__GNUC__ >= 10)
  __asm__(
      "xscvqpuqz %0,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#else
...
#endif
  return result;
}
 * \endcode
 * We use in-line assembler here as there are no current or planed
 * compiler intrinsics for this and the C language only supports
 * conversions between __float128 and __int128 scalars.
 * The scalar conversions returns the __int128 result in GPR pair,
 * while we need the result in vector register.
 *
 * \note We could try to implement the POWER9 convert to quadword
 * operation using two xscvqpudz instructions (at 12-cycles each).
 * But this also requires two QP-multiplies (at 24-cycles each),
 * plus xscvudqp/xssubqp (at 12-cycles each).
 * So far it looks like using the POWER8 implementation for POWER9
 * will actually perform better.
 *
 * The POWER8 implementation looks like this:
 * \code
  vui64_t q_exp, q_delta, x_exp;
  vui128_t q_sig;
  vb128_t b_sign;
  const vui128_t q_zero = { 0 };
  const vui128_t q_ones = (vui128_t) vec_splat_s32 (-1);
  const vui64_t exp_low = (vui64_t) CONST_VINT64_DW ( 0x3fff, 0x3fff );
  const vui64_t exp_high = (vui64_t) CONST_VINT64_DW ( (0x3fff+128), (0x3fff+128) );
  const vui64_t exp_127 = (vui64_t) CONST_VINT64_DW ( (0x3fff+127), (0x3fff+127) );
  const vui64_t q_naninf = (vui64_t) CONST_VINT64_DW ( 0x7fff, 0x7fff );

  result = q_zero;
  q_exp = vec_xsxexpqp (f128);
  q_sig = vec_xsxsigqp (f128);
  x_exp = vec_splatd (q_exp, VEC_DW_H);
  b_sign = vec_setb_qp (f128);
  if (__builtin_expect (!vec_cmpud_all_eq (x_exp, q_naninf), 1))
    {
      if (vec_cmpud_all_ge (x_exp, exp_low)
       && vec_cmpud_all_eq ((vui64_t)b_sign, (vui64_t)q_zero))
	{ // Greater than or equal to 1.0
	  if (vec_cmpud_all_lt (x_exp, exp_high))
	    { // Less than 2**128-1
	      q_sig = vec_slqi (q_sig, 15);
	      q_delta = vec_subudm (exp_127, x_exp);
	      result = vec_srq (q_sig, (vui128_t) q_delta);
	    }
	  else
	    { // set result to 2**128-1
	      result = (vui128_t) q_ones;
	    }
	}
      else
	{ // less than 1.0 or negative
	  result = (vui128_t) q_zero;
	}
    }
  else
    { // isinf or isnan.
      vb128_t is_inf;
      // Positive Inf returns all ones
      // else NaN or -Infinity returns zero
      is_inf = vec_cmpequq (q_sig, (vui128_t) q_zero);
      // result = ~NaN | (pos & Inf) -> Inf & (pos & Inf) -> pos & Inf
      result = (vui128_t) vec_andc ((vui32_t) is_inf, (vui32_t) b_sign);
    }
 * \endcode
 * As is the usual for floating-point conversions, we extract the sign,
 * significand, and exponent then test for class and range.
 * We compare the extracted exponent directly using vector doubleword
 * compares. These are faster (on POWER8) than quadword compares but
 * require doubleword splatting the QP exponent and compare constants
 * for correct results. This only requires one additional instruction
 * (xxpermdi) as the vector constants will be loaded as quadwords
 * either way.
 *
 * The outer test is for NaN/Infinity. These should be rare so we use
 * __builtin_expect().  The implementation returns special values
 * to match the instruction definition.
 *
 * Once we know the value is finite, we check for greater than or equal
 * to +1.0. Negative or fractional values return quadword zero.
 * Then we check for less than 2<sup>128</sup>. If not we return
 * all ones (2<sup>128</sup> -1).
 *
 * If the input is in the valid range for unsigned quadword we
 * left-justify the significand then shift the quadword right by
 * (127 - <sub>unbiased</sub>exp). The right shift truncates
 * (round toward zero) any fractional bits. See vec_xscvqpuqz().
 *
 * The signed operation follows similar logic with appropriate
 * adjustments for negative values and reduced magnitude range.
 * The doubleword versions of the convert operation follows the
 * same outline with different range constants.
 * See vec_xscvqpsqz(), vec_xscvqpudz() and vec_xscvqpsdz().
 *
 * \subsubsection f128_softfloat_0_0_2_3 Convert Quadword integer to Quad-Precision
 *
 * Conversions from doubleword integer to quad-precision float can be
 * represented exactly and do not require any rounding.
 * But conversions from quadword integer to quad-precision float may
 * overflow the 113-bit significand which does require rounding.
 *
 * For POWER9 we have the
 * <B>VSX Scalar Convert Signed/Unsigned Doubleword to Quad-Precision format
 * <I>(xscvsdqp/xscvudqp)</I></B> instructions.
 * For POWER10 we have the
 * <B>VSX Scalar Convert with Round Signed/Unsigned Quadword to
 * Quad-Precision format
 * <I>(xscvsqqp/xscvuqqp)</I></B> instructions.
 * One of four rounding modes is selected from the 2-bit
 * <B>FPSCR.<sub>RN</sub></B> field.
 * The default rounding mode is <I>Round to Nearest Even</I>
 * which we will use in this example.
 * Convert using other rounding modes by changing the
 * <B>FPSCR.<sub>RN</sub></B> field.
 *
 * For example:
 * \code
__binary128
static inline vec_xscvuqqp (vui128_t int128)
{
  __binary128 result;
#if defined (_ARCH_PWR10)  && (__GNUC__ >= 10)
  __asm__(
      "xscvuqqp %0,%1"
      : "=v" (result)
      : "v" (int128)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  vui64_t int64 = (vui64_t) int128;
  __binary128 hi64, lo64;
  __binary128 two64 = 0x1.0p64;
  hi64 = int64[VEC_DW_H];
  lo64 = int64[VEC_DW_L];
  result = (hi64 * two64) + lo64;
#elif  defined (_ARCH_PWR8)
...
#endif
  return result;
}
 * \endcode
 * The POWER10 implementation uses the <B>xscvuqqp</B> instruction.
 * While POWER9 implementation uses <B>xscvudqp</B> instructions to
 * convert the high/low 64-bit halves of the quadword integer.
 * To complete the conversion we need to multiply the converted high
 * 64-bits by 2**64 than add the lower converted 64-bits.
 * The compiler should generate something like this:
 * \code
<test_vec_xscvuqqp_PWR9>:
     addis   r9,r2,0 ## R_PPC64_TOC16_HA   .rodata."0x1.0p64"
     addi    r9,r9,0 ## R_PPC64_TOC16_LO   .rodata."0x1.0p64"
     xxspltd v0,v2,1
     xscvudqp v2,v2
     xscvudqp v0,v0
     lxv     v1,0(r9)
     xsmaddqp v2,v0,v1
     blr
 * \endcode
 *
 * The POWER8 implementation looks like this:
 * \code
#elif  defined (_ARCH_PWR8)
  vui64_t q_exp;
  vui128_t q_sig;
  const vui128_t q_zero = (vui128_t) { 0 };
  const vui32_t lowmask = CONST_VINT128_W ( 0, 0, 0, 1);

  q_sig = int128;
  // Quick test for 0UL as this case requires a special exponent.
  if (vec_cmpuq_all_eq (q_sig, q_zero))
    {
      result = vec_xfer_vui128t_2_bin128 (q_zero);
    }
  else
    { // We need to produce a normal QP, so we treat the QW integer
      // like a denormal, then normalize it.
      // Start with the quad exponent bias + 127 then subtract the count of
      // leading '0's. The 128-bit sig can have 0-127 leading '0's.
      vui64_t q_expm = (vui64_t) CONST_VINT64_DW (0, (0x3fff + 127));
      vui64_t i64_clz = (vui64_t) vec_clzq (q_sig);
      q_sig = vec_slq (q_sig, (vui128_t) i64_clz);
      q_exp = vec_subudm (q_expm, i64_clz);
      // This is the part that might require rounding.
      // For example Round to Zero
      // Shift right 15-bits to normalize and truncate
      q_sig = vec_srqi ((vui128_t) q_sig, 15);
      //...
      q_exp = vec_swapd (q_exp);
      result = vec_xsiexpqp (q_sig, q_exp);
    }
 * \endcode
 * In this example the significand (including the L-bit) is right
 * justified in the high-order 113-bits of q_sig.
 * The guard, round, and sticky (GRX) bits are in the low-order
 * 15 bits.
 * The sticky-bits are the last 13 bits and are logically ORed
 * (or added to 0x1fff) to produce the X-bit.
 *
 * The signed quadword conversion is bit more complicated for both
 * POWER9/8. For example:
 * \code
__binary128
static inline vec_xscvsqqp (vi128_t int128)
{
  __binary128 result;
#if defined (_ARCH_PWR10)  && (__GNUC__ >= 10)
  __asm__(
      "xscvsqqp %0,%1"
      : "=v" (result)
      : "v" (int128)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  __binary128 hi64, lo64, i_sign;
  __binary128 two64 = 0x1.0p64;
  vui128_t q_sig;
  vui32_t q_sign;
  vui128_t q_neg;
  vb128_t b_sign;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  // Collect the sign bit of the input value.
  q_sign = vec_and ((vui32_t) int128, signmask);
  // Convert 2s complement to unsigned magnitude form.
  q_neg  = (vui128_t) vec_negsq (int128);
  b_sign = vec_setb_sq (int128);
  q_sig = vec_seluq ((vui128_t) int128, q_neg, b_sign);
  // generate a signed 0.0 to use with vec_copysignf128
  i_sign = vec_xfer_vui32t_2_bin128 (q_sign);
  // Convert the unsigned int128 magnitude to __binary128
  vui64_t int64 = (vui64_t) q_sig;
  hi64 = int64[VEC_DW_H];
  lo64 = int64[VEC_DW_L];
  result = (hi64 * two64) + lo64;
  // copy the __int128's sign into the __binary128 result
  result = vec_copysignf128 (i_sign, result);
#elif  defined (_ARCH_PWR8)
...
#endif
  return result;
}
 * \endcode
 * For POWER9
 * we can not just use the signed doubleword conversions for this case.
 * First we split the signed quadword into a 128-bit boolean
 * (representing the sign) and an unsigned quadword magnitude.
 * Then perform the unsigned conversion to QP format as for
 * vec_xscvuqqp(), And finally use vec_copysignf128() to insert the
 * original sign into the QP result.
 *
 * Similarly for POWER8:
 *
 * \code
 #elif  defined (_ARCH_PWR8)
  vui64_t q_exp;
  vui128_t q_sig;
  vui128_t q_neg;
  vui32_t q_sign;
  vb128_t b_sign;
  const vui128_t q_zero = (vui128_t) { 0 };
  const vui32_t lowmask = CONST_VINT128_W ( 0, 0, 0, 1);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);

  // Quick test for 0UL as this case requires a special exponent.
  if (vec_cmpuq_all_eq ((vui128_t) int128, q_zero))
    {
      result = vec_xfer_vui128t_2_bin128 (q_zero);
    }
  else
    { // We need to produce a normal QP, so we treat the integer like a
      // denormal, then normalize it.
      // Collect the sign bit of the input value.
      q_sign = vec_and ((vui32_t) int128, signmask);
      // Convert 2s complement to signed magnitude form.
      q_neg  = (vui128_t) vec_negsq (int128);
      b_sign = vec_setb_sq (int128);
      q_sig = vec_seluq ((vui128_t) int128, q_neg, b_sign);
      // Start with the quad exponent bias + 127 then subtract the count of
      // leading '0's. The 128-bit sig can have 0-127 leading '0's.
      vui64_t q_expm = (vui64_t) CONST_VINT64_DW (0, (0x3fff + 127));
      vui64_t i64_clz = (vui64_t) vec_clzq (q_sig);
      q_sig = vec_slq (q_sig, (vui128_t) i64_clz);
      q_exp = vec_subudm (q_expm, i64_clz);
      // This is the part that might require rounding.
      // For example Round to Zero
      // Shift right 15-bits to normalize and truncate
      q_sig = vec_srqi ((vui128_t) q_sig, 15);

      q_exp = vec_swapd (q_exp);
      // Copy Sign-bit to QP significand before insert.
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
      result = vec_xsiexpqp (q_sig, q_exp);
    }
 * \endcode
 *
 * \subsubsection f128_softfloat_0_0_2_x Convert Quad-Precision to Double-Precision
 * TBD
 *
 * \subsubsection f128_softfloat_0_0_2_y Round to Quad-Precision Integer
 * TBD
 *
 * \subsection f128_softfloat_0_0_3 Quad-Precision Arithmetic
 *
 * The POWER9 (PowerISA 3.0B) processor provides a full set of
 * Quad-Precision arithmetic operations; add, divide, multiply,
 * multiply-add/sub, and subtract.
 * The compilers (that support IEEE128) provide normal C-language
 * arithmetic operators for the __float128 (ISO _Float128) data type.
 * The compiler will generate in-line quad-precision instructions for
 * the (-mcpu=power9) target and calls to the (soft-float) runtime
 * for earlier processor targets.
 *
 * POWER9 also provides a round-to-odd override for these operations.
 * This helps software avoid <I>double rounding</I> errors
 * when rounding to smaller precision. These are supported with
 * compile built-ins (or in-line assembler).
 *
 * The compilers (that support IEEE128) provide a soft-float
 * implementation for POWER8. However the runtime does not support
 * round-to-odd as a rounding mode. Also the compiler built-ins for
 * round-to-odd arithmetic are disabled for POWER8 and earlier.
 *
 * As a minimum the PVECLIB implementation should
 * implement POWER8 equivalents to the compiler built-ins
 * supported for ISA 3.0.
 * This would include the explicit round-to-odd operations.
 *
 * \note PVECLIB supports 128-bit and 64-bit vector integer operations
 * for POWER7 but is limited by the original VMX integer word (32-bit)
 * element instructions. This will not provide any performance
 * advantage over a Fixed-Point 64-bit implementation using GPRs.
 *
 * \subsubsection f128_softfloat_0_0_3_1 Multiply Quad-Precision with Round-to-Odd.
 *
 * The PVECLIB implementation of
 * <B>Multiply Quad-Precision with Round-to-Odd</B>
 * will use the POWER9 xsmulqpo instruction if the compile target
 * supports it. Otherwise provide a POWER8 VSX implementation using
 * operations from vec_int128_ppc.h and vec_int64_ppc.
 * For example:
 * \code
__binary128
test_mulqpo_PWR9 (__binary128 vfa, __binary128 vfb)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 6)
#if defined (__FLOAT128__) && (__GNUC__ > 7)
  // Earlier GCC versions may not support this built-in.
  result = __builtin_mulf128_round_to_odd (vfa, vfb);
#else
  // If the compiler supports _ARCH_PWR9, must support mnemonics.
  __asm__(
      "xsmulqpo %0,%1,%2"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#endif
#else
  // Soft-float implementation
#endif
  return result;
}
 * \endcode
 * We prefer the compiler built-in (if available) but can substitute
 * in-line assembler if needed.
 * The built-in is subject to additional compiler optimizations
 * (like instruction scheduling) while in-line assembler is not.
 *
 * The PVECLIB soft-float implementation can leverage the 128-bit
 * vector registers and operations supported by POWER8.
 * The implementation starts with the usual exponent and significand
 * extraction and ends with merging the computed sign-bit with the
 * significand and inserting the computed exponent. For example:
 *
 * \code
  // Working variables and constants
  vui64_t q_exp, a_exp, b_exp, x_exp;
  vui128_t q_sig, a_sig, b_sig, p_sig_h, p_sig_l, p_odd;
  vui32_t q_sign,  a_sign,  b_sign;
  const vui32_t signmask = CONST_VINT128_W(0x80000000, 0, 0, 0);
  const vui64_t q_zero = { 0, 0 };
  const vui64_t q_ones = { -1, -1 };
  const vui64_t exp_bias = (vui64_t) CONST_VINT64_DW( 0x3fff, 0x3fff );
  const vi64_t exp_min = (vi64_t) CONST_VINT64_DW( 1, 1 );
  const vui64_t exp_dnrm = (vui64_t) CONST_VINT64_DW( 0, 0 );
  const vui64_t exp_naninf = (vui64_t) CONST_VINT64_DW( 0x7fff, 0x7fff );
  const vui64_t exp_max = (vui64_t) CONST_VINT64_DW( 0x7ffe, 0x7ffe );
  const vui32_t sigov = CONST_VINT128_W(0x0001ffff, -1, -1, -1);

  // Extract the exponent, significand, and sign-bit of each operand
  a_exp = vec_xsxexpqp (vfa);
  a_sig = vec_xsxsigqp (vfa);
  a_sign = vec_and_bin128_2_vui32t (vfa, signmask);
  b_exp = vec_xsxexpqp (vfb);
  b_sig = vec_xsxsigqp (vfb);
  b_sign = vec_and_bin128_2_vui32t (vfb, signmask);
  // Vectorize the DW biased exponents
  x_exp = vec_mrgahd ((vui128_t) a_exp, (vui128_t) b_exp);
  // Generate the product sign-bit
  q_sign = vec_xor (a_sign, b_sign);

// test for (vec_all_isfinitef128 (vfa) && vec_all_isfinitef128 (vfb))
  if (vec_cmpud_all_lt (x_exp, exp_naninf))
    { // Both operands are finite (normal, denormal, or zero)
    }
  else
    { // One or both operands are NaN or Infinity
    }
  // Merge sign, significand, and exponent into final result
  q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
  result = vec_xsiexpqp (q_sig, q_exp);
 * \endcode
 * Finite operands should be to the most common case. So it may help
 * the compiler to use __builtin_expect(). For example:
 * \code
  if (__builtin_expect (vec_cmpud_all_lt (x_exp, exp_naninf), 1))
    { // Both operands are finite (normal, denormal, or zero)
    }
  else
    { // One or both operands are NaN or Infinity
    }
 * \endcode
 *
 *
 * For finite operands we need to multiply the significands,
 * sum the (unbiased) exponents, normalize the product,
 * round, and check for exponent under/overflow.
 * We can use vec_muludq() to multiply two 128-bit unsigned values
 * returning 256-bit product in two (high/low) 128-bit vectors.
 *
 * The product of two 113-bit significands is up to
 * up to 226-bits (depending on inputs).
 * From this we only need 117-bits (C-bit, L-bit, 112 Fraction bits,
 * G-bit, R-bit, and X (sticky) bit. The X-bit is the logical OR of
 * all 110-bits to the right of the R-bit.
 * But we should not discard (or collapse) any sticky bits until after
 * normalization.
 *
 * We have some latitude on how we represent this product in vector
 * registers.
 * It is convenient for rounding if the high order 114-bits
 * (C, L, Fraction -bits) are right justified in the high vector.
 * While the low order 112-bit are left justified in in the low vector.
 * One way to accomplish this is:
 * \code
      a_sig = vec_slqi (a_sig, 8);
      b_sig = vec_slqi (b_sig, 8);
      p_sig_l = vec_muludq (&p_sig_h, a_sig, b_sig);
 * \endcode
 *
 * There are some special considerations for denormal and zero (+- 0.0)
 * operands. Both have a biased exponent of 0x0000 but we can't use
 * that to compute the product exponent.
 * If the either significand is zero then the product is zero. We can
 * short circuit this by returning the product sign-bit
 * (A<sup>sign</sup> XOR B<sup>sign</sup>) followed by 127 0b0s.
 *
 * Otherwise a denormal is <I>encoded</I> as a biased exponent
 * of 0x0000 and a nonzero (112-bit) fraction.
 * But the architecture defines a denormal as
 * 2<sup>Emin</sup> x (0.fraction). For Quad-Precision,
 * Emin is defined as -16382 which is a biased exponent of 0x0001.
 * So which value (0x0000 or 0x0001) is used to compute the
 * product exponent for the intermediate result?
 *
 * For denormal values we use Emin to compute the product
 * exponent before normalization.
 * This requires a small fix-up before computing the exponent.
 * We can optimize things by nesting the denormal fix-up under
 * the zero check.
 * \code
 *    // Splat exponents to both DWs
      a_exp = vec_splatd (a_exp, VEC_DW_H);
      b_exp = vec_splatd (b_exp, VEC_DW_H);
      // x_exp is the vectorized DW a_exp/b_exp
      if (vec_cmpud_any_eq (x_exp, exp_dnrm))
	{ // Involves zeros or denormals
	  // check for zero significands in multiply
	  if (vec_cmpuq_all_eq (a_sig, (vui128_t) q_zero)
	      || vec_cmpuq_all_eq (b_sig, (vui128_t) q_zero))
	    { // Multiply by zero, return QP signed zero
	      result = vec_xfer_vui32t_2_bin128 (q_sign);
	      return result;
	    }
	  else
	    { // Denormals present
	      vb64_t exp_mask;
	      // Convert zero encode to Emin for any denormal
	      exp_mask = vec_cmpequd (x_exp, exp_dnrm);
	      x_exp = (vui64_t) vec_sel (x_exp, (vui64_t) exp_min, exp_mask);
	      // Propagate Emin back to a_exp/b_exp
	      a_exp = vec_splatd (x_exp, VEC_DW_H);
	      b_exp = vec_splatd (x_exp, VEC_DW_L);
	    }
	}
//...
      // sum biased exponents for multiply
      q_exp = vec_addudm (a_exp, b_exp);
      q_exp = vec_subudm (q_exp, exp_bias);
 * \endcode
 * The exponent vector are splatted to both doublewords. We need the
 * exponent in the high order doubleword for input to vec_xsiexpqp().
 * We need the exponent in the low order doubleword for computing shift
 * amounts as input to vec_sldq(), vec_slq(), vec_srq().
 * Here we use biased exponents in the computation and result.
 * The addition will double the bias so we need to correct it by
 * subtracting the constant exp_bias.
 *
 * Again the zero multiply case is rare, so we can help the compiler
 * by using __builtin_expect(). Also we can replace the <B>else</B>
 * block with boolean select logic set exponents to <B>Emin</B> for
 * denormal operands. This also eliminates a redundant compare for
 * <B>exp_dnrm</B>.
 * \code
      // check for zero significands in multiply
      if (__builtin_expect (
	   (vec_all_eq ((vui32_t) a_sig, (vui32_t) q_zero)
         || vec_all_eq ((vui32_t) b_sig, (vui32_t) q_zero)), 0))
	{ // Multiply by zero, return QP signed zero
	  result = vec_xfer_vui32t_2_bin128 (q_sign);
	  return result;
	}
      // Convert zero encode to Emin for any denormal
      {
	vb64_t exp_mask;
	exp_mask = vec_cmpequd (x_exp, exp_dnrm);
	x_exp = vec_selud (x_exp, exp_min, (vb64_t) exp_mask);
	a_exp = vec_splatd (x_exp, VEC_DW_H);
	b_exp = vec_splatd (x_exp, VEC_DW_L);
      }
      // sum exponents
      q_exp = vec_addudm (a_exp, b_exp);
      q_exp = vec_subudm (q_exp, exp_bias);
 * \endcode
 *
 * The multiply may generate a carry in the intermediate result.
 * This can occur when both operands are normal and sufficiently large.
 * For example 1.5 x 1.5 = 2.25 (0x1.8 x 0x1.8 = 0x2.4).
 * Any product with the C-bit set requires normalization by shifting
 * one bit right and incrementing the exponent. For example:
 * \code
	  // Check for carry and adjust
	  if (vec_cmpuq_all_gt (p_sig_h, (vui128_t) sigov))
	    {
	      p_tmp = vec_sldqi (p_sig_h, p_sig_l, 120);
	      p_sig_h = vec_srqi (p_sig_h, 1);
	      p_sig_l = vec_slqi (p_tmp, 7);
	      q_exp = vec_addudm (q_exp, q_one);
	    }
 * \endcode
 * The shift sequence above is optimized for the case of a 1 bit
 * shift right double quadword for POWER8.
 *
 * This case is not rare at all so __builtin_expect() is not helpful
 * for this case. Also quadword compare is an expensive test and
 * requires a load of the sigov constant value. We only need to compare
 * the high order 16-bits of the significand to detect the
 * Carry/Leading bits.
 *
 * An alternative uses splat halfword to replicate the CL-bits
 * across the vector and use splat immediate halfword for the
 * compare value. For example:
 * \code
	vb128_t carry_mask;
	vui16_t sig_l_mask = vec_splat_u16 (1);
	vui16_t t_sig = vec_splat ((vui16_t) p_sig_h, VEC_HW_H);
	// Detect Carry bit
	carry_mask =  (vb128_t) vec_cmpgt (t_sig, sig_l_mask);
 * \endcode
 * Then use boolean select logic to propagate the right shifted
 * significand and incremented exponent if carry detected. For example:
 * \code
	// Shift double quadword right 1 bit
	p_tmp = vec_sldqi (p_sig_h, p_sig_l, 120);
	sig_h = vec_srqi (p_sig_h, 1);
	sig_l = vec_slqi (p_tmp, 7);
	// Increment the exponent
	x_exp = vec_addudm (q_exp, exp_one);
	// Select original or normalized exp/sig
	p_sig_h = vec_seluq (p_sig_h, sig_h, carry_mask);
	p_sig_l = vec_seluq (p_sig_l, sig_l, carry_mask);
	q_exp = vec_selud (q_exp, x_exp, (vb64_t) carry_mask);
 * \endcode
 *
 * Next we may need to deal with denormal results.
 * A intermediate result is considered <I>tiny</I> if the exponent
 * is less than Emin.
 * If the result is still <I>tiny</I> after normalization and rounding
 * the result is denormal and we can set the product exponent to
 * 0x0000 before inserting it into the sign/significand for the final
 * result.
 *
 * The normalization of a <I>tiny</I> intermediate result is a little
 * complicated. The PowerUSA states:
 * > If the intermediate result is Tiny (i.e., the unbiased
 * > exponent is less than -16382) and UE=0, the
 * > significand is shifted right N bits, where N is the
 * > difference between -16382 and the unbiased
 * > exponent of the intermediate result. The exponent
 * > of the intermediate result is set to the value
 * > -16382.
 *
 * This requires two tests. For example:
 * \code
      if (vec_cmpsd_all_lt ((vi64_t) q_exp, (vi64_t) exp_min))
	{
	  // Intermediate result is tiny, unbiased exponent < -16382
	  //
	  // ...
        }

      if (vec_cmpuq_all_le (p_sig_h, (vui128_t) sigovt))
	{ // Not tiny but ...
	  // Signicand is below normal range. This can happen when
	  // multiplying a denormal by a normal.
	  // So try to normalize the significand.
	  // ...
        }
 * \endcode
 * Again both cases are rare, so we can help the compiler
 * by using __builtin_expect().
 * Also the second test is a quadword compare requiring another
 * quadword constant. Both expensive compared the alternative.
 *
 * We only need to compare the high order 16-bits of the significand
 * to detect the a zero Leading bit.
 * Again use splat halfword to replicate the CL-bits
 * across the vector and compare to zero. For example:
 * \code
	  // Isolate sig CL bits and compare
	  vui16_t t_sig = vec_splat ((vui16_t) p_sig_h, VEC_HW_H);
	  if (__builtin_expect ((vec_all_eq (t_sig, (vui16_t) q_zero)), 0))
 * \endcode
 *
 * There are some issues that need to be addressed as we (attempt to)
 * normalize tiny results.
 * We need to assure as we shift bits right,
 * any bits shifted out of the low order (112-bit) of the product,
 * are (effectively) accumulated in the sticky-bit (X-bit).
 * This is a consequence of deferred X-bit (accumulated) as we pass
 * the whole 226-bit product (p_sig_h and p_sig_l) into the
 * rounding step.
 * This breaks down in to two cases:
 * - N is less than or equal to 116.
 * - N is greater than 116.
 *
 * The first case may leave significant bits in the fraction and the
 * GR-bits. But we must preserve any bits below GR in the sticky-bit.
 * In this case we accumulate and clear the low-order 109-bits of the
 * product and OR them into the X-bit.
 * \code
    const vui32_t xmask = CONST_VINT128_W(0x1fffffff, -1, -1, -1);
    // Propagate low order bits into the sticky bit
    // GRX is left adjusted in p_sig_l
    // Isolate bits below GDX (bits 3-128).
    tmp = vec_and ((vui32_t) p_sig_l, xmask);
    // generate a carry into bit-2 for any nonzero bits 3-127
    tmp = (vui32_t) vec_adduqm ((vui128_t) tmp, (vui128_t) xmask);
    // Or this with the X-bit to propagate any sticky bits into X
    p_sig_l = (vui128_t) vec_or ((vui32_t) p_sig_l, tmp);
    p_sig_l = (vui128_t) vec_andc ((vui32_t) p_sig_l, xmask);
 * \endcode
 * Now we can perform a double quadword shift right of up to 116
 * bits without losing any bits from the intermediate representation.
 * For example:
 * \code
    // Need a Double Quadword shift here, so convert right
    // shift into shift left double quadword for p_sig_l.
    l_exp = vec_subudm (exp_128, x_exp);
    p_sig_l = vec_sldq (p_sig_h, p_sig_l, (vui128_t) l_exp);
    // Complete right shift for p_sig_h
    p_sig_h = vec_srq (p_sig_h, (vui128_t) x_exp);
    q_sig = p_sig_h;
 * \endcode
 *
 * The second case implies that all bits of the intermediate
 * representation (bits CL through GR) will be shifted away and
 * can only be accumulated in the sticky (X) bit.
 * \code
    // Intermediate result is too tiny, the shift will
    // zero the fraction and the GR-bit leaving only the
    // Sticky bit. The X-bit needs to include all bits
    // from p_sig_h and p_sig_l
    p_sig_l = vec_srqi (p_sig_l, 8);
    p_sig_l = (vui128_t) vec_or ((vui32_t) p_sig_l, (vui32_t) p_sig_h);
    // generate a carry into bit-2 for any nonzero bits 3-127
    p_sig_l = vec_adduqm (p_sig_l, (vui128_t) xmask);
    q_sig = (vui128_t) q_zero;
    p_sig_l = (vui128_t) vec_andc ((vui32_t) p_sig_l, xmask);
 * \endcode
 *
 * There is another special case where the product exponent is
 * greater than or equal to <B>Emin</B> but the significand is not
 * in normal range.
 * Otherwise we can continue to the rounding process.
 *
 * This case can happen if a normal value is multiplied by a denormal.
 * The normalization process for this case is to:
 * > If the exponent is <B>Emin</B> then the result is denormal.
 * > Set the biased exponent to zero and continue to rounding.
 * >
 * > Otherwise shift the significand left and decrement the exponent,
 * > until the exponent equals <B>Emin</B> or the significant is in
 * > normal range.
 * For the second case we use count leading zeros to (vec_clzq()) to
 * compute the number bits (<I>c_exp</I>) we need to shift left to
 * get a normalized significand. We also compute the number of bits
 * (<I>d_exp</I>) we need to decrement the exponent to <B>Emin</B>.
 * Then we take the minimum (of <I>c_exp</I> and <I>d_exp</I>) as the
 * shift count. For example:
 * \code
    const vui64_t exp_15 = { 15, 15 };
    vui64_t c_exp, d_exp;
    vui128_t c_sig;
    c_sig = vec_clzq (p_sig_h);
    c_exp = vec_splatd ((vui64_t) c_sig, VEC_DW_L);
    // Adjust for leading bits before L-bit
    c_exp = vec_subudm (c_exp, exp_15);
    d_exp = vec_subudm (q_exp, (vui64_t) exp_min);
    d_exp = vec_minud (c_exp, d_exp);
 * \endcode
 * We use the minimum value (<I>d_exp</I>) to shift the significand
 * left. Then check if the result is still denormal.
 * If so set the result biased exponent to zero.
 * Otherwise compute the normalized exponent.
 * \code
    // Try to normalize the significand.
    p_sig_h = vec_sldq (p_sig_h, p_sig_l, (vui128_t) d_exp);
    p_sig_l = vec_slq (p_sig_l, (vui128_t) d_exp);
    // Compare computed exp to shift count to normalize.
    if (vec_cmpud_all_le (q_exp, c_exp))
      { // exp less than shift count to normalize so
	// result is still denormal.
	q_exp = q_zero;
      }
    else // Adjust exp after normalize shift left.
      q_exp = vec_subudm (q_exp, d_exp);
 * \endcode
 * Of course we can replace the exponent compare if-then-else with
 * select logic. For example:
 * \code
      vb64_t exp_mask;
      exp_mask = vec_cmpgtud (q_exp, c_exp);
      q_exp = vec_subudm (q_exp, d_exp);
      q_exp = vec_selud (exp_dnrm, q_exp, exp_mask);
 * \endcode
 *
 * The round-to-odd case to has some special considerations.
 * - Round-to-odd will never generate a Carry, so we don't need to
 * check.
 * - Exponent overflow does not return infinity. Instead it
 * returns __FLT128_MAX__ of the appropriate sign.
 *
 * \code
      // Round to odd from lower product bits
      p_odd = vec_addcuq (p_sig_l, (vui128_t) q_ones);
      q_sig = (vui128_t)  vec_or ((vui32_t) q_sig, (vui32_t) p_odd);

      // Check for exponent overflow -> __FLT128_MAX__
      if  (vec_cmpud_all_gt ( q_exp, exp_max))
      {
	// Intermediate result is huge, unbiased exponent > 16383
	// so return __FLT128_MAX__
	q_exp = exp_max;
	q_sig = (vui128_t) sigov;
      }
 * \endcode
 * Round-to-odd is a simple operation. We already have the significand
 * in a separate vector register from the GRX-bits.
 * So we use vec_addcuq() to generate a carry-bit if there are any
 * non-zero bits in the extended fraction. This carry bit is simply
 * ORed into the low order bit of the significand.
 *
 * Returning __FLT128_MAX__ for exponent overfloat is special case for
 * round-to-odd. Other rounding modes would return infinity.
 * Again this is a rare case and we can help the compiler by using
 * __builtin_expect(). For example:
 * \code
      if (__builtin_expect ((vec_cmpud_all_gt ( q_exp, exp_max)), 0))
      {
	// Intermediate result is huge, unbiased exponent > 16383
	// so return __FLT128_MAX__ with the appropriate sign.
	const vui32_t f128_max = CONST_VINT128_W(0x7ffeffff, -1, -1, -1);
	vui32_t f128_smax = vec_or ((vui32_t) f128_max, q_sign);
	return vec_xfer_vui32t_2_bin128 (f128_smax);
      }
 * \endcode
 *
 *
 * \paragraph f128_softfloat_0_0_3_1_1 NaN and Infinity handling for Multiply
 * When operands are not finite (infinite or Not-a-Number) we have to
 * deal with a matrix of operand pairs and return specific result
 * values.
 *
 * \note The PowerISA provides a table with specified results for each
 * combination of operands.
 * See PowerISA 3.0B Table 82 Actions for xsmulqp[o].
 *
 * Implementing this matrix of results does not require much
 * computation, but does require conditional logic to separate all
 * the cases. Another complication is that the matrix above specifies
 * a <I>Default Quiet NaN</I> to be returned for any multiply of
 * Infinity by zero (ignoring the operand's sign).
 * In this case vec_const_nanf128() is returned while bypassing
 * normal vec_xsiexpqp() exit sequence.
 * All other cases must consider the operand's sign in the result.
 * For example:
 * \code
    { // One or both operands are NaN or Infinity
      if (vec_cmpuq_all_eq (a_sig, (vui128_t) q_zero)
	  && vec_cmpuq_all_eq (b_sig, (vui128_t) q_zero))
	{ // Both operands either infinity or zero
	  if (vec_cmpud_any_eq (x_exp, q_zero))
	    { // Infinity x Zero is Default Quiet NaN
	      return vec_const_nanf128 ();
	    }
	  else
	    { // Infinity x Infinity == signed Infinity
	      q_sign = vec_xor (a_sign, b_sign);
	      q_exp = a_exp;
	      q_sig = a_sig;
	    }
	}
      else
	{ // One or both operands are NaN
	  const vui32_t q_nan = CONST_VINT128_W(0x00008000, 0, 0, 0);
	  if (vec_all_isnanf128 (vfa))
	    { // vfa is NaN, return vfa as quiet NAN
	      q_sign = a_sign;
	      q_sig = (vui128_t) vec_or ((vui32_t) a_sig, q_nan);
	      q_exp = a_exp;
	    }
	  else if (vec_all_isnanf128 (vfb))
	    { // vfb is NaN, return vfb as quiet NAN
	      q_sign = b_sign;
	      q_sig = (vui128_t) vec_or ((vui32_t) b_sig, q_nan);
	      q_exp = b_exp;
	    }
	  else  // Not NaN, so Infinity and a Nonzero finite number
	    { // Return signed Infinity
	      q_sign = vec_xor (a_sign, b_sign);
	      q_sig = (vui128_t) q_zero;
	      q_exp = exp_naninf;
	    }
	}
    }
 * \endcode
 *
 *
 * \subsubsection f128_softfloat_0_0_3_2 Add Quad-Precision with Round-to-Odd.
 *
 * The PVECLIB implementation of
 * <B>Add Quad-Precision with Round-to-Odd</B>
 * will use the POWER9 xsaddqpo instruction if the compile target
 * supports it. Otherwise provide a POWER8 VSX implementation using
 * operations from vec_int128_ppc.h and vec_int64_ppc.
 * For example:
 * \code
__binary128
test_addqpo (__binary128 vfa, __binary128 vfb)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 7)
#if defined (__FLOAT128__) && (__GNUC__ > 8)
  // earlier GCC versions generate extra data moves for this.
  result = __builtin_addf128_round_to_odd (vfa, vfb);
#else
  // No extra data moves here.
  __asm__(
      "xsaddqpo %0,%1,%2"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#endif
#else
  // Soft-float implementation
#endif
  return result;
}
 * \endcode
 * We prefer the compiler built-in (if available) but can substitute
 * in-line assembler if needed.
 * The built-in is subject to additional compiler optimizations
 * (like instruction scheduling) while in-line assembler is not.
 *
 * The PVECLIB soft-float implementation can leverage the 128-bit
 * vector registers and operations supported by POWER8.
 * The implementation starts with the usual sign-bit, exponent and
 * significand extraction and ends with merging the computed sign-bit
 * with the significand and inserting the computed exponent.
 * For example:
 * \code
  vui64_t q_exp, a_exp, b_exp, x_exp;
  vui128_t q_sig, a_sig, b_sig, p_tmp, p_odd;
  vui128_t a_mag, b_mag;
  vui128_t s_sig, x_bits;
  vui32_t q_sign,  a_sign,  b_sign;
  vb128_t a_lt_b;
  const vui32_t signmask = CONST_VINT128_W(0x80000000, 0, 0, 0);
  const vui64_t q_zero = { 0, 0 };
  const vui64_t q_ones = { -1, -1 };
  const vi64_t exp_min = (vi64_t) CONST_VINT64_DW( 1, 1 );
  const vui64_t exp_dnrm = (vui64_t) CONST_VINT64_DW( 0, 0 );
  const vui64_t q_expnaninf = (vui64_t) CONST_VINT64_DW( 0x7fff, 0x7fff );
  const vui64_t q_expmax = (vui64_t) CONST_VINT64_DW( 0x7ffe, 0x7ffe );
  const vui32_t sigov = CONST_VINT128_W(0x000fffff, -1, -1, -1);
  const vui32_t sigovt = CONST_VINT128_W(0x0007ffff, -1, -1, -1);
  const vui32_t xbitmask = CONST_VINT128_W(0, 0, 0, 1);

  a_exp = vec_xsxexpqp (vfa);
  a_sig = vec_xsxsigqp (vfa);
  a_sign = vec_and_bin128_2_vui32t (vfa, signmask);
  b_exp = vec_xsxexpqp (vfb);
  b_sig = vec_xsxsigqp (vfb);
  b_sign = vec_and_bin128_2_vui32t (vfb, signmask);
  // Vectorize the DW biased exponents
  x_exp = vec_mrgahd ((vui128_t) a_exp, (vui128_t) b_exp);

//  if (vec_all_isfinitef128 (vfa) && vec_all_isfinitef128 (vfb))
//  The above can be optimized to the following
  if (__builtin_expect (vec_cmpud_all_lt (x_exp, q_expnaninf), 1))
    { // Both operands are finite (normal, denormal, or zero)
    }
  else
    { // One or both operands are NaN or Infinity
    }

  // Merge sign, significand, and exponent into final result
  q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
  result = vec_xsiexpqp (q_sig, q_exp);
 * \endcode
 *
 * Floating-point addition requires that operands are represented with
 * the same exponent before the add operation. This means shifting
 * the significand of the smaller magnitude right by the absolute
 * difference between the exponents. To reduce the effect of round-off
 * error this shift must preserve any low order bits (shifted away)
 * as the Guard, Round and Sticky bits for internal
 * <I>Intermediate Results (<B>IR</B>)</I>.
 * For addition the <B>IR</B> only needs 117-bits which can be
 * accommodated in a single 128-vector.
 * Significands (which includes the leading/implicit bit) can be
 * converted to <B>IR</B> form by shifting left 3-bits.
 * This still leaves room on the left for the carry-bit.
 * For example:
 * \code
      // Precondition the significands before add so the GRX bits
      // are in the least significant 3 bit.
      a_sig = vec_slqi (a_sig, 3);
      b_sig = vec_slqi (b_sig, 3);
 * \endcode
 * (See: \ref f128_softfloat_IRRN_0_1)
 * \note The sign bits are maintained as separate local variables
 * (a_sign, b_sign, q_sign) for the power8 VSX software implementation.
 *
 * Floating-point addition will add or subtract magnitudes depending
 * the signs of the operands.
 * If the signs are the same, simply add (unsigned quadword) the two
 * (<B>IR</B> format) operands.
 * Otherwise if the signs differ, simply subtract (unsigned quadword)
 * the smaller magnitude from the larger.
 * In this case the sign of the result is the sign of the larger
 * magnitude.
 *
 * This discussion implies that knowing the relative absolute magnitude
 * up front can simplify the implementation.
 * As we saw in \ref f128_softfloat_0_0_1 we can use quadword integer
 * compares if we know that QP values are finite.
 * By masking off sign-bit from the operands we can use
 * unsigned quadword compare to determine relative magnitude.
 * For example:
 * \code
      // Mask off sign bits so can use integers for magnitude compare.
      a_mag = (vui128_t) vec_andc_bin128_2_vui32t (vfa, signmask);
      b_mag = (vui128_t) vec_andc_bin128_2_vui32t (vfb, signmask);
      // If magnitude(b) >  magnitude(a) will need to swap a/b, later
      a_lt_b = vec_cmpltuq (a_mag, b_mag);
 * \endcode
 * We can also generate a sign difference mask we can use later.
 * For example:
 * \code
      vui32_t diff_sign;
      // If sign(vfa) != sign(vfb) will need to:
      // 1) Subtract instead of add significands
      // 2) Generate signed zeros
      q_sign = vec_xor (a_sign, b_sign);
      diff_sign = (vui32_t) vec_setb_sq ((vi128_t) q_sign);
 * \endcode
 * Again a denormal is <I>encoded</I> as a biased exponent
 * of 0x0000 and a nonzero (112-bit) fraction.
 * But the architecture defines a denormal as
 * 2<sup>Emin</sup> x (0.fraction). For Quad-Precision,
 * Emin is defined as -16382 which is a biased exponent of 0x0001.
 * We need to use Emin in the <B>IR</B> to to compute shift values.
 * This requires a small fix-up before computing the <B>IR</B>.
 * It is simpler at this point to apply Emin to both denormal and
 * QP 0.0 values.
 * \code
  // Vectorize the DW biased exponents
  x_exp = vec_mrgahd ((vui128_t) a_exp, (vui128_t) b_exp);
  //...
      // Correct exponent for zeros or denormals to E_min
	{
	  vb64_t exp_mask;
	  exp_mask = vec_cmpequd (x_exp, exp_dnrm);
	  x_exp = (vui64_t) vec_sel (x_exp, (vui64_t) exp_min, exp_mask);
	}
 * \endcode
 * We vectorize this fixup by merging a_exp/b_exp into a
 * vector doubleword x_exp and applying compare/select.
 *
 * Before diving into the add operation we simplify the code that
 * follows by swapping the internal representation of vfa/vfb if
 * if absolute magnitude of vfa is less than vfb.
 * \code
      // Now swap operands a/b if necessary so a has greater magnitude.
	{
	  vui128_t a_tmp = a_sig;
	  vui128_t b_tmp = b_sig;
	  vui64_t x_tmp = vec_swapd (x_exp);

	  q_sign = vec_sel (a_sign, b_sign, (vui32_t) a_lt_b);

	  x_exp = vec_sel (x_exp, x_tmp, (vui64_t) a_lt_b);
	  a_exp = vec_splatd (x_exp, VEC_DW_H);
	  b_exp = vec_splatd (x_exp, VEC_DW_L);
	  q_exp = a_exp;

	  a_sig = vec_seluq (a_tmp, b_tmp, (vb128_t) a_lt_b);
	  b_sig = vec_seluq (b_tmp, a_tmp, (vb128_t) a_lt_b);
	}
 * \endcode
 *
 * At this point we can assume that:
 * - Exponents (a_exp, b_exp) are in the range E_min -> E_max
 * - And a_exp >= b_exp
 *   - If a_exp == b_exp then a_sig >= b_sig
 *   - If a_exp > b_exp then
 *     - shift (b_sig) right by (a_exp - b_exp)
 *     - any bits shifted out of b_sig are ORed into the X-bit
 * - The result exponent is usually a_exp +/-1
 *
 * \code
      if (vec_cmpud_all_lt (b_exp, a_exp))
	{
	  vui64_t d_exp, l_exp;
	  vui128_t t_sig;
	  const vui64_t exp_128 = (vui64_t) CONST_VINT64_DW( 128, 128 );
          // d_exp = a_exp - b_exp
	  d_exp = vec_subudm (a_exp, b_exp);
	  if (vec_cmpud_all_lt (d_exp, exp_128))
	    { // if (d_exp < 128) then shift b_sig right
	      t_sig = vec_srq (b_sig, (vui128_t) d_exp);
	      // retain bits shifted way for X-bits
	      l_exp = vec_subudm (exp_128, d_exp);
	      x_bits = vec_slq (b_sig, (vui128_t) l_exp);
	    }
	  else
	    { // else all significant bits shifted away
	      t_sig = (vui128_t) q_zero;
	      // retain original b_sig for X-bits
	      x_bits = b_sig;
	    }
          // reduce and X-bits for X-bit in IR
	  p_odd = vec_addcuq (x_bits, (vui128_t) q_ones);
	  // merge shifted b_sig with accumlated X-bit
	  b_sig = (vui128_t) vec_or ((vui32_t) t_sig, (vui32_t) p_odd);
	}
 * \endcode
 * The inner if/then/else can be replaced with select logic.
 * For example:
 * \code
	  vb128_t exp_mask = (vb128_t) vec_cmpltud (d_exp, exp_128);
	  l_exp = vec_subudm (exp_128, d_exp);
	  t_sig = vec_srq (b_sig, (vui128_t) d_exp);
	  x_bits = vec_slq (b_sig, (vui128_t) l_exp);
	  t_sig = vec_seluq ((vui128_t) q_zero, t_sig, exp_mask);
	  x_bits = vec_seluq (b_sig, x_bits, exp_mask);
 * \endcode
 *
 * Now we are ready to compute the significand. For example:
 * \code
      // If operands have the same sign then s_sig = a_sig + b_sig
      // Otherwise s_sig = a_sig - b_sig
      add_sig = vec_adduqm (a_sig, b_sig);
      sub_sig = vec_subuqm (a_sig, b_sig);
      s_sig = vec_seluq (add_sig, sub_sig, (vb128_t) diff_sign);
 * \endcode
 * Now handle the special case of a zero (0.0) result.
 * Either added two zero operands or a subtraction
 * (equal operands with different signs) produced an exact zero result.
 * Exact zero results always have an positive sign,
 * otherwise return return the result with same sign as vfa.
 * For example:
 * \code
      if (vec_cmpuq_all_eq (s_sig, (vui128_t) q_zero))
	{ // Special case of both zero with different sign
	  q_sign = vec_sel (a_sign, (vui32_t) q_zero, diff_sign);
	  return vec_xfer_vui32t_2_bin128 (q_sign);
	}
 * \endcode
 * Next check for overflow/carry.
 * In this case we need to shift the significand right one bit and
 * increment the exponent by one.
 * Also insure that and X-bit is not lost (shifted away) before we
 * get to the rounding stage.
 * For example:
 * \code
      // Check for carry and adjust
      else if (vec_cmpuq_all_gt (s_sig, (vui128_t) sigov))
	{
	  p_odd = (vui128_t) vec_and ((vui32_t) s_sig, xbitmask);
	  s_sig = vec_srqi (s_sig, 1);
	  s_sig = (vui128_t) vec_or ((vui32_t) s_sig, (vui32_t) p_odd);
	  q_exp = vec_addudm (q_exp, q_one);
	}
 * \endcode
 * Otherwise check for underflow (C- and L-bits are 0b0) which
 * requires normalization and may result in a denormal value.
 * Use count leading zeros to estimate the required normalizing
 * left shift.
 * The <B>IR</B> internal representation with normally have 12
 * leading zeros and so we need to adjust for that.
 * Also we need to prevent shifting beyond denormal range
 * (q_exp <= E_min), so use the minimum of leading zero count and
 * the delta between the current (q_exp) exponent and E_min.
 * For example:
 * \code
      // Or the significand is below normal range.
      // This can happen with subtraction (different signs).
      else if (vec_cmpuq_all_le (s_sig, (vui128_t) sigovt))
	{
	  const vui64_t exp_12 = { 12, 12 };
	  vui64_t c_exp, d_exp;
	  vui128_t c_sig;

	  c_sig = vec_clzq (s_sig);
	  c_exp = vec_splatd ((vui64_t) c_sig, VEC_DW_L);
	  c_exp = vec_subudm (c_exp, exp_12);
	  d_exp = vec_subudm (q_exp, (vui64_t) exp_min);
	  d_exp = vec_minud (c_exp, d_exp);
	  // ...
	}
 * \endcode
 * The overflow/underflow test above are expensive quadword integer
 * compares and quadword constants.
 * However all that is required is testing the C/L-bits.
 * We can convert these tests into byte compares.
 * For example:
 * \code
      // The C/L-bits are shifted 3-bit left like the significand
      // So the C/L-bits would be constants 16 and 8.
      // But splat-immediate can't generate a +16, so we subtract 1
      // from each and adjust the compares to compensate.
      const vui8_t t_sig_L = vec_splat_u8 (7);
      const vui8_t t_sig_C = vec_splat_u8 (15);
      // ...
      // Issolate CL bits from significand too simplify the compare
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      vui8_t t_sig = vec_splat ((vui8_t) s_sig, 14);
#else
      vui8_t t_sig = vec_splat ((vui8_t) s_sig, 1);
#endif
        // Check for carry and adjust
	if (vec_all_gt (t_sig, t_sig_C))
	{
	  // ...
	}
      // else check for underflow
      else if (vec_all_le (t_sig, t_sig_L))
	{
	  // ...
	}
 * \endcode
 *
 * If the current exponent is greater then  E_min then we can
 * safely use the minimum shift count to:
 * - Normalize the significand by left shift and adjust the exponent.
 * - If the adjusted exponent is <= E_min then return denormal by
 *   setting the biased exponent to zero (q_zero).
 *
 * For example:
 * \code
      // ...
	  if (vec_cmpsd_all_gt ((vi64_t) q_exp, exp_min))
	    {
	      s_sig = vec_slq (s_sig, (vui128_t) d_exp);
	      if (vec_cmpud_all_le (q_exp, c_exp))
		{
		  // Exponent is less than the required normalization
		  // so return denormal result
		  q_exp = q_zero;
		}
	      else
		q_exp = vec_subudm (q_exp, d_exp);
	    }
	  else
	    {
	      // Exponent is less than or equal to E_min
	      // so return denormal result
	      q_exp = q_zero;
	    }
 * \endcode
 * This can be simplified using boolean select logic.
 * Here we replace vec_cmpud_all_le() with vec_cmpgtud() and vec_sel()
 * which generates smaller faster code.
 * For example:
 * \code
      // ...
      	  if (vec_cmpsd_all_gt ((vi64_t) q_exp, exp_min))
	    {
	      vb64_t exp_mask = vec_cmpgtud (q_exp, c_exp);

	      s_sig = vec_slq (s_sig, (vui128_t) d_exp);
	      q_exp = vec_subudm (q_exp, d_exp);
	      q_exp = (vui64_t) vec_sel (q_zero, (vui64_t) q_exp, exp_mask);
	    }
	  else // ...
 * \endcode
 * After normalization we are ready to round the <B>IR</B>,
 * where round-to-odd is the simplest case.
 * For example:
 * \code
      // Round to odd from low order GRX-bits
      p_tmp = (vui128_t) vec_and ((vui32_t) s_sig, grx_mask);
      p_odd = vec_addcuq (p_tmp, (vui128_t) q_ones);
      q_sig = vec_srqi (s_sig, 3);
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, (vui32_t) p_odd);
 * \endcode
 * After rounding we need one more check for overflow.
 * For round-to-odd we will not see an significand overflow
 * (none-zero C-bit) but we may have overflowed the exponent range.
 *
 * For normal rounding modes, exponent overflow would generate an
 * infinity. However round-to-odd is a special case that returns
 * the maximum finite value <B>__FLT128_MAX__</B>.
 * For example:
 * \code
      // Check for exponent overflow -> __FLT128_MAX__
      if (vec_cmpud_all_gt (q_exp, q_expmax))
	{
	  // return maximum finite exponent and significand
	  q_exp = q_expmax;
	  q_sig = (vui128_t) sigov;
	}
 * \endcode
 * This ends the overall path for finite operands.
 * the last step will merge the sign bit with the significand then
 * insert the exponent.
 * For example:
 * \code
  // Merge sign, significand, and exponent into final result
  q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
  result = vec_xsiexpqp (q_sig, q_exp);
 * \endcode
 *
 *\paragraph f128_softfloat_0_0_3_2_1 NaN and Infinity handling for Add
 * If either operand is a NaN or infinity special handling is required.
 * See PowerISA 3.0B Table 53 Actions for xsaddqp[o].
 *
 * The PowerISA specifies that the xsaddqpo operation
 * returns a quiet NaN if either operand is a NaN:
 * - <B>src1</B> is a NaN, convert <B>src1</B> to a quiet NaN
 *   and return that value.
 * - <B>src1</B> is not a NaN, convert <B>src2</B> to a quiet NaN
 *   and return that value.
 *
 * For example:
 * \code
	  // One or both operands are NaN
	  const vui32_t q_nan = CONST_VINT128_W(0x00008000, 0, 0, 0);
	  if (vec_all_isnanf128 (vfa))
	    {
	      // vfa is NaN
	      q_sign = a_sign;
	      q_sig = (vui128_t) vec_or ((vui32_t) a_sig, q_nan);
	      q_exp = a_exp;
	    }
	  else if (vec_all_isnanf128 (vfb))
	    {
	      // vfb is NaN
	      q_sign = b_sign;
	      q_sig = (vui128_t) vec_or ((vui32_t) b_sig, q_nan);
	      q_exp = b_exp;
	    }
	  else  //
 * \endcode
 * The sign. significand, and exponent are combined into a
 * __float128 quiet NaN and returned.
 *
 * Otherwise one or both operands are infinity.
 * Addition of an infinity and a finite or two infinities
 * (of the same sign) returns infinity.
 * But subtraction of infinities (additional of different signs)
 * returns a default quiet NaN. For example:
 * \code
 *        // else one or both operands are Infinity
	    {
	      if (vec_cmpud_all_eq (x_exp, q_expnaninf)
		  && vec_cmpud_any_ne ((vui64_t) a_sign, (vui64_t) b_sign))
		{ // Both operands infinity and opposite sign
		  // Inifinty + Infinity (opposite sign) is Default Quiet NaN
		  return vec_const_nanf128 ();
		}
	      else
		{ // Either both operands infinity and same sign
		  // Or one infinity and one finite
		  if (vec_cmpud_any_eq (a_exp, q_expnaninf))
		    {
		      // return infinity
		      return vfa;
		    }
		  else
		    {
		      // return infinity
		      return vfb;
		    }
		}
	    }
 * \endcode
 *
 *
 * \subsubsection f128_softfloat_0_0_3_3 Subtract Quad-Precision with Round-to-Odd.
 *
 * The PVECLIB implementation of
 * <B>Subtract Quad-Precision with Round-to-Odd</B>
 * will use the POWER9 xssubqpo instruction if the compile target
 * supports it. Otherwise provide a POWER8 VSX implementation
 * based on the PVECLIB <B>Add Quad-Precision</B> implementation.
 * This is based on the observation that;
 * - floating-point addition becomes subtraction if the signs differ,
 * and
 * - negation of the second operand allows use of the existing add
 * operation to perform subtraction.
 *
 * For example:
 * \code
 __binary128
test_vec_subqpo_V0 (__binary128 src1, __binary128 src2)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 7)
#if defined (__FLOAT128__) && (__GNUC__ > 8)
  // earlier GCC versions generate extra data moves for this.
  result = __builtin_subf128_round_to_odd (src1, src2);
#else
  // No extra data moves here.
  __asm__(
      "xssubqpo %0,%1,%2"
      : "=v" (result)
      : "v" (src1), "v" (src2)
      : );
#endif
#else  // defined (_ARCH_PWR8)
  __binary128 nsrc2 = vec_negf128 (src2);
  return vec_xsaddqpo (src1, nsrc2);
}
 * \endcode
 * Unfortunately it is not that simple. The PowerISA specifies that
 * the xssubqpo operation must return the original <B>src2</B> operand
 * as a quiet NaN if:
 * - <B>src2</B> is a NaN, and
 * - <B>src1</B> is not a NaN.
 *
 * See PowerISA 3.0B Table 95 Actions for xssubqp[o].
 *
 * To insure compatible results for POWER8 and POWER9 implementations
 * we need to avoid negation if <B>src2</B> is a NaN.
 * For example:
 * \code
 __binary128
test_vec_subqpo_V1 (__binary128 src1, __binary128 src2)
{
// ...
#else  // defined (_ARCH_PWR8)
  __binary128 nsrc2;

  nsrc2 = vec_self128 (vec_negf128 (src2), src2, vec_isnanf128(src2));
  return vec_xsaddqpo (src1, nsrc2);
}
 * \endcode
 * This requires a relatively expensive (~14 cycle) vec_isnanf128()
 * test and adds ~12 instructions to the vec_xssubqpo() operation.
 * But this does product correct results for all operand combinations.
 *
 * \note This assumes that
 * vec_xsaddqpo() is fully inlined into vec_xssubqpo().
 *
 * To get POWER8 an implementation of
 * <B>Subtract Quad-Precision with Round-to-Odd</B>
 * with better performance we need to inject
 * the <B>src2</B> sign negation strategically into a copy of the
 * <B>Add Quad-Precision</B> implementation.
 * This should be after;
 * - extracting the sign, exponent and significand
 * from the <B>Quad-Precision</B> source operands, and
 * - testing for finite, NaN, and infinity
 *
 * Once we know that neither operand is NaN we can simply flip the
 * sign bit for source operand 2.
 * For example once we know that both operands are finite:
 * \code
  const vui32_t signmask = CONST_VINT128_W(0x80000000, 0, 0, 0);
//  if (vec_all_isfinitef128 (vfa) && vec_all_isfinitef128 (vfb))
//  The above can be optimized to the following
  if (__builtin_expect (vec_cmpud_all_lt (x_exp, q_expnaninf), 1))
    {
      // Negate sign for subtract, then use add logic
      b_sign = vec_xor (signmask, b_sign);
      // ...
 * \endcode
 *
 *
 * \paragraph f128_softfloat_0_0_3_3_1 NaN and Infinity handling for Subtract
 * The other case is the <B>else</B> leg of the finite case where at
 * least one of the operands is NaN or infinity.
 * For example once we know the none of the operands are NaN:
 * \code
          // ... NaNs
	  else  // Or one or both operands are Infinity
	    {
	      if (vec_cmpud_all_eq (x_exp, q_expnaninf)
		  && vec_cmpud_all_eq ((vui64_t) a_sign, (vui64_t) b_sign))
		{ // Both operands infinity and same sign
		  // Inifinty - Infinity (same sign) is Default Quiet NaN
		  return vec_const_nanf128 ();
		}
	      else
		{ // Either both operands infinity and different sign
		  // Or one infinity and one finite
		  if (vec_cmpud_any_eq (a_exp, q_expnaninf))
		    {
		      // return infinity
		      return src1;
		    }
		  else
		    {
		      // return -src2 (infinity)
		      return vec_negf128(src2);
		    }
		}
	    }
 * \endcode
 * The changes for vec_xssubqpo() from vec_xsaddqpo() are:
 * - When both operands are infinity and the same sign, then
 * return the default quiet NaN.
 * For vec_xsaddqpo() this tests for signs not equal via
 * vec_cmpud_any_ne().
 * - Otherwise if <B>src1</B> is finite and <B>src2</B> is infinity then
 * return negative <B>src2</B>.
 *
 * \subsection f128_softfloat_0_0_4 Constants and Masks for Quad-Precision Soft-float
 * The implementation examples above require a number of __binary128,
 * vector __int128, and vector long long constants. These are used as
 * (special) return values, binary masks, and integer constants for
 * comparison and arithmetic.
 *
 * In this case finite \_\_float128 (AKA \_\_binary128) constants are
 * provided by the compiler (via -mfloat128) if supported.
 * Literal constants can be defined using the <B>'q'/'Q'</B> suffix.
 * The compile also provides macros for the IEEE range values;
 * \_\_FLT128_MAX\_\_, \_\_FLT128_MIN\_\_, \_\_FLT128_EPSILON\_\_, and
 * \_\_FLT128_DENORM_MIN\_\_.
 * These macros define the appropriate literal constant using the 'Q'
 * suffix.
 *
 * The compiler also provides build-in functions for some special
 * constant values;
 * __builtin_infq (), __builtin_huge_valq (), _builtin_nanq (), and
 * __builtin_nansq ().
 * This assumes that you are using a compiler that supports \_\_float128
 * types and operations and is enabled (via -mfloat128).
 *
 * PVECLIB has to operated in an environment where the compiler's support
 * for IEEE Float128 is disabled, missing, or incomplete.
 * So this implementation may construct any \_\_binary128 values it may
 * need as vector constants then use the appropriate <I>xfer</I>
 * function to create the required scalar value.
 * For example:
 * \code
  static inline __binary128
  vec_const_inff128 ()
  {
    const vui32_t posinf = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

    return vec_xfer_vui32t_2_bin128 (posinf);
  }
 * \endcode
 *
 * PVECLIB provides splat operations vec_splat_s64() and
 * vec_splat_s128() for small integer constants.
 * For integers in the range -16 <-> 15 the implementation can use
 * vec_splat_s32() then sign extent word elements to doubleword or
 * quadword integer elements.
 * This has the advantage of shorter instruction latency and avoids a
 * vector load from storage. Any vector load has a chance of cache miss
 * and associated cache reload latencies.
 *
 * For constants outside of this range the implementation uses
 * vec_splats() for which the compiler is expected to load a vector
 * constant from storage.
 * See:
 * \ref i64_missing_ops_0_2_0
 * and
 * \ref int128_const_0_0_3
 *
 * \subsubsection f128_softfloat_0_0_4_1 Special Constants for Quad-Precision Soft-float
 *
 * Some special values and masks don't qualify as small integers and
 * need to be constructed as vector constants in hexadecimal.
 * For example:
 * \code
  const vui32_t signmask = CONST_VINT128_W(0x80000000, 0, 0, 0);
  const vui32_t magmask = CONST_VINT128_W (0x7fffffff, -1, -1, -1);
  const vui32_t sigmask = CONST_VINT128_W (0x0000ffff, -1, -1, -1);
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
  const vui32_t hiddenbit = CONST_VINT128_W (0x00010000, 0, 0, 0);
 * \endcode
 * For most of these values the compiler will generate a quadword
 * constant in the read-only data segment and generate a vector load
 * for any function that references that constant.
 *
 * One exception is the quadword sign mask which the GCC compiler
 * recognizes as special case (See: \ref int128_const_0_0_3_2).
 * This is a 4 instruction sequence of vector splat immediate words
 * and shift lefts. The nominal execution latency is 6 cycles.
 * The quadword sign mask can be used for both signed quadword integer
 * and quad-precision float values.
 *
 * It would be useful if we could identify similar (none load)
 * sequences for other mask values. The Quad-Precision Quiet NaN bit
 * mask <I>QNaNbit</I>) is similar to the <I>signmask</I>.
 * Quiet NaNs <B>QNaNs</B> represent the result of specific invalid
 * operations or when <I>Signaling NaNs</I> are (Operation Exception)
 * disabled. Quad-Precision Soft-float needs this mask to generate
 * default QNaNs or convert an operand of an invalid operations to
 * return a QNaN result.
 * See: \ref f128_softfloat_0_0_3_1_1,
 * \ref f128_softfloat_0_0_3_2_1, and
 * \ref f128_softfloat_0_0_3_3_1.
 *
 * Like the <I>signmask</I> it requires the generation of a 0x80000000
 * constant that can be shifted into position.
 * For example:
 * \code
  vui32_t
  test_mask128_f128Qbit (void)
  {
    //  const vui32_t QNaNbit = CONST_VINT128_W (0x00008000, 0, 0, 0);
    const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
    const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
    vui32_t QNaNbit;
    QNaNbit = vec_sl (q_ones, q_ones);
    return vec_sld (QNaNbit, q_zero, 10);
  }
 * \endcode
 * Unfortunately the compiler does not recognize this vector constant
 * as a special case. So PVECLIB provides it own implementation.
 * See: vec_mask128_f128Qbit().
 *
 * \paragraph f128_softfloat_0_0_4_1_1 Tricks with one's complement
 * For example the sign and magnitude masks are one's complements of
 * each other. This allows us to get the effect of the <I>magmask</I>
 * by reusing the <I>signmask</I> and using Vector AND with Complement.
 * For example:
 * \code
  // Mask off sign bits so can use quadword integer magnitude compare.
  a_mag = (vui128_t) vec_andc_bin128_2_vui32t (vfa, signmask);
  b_mag = (vui128_t) vec_andc_bin128_2_vui32t (vfb, signmask);
  // Extract the sign bit.
  a_sign = vec_and_bin128_2_vui32t (vfa, signmask);
  b_sign = vec_and_bin128_2_vui32t (vfb, signmask);
 * \endcode
 *
 * The alternative is to use the <I>magmask</I> and use Vector AND with
 * Complement to extact the sign bits.
 * For example:
 * \code
  // Mask off sign bits so can use quadword integer magnitude compare.
  a_mag = (vui128_t) vec_and_bin128_2_vui32t (vfa, magmask);
  b_mag = (vui128_t) vec_and_bin128_2_vui32t (vfb, magmask);
  // Extract the sign bit.
  a_sign = vec_andc_bin128_2_vui32t (vfa, magmask);
  b_sign = vec_andc_bin128_2_vui32t (vfb, magmask);
 * \endcode
 *
 * This also applies to the vector select (vec_sel()) operation.
 * Vector selects is defined as bit-wise (vra \& ~vrc) \| (vrb \& vrc)
 * where vrc is the select mask.
 * So the complement of the select mask can be used by swapping select
 * inputs vra/vrb.
 *
 * Using the complement has no advantage unless we can generate the
 * <I>magmask</I> with a shorter/faster (than <I>signmask</I>)
 * sequence. Or if the complement is used elsewhere (in the function)
 * and using the complement (for both/multiple operations) allows the
 * implementation to eliminated an expensive constant load.
 *
 * So lets look at what we can do to generate the magnitude mask
 * (<I>magmask</I>).
 * For example we can shift the <I>all ones</I> vector right one bit:
 * \code
  vui32_t
  test_mask128_f128mag (void)
  {
    //  const vui32_t magmask = CONST_VINT128_W (0x7fffffff, -1, -1, -1);
    const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
    return (vui32_t) vec_srqi ((vui128_t) q_ones, 1);
  }
 * \endcode
 * which generates:
 * \code
0000000000001560 <test_mask128_f128mag>:
     vspltisw v0,-1
     vspltisb v2,1
     vsr     v2,v0,v2
     blr
 * \endcode
 * This sequence only requires 3 instructions and a nominal execution
 * latency of 4 cycles. The vector splat immediate instructions are
 * independent and can execute in parallel.
 * So this technique for generating <I>magmask</I> and using it instead
 * of <I>signmask</I> is advantageous. Also a clear advantage versus
 * loading <I>magmask</I> as a vector constant (at 9+ cycles latency).
 *
 * \paragraph f128_softfloat_0_0_4_1_2 Other examples for special constants
 * Now lets look at how we might generate the significand mask
 * (<I>sigmask</I>.)
 * For example we can shift the <I>all ones</I> vector right 16-bits:
 *
 * \code
  vui32_t
  test_mask128_f128sig (void)
{
  //  const vui32_t sigmask = CONST_VINT128_W (0x0000ffff, -1, -1, -1);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
  // Equivalent to shift right 16-bits immediate.
  return vec_sld (q_zero, q_ones, 14);
}
 * \endcode
 * which generates:
 * \code
0000000000001580 <test_mask128_f128sig>:
     vspltisw v0,0
     vspltisw v2,-1
     vsldoi  v2,v0,v2,14
     blr
 * \endcode
 * Again this sequence only requires 3 instructions and a nominal
 * execution latency of 4 cycles. The vector splat immediate
 * instructions are independent and can execute in parallel.
 * So generating the <I>sigmask</I> this way has an advantage
 * over loading a vector constant.
 *
 * \note The quadword constant q_zero and q_ones are already recognized
 * by the compiler as a special case and commonly used in mask
 * generation. As such these constants should be subject to
 * common subexpression optimization and generated once per function.
 *
 * More examples of special constants are single-bit masks used to
 * compare/test/set the carry/leading bits of the significand.
 * These constants are easy to generate as halfword/word immediate
 * values but need to be shifted left into the high order halfword of
 * the quadword. For example:
 * \code
  vui32_t
  test_mask128_f128Lbit (void)
{
  //  const vui32_t hidden = CONST_VINT128_W (0x00010000, 0, 0, 0);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  vui32_t hidden = vec_splat_u32 (1);
  return vec_sld (hidden, q_zero, 14);
}
 * \endcode
 * which generates:
 * \code
0000000000001600 <test_mask128_f128Lbit>:
     vspltisw v2,0
     vspltisw v0,1
     vsldoi  v2,v0,v2,14
     blr
 * \endcode
 * Again this sequence only requires 3 instructions and a nominal
 * execution latency of 4 cycles. The vector splat immediate
 * instructions are independent and can execute in parallel.
 * We could use the splat halfword immediate for '1' here but word
 * constants are more commonly used and there is no advantage
 * (in instruction count or latency).
 * See: vec_splat_s64() and vec_splat_s128().
 *
 * Similarly for the carry bit:
 * \code
  vui32_t
  test_mask128_f128Cbit (void)
  {
    //  const vui32_t carry = CONST_VINT128_W (0x00020000, 0, 0, 0);
    const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
    vui32_t carry = vec_splat_u32 (2);
    return vec_sld (carry, q_zero, 14);
  }
 * \endcode
 *
 * \paragraph f128_softfloat_0_0_4_1_3 Some constants are trickier
 * Some constants are just more difficult to construct.
 * For example:
 * \code
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
  const vui64_t exp_naninf = CONST_VINT128_W (0, 0x7fff, 0, 0x7fff);
 * \endcode
 * These constants are used to extract/insert exponents
 * and to test operands for NaN/Infinity.
 * They are used early in many Quad-precision operations, so any delay
 * in getting these constants ready for use, will impact the
 * performance of the whole operation.
 * \note The <I>expmask</I> is also used to construct NaN/Infinity
 * values as special return values. But these cases should be rare
 * and so any delays in loading these should not cause significant
 * performance impact.
 *
 * The tricky part is generating any value with sequence of '1's with
 * '0's on both sides (not left or right justified). Especially within
 * the constraints imposed by POWER8 (PowerISA 2.07B) and earlier
 * machines. Any (none load) sequence require both a left and right
 * shift and any bit (versus octet) shifts will require additional
 * vector constants as shift counts.
 * For example:
 * \code
  vui32_t
  test_mask128_f128exp_V0 (void)
  {
    //  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
    const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
    const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
    vui32_t expmask;
    // generate {0xffff0000, 0, 0, 0} from q_ones/q_zero
    expmask = vec_sld (q_ones, q_zero, 14);
    // Then shift right halfword to get  {0x7fff0000, 0, 0, 0}
    expmask = (vui32_t) vec_srhi ((vui16_t) expmask, 1);
    return expmask;
  }
 * \endcode
 * Which generates:
 * \code
 0000000000001590 <test_mask128_f128exp_V0>:
     vspltisw v1,0
     vspltisw v0,-1
     vspltish v2,1
     vsldoi  v0,v0,v1,14
     vsrh    v2,v0,v2
     blr
 * \endcode
 * This is 5 instructions with a nominal latency of 6 cycles.
 * The halfword shift right requires its own halfword shift count which
 * is unlikely to find a common subexpression within Quad-precision
 * codes.
 *
 * Another possibility is use Vector Pack Pixel (<B>vpkpx</B>) to
 * convert a sequences of words (4x8-bit pixel channels) into a
 * sequence of halfwords (a leading 1-bit channel followed by
 * 3x5-bit channels).
 * The high order channel copies word bit 7 to halfword bit 0.
 * The next three channels are copied 5-bits at a time from the
 * high-order 5-bits of word bytes 1-3.
 * The pack pixel operation requires 8 word elements (256-bits from
 * 2 VRs) to generate 8 halfword elements (one 128-bit vector result).
 *
 * For example the word element constant 0x00ffffff generates the
 * halfword element 0x7fff.
 * The trick becomes to generate the vectors {0x00ffffff, 0, 0, 0}
 * and  {0, 0, 0, 0} using only vector immediate instructions.
 * Where possible we should use built-ins and values common in other
 * operations within PVECLIB and vec_f128_ppc.h.
 * This effectively limits us to the vector splat immediate and
 * Vector Splat Immediate Signed Word (vspltisw) and
 * Vector Shift Left Double by Octet Immediate (vsldoi).
 * For example:
 * \code
 vui32_t
  test_mask128_f128exp_V1 (void)
  {
    //  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
    const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
    const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
    vui32_t expmask;
    // Genrate {0x00ffffff, 0, 0, 0} from q_ones/q_zeros
    expmask = vec_sld (q_ones, q_zero, 13);
    expmask = vec_sld (expmask, expmask, 15);
    // Pack to Pixel to generate {0x7fff0000, 0, 0, 0}
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (vui32_t) vec_packpx (q_zero, expmask);
  #else
    return (vui32_t) vec_packpx (expmask, q_zero);
  #endif
  }
 * \endcode
 * Which generates:
 * \code
 0000000000001580 <test_mask128_f128exp_V1>:
     vspltisw v2,0
     vspltisw v0,-1
     vsldoi  v0,v0,v2,13
     vsldoi  v0,v0,v0,15
     vpkpx   v2,v0,v2
     blr
 * \endcode
 * This is actually worse than the previous example.
 * Still 5 instructions with a nominal latency of 6-8 cycles.
 * The last 3 instructions are serially dependent on both of the
 * q-ones/q_zero generation operations.
 * The second vsldoi in addition to vpkpx did not improve the
 * performance.
 *
 * \note The vec_packpx() intrinsic is is defined as endian sensitive
 * in the  <I>Power Vector Intrinsic Programming Reference</I>.
 * So we need the \_\_BYTE_ORDER\_\_ conditional to
 * generate the correct quadword magnitude for both endians.
 *
 * Time to look more closely at the <B>vpkpx</B> operation.
 * Perhaps we can take advantage of different handling of the
 * <I>auxiliary channel</I> versus the <I>color channels</I>.
 * If we can find a byte value where the low-order bit (bit-7) is '0'
 * while the high-order five bits are 0b11111.
 * For example the byte value 0xf8 (signed char -8) meets these requirements.
 * The word element 0xf8f8f8f8 will pack to the halfword pixel 0x7fff.
 * A vector of these word elements can be generated with vec_splat_s8(-8).
 *
 * We need the q_zero constant to clear the right three word elements
 * of the left vector and provide zeros to the right 128-bits of the
 * 256-bits required for <B>vpkpx</B>.
 * The q_ones constant is not required in this version.
 * For example:
 * \code
  vui32_t
  test_mask128_f128exp_V2 (void)
  {
    //  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
    const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
    vui32_t expmask;
    // Genrate {0xf8f8f8f8, 0, 0, 0}
    expmask = (vui32_t) vec_splat_s8 (-8);
    expmask = vec_sld (expmask, q_zero, 12);
    // Pack to Pixel and generate {0x7fff0000, 0, 0, 0}
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (vui32_t) vec_packpx (q_zero, expmask);
  #else
    return (vui32_t) vec_packpx (expmask, q_zero);
  #endif
  }
 * \endcode
 * Which generates:
 * \code
 0000000000001560 <test_mask128_f128exp_V2>:
     vspltisw v2,0
     vspltisb v0,-8
     vsldoi  v0,v0,v2,12
     vpkpx   v2,v0,v2
     blr
 * \endcode
 * This is better at 4 instructions with a nominal latency of 4-6
 * cycles.
 * The first two (splat immediate) instructions are independent and
 * can execute in parallel, but the vsldoi and vpkpx are serially
 * dependent on the results of both instructions.
 * The q-zero constant is likely to be shared with the generation of
 * other quadword constants. The vec_splat_s8(-8) may be shared with
 * the doubleword exp_naninf constant. See vec_mask64_f128exp().
 *
 * The doubleword form of the exponent mask (<I>exp_naninf</I>) is used
 * as a comparand when we need to test two Quad-precision operands for
 * main path (both finite) or for special case handling
 * (either is NaN/Infinity). We can use a similar technique to that above.
 *
 * The differences are that the mask is right (versus left) justified and
 * we are splating the mask to both doubleword.
 * For example:
 * \code
  vui64_t
  test_mask64_f128exp (void)
  {
    //const vui32_t exp_naninf = CONST_VINT128_W (0, 0x7fff, 0, 0x7fff);
    const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
    vui32_t expmask;

    // Genrate {0, 0, 0, 0xf8f8f8f8}
    expmask = (vui32_t) vec_splat_u8 (-8);
    expmask = vec_sld (q_zero, expmask, 4);
    // Pack to Pixel and generate {0, 0x7fff, 0, 0x7fff}
    return (vui64_t) vec_packpx (expmask, expmask);
  }
 * \endcode
 * Here we can skip the \_\_BYTE_ORDER\_\_ conditional as the
 * vec_packpx() operands are symmetrical.
 * The example above generates:
 * \code
 0000000000001560 <test_mask128_f128exp_V2>:
     vspltisw v2,0
     vspltisb v0,-8
     vsldoi  v2,v2,v0,4
     vpkpx   v2,v2,v2
     blr
 * \endcode
 * Again 4 instructions with a nominal latency of 4-6 cycles.
 *
 * Another set of difficult constants to generate are the small powers
 * of 2 (greater than 8).
 * Values 64 and 128 are used for range checks before normalization.
 * These values do not fit into the signed 5-bit (vector splat)
 * immediate fields available to POWER8 and earlier.
 *
 * The sign-bit constant can be converted to the value 128 with a
 * Vector Shift Left Double by Octet Immediate (vsldoi).
 * For example:
 * \code
  static inline vui32_t
  test_const128_f128_128(void)
  {
    //  const vui32_t signmask = CONST_VINT128_W(0, 0, 0, 128);
    const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
    const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
    vui32_t signmask;
    signmask = vec_sl (q_ones, q_ones);
    return vec_sld (q_zero, signmask, 1);
  }
 * \endcode
 * This runs 4 instruction with the q_zero/q_ones immediate constants
 * subject to common subexpression evaluation and sharing.
 * This yields a latency of 4 to 6 cycles.
 *
 * The doubleword form can be based on the above with an additional
 * instruction to splat the low doubleword. For example:
 * \code
  static inline vui64_t
  test_const64_f128_128(void)
  {
    //  const vui32_t dw_128 = CONST_VINT128_W(0, 0, 0, 128);
    const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
    const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
    vui32_t signmask;
    signmask = vec_sl (q_ones, q_ones);
    signmask = vec_sld (q_zero, signmask, 1);
    return vec_mrgald ((vui128_t) signmask, (vui128_t) signmask);
  }
 * \endcode
 * This works for VSX enable platforms (POWER7 and later) but bumps
 * the latency to 6-8 cycles.
 *
 * Another possibility is to use the Vector Count Leading Zeros
 * instructions to generate constants for the number of bits in the
 * element.
 * For example:
 * \code
  static inline vui64_t
  test_const64_f128_64(void)
  {
    //  const vui32_t dw_128 = CONST_VINT128_W(0, 64, 0, 64);
    const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
    // Generate {64, 64} from count leading zeros of {0, 0}
    return  vec_clzd((vui64_t) q_zero);
  }
 * \endcode
 * This requires only 2
 * instruction and the q_zero immediate constant is
 * subject to common subexpression evaluation and sharing.
 * This yields a latency of 2 to 4 cycles.
 *
 * \note This technique applies to all element sizes where
 * <B>Vector Count Leading Zeros</B> (vclz) are supported.
 * For POWER7 and earlier support byte/halfword/word (constants 8,16, and 32).
 * POWER8 supports Vector Count Leading Zeros Doubleword (vclzd) for constant 64.
 * So far (as of POWER10) there is not quadword (constant 128) form,
 * but for POWER9 we can use
 * <B>VSX Vector Splat Immediate Byte</B> and
 * <B>Vector Extend Sign Byte To Doubleword</B>.
 *
 * To generate the doubleword constant 128 we can generate the
 * constant 64 and double it. For example:
 * \code
  static inline vui64_t
  test_const64_f128_128(void)
  {
    //  const vui32_t dw_128 = CONST_VINT128_W(0, 128, 0,128);
    const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
    // Generate {64, 64} from count leading zeros of {0, 0}
    vui64_t dw64 = vec_clzd((vui64_t) q_zero);
    // Generate {128, 128}
    return vec_addudm (dw64, dw64);
  }
 * \endcode
 * This runs 3 instruction with the q_zero immediate constant
 * subject to common subexpression evaluation and sharing.
 * This yields a latency of 4 to 6 cycles.
 *
 *
 * \section f128_examples_0_0 Examples
 * For example: using the the classification functions for implementing
 * the math library function sine and cosine.
 * The Posix specification requires that special input values are
 * processed without raising extraneous floating point exceptions and
 * return specific floating point values in response.
 * For example the sin() function.
 * - If the input <I>value</I> is NaN then return a NaN.
 * - If the input <I>value</I> is +-0.0 then return <I>value</I>.
 * - If the input <I>value</I> is subnormal then return <I>value</I>.
 * - If the input <I>value</I> is +-Inf then return a NaN.
 * - Otherwise compute and return sin(value).
 *
 * The following code example uses functions from this header to
 * address the POSIX requirements for special values input to
 * sinf128():
 * \code
__binary128
test_sinf128 (__binary128 value)
{
  __binary128 result;

  if (vec_all_isnormalf128 (value))
    {
      // body of taylor series.
      ...
    }
  else
    {
      if (vec_all_isinff128 (value))
	result = vec_const_nanf128 ();
      else
	result = value;
    }
  return result;
}
 * \endcode
 *
 * For another example the cos() function.
 * - If the input <I>value</I> is NaN then return a NaN.
 * - If the input <I>value</I> is +-0.0 then return <I>1.0</I>.
 * - If the input <I>value</I> is +-Inf then return a NaN.
 * - Otherwise compute and return cos(value).
 *
 * The following code example uses functions from this header to
 * address the Posix requirements for special values input to
 * cosf128():
 * \code
__binary128
test_cosf128 (__binary128 value)
{
  __binary128 result;

  if (vec_all_isfinitef128 (value))
    {
      if (vec_all_iszerof128 (value))
	result = 1.0Q;
      else
	{
	  // body of taylor series ...
	}
    }
  else
    {
      if (vec_all_isinff128 (value))
	result = vec_const_nanf128 ();
      else
	result = value;
    }
  return result;
}
 * \endcode
 *
 * Neither example raises floating point exceptions or sets
 * <B>errno</B>, as appropriate for a vector math library.
 *
 * \section f128_perf_0_0 Performance data
 * High level performance estimates are provided as an aid to function
 * selection when evaluating algorithms. For background on how
 * <I>Latency</I> and <I>Throughput</I> are derived see:
 * \ref perf_data
 */

#ifndef VEC_F128_PPC_H_
#define VEC_F128_PPC_H_

#include <pveclib/vec_common_ppc.h>
#include <pveclib/vec_int128_ppc.h>
#include <pveclib/vec_f64_ppc.h>


/* __float128 was added in GCC 6.0.  But only with -mfloat128.
   Later compilers typedef __float128 to __ieee128 and
   long double to __ibm128. The intent was to allow the switch of
   long double from __ibm128 to __ieee128 (someday).

   Clang does not define __FLOAT128__ or __float128 without both
   -mcu=power9 and -mfloat128.
   So far clang does not support/define the __ibm128 type. */
#ifdef __FLOAT128__
typedef __float128 __Float128;
#ifndef __clang__
typedef __float128 __binary128;
typedef __float128 __ieee128;
typedef __ibm128 __IBM128;
#else
/* Clang started defining __FLOAT128__ and does not allow redefining
   __float128 or __ieee128. Worse it will give errors if you try to
   use either type. So define __binary128 as if __FLOAT128__ is not
   defined. */
typedef vui128_t __binary128;
/* Clang does not define __ibm128 over IBM long double.
   So defined it here. */
typedef long double __IBM128;
#endif
#else
/* Before GCC 6.0 (or without -mfloat128) we need to fake it.  */
/*! \brief vector of 128-bit binary128 element.
 *  Same as __float128 for PPC.  */
typedef vui128_t vf128_t;
/*! \brief Define __Float128 if not defined by the compiler.
 *  Same as __float128 for PPC.  */
typedef vf128_t __Float128;
/*! \brief Define __binary128 if not defined by the compiler.
 *  Same as __float128 for PPC.  */
typedef vf128_t __binary128;
#ifndef __clang__
// Clang will not allow redefining __float128 even is it not enabled
/*! \brief Define __float128 if not defined by the compiler.
 *  Same as __float128 for PPC.  */
typedef vf128_t __float128;
#endif
/*! \brief Define __IBM128 if not defined by the compiler.
 *  Same as old long double for PPC.  */
typedef long double __IBM128;
#endif

/*! \brief Union used to transfer 128-bit data between vector and
 * __float128 types. */
typedef union
     {
       /*! \brief union field of vector unsigned char elements. */
       vui8_t    vx16;
       /*! \brief union field of vector unsigned short elements. */
       vui16_t   vx8;
       /*! \brief union field of vector unsigned int elements. */
       vui32_t   vx4;
       /*! \brief union field of vector unsigned long long elements. */
       vui64_t   vx2;
       /*! \brief union field of vector unsigned __int128 elements. */
       vui128_t  vx1;
       /*! \brief union field of vector __bool __int128 elements. */
       vb128_t vbool1;
       /*! \brief union field of __float128 elements. */
       __binary128 vf1;
       /*! \brief union field of __int128 elements. */
       unsigned __int128 ix1;
     } __VF_128;

///@cond INTERNAL
static inline __binary128 vec_xfer_vui32t_2_bin128 (vui32_t f128);
static inline int vec_all_isnanf128 (__binary128 f128);
static inline vb128_t vec_isnanf128 (__binary128 f128);
static inline vb128_t vec_isunorderedf128 (__binary128 vfa, __binary128 vfb);
static inline vb128_t vec_setb_qp (__binary128 f128);
static inline __binary128 vec_xsiexpqp (vui128_t sig, vui64_t exp);
static inline vui64_t vec_xsxexpqp (__binary128 f128);
static inline vui128_t vec_xsxsigqp (__binary128 f128);
static inline vui64_t vec_xxxexpqpp (__binary128 vfa, __binary128 vfb);
///@endcond

/** \brief Generate doubleword splat constant 128.
 *
 * Load immediate the quadword constant vui32_t {0, 0, 0, 128}.
 *
 * \note See vec_mask64_f128exp() for rationale.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  4-6  | 1/cycle  |
 *
 *  @return The 128-bit const vui64_t {128, 128}.
 */
static inline vui64_t
vec_const64_f128_128(void)
{
  //  const vui32_t dw_128 = CONST_VINT128_W(0, 0, 0, 128);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
#if defined (_ARCH_PWR8)
  // Generate {64, 64} from count leading zeros of {0, 0}
  vui64_t dw64 = vec_clzd((vui64_t) q_zero);
  // Generate {128, 128}
  return vec_addudm (dw64, dw64);
#else
  const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
  vui32_t signmask;
  signmask = vec_sl (q_ones, q_ones);
  signmask = vec_sld (q_zero, signmask, 1);
  return vec_mrgald ((vui128_t) signmask, (vui128_t) signmask);
#endif
}

/** \brief Generate Quadword constant 128.
 *
 * Load immediate the quadword constant vui32_t {0, 0, 0, 128}.
 *
 * \note See vec_mask64_f128exp() for rationale.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  4-6  | 1/cycle  |
 *
 *  @return The 128-bit const vui32_t {0, 0, 0, 128}.
 */
static inline vui32_t
vec_const128_f128_128(void)
{
  //  const vui32_t signmask = CONST_VINT128_W(0, 0, 0, 128);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
  vui32_t signmask;
  signmask = vec_sl (q_ones, q_ones);
  return vec_sld (q_zero, signmask, 1);
}

/** \brief Generate Doubleword Quad-Precision exponent mask.
 *
 * Load the quadword constant vui32_t {0, 0x7fff, 0, 0x7fff}.
 *
 * \note The compiler, left to its own devices, will load vector
 * constants from the read-only data section (.rodata).
 * This requires (at least) 5 cycles assuming a L1 cache hit.
 * This does not include any instructions needed for address
 * calculation, alignment, or endian fix-up (another 4-8 cycles).
 * A L1 cache miss adds more (7-27) cycles to retrieve data from
 * the L2/L3 cache levels.
 * A short sequence, using only vector register and immediate operands,
 * eliminates load associated address calculation and cache misses.
 * This provides better and more repeatable performance.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  4-6  | 1/cycle  |
 *
 * \note The consistent use of q_zero/q_ones constants across these
 * operations allows the compiler to apply common subexpression
 * elimination (CSE) optimizations.
 *
 *  @return The 128-bit mask vui32_t {0x00010000, 0, 0, 0}.
 */
static inline vui64_t
vec_mask64_f128exp (void)
{
  //const vui32_t expmask = CONST_VINT128_W (0, 0x7fff, 0, 0x7fff);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  vui32_t expmask;
  expmask = (vui32_t) vec_splat_u8 (-8);
  expmask = vec_sld (q_zero, expmask, 4);
  return (vui64_t) vec_packpx (expmask, expmask);
}

/** \brief Generate Quadword Quad-Precision exponent mask.
 *
 * Load immediate the quadword constant vui32_t {0x7fff0000, 0, 0, 0}.
 *
 * \note See vec_mask64_f128exp() for rationale.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |   6   | 1/cycle  |
 *
 *  @return The 128-bit mask vui32_t {0x7fff0000, 0, 0, 0}.
 */
static inline vui32_t
vec_mask128_f128exp (void)
{
  //  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  vui32_t expmask;

  expmask = (vui32_t) vec_splat_u8 (-8);
  expmask = vec_sld (expmask, q_zero, 12);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return (vui32_t) vec_packpx (q_zero, expmask);
#else
  return (vui32_t) vec_packpx (expmask, q_zero);
#endif
}


/** \brief Generate Quadword Quad-Precision magnitude mask.
 *
 * Load immediate the quadword constant vui32_t {0x7fffffff, -1, -1, -1}.
 *
 * \note See vec_mask64_f128exp() for rationale.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  2-4  | 1/cycle  |
 *
 *  @return The 128-bit mask vui32_t {0x7fffffff, -1, -1, -1}.
 */
static inline vui32_t
vec_mask128_f128mag (void)
{
  //  const vui32_t magmask = CONST_VINT128_W (0x7fffffff, -1, -1, -1);
  const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
  return (vui32_t) vec_srqi ((vui128_t) q_ones, 1);
}

/** \brief Generate Quadword Quad-Precision significand mask.
 *
 * Load immediate the quadword constant vui32_t {0x0000ffff, -1, -1, -1}.
 *
 * \note See vec_mask64_f128exp() for rationale.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  2-4  | 1/cycle  |
 *
 *  @return The 128-bit mask vui32_t {0x0000ffff, -1, -1, -1}.
 */
static inline vui32_t
vec_mask128_f128sig (void)
{
  //  const vui32_t sigmask = CONST_VINT128_W (0x0000ffff, -1, -1, -1);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
  return vec_sld (q_zero, q_ones, 14);
}

/** \brief Generate Quadword Quad-Precision Sign-bit mask.
 *
 * Load immediate the quadword constant vui32_t {0x80000000, -1, -1, -1}.
 *
 * \note See vec_mask64_f128exp() for rationale.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  4-6  | 1/cycle  |
 *
 *  @return The 128-bit mask vui32_t {0x80000000, -1, -1, -1}.
 */
static inline vui32_t
vec_mask128_f128sign (void)
{
  //  const vui32_t signmask = CONST_VINT128_W(0x80000000, 0, 0, 0);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
  vui32_t signmask;
  signmask = vec_sl (q_ones, q_ones);
  return vec_sld (signmask, q_zero, 12);
}

/** \brief Generate Quadword C-bit mask Immediate.
 *
 * Load immediate the quadword constant vui32_t {0x00020000, 0, 0, 0}.
 *
 * \note See vec_mask64_f128exp() for rationale.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  2-4  | 1/cycle  |
 *
 *  @return The 128-bit mask vui32_t {0x00020000, 0, 0, 0}.
 */
static inline vui32_t
vec_mask128_f128Cbit (void)
{
  //  const vui32_t carry = CONST_VINT128_W (0x00020000, 0, 0, 0);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  vui32_t carry = vec_splat_u32 (2);
  return vec_sld (carry, q_zero, 14);
}

/** \brief Generate Quadword L-bit mask Immediate.
 *
 * Load immediate the quadword constant vui32_t {0x00010000, 0, 0, 0}.
 *
 * \note See vec_mask64_f128exp() for rationale.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  2-4  | 1/cycle  |
 *
 *  @return The 128-bit mask vui32_t {0x00010000, 0, 0, 0}.
 */
static inline vui32_t
vec_mask128_f128Lbit (void)
{
  //  const vui32_t hidden = CONST_VINT128_W (0x00010000, 0, 0, 0);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  vui32_t hidden = vec_splat_u32 (1);
  return vec_sld (hidden, q_zero, 14);
}

/** \brief Generate Quadword QNaN-bit mask Immediate.
 *
 * Load immediate the quadword constant vui32_t {0x00008000, 0, 0, 0}.
 *
 * \note See vec_mask64_f128exp() for rationale.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  4-6  | 1/cycle  |
 *
 *  @return The 128-bit mask vui32_t {0x00008000, 0, 0, 0}.
 */
static inline vui32_t
vec_mask128_f128Qbit (void)
{
  //  const vui32_t QNaNbit = CONST_VINT128_W (0x00008000, 0, 0, 0);
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
  vui32_t QNaNbit;
  QNaNbit = vec_sl (q_ones, q_ones);
  return vec_sld (QNaNbit, q_zero, 10);
}

 /** \brief Select and Transfer from one of two __binary128 scalars
  * under a 128-bit mask. The result is a __binary128 of the selected
  * value.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param vfa a __binary128 floating point scalar value.
 *  @param vfb a __binary128 floating point scalar value.
 *  @param mask a vector bool __int128
 *  @return The bit of vfa or vfb depending on the mask.
 */
 static inline __binary128
 vec_sel_bin128_2_bin128 (__binary128 vfa, __binary128 vfb, vb128_t mask)
 {
 #if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
      && !defined (_ARCH_PWR9)
   // Work around for GCC PR 100085
   __binary128 result;
 #ifdef __VSX__
   __asm__(
       "xxsel %x0,%x1,%x2,%x3"
       : "=wa" (result)
       : "wa" (vfa), "wa" (vfb), "wa" (mask)
       : );
 #else
   __asm__(
       "vsel %0,%1,%2,%3"
       : "=v" (result)
       : "v" (vfa), "v" (vfb), "v" (mask)
       : );
 #endif
   return result;
 #else
   __VF_128 ua, ub;
   vui32_t result;

   ua.vf1 = vfa;
   ub.vf1 = vfb;

   result = vec_sel (ua.vx4, ub.vx4, (vb32_t) mask);
   return vec_xfer_vui32t_2_bin128 (result);
 #endif
 }

 /** \brief Transfer a quadword from a __binary128 scalar to a vector int
  * and logical AND with a mask.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param f128 a __binary128 floating point scalar value.
 *  @param mask a vector unsigned int
 *  @return The original value ANDed with mask as a 128-bit vector int.
 */
 static inline vui32_t
 vec_and_bin128_2_vui32t (__binary128 f128, vui32_t mask)
 {
   vui32_t result;
 #if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9)
   // Work around for GCC PR 100085
 #ifdef __VSX__
   __asm__(
       "xxland %x0,%x1,%x2"
       : "=wa" (result)
       : "wa" (f128), "wa" (mask)
       : );
 #else
   __asm__(
       "vand %0,%1,%2"
       : "=v" (result)
       : "v" (f128), "v" (mask)
       : );
 #endif
 #else
   __VF_128 vunion;

   vunion.vf1 = f128;

   result = (vec_and (vunion.vx4, mask));
 #endif
   return result;
 }

 /** \brief Transfer a quadword from a __binary128 scalar to a vector int
  * and logical AND Compliment with mask.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param f128 a __binary128 floating point scalar value.
 *  @param mask a vector unsigned int
 *  @return The original value ANDed with mask as a 128-bit vector int.
 */
 static inline vui32_t
 vec_andc_bin128_2_vui32t (__binary128 f128, vui32_t mask)
 {
   vui32_t result;
 #if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9)
   // Work around for GCC PR 100085
 #ifdef __VSX__
   __asm__(
       "xxlandc %x0,%x1,%x2"
       : "=wa" (result)
       : "wa" (f128), "wa" (mask)
       : );
 #else
   __asm__(
       "vandc %0,%1,%2"
       : "=v" (result)
       : "v" (f128), "v" (mask)
       : );
 #endif
 #else
   __VF_128 vunion;

   vunion.vf1 = f128;

   result = (vec_andc (vunion.vx4, mask));
 #endif
   return result;
 }

 /** \brief Transfer a quadword from a __binary128 scalar to a vector int
  * and logical OR with mask.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param f128 a __binary128 floating point scalar value.
 *  @param mask a vector unsigned int
 *  @return The original value ORed with mask as a 128-bit vector int.
 */
 static inline vui32_t
 vec_or_bin128_2_vui32t (__binary128 f128, vui32_t mask)
 {
   vui32_t result;
 #if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9)
   // Work around for GCC PR 100085
 #ifdef __VSX__
   __asm__(
       "xxlor %x0,%x1,%x2"
       : "=wa" (result)
       : "wa" (f128), "wa" (mask)
       : );
 #else
   __asm__(
       "vor %0,%1,%2"
       : "=v" (result)
       : "v" (f128), "v" (mask)
       : );
 #endif
 #else
   __VF_128 vunion;

   vunion.vf1 = f128;

   result = (vec_or (vunion.vx4, mask));
 #endif
   return result;
 }

 /** \brief Transfer a quadword from a __binary128 scalar to a vector int
  * and logical Exclusive OR with mask.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param f128 a __binary128 floating point scalar value.
 *  @param mask a vector unsigned int
 *  @return The original value XORed with mask as a 128-bit vector int.
 */
 static inline vui32_t
 vec_xor_bin128_2_vui32t (__binary128 f128, vui32_t mask)
 {
   vui32_t result;
 #if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9)
   // Work around for GCC PR 100085
 #ifdef __VSX__
   __asm__(
       "xxlxor %x0,%x1,%x2"
       : "=wa" (result)
       : "wa" (f128), "wa" (mask)
       : );
 #else
   __asm__(
       "vxor %0,%1,%2"
       : "=v" (result)
       : "v" (f128), "v" (mask)
       : );
 #endif
 #else
   __VF_128 vunion;

   vunion.vf1 = f128;

   result = (vec_xor (vunion.vx4, mask));
 #endif
   return result;
 }

 /** \brief Transfer a quadword from a __binary128 scalar to a vector __int128
  * and logical AND Compliment with mask.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param f128 a __binary128 floating point scalar value.
 *  @param mask a vector unsigned __int128
 *  @return The original value ANDed with mask as a 128-bit vector int.
 */
 static inline vui128_t
 vec_andc_bin128_2_vui128t (__binary128 f128, vui128_t mask)
 {
   vui128_t result;
 #if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9)
   // Work around for GCC PR 100085
 #ifdef __VSX__
   __asm__(
       "xxlandc %x0,%x1,%x2"
       : "=wa" (result)
       : "wa" (f128), "wa" (mask)
       : );
 #else
   __asm__(
       "vandc %0,%1,%2"
       : "=v" (result)
       : "v" (f128), "v" (mask)
       : );
 #endif
 #else
   __VF_128 vunion;

   vunion.vf1 = f128;
   // vec_andc does not accept vector __int128 type
   result = (vui128_t) vec_andc (vunion.vx4, (vui32_t) mask);
 #endif
   return result;
 }

/** \brief Transfer function from a __binary128 scalar to a vector char.
*
*  The compiler does not allow direct transfer (assignment or type
*  cast) between __binary128 (__float128) scalars and vector types.
*  This despite the fact the the ABI and ISA require __binary128 in
*  vector registers (VRs).
*
*  \note this function uses a union to effect the (logical) transfer.
*  The compiler should not generate any code for this.
*
*  @param f128 a __binary128 floating point scalar value.
*  @return The original value as a 128-bit vector char.
*/
static inline vui8_t
vec_xfer_bin128_2_vui8t (__binary128 f128)
{
  vui8_t result;
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9)
  // Work around for GCC PR 100085
#ifdef __VSX__
  __asm__(
      "xxlor %x0,%x1,%x1"
      : "=wa" (result)
      : "wa" (f128)
      : );
#else
  __asm__(
      "vor %0,%1,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#endif
#else
  __VF_128 vunion;

  vunion.vf1 = f128;

  result = (vunion.vx16);
#endif
  return result;
}

/** \brief Transfer function from a __binary128 scalar to a vector short int.
*
*  The compiler does not allow direct transfer (assignment or type
*  cast) between __binary128 (__float128) scalars and vector types.
*  This despite the fact the the ABI and ISA require __binary128 in
*  vector registers (VRs).
*
*  \note this function uses a union to effect the (logical) transfer.
*  The compiler should not generate any code for this.
*
*  @param f128 a __binary128 floating point scalar value.
*  @return The original value as a 128-bit vector short int.
*/
static inline vui16_t
vec_xfer_bin128_2_vui16t (__binary128 f128)
{
  __VF_128 vunion;

  vunion.vf1 = f128;

  return (vunion.vx8);
}

/** \brief Transfer function from a __binary128 scalar to a vector int.
*
*  The compiler does not allow direct transfer (assignment or type
*  cast) between __binary128 (__float128) scalars and vector types.
*  This despite the fact the the ABI and ISA require __binary128 in
*  vector registers (VRs).
*
*  \note this function uses a union to effect the (logical) transfer.
*  The compiler should not generate any code for this.
*
*  @param f128 a __binary128 floating point scalar value.
*  @return The original value as a 128-bit vector int.
*/
static inline vui32_t
vec_xfer_bin128_2_vui32t (__binary128 f128)
{
  vui32_t result;
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9)
  // Work around for GCC PR 100085
#ifdef __VSX__
  __asm__(
      "xxlor %x0,%x1,%x1"
      : "=wa" (result)
      : "wa" (f128)
      : );
#else
  __asm__(
      "vor %0,%1,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#endif
#else
  __VF_128 vunion;

  vunion.vf1 = f128;

  result = (vunion.vx4);
#endif
  return result;
}

/** \brief Merge High and Transfer function from a pair of __binary128
*  scalars to a vector long long int.
*
*  The compiler does not allow direct transfer (assignment or type
*  cast) between __binary128 (__float128) scalars and vector types.
*  This despite the fact the the ABI and ISA require __binary128 in
*  vector registers (VRs).
*
*  \note this function uses a union to effect the (logical) transfer.
*  The compiler should generate a single xxmrghd for this.
*
*  @param vfa a __binary128 floating point scalar value.
*  @param vfb a __binary128 floating point scalar value.
*  @return The high doublewords of vfa/vfb value merged into a vector long long int.
*/
static inline vui64_t
vec_mrgh_bin128_2_vui64t (__binary128 vfa, __binary128 vfb)
{
  vui64_t result;
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9) && defined (__VSX__)
  // Work around for GCC PR 100085
  __asm__(
      "xxmrghd %x0,%x1,%x2"
      : "=wa" (result)
      : "wa" (vfa), "wa" (vfb)
      : );
#else
  __VF_128 vunion_a, vunion_b;

  vunion_a.vf1 = vfa;
  vunion_b.vf1 = vfb;

  result = vec_mrgahd (vunion_a.vx1, vunion_b.vx1);
#endif
  return result;
}

/** \brief Merge Low and Transfer function from a pair of __binary128
*  scalars to a vector long long int.
*
*  The compiler does not allow direct transfer (assignment or type
*  cast) between __binary128 (__float128) scalars and vector types.
*  This despite the fact the the ABI and ISA require __binary128 in
*  vector registers (VRs).
*
*  \note this function uses a union to effect the (logical) transfer.
*  The compiler should generate a single xxmrgld for this.
*
*  @param vfa a __binary128 floating point scalar value.
*  @param vfb a __binary128 floating point scalar value.
*  @return The low doublewords of vfa/vfb value merged into a vector long long int.
*/
static inline vui64_t
vec_mrgl_bin128_2_vui64t (__binary128 vfa, __binary128 vfb)
{
  vui64_t result;
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9) && defined (__VSX__)
  // Work around for GCC PR 100085
  __asm__(
      "xxmrgld %x0,%x1,%x2"
      : "=wa" (result)
      : "wa" (vfa), "wa" (vfb)
      : );
#else
  __VF_128 vunion_a, vunion_b;

  vunion_a.vf1 = vfa;
  vunion_b.vf1 = vfb;

  result = vec_mrgald (vunion_a.vx1, vunion_b.vx1);
#endif
  return result;
}

/** \brief Transfer function from a __binary128 scalar to a vector long long int.
*
*  The compiler does not allow direct transfer (assignment or type
*  cast) between __binary128 (__float128) scalars and vector types.
*  This despite the fact the the ABI and ISA require __binary128 in
*  vector registers (VRs).
*
*  \note this function uses a union to effect the (logical) transfer.
*  The compiler should not generate any code for this.
*
*  @param f128 a __binary128 floating point scalar value.
*  @return The original value as a 128-bit vector long long int.
*/
static inline vui64_t
vec_xfer_bin128_2_vui64t (__binary128 f128)
{
  vui64_t result;
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9)
  // Work around for GCC PR 100085
#ifdef __VSX__
  __asm__(
      "xxlor %x0,%x1,%x1"
      : "=wa" (result)
      : "wa" (f128)
      : );
#else
  __asm__(
      "vor %0,%1,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#endif
#else
  __VF_128 vunion;

  vunion.vf1 = f128;

  result = (vunion.vx2);
#endif
  return result;
}

/** \brief Transfer function from a __binary128 scalar to a vector __int128.
*
*  The compiler does not allow direct transfer (assignment or type
*  cast) between __binary128 (__float128) scalars and vector types.
*  This despite the fact the the ABI and ISA require __binary128 in
*  vector registers (VRs).
*
*  \note this function uses a union to effect the (logical) transfer.
*  The compiler should not generate any code for this.
*
*  @param f128 a __binary128 floating point scalar value.
*  @return The original value as a 128-bit vector __int128.
*/
static inline vui128_t
vec_xfer_bin128_2_vui128t (__binary128 f128)
{
  vui128_t result;
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && (__GNUC__ > 7) \
    && !defined (_ARCH_PWR9)
  // Work around for GCC PR 100085
#ifdef __VSX__
  __asm__(
      "xxlor %x0,%x1,%x1"
      : "=wa" (result)
      : "wa" (f128)
      : );
#else
  __asm__(
      "vor %0,%1,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#endif
#else
  __VF_128 vunion;

  vunion.vf1 = f128;

  result = (vunion.vx1);
#endif
  return result;
}

/** \brief Transfer a vector unsigned char to __binary128 scalar.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param f128 a vector unsigned char value.
 *  @return The original value returned as a __binary128 scalar.
 */
static inline __binary128
vec_xfer_vui8t_2_bin128 (vui8_t f128)
{
  __VF_128 vunion;

  vunion.vx16 = f128;

  return (vunion.vf1);
}

/** \brief Transfer a vector unsigned short to __binary128 scalar.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param f128 a vector unsigned short value.
 *  @return The original value returned as a __binary128 scalar.
 */
static inline __binary128
vec_xfer_vui16t_2_bin128 (vui16_t f128)
{
  __VF_128 vunion;

  vunion.vx8 = f128;

  return (vunion.vf1);
}

/** \brief Transfer a vector unsigned int to __binary128 scalar.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param f128 a vector unsigned int value.
 *  @return The original value returned as a __binary128 scalar.
 */
static inline __binary128
vec_xfer_vui32t_2_bin128 (vui32_t f128)
{
  __VF_128 vunion;

  vunion.vx4 = f128;

  return (vunion.vf1);
}

/** \brief Transfer a vector unsigned long long  to __binary128 scalar.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param f128 a vector unsigned long long value.
 *  @return The original value returned as a __binary128 scalar.
 */
static inline __binary128
vec_xfer_vui64t_2_bin128 (vui64_t f128)
{
  __VF_128 vunion;

  vunion.vx2 = f128;

  return (vunion.vf1);
}

/** \brief Transfer a vector unsigned __int128 to __binary128 scalar.
 *
 *  The compiler does not allow direct transfer (assignment or type
 *  cast) between __binary128 (__float128) scalars and vector types.
 *  This despite the fact the the ABI and ISA require __binary128 in
 *  vector registers (VRs).
 *
 *  \note this function uses a union to effect the (logical) transfer.
 *  The compiler should not generate any code for this.
 *
 *  @param f128 a vector unsigned __int128 value.
 *  @return The original value returned as a __binary128 scalar.
 */
static inline __binary128
vec_xfer_vui128t_2_bin128 (vui128_t f128)
{
  __VF_128 vunion;

  vunion.vx1 = f128;

  return (vunion.vf1);
}

/** \brief Absolute Quad-Precision
 *
 *  Clear the sign bit of the __float128 input
 *  and return the resulting positive __float128 value.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 2-11  | 2/cycle  |
 *  |power9   | 2     | 4/cycle  |
 *
 *  @param f128 a __float128 value containing a signed value.
 *  @return a __float128 value with magnitude from f128 and a positive
 *  sign of f128.
 */
static inline __binary128
vec_absf128 (__binary128 f128)
{
  __binary128 result;
#if _ARCH_PWR9
  __asm__(
      "xsabsqp %0,%1;\n"
      : "=v" (result)
      : "v" (f128)
      :);
#else
  vui32_t tmp;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);

  tmp = vec_andc_bin128_2_vui32t (f128, signmask);
  result = vec_xfer_vui32t_2_bin128 (tmp);
#endif
  return (result);
}

/** \brief Return true if the __float128 value is Finite
 *  (Not NaN nor Inf).
 *
 *  A IEEE Binary128 finite value has an exponent between 0x0000 and
 *  0x7ffe (a 0x7fff indicates NaN or Inf).  The significand can be
 *  any value.
 *  Using the !vec_all_eq compare conditional verify this condition and
 *  avoids a vector -> GPR transfer for platforms before PowerISA-2.07.
 *  The sign bit is ignored.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 4-20  | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  @param f128 a __float128 value in vector.
 *  @return an int containing 0 or 1.
 */
static inline int
vec_all_isfinitef128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  return !scalar_test_data_class (f128, 0x70);
#else
  vui32_t tmp;
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  tmp = vec_and_bin128_2_vui32t (f128, expmask);
  return !vec_all_eq(tmp, expmask);
#endif
}

/** \brief Return true if the __float128 value is infinity.
 *
 *  A IEEE Binary128 infinity has a exponent of 0x7fff and significand
 *  of all zeros.  Using the vec_all_eq compare conditional verifies
 *  both conditions and avoids a vector -> GPR transfer for platforms
 *  before PowerISA-2.07.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 4-20  | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  @param f128 a __float128 value in vector.
 *  @return an int containing 0 or 1.
 */
static inline int
vec_all_isinff128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  return scalar_test_data_class (f128, 0x30);
#else
  vui32_t tmp;
#if 0
  const vui32_t magmask = CONST_VINT128_W (0x7fffffff, -1, -1, -1);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
#else
  vui32_t magmask = vec_mask128_f128mag ();
  vui32_t expmask = vec_mask128_f128exp ();
#endif

  tmp = vec_and_bin128_2_vui32t (f128, magmask);
  return vec_all_eq(tmp, expmask);
#endif
}

/** \brief Return true if the __float128 value is Not a Number (NaN).
 *
 *  A IEEE Binary128 NaN has a exponent of 0x7fff and nonzero
 *  significand.  Using the combined vec_all_eq / vec_any_gt compare
 *  conditional verify both conditions and avoids a vector -> GPR
 *  transfer for platforms before PowerISA-2.07.
 *  The sign bit is ignored.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  6-14 | 1/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  @param f128 a __float128 value in vector.
 *  @return an int containing 0 or 1.
 */
static inline int
vec_all_isnanf128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  return scalar_test_data_class (f128, 0x40);
#elif defined (_ARCH_PWR8)
  vui32_t tmp;
  vui32_t magmask = vec_mask128_f128mag ();
  vui32_t expmask = vec_mask128_f128exp ();

  tmp  = vec_and_bin128_2_vui32t (f128, magmask);
  return vec_cmpuq_all_gt ((vui128_t) tmp, (vui128_t) expmask);
#else
  vui32_t tmp, tmp2;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  tmp  = vec_andc_bin128_2_vui32t (f128, signmask);
  tmp2 = vec_and_bin128_2_vui32t (f128, expmask);
  return (vec_all_eq (tmp2, expmask) && vec_any_gt(tmp, expmask));
#endif
}

/** \brief Return true if the __float128 value is normal
 *  (Not NaN, Inf, denormal, or zero).
 *
 *  A IEEE Binary128 normal value has an exponent between 0x0001 and
 *  0x7ffe (a 0x7fff indicates NaN or Inf).  The significand can be
 *  any value (expect 0 if the exponent is zero).
 *  Using the combined vec_all_ne compares conditional verify both
 *  conditions and avoids a vector -> GPR transfer for platforms
 *  before PowerISA-2.07.  The sign bit is ignored.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 4-29  | 1/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  @param f128 a __float128 value in vector.
 *  @return an int containing 0 or 1.
 */
static inline int
vec_all_isnormalf128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  return !scalar_test_data_class (f128, 0x7f);
#else
  vui32_t tmp;
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
  const vui32_t vec_zero = CONST_VINT128_W (0, 0, 0, 0);

  tmp = vec_and_bin128_2_vui32t (f128, expmask);
  return !(vec_all_eq (tmp, expmask) || vec_all_eq(tmp, vec_zero));
#endif
}

/** \brief Return true if the __float128
 *  value is subnormal (denormal).
 *
 *  A IEEE Binary128 subnormal has an exponent of 0x0000 and a
 *  nonzero significand.
 *  The sign bit is ignored.
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 8-29  | 1/cycle  |
 *  |power9   |   3   | 2/cycle  |
 *
 *  @param f128 a vector of __binary128 values.
 *  @return a boolean int, true if the __float128 value is
 *  subnormal.
 */
static inline int
vec_all_issubnormalf128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  return scalar_test_data_class (f128, 0x03);
#else
  const vui64_t minnorm = CONST_VINT128_DW(0x0001000000000000UL, 0UL);
  const vui64_t vec_zero = CONST_VINT128_DW(0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t tmp1;

  // Equivalent to vec_absf128 (f128)
  tmp1 = (vui128_t) vec_andc_bin128_2_vui32t (f128, signmask);

  return vec_cmpuq_all_gt ((vui128_t) minnorm, tmp1)
      && !vec_cmpuq_all_eq (tmp1, (vui128_t) vec_zero);
#endif
}

/** \brief Return true if either __float128 value (vra, vrb) is NaN.
 *
 *  A IEEE Binary128 NaN has a exponent of 0x7fff and nonzero
 *  significand. The sign bit is ignored.
 *  For POWER9 and later we use scalar_test_data_class().
 *  Otherwise mask off the sign bit and compare greater than unsigned
 *  quadword to the integer equivalent of Quad-Precision infinity.
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 16-25 | 1/cycle  |
 *  |power9   |   6   | 1/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return a boolean int, true if either __float128 value
 *  (vra, vrb) is NaN.
 */
static inline int
vec_all_isunorderedf128 (__binary128 vfa, __binary128 vfb)
{
  return (vec_all_isnanf128 (vfa) || vec_all_isnanf128 (vfb));
}

/** \brief Return true if the __float128
 *  value is +-0.0.
 *
 *  A IEEE Binary128 zero has an exponent of 0x0000 and a
 *  zero significand.
 *  The sign bit is ignored.
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 4-20  | 1/cycle  |
 *  |power9   |  3    | 2/cycle  |
 *
 *  @param f128 a vector of __binary64 values.
 *  @return a boolean int, true if the __float128 value is
 *  +/- zero.
 */
static inline int
vec_all_iszerof128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  return scalar_test_data_class (f128, 0x0c);
#else
  vui64_t tmp2;
  const vui64_t vec_zero = CONST_VINT128_DW(0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);

  // Equivalent to vec_absf128 (f128)
  tmp2 = (vui64_t) vec_andc_bin128_2_vui32t (f128, signmask);
#if _ARCH_PWR8
  return vec_all_eq(tmp2, vec_zero);
#else
  return vec_all_eq((vui32_t)tmp2, (vui32_t)vec_zero);
#endif
#endif
}

/** \brief Copy the sign bit from f128x and merge with the magnitude
 *  from f128y. The merged result is returned as a __float128 value.
 *
 *  \note This operation was patterned after the intrinsic vec_cpsgn
 *  (altivec.h) introduced for POWER7 and VSX. It turns out the
 *  original (GCC 4.9) compiler implementation reversed the operands
 *  and does not match the PowerISA or the Vector Intrinsic Programming
 *  Reference manuals. Subsequent compilers and PVECLIB
 *  implementations replicated this (operand order) error.
 *  This has now been reported as bug against the compilers, which are
 *  in the process of applying fixes and distributing updates.
 *  This version of PVECLIB is updated to match the Vector Intrinsic
 *  Programming Reference.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 2-11  | 2/cycle  |
 *  |power9   | 2     | 4/cycle  |
 *
 *  @param f128x a __float128 value containing the sign bit.
 *  @param f128y a __float128 value containing the magnitude.
 *  @return a __float128 value with magnitude from f128y and the
 *  sign of f128x.
 */
static inline __binary128
vec_copysignf128 (__binary128 f128x, __binary128 f128y)
{
  __binary128 result;
#if _ARCH_PWR9
  __asm__(
      "xscpsgnqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (f128x), "v" (f128y)
      :);
#else
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui32_t tmpx, tmpy, tmp;
  tmpx = vec_xfer_bin128_2_vui32t (f128x);
  tmpy = vec_xfer_bin128_2_vui32t (f128y);

  tmp = vec_sel (tmpy, tmpx, signmask);
  result = vec_xfer_vui32t_2_bin128 (tmp);
#endif
  return (result);
}

/** \brief return a positive infinity.
 *
 *  @return const __float128 positive infinity.
 */
static inline __binary128
vec_const_huge_valf128 ()
{
  const vui32_t posinf = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  return vec_xfer_vui32t_2_bin128 (posinf);
}

/** \brief return a positive infinity.
 *
 *  @return a const __float128 positive infinity.
 */
static inline __binary128
vec_const_inff128 ()
{
  const vui32_t posinf = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  return vec_xfer_vui32t_2_bin128 (posinf);
}

/** \brief return a quiet NaN.
 *
 *  @return a const __float128 quiet NaN.
 */
static inline __binary128
vec_const_nanf128 ()
{
  const vui32_t posnan = CONST_VINT128_W (0x7fff8000, 0, 0, 0);

  return vec_xfer_vui32t_2_bin128 (posnan);
}

/** \brief return a signaling NaN.
 *
 *  @return a const __float128 signaling NaN.
 */
static inline __binary128
vec_const_nansf128 ()
{
  const vui32_t signan = CONST_VINT128_W (0x7fff4000, 0, 0, 0);

  return vec_xfer_vui32t_2_bin128 (signan);
}

/** \brief Vector Compare Equal (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa == vfb, otherwise all '0's.
 *  Zeros, Infinities and NaN of the same sign compare equal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or a VSX Scalar Compare Equal
 *  Quad-Precision instruction.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 6     | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare equal.
 */
static inline vb128_t
vec_cmpeqtoqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpeqqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vb128_t) vec_splat_u32 (0);
  if (vfa == vfb)
    result= (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  vui128_t vra, vrb;
  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);
  result = vec_cmpequq ( vra,  vrb );
#endif
  return result;
}

/** \brief Vector Compare Equal (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa == vfb, otherwise all '0's.
 *  Zeros of either sign compare equal.
 *  Infinities and NaNs of the same sign compare equal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or a VSX Scalar Compare Equal
 *  Quad-Precision instruction.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 10    | 1/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare equal.
 */
static inline vb128_t
vec_cmpequzqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpeqqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vb128_t) vec_splat_u32 (0);
  if (vfa == vfb)
    result = (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vb128_t cmps, or_ab, eq_s;
  vui64_t vra, vrb;

  vra = vec_xfer_bin128_2_vui64t (vfa);
  vrb = vec_xfer_bin128_2_vui64t (vfb);

  or_ab = (vb128_t) vec_or ( vra, vrb );
  eq_s = vec_cmpequq ((vui128_t) or_ab, (vui128_t) signmask);
  cmps = vec_cmpequq ((vui128_t) vra, (vui128_t)vrb);
  result = (vb128_t) vec_or ((vui32_t) cmps, (vui32_t) eq_s);
#endif
  return result;
}

/** \brief Vector Compare Equal (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa == vfb, otherwise all '0's.
 *  Zeros of either sign compare equal.
 *  Infinities of the same sign compare equal.
 *  A NaN in either or both operands compare unequal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Equal
 *  Quad-Precision instruction.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 18-30 | 1/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare equal.
 */
static inline vb128_t
vec_cmpequqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpeqqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vb128_t) vec_splat_u32 (0);
  if (vfa == vfb)
    result = (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vb128_t cmps, or_ab, eq_s;
  vui64_t vra, vrb;
  vb128_t unordered;

  unordered = vec_isunorderedf128 (vfa, vfb);
  vra = vec_xfer_bin128_2_vui64t (vfa);
  vrb = vec_xfer_bin128_2_vui64t (vfb);

  or_ab = (vb128_t) vec_or ( vra, vrb );
  eq_s = vec_cmpequq ((vui128_t) or_ab, (vui128_t) signmask);
  cmps = vec_cmpequq ((vui128_t) vra, (vui128_t) vrb);
  result = (vb128_t) vec_or ((vui32_t) cmps, (vui32_t) eq_s);
  result = (vb128_t) vec_andc ((vui32_t) result, (vui32_t) unordered);
#endif
  return result;
}

/** \brief Vector Compare Greater Than or Equal (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa >= vfb, otherwise all '0's.
 *  Zeros, Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater
 *  Than Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 26-35 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare greater than or equal.
 */
static inline vb128_t
vec_cmpgetoqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgeqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vb128_t) vec_splat_u32 (0);
  if (vfa >= vfb)
    result= (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  vui128_t vfa128, vfb128;
  vb128_t altb, agtb;
  vb128_t signbool;
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t splatvfa;

  vfa128 = vec_xfer_bin128_2_vui128t (vfa);
  vfb128 = vec_xfer_bin128_2_vui128t (vfb);

  // Replace (vfa >= 0) with (vfa < 0) == vec_setb_qp (vfa)
  splatvfa = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  signbool = (vb128_t) vec_sra (splatvfa, shift);

  agtb = vec_cmpgesq ((vi128_t) vfa128, (vi128_t) vfb128);
  altb = vec_cmpleuq ((vui128_t) vfa128, (vui128_t) vfb128);
  result = (vb128_t) vec_sel ((vui32_t)agtb, (vui32_t)altb, (vui32_t)signbool);
#endif
  return result;
}

/** \brief Vector Compare Greater Than Or Equal (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa >= vfb, otherwise all '0's.
 *  Zeros of either sign are converted to +0.
 *  Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater
 *  Than Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare greater than or equal.
 */
static inline vb128_t
vec_cmpgeuzqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgeqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vb128_t) vec_splat_u32 (0);
  if (vfa >= vfb)
    result = (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpgeuq ((vui128_t) vra, (vui128_t) vrb);
#endif
  return result;
}

/** \brief Vector Compare Greater Than or Equal (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa >= vfb, otherwise all '0's.
 *  Zeros of either sign are converted to +0.
 *  Infinities of different signs compare ordered.
 *  A NaN in either or both operands compare unordered.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater Than
 *  Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare greater than or equal.
 */
static inline vb128_t
vec_cmpgeuqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgeqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vb128_t) vec_splat_u32 (0);
  if (vfa >= vfb)
    result= (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;
  vb128_t unordered;

  unordered = vec_isunorderedf128 (vfa, vfb);

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpgeuq ((vui128_t) vra, (vui128_t) vrb);
  result = (vb128_t) vec_andc ((vui32_t) result, (vui32_t) unordered);
#endif
  return result;
}

/** \brief Vector Compare Greater Than (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa > vfb, otherwise all '0's.
 *  Zeros, Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater
 *  Than Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 26-35 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare greater than.
 */
static inline vb128_t
vec_cmpgttoqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgtqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vb128_t) vec_splat_u32 (0);
  if (vfa > vfb)
    result= (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  vui128_t vfa128, vfb128;
  vb128_t altb, agtb;
  vb128_t signbool;
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t splatvfa;

  vfa128 = vec_xfer_bin128_2_vui128t (vfa);
  vfb128 = vec_xfer_bin128_2_vui128t (vfb);

  // Replace (vfa >= 0) with (vfa < 0) == vec_setb_qp (vfa)
  splatvfa = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  signbool = (vb128_t) vec_sra (splatvfa, shift);

  agtb = vec_cmpgtsq ((vi128_t) vfa128, (vi128_t) vfb128);
  altb = vec_cmpltuq ((vui128_t) vfa128, (vui128_t) vfb128);
  result = (vb128_t) vec_sel ((vui32_t)agtb, (vui32_t)altb, (vui32_t)signbool);
#endif
  return result;
}

/** \brief Vector Compare Greater Than (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa > vfb, otherwise all '0's.
 *  Zeros of either sign are converted to +0.
 *  Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater
 *  Than Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare greater than.
 */
static inline vb128_t
vec_cmpgtuzqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgtqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vb128_t) vec_splat_u32 (0);
  if (vfa > vfb)
    result = (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpgtuq ((vui128_t) vra, (vui128_t) vrb);
#endif
  return result;
}

/** \brief Vector Compare Greater Than (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa > vfb, otherwise all '0's.
 *  Zeros of either sign are converted to +0.
 *  Infinities of different signs compare ordered.
 *  A NaN in either or both operands compare unordered.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater Than
 *  Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare greater than.
 */
static inline vb128_t
vec_cmpgtuqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgtqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vb128_t) vec_splat_u32 (0);
  if (vfa > vfb)
    result= (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;
  vb128_t unordered;

  unordered = vec_isunorderedf128 (vfa, vfb);

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpgtuq ((vui128_t) vra, (vui128_t) vrb);
  result = (vb128_t) vec_andc ((vui32_t) result, (vui32_t) unordered);
#endif
  return result;
}

/** \brief Vector Compare Less Than or Equal (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa <= vfb, otherwise all '0's.
 *  Zeros, Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater
 *  Than Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 26-35 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare less than or equal.
 */
static inline vb128_t
vec_cmpletoqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgeqp %0,%2,%1;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vb128_t) vec_splat_u32 (0);
  if (vfa <= vfb)
    result= (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  vui128_t vfa128, vfb128;
  vb128_t altb, agtb;
  vb128_t signbool;
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t splatvfa;

  vfa128 = vec_xfer_bin128_2_vui128t (vfa);
  vfb128 = vec_xfer_bin128_2_vui128t (vfb);

  // Replace (vfa >= 0) with (vfa < 0) == vec_setb_qp (vfa)
  splatvfa = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  signbool = (vb128_t) vec_sra (splatvfa, shift);

  altb = vec_cmplesq ((vi128_t) vfa128, (vi128_t) vfb128);
  agtb = vec_cmpgeuq ((vui128_t) vfa128, (vui128_t) vfb128);
  result = (vb128_t) vec_sel ((vui32_t)altb, (vui32_t)agtb, (vui32_t)signbool);
#endif
  return result;
}

/** \brief Vector Compare Less Than or Equal (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa <= vfb, otherwise all '0's.
 *  Zeros of either sign are converted to +0.
 *  Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater
 *  Than Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare less than or equal.
 */
static inline vb128_t
vec_cmpleuzqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgeqp %0,%2,%1;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vb128_t) vec_splat_u32 (0);
  if (vfa <= vfb)
    result = (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpleuq ((vui128_t) vra, (vui128_t) vrb);
#endif
  return result;
}

/** \brief Vector Compare Less Than or Equal (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa <= vfb, otherwise all '0's.
 *  Zeros of either sign are converted to +0.
 *  Infinities of different signs compare ordered.
 *  A NaN in either or both operands compare unordered.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater Than
 *  Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare less than or equal.
 */
static inline vb128_t
vec_cmpleuqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgeqp %0,%2,%1;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vb128_t) vec_splat_u32 (0);
  if (vfa <= vfb)
    result= (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;
  vb128_t unordered;

  unordered = vec_isunorderedf128 (vfa, vfb);

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpleuq ((vui128_t) vra, (vui128_t) vrb);
  result = (vb128_t) vec_andc ((vui32_t) result, (vui32_t) unordered);
#endif
  return result;
}

/** \brief Vector Compare Less Than (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa < vfb, otherwise all '0's.
 *  Zeros, Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater
 *  Than Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 26-35 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare less than.
 */
static inline vb128_t
vec_cmplttoqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgtqp %0,%2,%1;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vb128_t) vec_splat_u32 (0);
  if (vfa < vfb)
    result= (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  vui128_t vfa128, vfb128;
  vb128_t altb, agtb;
  vb128_t signbool;
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t splatvfa;

  vfa128 = vec_xfer_bin128_2_vui128t (vfa);
  vfb128 = vec_xfer_bin128_2_vui128t (vfb);

  // Replace (vfa >= 0) with (vfa < 0) == vec_setb_qp (vfa)
  splatvfa = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  signbool = (vb128_t) vec_sra (splatvfa, shift);

  altb = vec_cmpltsq ((vi128_t) vfa128, (vi128_t) vfb128);
  agtb = vec_cmpgtuq ((vui128_t) vfa128, (vui128_t) vfb128);
  result = (vb128_t) vec_sel ((vui32_t)altb, (vui32_t)agtb, (vui32_t)signbool);
#endif
  return result;
}

/** \brief Vector Compare Less Than (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa < vfb, otherwise all '0's.
 *  Zeros of either sign are converted to +0.
 *  Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater
 *  Than Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare less than.
 */
static inline vb128_t
vec_cmpltuzqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgtqp %0,%2,%1;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vb128_t) vec_splat_u32 (0);
  if (vfa < vfb)
    result = (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpltuq ((vui128_t) vra, (vui128_t) vrb);
#endif
  return result;
}

/** \brief Vector Compare Less Than (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa < vfb, otherwise all '0's.
 *  Zeros of either sign are converted to +0.
 *  Infinities of different signs compare ordered.
 *  A NaN in either or both operands compare unordered.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Greater Than
 *  Quad-Precision instruction.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare less than.
 */
static inline vb128_t
vec_cmpltuqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpgtqp %0,%2,%1;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vb128_t) vec_splat_u32 (0);
  if (vfa < vfb)
    result= (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;
  vb128_t unordered;

  unordered = vec_isunorderedf128 (vfa, vfb);

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpltuq ((vui128_t) vra, (vui128_t) vrb);
  result = (vb128_t) vec_andc ((vui32_t) result, (vui32_t) unordered);
#endif
  return result;
}

/** \brief Vector Compare Not Equal (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa != vfb, otherwise all '0's.
 *  Zeros, Infinities and NaN of the same sign compare equal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or a VSX Scalar Compare Equal
 *  Quad-Precision instruction.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 6     | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare not equal.
 */
static inline vb128_t
vec_cmpnetoqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpeqqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
  result = (vb128_t) vec_nor ((vui32_t) result, (vui32_t) result);
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vb128_t) vec_splat_u32 (0);
  if (vfa != vfb)
    result= (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  vui128_t vra, vrb;
  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);
  result = vec_cmpneuq ( vra,  vrb );
#endif
  return result;
}

/** \brief Vector Compare Not Equal (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa != vfb, otherwise all '0's.
 *  Zeros of either sign compare equal.
 *  Infinities and NaNs of the same sign compare equal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or a VSX Scalar Compare Equal
 *  Quad-Precision instruction.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 10    | 1/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare not equal.
 */
static inline vb128_t
vec_cmpneuzqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpeqqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
  result = (vb128_t) vec_nor ((vui32_t) result, (vui32_t) result);
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vb128_t) vec_splat_u32 (0);
  if (vfa != vfb)
    result = (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vb128_t cmps, or_ab, eq_s;
  vui64_t vra, vrb;

  vra = vec_xfer_bin128_2_vui64t (vfa);
  vrb = vec_xfer_bin128_2_vui64t (vfb);

  or_ab = (vb128_t) vec_or ( vra, vrb );
  eq_s = vec_cmpequq ((vui128_t) or_ab, (vui128_t) signmask);
  cmps = vec_cmpequq ((vui128_t) vra, (vui128_t)vrb);
  result = (vb128_t) vec_nor ((vui32_t) cmps, (vui32_t) eq_s);
#endif
  return result;
}

/** \brief Vector Compare Not Equal (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return all '1's,
 *  if vfa == vfb, otherwise all '0's.
 *  Zeros of either sign compare equal.
 *  Infinities of the same sign compare equal.
 *  A NaN in either or both operands compare unequal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision or (POWER10) VSX Scalar Compare Equal
 *  Quad-Precision instruction.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 18-30 | 1/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return 128-bit vector boolean reflecting __binary128
 *  compare not equal.
 */
static inline vb128_t
vec_cmpneuqp (__binary128 vfa, __binary128 vfb)
{
  vb128_t result;
#if defined (_ARCH_PWR10) && defined (__FLOAT128__)  && (__GNUC__ >= 10)
  __asm__(
      "xscmpeqqp %0,%1,%2;\n"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
  result = (vb128_t) vec_nor ((vui32_t) result, (vui32_t) result);
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vb128_t) vec_splat_u32 (0);
  if (vfa != vfb)
    result = (vb128_t) vec_splat_s32 (-1);
#else // defined( _ARCH_PWR8 )
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vb128_t cmps, or_ab, eq_s;
  vui64_t vra, vrb;
  vb128_t unordered;

  unordered = vec_isunorderedf128 (vfa, vfb);

  vra = vec_xfer_bin128_2_vui64t (vfa);
  vrb = vec_xfer_bin128_2_vui64t (vfb);

  or_ab = (vb128_t) vec_or ( vra, vrb );
  eq_s = vec_cmpequq ((vui128_t) or_ab, (vui128_t) signmask);
  cmps = vec_cmpequq ((vui128_t) vra, (vui128_t) vrb);
  result = (vb128_t) vec_nor ((vui32_t) cmps, (vui32_t) eq_s);
  result = (vb128_t) vec_or ((vui32_t) result, (vui32_t) unordered);
#endif
  return result;
}

/** \brief Vector Compare all Equal (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1,
 *  if vfa == vfb, otherwise 0.
 *  Zeros, Infinities and NaN of the same sign compare equal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 6     | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 compare equal.
 */
static inline int
vec_cmpqp_all_toeq (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vfa == vfb);
#else // defined( _ARCH_PWR8 )
  vui128_t vra, vrb;
  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);
  result = vec_cmpuq_all_eq ( vra,  vrb );
#endif
  return result;
}

/** \brief Vector Compare all Equal (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1,
 *  if vfa == vfb, otherwise 0.
 *  Zeros of either sign compare equal.
 *  Infinities and NaNs of the same sign compare equal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 10    | 1/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 compare equal.
 */
static inline int
vec_cmpqp_all_uzeq (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa == vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vb128_t or_ab;
  vui64_t vra, vrb;

  vra = vec_xfer_bin128_2_vui64t (vfa);
  vrb = vec_xfer_bin128_2_vui64t (vfb);

  or_ab = (vb128_t) vec_or ( vra, vrb );
  result = vec_cmpuq_all_eq ((vui128_t) or_ab, (vui128_t) signmask)
        || vec_cmpuq_all_eq ((vui128_t) vra, (vui128_t)vrb);
#endif
  return result;
}

/** \brief Vector Compare all Equal (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1,
 *  if vfa == vfb, otherwise 0.
 *  Zeros of either sign compare equal.
 *  Infinities of the same sign compare equal.
 *  A NaN in either or both operands compare unequal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 18-30 | 1/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 compare equal.
 */
static inline int
vec_cmpqp_all_eq (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa == vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vb128_t or_ab;
  vui64_t vra, vrb;

  vra = vec_xfer_bin128_2_vui64t (vfa);
  vrb = vec_xfer_bin128_2_vui64t (vfb);

  or_ab = (vb128_t) vec_or ( vra, vrb );
  result = (vec_cmpuq_all_eq ((vui128_t) or_ab, (vui128_t) signmask)
            || vec_cmpuq_all_eq ((vui128_t) vra, (vui128_t)vrb))
         && !vec_all_isunorderedf128 (vfa, vfb);
#endif
  return result;
}

/** \brief Vector Compare all Greater Than Or Equal (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa >= vfb, otherwise 0 (false).
 *  Zeros, Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 26-35 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 greater than or equal.
 */
static inline int
vec_cmpqp_all_toge (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa >= vfb);
#else // defined( _ARCH_PWR8 )
  vui128_t vfa128, vfb128;
  vb128_t altb, agtb;
  vb128_t signbool;
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t splatvfa;
  vui32_t togt;
  const vui32_t zeros = (vui32_t) vec_splat_u32 (0);

  vfa128 = vec_xfer_bin128_2_vui128t (vfa);
  vfb128 = vec_xfer_bin128_2_vui128t (vfb);

  // Replace (vfa >= 0) with (vfa < 0) == vec_setb_qp (vfa)
  splatvfa = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  signbool = (vb128_t) vec_sra (splatvfa, shift);

  agtb = vec_cmpgesq ((vi128_t) vfa128, (vi128_t) vfb128);
  altb = vec_cmpleuq ((vui128_t) vfa128, (vui128_t) vfb128);
  togt = vec_sel ((vui32_t)agtb, (vui32_t)altb, (vui32_t)signbool);
  result = vec_all_ne (togt, zeros);
#endif
  return result;
}

/** \brief Vector Compare all Greater Than Or Equal (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa >= vfb, otherwise 0 (false).
 *  Zeros of either sign are converted to +0.
 *  Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 greater than or equal.
 */
static inline int
vec_cmpqp_all_uzge (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa >= vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpuq_all_ge ((vui128_t) vra, (vui128_t) vrb);
#endif
  return result;
}

/** \brief Vector Compare all  Greater Than Or Equal (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa >= vfb, otherwise 0 (false).
 *  Zeros of either sign are converted to +0.
 *  Infinities of different signs compare ordered.
 *  A NaN in either or both operands compare unordered.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 greater than.
 */
static inline int
vec_cmpqp_all_ge (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa >= vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpuq_all_ge ((vui128_t) vra, (vui128_t) vrb)
         && !vec_all_isunorderedf128 (vfa, vfb);
#endif
  return result;
}

/** \brief Vector Compare all Greater Than (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa > vfb, otherwise 0 (false).
 *  Zeros, Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 26-35 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 greater than.
 */
static inline int
vec_cmpqp_all_togt (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa > vfb);
#else // defined( _ARCH_PWR8 )
  vui128_t vfa128, vfb128;
  vb128_t altb, agtb;
  vb128_t signbool;
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t splatvfa;
  vui32_t togt;
  const vui32_t zeros = (vui32_t) vec_splat_u32 (0);

  vfa128 = vec_xfer_bin128_2_vui128t (vfa);
  vfb128 = vec_xfer_bin128_2_vui128t (vfb);

  // Replace (vfa >= 0) with (vfa < 0) == vec_setb_qp (vfa)
  splatvfa = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  signbool = (vb128_t) vec_sra (splatvfa, shift);

  agtb = vec_cmpgtsq ((vi128_t) vfa128, (vi128_t) vfb128);
  altb = vec_cmpltuq ((vui128_t) vfa128, (vui128_t) vfb128);
  togt = vec_sel ((vui32_t)agtb, (vui32_t)altb, (vui32_t)signbool);
  result = vec_all_ne (togt, zeros);
#endif
  return result;
}

/** \brief Vector Compare all Greater Than (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa > vfb, otherwise 0 (false).
 *  Zeros of either sign are converted to +0.
 *  Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 greater than.
 */
static inline int
vec_cmpqp_all_uzgt (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa > vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpuq_all_gt ((vui128_t) vra, (vui128_t) vrb);
#endif
  return result;
}

/** \brief Vector Compare all Greater Than (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa > vfb, otherwise 0 (false).
 *  Zeros of either sign are converted to +0.
 *  Infinities of different signs compare ordered.
 *  A NaN in either or both operands compare unordered.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 greater than.
 */
static inline int
vec_cmpqp_all_gt (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa > vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpuq_all_gt ((vui128_t) vra, (vui128_t) vrb)
         && !vec_all_isunorderedf128 (vfa, vfb);
#endif
  return result;
}
/** \brief Vector Compare All Less Than Or Equal (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa <= vfb, otherwise 0 (false).
 *  Zeros, Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 26-35 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 compare less than or equal.
 */
static inline int
vec_cmpqp_all_tole (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa <= vfb);
#else // defined( _ARCH_PWR8 )
  vui128_t vfa128, vfb128;
  vb128_t altb, agtb;
  vb128_t signbool;
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t splatvfa;
  vui32_t tolt;
  const vui32_t zeros = (vui32_t) vec_splat_u32 (0);

  vfa128 = vec_xfer_bin128_2_vui128t (vfa);
  vfb128 = vec_xfer_bin128_2_vui128t (vfb);

  // Replace (vfa >= 0) with (vfa < 0) == vec_setb_qp (vfa)
  splatvfa = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  signbool = (vb128_t) vec_sra (splatvfa, shift);

  altb = vec_cmplesq ((vi128_t) vfa128, (vi128_t) vfb128);
  agtb = vec_cmpgeuq ((vui128_t) vfa128, (vui128_t) vfb128);
  tolt = vec_sel ((vui32_t)altb, (vui32_t)agtb, (vui32_t)signbool);
  result = vec_all_ne (tolt, zeros);
#endif
  return result;
}

/** \brief Vector Compare all Less Than Or Equal (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa <= vfb, otherwise 0 (false).
 *  Zeros of either sign are converted to +0.
 *  Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 less than or equal.
 */
static inline int
vec_cmpqp_all_uzle (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa <= vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpuq_all_le ((vui128_t) vra, (vui128_t) vrb);
#endif
  return result;
}

/** \brief Vector Compare all Less Than Or Equal (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa <= vfb, otherwise 0 (false).
 *  Zeros of either sign are converted to +0.
 *  Infinities of different signs compare ordered.
 *  A NaN in either or both operands compare unordered.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 less than or equal.
 */
static inline int
vec_cmpqp_all_le (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa <= vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpuq_all_le ((vui128_t) vra, (vui128_t) vrb)
         && !vec_all_isunorderedf128 (vfa, vfb);
#endif
  return result;
}

/** \brief Vector Compare All Less Than (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa < vfb, otherwise 0 (false).
 *  Zeros, Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 26-35 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 compare less than.
 */
static inline int
vec_cmpqp_all_tolt (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa < vfb);
#else // defined( _ARCH_PWR8 )
  vui128_t vfa128, vfb128;
  vb128_t altb, agtb;
  vb128_t signbool;
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t splatvfa;
  vui32_t tolt;
  const vui32_t zeros = (vui32_t) vec_splat_u32 (0);

  vfa128 = vec_xfer_bin128_2_vui128t (vfa);
  vfb128 = vec_xfer_bin128_2_vui128t (vfb);

  // Replace (vfa >= 0) with (vfa < 0) == vec_setb_qp (vfa)
  splatvfa = vec_splat ((vui8_t) vfa128, VEC_BYTE_H);
  signbool = (vb128_t) vec_sra (splatvfa, shift);

  altb = vec_cmpltsq ((vi128_t) vfa128, (vi128_t) vfb128);
  agtb = vec_cmpgtuq ((vui128_t) vfa128, (vui128_t) vfb128);
  tolt = vec_sel ((vui32_t)altb, (vui32_t)agtb, (vui32_t)signbool);
  result = vec_all_ne (tolt, zeros);
#endif
  return result;
}

/** \brief Vector Compare all Less Than (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa < vfb, otherwise 0 (false).
 *  Zeros of either sign are converted to +0.
 *  Infinities and NaNs are compared as signed values.
 *  Infinities and NaNs have the highest/lowest magnitudes.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 less than.
 */
static inline int
vec_cmpqp_all_uzlt (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa < vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpuq_all_lt ((vui128_t) vra, (vui128_t) vrb);
#endif
  return result;
}

/** \brief Vector Compare all Less Than (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1 (true),
 *  if vfa < vfb, otherwise 0 (false).
 *  Zeros of either sign are converted to +0.
 *  Infinities of different signs compare ordered.
 *  A NaN in either or both operands compare unordered.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise comparands are converted to unsigned integer magnitudes
 *  before using vector __int128 comparison to implement the equivalent
 *  Quad-precision floating-point operation. This leverages operations
 *  from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 28-37 | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 less than.
 */
static inline int
vec_cmpqp_all_lt (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa < vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vui128_t vra, vrb;
  vb128_t age0, bge0;
  vui128_t vrap, vran;
  vui128_t vrbp, vrbn;

  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);

  age0 = vec_setb_qp (vfa);
  vrap = (vui128_t) vec_xor ((vui32_t) vra, signmask);
  vran = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vra);
  vra  = (vui128_t) vec_sel ((vui32_t)vrap, (vui32_t)vran, (vui32_t)age0);

  bge0 = vec_setb_qp (vfb);
  vrbp = (vui128_t) vec_xor ((vui32_t) vrb, signmask);
  vrbn = (vui128_t) vec_subuqm ((vui128_t) zero, (vui128_t) vrb);
  vrb  = (vui128_t) vec_sel ((vui32_t)vrbp, (vui32_t)vrbn, (vui32_t)bge0);

  result = vec_cmpuq_all_lt ((vui128_t) vra, (vui128_t) vrb)
         && !vec_all_isunorderedf128 (vfa, vfb);
#endif
  return result;
}

/** \brief Vector Compare all Not-Equal (Total-order) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1,
 *  if vfa != vfb, otherwise 0.
 *  Zeros, Infinities and NaN of the same sign compare equal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative to signed zero, or NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 6     | 2/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 compare not-equal.
 */
static inline int
vec_cmpqp_all_tone (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result= (vfa != vfb);
#else // defined( _ARCH_PWR8 )
  vui128_t vra, vrb;
  vra = vec_xfer_bin128_2_vui128t (vfa);
  vrb = vec_xfer_bin128_2_vui128t (vfb);
  result = vec_cmpuq_all_ne ( vra,  vrb );
#endif
  return result;
}

/** \brief Vector Compare all Not-Equal (Zero-unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1,
 *  if vfa != vfb, otherwise 0.
 *  Zeros of either sign compare equal.
 *  Infinities and NaNs of the same sign compare equal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the IEEE standard
 *  relative NaN comparison.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 10    | 1/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 compare equal.
 */
static inline int
vec_cmpqp_all_uzne (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa != vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vb128_t or_ab;
  vui64_t vra, vrb;

  vra = vec_xfer_bin128_2_vui64t (vfa);
  vrb = vec_xfer_bin128_2_vui64t (vfb);

  or_ab = (vb128_t) vec_or ( vra, vrb );
  result = vec_cmpuq_all_ne ((vui128_t) or_ab, (vui128_t) signmask)
        && vec_cmpuq_all_ne ((vui128_t) vra, (vui128_t)vrb);
#endif
  return result;
}

/** \brief Vector Compare all Not-Equal (Unordered) Quad-Precision.
 *
 *  Compare Binary-float 128-bit values and return 1,
 *  if vfa == vfb, otherwise 0.
 *  Zeros of either sign compare equal.
 *  Infinities of the same sign compare equal.
 *  A NaN in either or both operands compare unequal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use a VSX Scalar Compare
 *  Unordered Quad-Precision.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to NaN comparison and setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 18-30 | 1/cycle  |
 *  |power9   | 3     | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 compare not-equal.
 */
static inline int
vec_cmpqp_all_ne (__binary128 vfa, __binary128 vfb)
{
  int result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  result = (vfa != vfb);
#else // defined( _ARCH_PWR8 )
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  vb128_t or_ab;
  vui64_t vra, vrb;

  vra = vec_xfer_bin128_2_vui64t (vfa);
  vrb = vec_xfer_bin128_2_vui64t (vfb);

  or_ab = (vb128_t) vec_or ( vra, vrb );
  result = (vec_cmpuq_all_ne ((vui128_t) or_ab, (vui128_t) signmask)
            && vec_cmpuq_all_ne ((vui128_t) vra, (vui128_t)vrb))
         || vec_all_isunorderedf128 (vfa, vfb);
#endif
  return result;
}

/** \brief Vector Compare Quad-Precision Exponents for Equal.
 *
 *  Compare the exponents of two Binary-float 128-bit values and
 *  return 1,
 *  if vfa<sup>exp</sup> == vfb<sup>exp</sup>, otherwise 0.
 *  A NaN in either or both operands compare unequal.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use the
 *  VSX Scalar Compare Exponents Quad-Precision instruction.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  8-17 | 1/cycle  |
 *  |power9   |   3   | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 exponent compare equal.
 */
static inline int
vec_cmpqp_exp_eq (__binary128 vfa, __binary128 vfb)
{
#if defined (_ARCH_PWR9) && defined (scalar_cmp_exp_gt) \
  && defined (__FLOAT128__) && (__GNUC__ >= 9)
  return scalar_cmp_exp_eq (vfa, vfb);
#else
  vui32_t vra, vrb;
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  vra = vec_and_bin128_2_vui32t (vfa, expmask);
  vrb = vec_and_bin128_2_vui32t (vfb, expmask);
  return vec_cmpuq_all_eq ((vui128_t) vra, (vui128_t) vrb);
#endif
}

/** \brief Vector Compare Exponents Quad-Precision for Greater Than.
 *
 *  Compare the exponents of two Binary-float 128-bit values and
 *  return 1,
 *  if vfa<sup>exp</sup> > vfb<sup>exp</sup>, otherwise 0.
 *  A NaN in either or both operands returns 0.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use the
 *  VSX Scalar Compare Exponents Quad-Precision instruction.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  8-17 | 1/cycle  |
 *  |power9   |   3   | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 exponent compare greater than.
 */

static inline int
vec_cmpqp_exp_gt (__binary128 vfa, __binary128 vfb)
{
#if defined (_ARCH_PWR9) && defined (scalar_cmp_exp_gt) \
  && defined (__FLOAT128__) && (__GNUC__ >= 9)
  return scalar_cmp_exp_gt (vfa, vfb);
#else
  vui32_t vra, vrb;
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  vra = vec_and_bin128_2_vui32t (vfa, expmask);
  vrb = vec_and_bin128_2_vui32t (vfb, expmask);
  return vec_cmpuq_all_gt ((vui128_t) vra, (vui128_t) vrb);
#endif
}


/** \brief Vector Compare Exponents Quad-Precision for Less Than.
 *
 *  Compare the exponents of two Binary-float 128-bit values and
 *  return 1,
 *  if vfa<sup>exp</sup> < vfb<sup>exp</sup>, otherwise 0.
 *  A NaN in either or both operands returns 0.
 *
 *  For POWER9 (PowerISA 3.0B) or later, use the
 *  VSX Scalar Compare Exponents Quad-Precision instruction.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  8-17 | 1/cycle  |
 *  |power9   |   3   | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 exponent compare equal.
 */
static inline int
vec_cmpqp_exp_lt (__binary128 vfa, __binary128 vfb)
{
#if defined (_ARCH_PWR9) && defined (scalar_cmp_exp_gt) \
  && defined (__FLOAT128__) && (__GNUC__ >= 9)
  return scalar_cmp_exp_lt (vfa, vfb);
#else
  vui32_t vra, vrb;
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  vra = vec_and_bin128_2_vui32t (vfa, expmask);
  vrb = vec_and_bin128_2_vui32t (vfb, expmask);
  return vec_cmpuq_all_lt ((vui128_t) vra, (vui128_t) vrb);
#endif
}

/** \brief Vector Compare Exponents Quad-Precision for Unordered.
 *
 *  Compare two Binary-float 128-bit values and
 *  return 1, if either or both operands are NaN,
 *  otherwise 0.
 *
 *
 *  For POWER9 (PowerISA 3.0B) or later, use the
 *  VSX Scalar Compare Exponents Quad-Precision instruction.
 *  Otherwise use vector __int128 arithmetic and logical operations
 *  to implement the equivalent Quad-precision floating-point
 *  operation. This leverages operations from vec_int128_ppc.h.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to setting the FPSCR.
 *  However if the hardware target includes an instruction that does
 *  implement the IEEE standard, the implementation may use that.
 *  This relaxed implementation may be useful for implementations on
 *  POWER8 and earlier. Especially for soft-float implementations
 *  where it is known these special cases do not occur.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  8-17 | 1/cycle  |
 *  |power9   |   3   | 2/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return int boolean reflecting __binary128 unordered.
 */

static inline int
vec_cmpqp_exp_unordered (__binary128 vfa, __binary128 vfb)
{
#if defined (_ARCH_PWR9) && defined (scalar_cmp_exp_gt) \
  && defined (__FLOAT128__) && (__GNUC__ >= 9)
  return scalar_cmp_exp_unordered (vfa, vfb);
#else
  vui32_t vra, vrb;
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  vra = vec_and_bin128_2_vui32t (vfa, expmask);
  vrb = vec_and_bin128_2_vui32t (vfb, expmask);
  return vec_cmpuq_all_lt ((vui128_t) vra, (vui128_t) vrb);
#endif
}

/** \brief Return 128-bit vector boolean true if the __float128 value
 *  is Finite (Not NaN nor Inf).
 *
 *  A IEEE Binary128 finite value has an exponent between 0x0000 and
 *  0x7ffe (a 0x7fff indicates NaN or Inf).  The significand can be
 *  any value.
 *  Using the vec_cmpeq conditional to generate the predicate mask for
 *  NaN / Inf and then invert this for the finite condition.
 *  The sign bit is ignored.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 8-17  | 2/cycle  |
 *  |power9   |   6   | 2/cycle  |
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  @param f128 a __float128 value in vector.
 *  @return a vector boolean containing all 0s or 1s.
 */
static inline vb128_t
vec_isfinitef128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  vui32_t result = CONST_VINT128_W (-1, -1, -1, -1);

  if (scalar_test_data_class (f128, 0x70))
    result = CONST_VINT128_W (0, 0, 0, 0);

  return (vb128_t)result;
#else
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
  vui32_t tmp;
  vb128_t tmp2, tmp3;

  tmp = vec_and_bin128_2_vui32t (f128, expmask);
  tmp2 = (vb128_t) vec_cmpeq (tmp, expmask);
  tmp3 = (vb128_t) vec_splat ((vui32_t) tmp2, VEC_W_H);
  return (vb128_t) vec_nor ((vui32_t) tmp3, (vui32_t) tmp3); // vec_not
#endif
}

/** \brief Return true (nonzero) value if the __float128 value is
 *  infinity. If infinity, indicate the sign as +1 for positive infinity
 *  and -1 for negative infinity.
 *
 *  A IEEE Binary128 infinity has a exponent of 0x7fff and significand
 *  of all zeros.  Using the vec_all_eq compare conditional verifies
 *  both conditions. A subsequent vec_any_gt checks the sign bit and
 *  set the result appropriately. The sign bit is ignored.
 *
 *  This sequence avoids a vector -> GPR transfer for platforms
 *  before PowerISA-2.07.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 12-32 | 1/cycle  |
 *  |power9   | 3-12  | 2/cycle  |
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  @param f128 a __float128 value in vector.
 *  @return an int containing 0 if not infinity and +1/-1 otherwise.
 */
static inline int
vec_isinf_signf128 (__binary128 f128)
{
  int result;
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  if (scalar_test_data_class (f128, 0x20))
    result = 1;
  else if (scalar_test_data_class (f128, 0x10))
    result = -1;
  else
    result = 0;
#else
  vui32_t tmp, t128;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  t128 = vec_xfer_bin128_2_vui32t (f128);
  tmp = vec_andc_bin128_2_vui32t (f128, signmask);

  if (vec_all_eq(tmp, expmask))
    {
      if (vec_any_gt(t128, expmask))
	result = -1;
      else
	result = 1;
    }
  else
    result = 0;
#endif
  return (result);
}

/** \brief Return a 128-bit vector boolean true if the __float128 value
 *  is infinity.
 *
 *  A IEEE Binary128 infinity has a exponent of 0x7fff and significand
 *  of all zeros.
 *  The sign bit is ignored.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 8-17  | 2/cycle  |
 *  |power9   |   6   | 2/cycle  |
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  @param f128 a __float128 value in vector.
 *  @return a vector boolean containing all 0s or 1s..
 */
static inline vb128_t
vec_isinff128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  vui32_t result = CONST_VINT128_W (0, 0, 0, 0);

  if (scalar_test_data_class (f128, 0x30))
    result = CONST_VINT128_W (-1, -1, -1, -1);

  return (vb128_t)result;
#else
  vui32_t tmp;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  tmp = vec_andc_bin128_2_vui32t (f128, signmask);
  return vec_cmpequq ((vui128_t)tmp , (vui128_t)expmask);
#endif
}

/** \brief Return 128-bit vector boolean true if the __float128 value
 *  is Not a Number (NaN).
 *
 *  A IEEE Binary128 NaN has a exponent of 0x7fff and nonzero
 *  significand. The sign bit is ignored.
 *  For POWER9 and later we use scalar_test_data_class().
 *  Otherwise mask off the sign bit and compare greater than unsigned
 *  quadword to the integer equivalent of Quad-Precision infinity.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 14-23 | 1/cycle  |
 *  |power9   |   6   | 2/cycle  |
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  @param f128 a __float128 value in vector.
 *  @return a vector boolean containing all 0s or 1s.
 */
static inline vb128_t
vec_isnanf128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  vui32_t result = CONST_VINT128_W (0, 0, 0, 0);

  if (scalar_test_data_class (f128, 0x40))
    result = CONST_VINT128_W (-1, -1, -1, -1);

  return (vb128_t)result;
#else
  vui32_t tmp;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);

  tmp = vec_andc_bin128_2_vui32t (f128, signmask);
  return vec_cmpgtuq ((vui128_t)tmp , (vui128_t)expmask);
#endif
}

/** \brief Return 128-bit vector boolean true if the __float128 value
 *  is normal (Not NaN, Inf, denormal, or zero).
 *
 *  A IEEE Binary128 normal value has an exponent between 0x0001 and
 *  0x7ffe (a 0x7fff indicates NaN or Inf).  The significand can be
 *  any value (expect 0 if the exponent is zero).
 *  The sign bit is ignored.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 10-19 | 2/cycle  |
 *  |power9   |   6   | 2/cycle  |
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  @param f128 a __float128 value in vector.
 *  @return a vector boolean containing all 0s or 1s.
 */
static inline vb128_t
vec_isnormalf128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  vui32_t result = CONST_VINT128_W (-1, -1, -1, -1);

  if (scalar_test_data_class (f128, 0x7f))
    result = CONST_VINT128_W (0, 0, 0, 0);

  return (vb128_t)result;
#else
  vui32_t tmp;
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
  const vui32_t vec_zero = CONST_VINT128_W (0, 0, 0, 0);
  vb128_t result;

  tmp = vec_and_bin128_2_vui32t (f128, expmask);
  result = (vb128_t) vec_nor (vec_cmpeq (tmp, expmask),
			      vec_cmpeq (tmp, vec_zero));
  return (vb128_t) vec_splat ((vui32_t) result, VEC_W_H);
#endif
}

/** \brief Return 128-bit vector boolean true value,
 *  if the __float128 value is subnormal (denormal).
 *
 *  A IEEE Binary128 subnormal has an exponent of 0x0000 and a
 *  nonzero significand.
 *  The sign bit is ignored.
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 16-25 | 1/cycle  |
 *  |power9   |   6   | 1/cycle  |
 *
 *  @param f128 a vector of __binary64 values.
 *  @return a vector boolean long long, each containing all 0s(false)
 *  or 1s(true).
 */
static inline vb128_t
vec_issubnormalf128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  vui32_t result = CONST_VINT128_W (0, 0, 0, 0);

  if (scalar_test_data_class (f128, 0x03))
    result = CONST_VINT128_W (-1, -1, -1, -1);

  return (vb128_t)result;
#else
  vui32_t tmp, tmpz, tmp2;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  const vui32_t vec_zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t minnorm = CONST_VINT128_W (0x00010000, 0, 0, 0);

  // Equivalent to vec_absf128 (f128)
  tmp = vec_andc_bin128_2_vui32t (f128, signmask);

  tmp2 = (vui32_t) vec_cmpltuq ((vui128_t)tmp, (vui128_t)minnorm);
  tmpz = (vui32_t) vec_cmpequq ((vui128_t)tmp, (vui128_t)vec_zero);
  return (vb128_t) vec_andc (tmp2, tmpz);
#endif
}

/** \brief Return 128-bit vector boolean true value,
 *  if either __float128 value (vra, vrb) is NaN.
 *
 *  A IEEE Binary128 NaN has a exponent of 0x7fff and nonzero
 *  significand. The sign bit is ignored.
 *  For POWER9 and later we use scalar_test_data_class().
 *  Otherwise mask off the sign bit and compare greater than unsigned
 *  quadword to the integer equivalent of Quad-Precision infinity.
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 16-25 | 1/cycle  |
 *  |power9   |   6   | 1/cycle  |
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return a vector boolean _int128.
 */
static inline vb128_t
vec_isunorderedf128 (__binary128 vfa, __binary128 vfb)
{
  return (vb128_t) vec_or ((vui32_t) vec_isnanf128 (vfa),
			   (vui32_t) vec_isnanf128 (vfb));
}

/** \brief Return 128-bit vector boolean true value, if the
 *  value that is +-0.0.
 *
 *  A IEEE Binary64 zero has an exponent of 0x000 and a
 *  zero significand.
 *  The sign bit is ignored.
 *
 *  \note This function will not raise VXSNAN or VXVC (FE_INVALID)
 *  exceptions. A normal __float128 compare can.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 8-17  | 2/cycle  |
 *  |power9   |   6   | 2/cycle  |
 *
 *  @param f128 a vector of __binary32 values.
 *  @return a vector boolean int, each containing all 0s(false)
 *  or 1s(true).
 */
static inline vb128_t
vec_iszerof128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_data_class) && defined (__FLOAT128__) && (__GNUC__ > 7)
  vui32_t result = CONST_VINT128_W (0, 0, 0, 0);

  if (scalar_test_data_class (f128, 0x0c))
    result = CONST_VINT128_W (-1, -1, -1, -1);

  return (vb128_t)result;
#else
  vui128_t t128;
  const vui64_t vec_zero = CONST_VINT128_DW(0, 0);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);

  // Equivalent to vec_absf128 (f128)
  t128 = (vui128_t) vec_andc_bin128_2_vui32t (f128, signmask);
  return  (vb128_t)vec_cmpequq (t128, (vui128_t)vec_zero);
#endif
}

/** \brief Negative Absolute value Quad-Precision
 *
 *  Unconditionally set sign bit of the __float128 input
 *  and return the resulting positive __float128 value.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 2-11  | 2/cycle  |
 *  |power9   | 2     | 4/cycle  |
 *
 *  @param f128 a __float128 value containing a signed value.
 *  @return a __float128 value with magnitude from f128 and a negative
 *  sign.
 */
static inline __binary128
vec_nabsf128 (__binary128 f128)
{
  __binary128 result;
#if _ARCH_PWR9
  __asm__(
      "xsnabsqp %0,%1;\n"
      : "=v" (result)
      : "v" (f128)
      :);
#else
  vui32_t tmp;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);

  tmp = vec_andc_bin128_2_vui32t (f128, signmask);
  result = vec_xfer_vui32t_2_bin128 (tmp);
#endif
  return (result);
}

/** \brief Negate the sign bit of a __float128 input
 *  and return the resulting __float128 value.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 2-11  | 2/cycle  |
 *  |power9   | 2     | 4/cycle  |
 *
 *  @param f128 a __float128 value containing a signed value.
 *  @return a __float128 value with magnitude from f128 and the opposite
 *  sign of f128.
 */
static inline __binary128
vec_negf128 (__binary128 f128)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 6)
#if defined (__FLOAT128__) && (__GNUC__ > 7)
  // Let the compilers generate and optimize code.
  result = -f128;
#else
  // If the compiler supports _ARCH_PWR9, must support mnemonics.
  __asm__(
      "xsnegqp %0,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#endif
#else
  vui32_t tmp;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);

  tmp = vec_xor_bin128_2_vui32t (f128, signmask);
  result = vec_xfer_vui32t_2_bin128 (tmp);
#endif
  return (result);
}

/** \brief Select and Transfer from one of two __binary128 scalars
 * under a 128-bit mask. The result is a __binary128 of the selected
 * value.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 2     | 2/cycle  |
 *  |power9   | 2     | 4/cycle  |
*
*  @param vfa a __binary128 floating point scalar value.
*  @param vfb a __binary128 floating point scalar value.
*  @param mask a vector bool __int128
*  @return The bit of vfa or vfb depending on the mask.
*/
static inline __binary128
vec_self128 (__binary128 vfa, __binary128 vfb, vb128_t mask)
{
  return vec_sel_bin128_2_bin128 (vfa, vfb, mask);
}

/*! \brief Vector Set Bool from Quadword Floating-point.
 *
 *  If the quadword's sign bit is '1' then return a vector bool
 *  __int128 that is all '1's. Otherwise return all '0's.
 *
 *  The resulting mask can be used in vector masking and select
 *  operations.
 *
 *  \note This operation will set the sign mask regardless of data
 *  class. For POWER9 the Scalar Test Data Class instructions copy the
 *  sign bit to CR bit 0 which distinguishes between +/- NaN.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 4 - 6 | 2/cycle  |
 *  |power9   |   6   | 2/cycle  |
 *
 *  @param f128 a 128-bit vector treated a signed __int128.
 *  @return a 128-bit vector bool of all '1's if the sign bit is '1'.
 *  Otherwise all '0's.
 */
static inline vb128_t
vec_setb_qp (__binary128 f128)
{
  vb128_t result;
#if defined (_ARCH_PWR10)  && (__GNUC__ >= 10)
  __asm__(
      "vexpandqm %0,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#elif defined (_ARCH_PWR9) && defined (scalar_test_neg) && (__GNUC__ > 7)
  result = (vb128_t) {(__int128) 0};

  if (scalar_test_neg (f128))
    result = (vb128_t) {(__int128)-1};

  return (vb128_t)result;
#else
  const vui8_t shift = vec_splat_u8 (7);
  vui8_t t128 = vec_xfer_bin128_2_vui8t (f128);
  vui8_t splat = vec_splat (t128, VEC_BYTE_H);

  result = (vb128_t) vec_sra (splat, shift);
#endif
  return result;
}

/** \brief Return int boolean true if the __float128 value
 *  is negative (sign bit is '1').
 *
 *  For POWER9 use scalar_test_neg (a special case of
 *  scalar_test_data_class). For POWER8 and earlier, vec_and
 *  with a signmask and then vec_all_eq compare with that mask
 *  generates the boolean of the sign bit.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 4-10  | 2/cycle  |
 *  |power9   |   3   | 2/cycle  |
 *
 *  @param f128 a __float128 value in vector.
 *  @return a int boolean indicating the sign bit.
 */
static inline int
vec_signbitf128 (__binary128 f128)
{
#if defined (_ARCH_PWR9) && defined (scalar_test_neg) && (__GNUC__ > 7)
  return scalar_test_neg (f128);
#else
  vui32_t tmp;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);

  tmp = vec_and_bin128_2_vui32t (f128, signmask);
  return vec_all_eq(tmp, signmask);
#endif
}

/** \brief VSX Scalar Add Quad-Precision using round to Odd.
 *
 *  The quad-precision element of vectors vfa and vfb are added
 *  to produce the quad-precision result.
 *  The rounding mode is round to odd.
 *
 *  For POWER9 use the xsaddqpo instruction.
 *  For POWER8 use this soft-float implementation using
 *  vector instruction generated by PVECLIB operations.
 *  For POWER7 and earlier us the compilers soft-float implementation.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to setting the FPSCR.
 *  However if the hardware target includes the xsmulqpo instruction,
 *  the implementation may use that.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 54-71 | 1/cycle  |
 *  |power9   |   12  |1/12 cycle|
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return a vector unsigned __int128 value.
 */
static inline __binary128
vec_xsaddqpo (__binary128 vfa, __binary128 vfb)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 7)
#if defined (__FLOAT128__) && (__GNUC__ > 8)
  // earlier GCC versions generate extra data moves for this.
  result = __builtin_addf128_round_to_odd (vfa, vfb);
#else
  // No extra data moves here.
  __asm__(
      "xsaddqpo %0,%1,%2"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#endif
  return result;
#else // defined (_ARCH_PWR7)
  vui64_t q_exp, a_exp, b_exp, x_exp;
  vui128_t q_sig, a_sig, b_sig, p_tmp, p_odd;
  vui128_t a_mag, b_mag;
  vui32_t q_sign,  a_sign,  b_sign;
  vb128_t a_lt_b;
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
  const vui32_t magmask = vec_mask128_f128mag();
  const vui64_t exp_naninf = vec_mask64_f128exp();
  // Vector extract the exponents from vfa, vfb
  x_exp = vec_xxxexpqpp (vfa, vfb);
  // Mask off sign bits so can use integers for magnitude compare.
  a_mag = (vui128_t) vec_and_bin128_2_vui32t (vfa, magmask);
  b_mag = (vui128_t) vec_and_bin128_2_vui32t (vfb, magmask);
  a_sign = vec_andc_bin128_2_vui32t (vfa, magmask);
  b_sign = vec_andc_bin128_2_vui32t (vfb, magmask);
//  if (vec_all_isfinitef128 (vfa) && vec_all_isfinitef128 (vfb))
//  The above can be optimized to the following
  if (__builtin_expect (vec_cmpud_all_lt (x_exp, exp_naninf), 1))
    {
      const vui128_t xbitmask = vec_splat_u128 (1);
      const vui128_t grx_mask = vec_splat_u128 (7);
      const vui64_t exp_min = vec_splat_u64 (1);
      const vui8_t t_sig_L = vec_splat_u8 (7);
      const vui8_t t_sig_C = vec_splat_u8 (15);
      const vui64_t exp_one = exp_min;
      const vui64_t exp_dnrm = (vui64_t) q_zero;
      vui128_t add_sig, sub_sig;
      vui128_t s_sig, x_bits;
      vui32_t diff_sign;
      vui32_t sigmask = vec_mask128_f128sig();
      vui32_t hidden = vec_mask128_f128Lbit();
      vui32_t a_norm, b_norm, x_norm;
      vui32_t a_s32, b_s32;

      // Extract the significand
      // Assume that the sign-bit is already masked off
      // Mask off the significands
      a_s32 = vec_and ((vui32_t) a_mag, sigmask);
      b_s32 = vec_and ((vui32_t) b_mag, sigmask);
      // Assume that exponents are already extracted and merged
      // Compare exponents for denormal, assume finite
      x_norm = (vui32_t) vec_cmpgt ((vui32_t) x_exp, q_zero);
      a_norm = vec_splat (x_norm, VEC_WE_1);
      b_norm = vec_splat (x_norm, VEC_WE_3);
      // For Normal QP insert (hidden) L-bit into significand
      a_sig =  (vui128_t) vec_sel (a_s32, a_norm, hidden);
      b_sig =  (vui128_t) vec_sel (b_s32, b_norm, hidden);
      // Correct exponent for zeros or denormals to E_min
      // will force 0 exponents for zero/denormal results later
      //exp_mask = vec_cmpequd (x_exp, exp_dnrm);
      x_exp = vec_selud ( exp_min, x_exp, (vb64_t) x_norm);
      // Generation sign difference for signed 0.0
      q_sign = vec_xor (a_sign, b_sign);
      // Precondition the significands before add so the GRX bits
      // are in the least significant 3 bit.
      a_sig = vec_slqi (a_sig, 3);
      b_sig = vec_slqi (b_sig, 3);

      // If sign(vfa) != sign(vfb) will need to:
      // 1) Subtract instead of add significands
      // 2) Generate signed zeros
      diff_sign = (vui32_t) vec_setb_sq ((vi128_t) q_sign);
      // If magnitude(b) >  magnitude(a) will need to swap a/b, later
      a_lt_b = vec_cmpltuq (a_mag, b_mag);

      // Now swap operands a/b if necessary so a has greater magnitude.
      {
	vui128_t a_tmp = a_sig;
	vui128_t b_tmp = b_sig;
	vui64_t x_tmp = vec_swapd (x_exp);

	q_sign = vec_sel (a_sign, b_sign, (vui32_t) a_lt_b);

	x_exp = vec_selud (x_exp, x_tmp, (vb64_t) a_lt_b);
	a_exp = vec_splatd (x_exp, VEC_DW_H);
	b_exp = vec_splatd (x_exp, VEC_DW_L);
	q_exp = a_exp;

	a_sig = vec_seluq (a_tmp, b_tmp, (vb128_t) a_lt_b);
	b_sig = vec_seluq (b_tmp, a_tmp, (vb128_t) a_lt_b);
      }
      // At this point we can assume that:
      // The magnitude (vfa) >= magnitude (vfb)
      // 1) Exponents (a_exp, b_exp) in the range E_min -> E_max
      // 2) a_exp >= b_exp
      // 2a) If a_exp == b_exp then a_sig >= b_sig
      // 2b) If a_exp > b_exp then
      //     shift (b_sig) right by (a_exp - b_exp)
      //     any bits shifted out of b_sig are ORed into the X-bit
      if (vec_cmpud_all_lt (b_exp, a_exp))
	{
	  vui64_t d_exp, l_exp;
	  vui128_t t_sig;
	  vb128_t exp_mask;
	  const vui64_t exp_128 = vec_const64_f128_128();

	  d_exp = vec_subudm (a_exp, b_exp);
	  exp_mask = (vb128_t) vec_cmpltud (d_exp, exp_128);
	  l_exp = vec_subudm (exp_128, d_exp);
	  t_sig = vec_srq (b_sig, (vui128_t) d_exp);
	  x_bits = vec_slq (b_sig, (vui128_t) l_exp);
	  t_sig = vec_seluq ((vui128_t) q_zero, t_sig, exp_mask);
	  x_bits = vec_seluq (b_sig, x_bits, exp_mask);
	  p_odd = vec_addcuq (x_bits, (vui128_t) q_ones);
	  b_sig = (vui128_t) vec_or ((vui32_t) t_sig, (vui32_t) p_odd);
	}

      // If operands have the same sign then s_sig = a_sig + b_sig
      // Otherwise s_sig = a_sig - b_sig
      add_sig = vec_adduqm (a_sig, b_sig);
      sub_sig = vec_subuqm (a_sig, b_sig);
      s_sig = vec_seluq (add_sig, sub_sig, (vb128_t) diff_sign);

      if (__builtin_expect (vec_cmpuq_all_eq (s_sig, (vui128_t) q_zero), 0))
	{ // Special case of both zero with different sign
	  q_sign = vec_sel (a_sign, (vui32_t) q_zero, diff_sign);
	  return vec_xfer_vui32t_2_bin128 (q_sign);
	}

      // Issolate CL bits from significand too simplify the compare
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      vui8_t t_sig = vec_splat ((vui8_t) s_sig, 14);
#else
      vui8_t t_sig = vec_splat ((vui8_t) s_sig, 1);
#endif
//      if (vec_cmpuq_all_gt (s_sig, (vui128_t) sigov))
	if (vec_all_gt (t_sig, t_sig_C))
	{ // Check for carry and adjust
	  p_odd = (vui128_t) vec_and ((vui32_t) s_sig, (vui32_t) xbitmask);
	  s_sig = vec_srqi (s_sig, 1);
	  s_sig = (vui128_t) vec_or ((vui32_t) s_sig, (vui32_t) p_odd);
	  q_exp = vec_addudm (q_exp, exp_one);
	}
      else // if (vec_cmpuq_all_le (s_sig, (vui128_t) sigovt))
	  if (vec_all_le (t_sig, t_sig_L))
	{
	  // Or the significand is below normal range.
	  // This can happen with subtraction.
	  vui64_t c_exp, d_exp;
	  vui128_t c_sig;
	  const vui64_t exp_12 = vec_splat_u64 (12);

	  c_sig = vec_clzq (s_sig);
	  c_exp = vec_splatd ((vui64_t) c_sig, VEC_DW_L);
	  // The IR has 12 leading zeros that should not effect the shift count.
	  c_exp = vec_subudm (c_exp, exp_12);
	  d_exp = vec_subudm (q_exp, (vui64_t) exp_min);
	  d_exp = vec_minud (c_exp, d_exp);
	  {
	    vb64_t nrm_mask = vec_cmpgtsd ((vi64_t) q_exp, (vi64_t) exp_min);
	    vb64_t exp_mask = vec_cmpgtud (q_exp, c_exp);

	    c_sig = vec_slq (s_sig, (vui128_t) d_exp);
	    q_exp = vec_subudm (q_exp, d_exp);
	    exp_mask = (vb64_t) vec_and ((vui32_t) exp_mask, (vui32_t) nrm_mask);
	    q_exp = vec_selud (exp_dnrm,  q_exp, exp_mask);
	    s_sig = vec_seluq (s_sig, c_sig, (vb128_t) nrm_mask);
	  }
	}
      // Round to odd from low order GRX-bits
      p_tmp = (vui128_t) vec_and ((vui32_t) s_sig, (vui32_t) grx_mask);
      p_odd = vec_addcuq (p_tmp, (vui128_t) q_ones);
      q_sig = vec_srqi (s_sig, 3);
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, (vui32_t) p_odd);
      // Check for exponent overflow -> __FLT128_MAX__
      if (__builtin_expect ((vec_cmpud_all_ge ( q_exp, exp_naninf)), 0))
	{
	  // return maximum finite exponent and significand
	  const vui32_t f128_max = CONST_VINT128_W(0x7ffeffff, -1, -1, -1);
	  vui32_t f128_smax = vec_or ((vui32_t) f128_max, q_sign);
	  return vec_xfer_vui32t_2_bin128 (f128_smax);
	}
      // Merge sign, significand, and exponent into final result
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
      result = vec_xsiexpqp (q_sig, q_exp);
      return result;
    }
  else // One or both operands are NaN or Infinity
    {
      //const vui32_t q_nan = CONST_VINT128_W(0x00008000, 0, 0, 0);
      vui32_t q_nan = vec_mask128_f128Qbit ();
      // One or both operands are NaN
      if (vec_all_isnanf128 (vfa))
	{
	  // vfa is NaN, Convert vfa to QNaN and return
	  vui32_t vf128 = vec_or_bin128_2_vui32t (vfa, q_nan);
	  return vec_xfer_vui32t_2_bin128 (vf128);
	}
      else if (vec_all_isnanf128 (vfb))
	{
	  // vfb is NaN, Convert vfb to QNaN and return
	  vui32_t vf128 = vec_or_bin128_2_vui32t (vfb, q_nan);
	  return vec_xfer_vui32t_2_bin128 (vf128);
	}
      else  // Or one or both operands are Infinity
	{
	  a_exp = vec_splatd (x_exp, VEC_DW_H);
	  // b_exp = vec_splatd (x_exp, VEC_DW_L);
	  if (vec_cmpud_all_eq (x_exp, exp_naninf)
	      && vec_cmpud_any_ne ((vui64_t) a_sign, (vui64_t) b_sign))
	    { // Both operands infinity and opposite sign
	      // Inifinty + Infinity (opposite sign) is Default Quiet NaN
	      return vec_const_nanf128 ();
	    }
	  else
	    { // Either both operands infinity and same sign
	      // Or one infinity and one finite
	      if (vec_cmpud_any_eq (a_exp, exp_naninf))
		{
		  // return infinity
		  return vfa;
		}
	      else
		{
		  // return infinity
		  return vfb;
		}
	    }
	}
    }
#endif
  return result;
}

/** \brief VSX Scalar Subtract Quad-Precision using round to Odd.
 *
 *  The quad-precision element of vector vfb is subtracted from vfa
 *  to produce the quad-precision result.
 *  The rounding mode is round to odd.
 *
 *  For POWER9 use the xssubqpo instruction.
 *  For POWER8 use this soft-float implementation using
 *  vector instruction generated by PVECLIB operations.
 *  For POWER7 and earlier us the compilers soft-float implementation.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to setting the FPSCR.
 *  However if the hardware target includes the xsmulqpo instruction,
 *  the implementation may use that.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 51-70 | 1/cycle  |
 *  |power9   |   12  |1/12 cycle|
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return a vector unsigned __int128 value.
 */
static inline __binary128
vec_xssubqpo (__binary128 vfa, __binary128 vfb)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 7)
#if defined (__FLOAT128__) && (__GNUC__ > 8)
  // earlier GCC versions generate extra data moves for this.
  result = __builtin_subf128_round_to_odd (vfa, vfb);
#else
  // No extra data moves here.
  __asm__(
      "xssubqpo %0,%1,%2"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#endif
  return result;
#else //  defined (_ARCH_PWR7)
  vui64_t q_exp, a_exp, b_exp, x_exp;
  vui128_t q_sig, a_sig, b_sig, p_tmp, p_odd;
  vui128_t a_mag, b_mag;
  vui32_t q_sign,  a_sign,  b_sign;
  vb128_t a_lt_b;
  const vui32_t q_zero = CONST_VINT128_W (0, 0, 0, 0);
  const vui32_t q_ones = CONST_VINT128_W (-1, -1, -1, -1);
  const vui32_t magmask = vec_mask128_f128mag();
  const vui64_t exp_naninf = vec_mask64_f128exp();
  // Vector extract the exponents from vfa, vfb
  x_exp = vec_xxxexpqpp (vfa, vfb);
  // Mask off sign bits so can use integers for magnitude compare.
  a_mag = (vui128_t) vec_and_bin128_2_vui32t (vfa, magmask);
  b_mag = (vui128_t) vec_and_bin128_2_vui32t (vfb, magmask);
  a_sign = vec_andc_bin128_2_vui32t (vfa, magmask);
  b_sign = vec_andc_bin128_2_vui32t (vfb, magmask);
//  if (vec_all_isfinitef128 (vfa) && vec_all_isfinitef128 (vfb))
//  The above can be optimized to the following
  if (__builtin_expect (vec_cmpud_all_lt (x_exp, exp_naninf), 1))
    {
      const vui128_t xbitmask = vec_splat_u128 (1);
      const vui128_t grx_mask = vec_splat_u128 (7);
      const vui64_t exp_min = vec_splat_u64 (1);
      const vui8_t t_sig_L = vec_splat_u8 (7);
      const vui8_t t_sig_C = vec_splat_u8 (15);
      const vui64_t exp_one = exp_min;
      const vui64_t exp_dnrm = (vui64_t) q_zero;
      // signmask is the complement of the magmask
      const vui32_t signmask = vec_nor(magmask, magmask);
      vui128_t add_sig, sub_sig;
      vui128_t s_sig, x_bits;
      vui32_t diff_sign;
      vui32_t sigmask = vec_mask128_f128sig();
      vui32_t hidden = vec_mask128_f128Lbit();
      vui32_t a_norm, b_norm, x_norm;
      vui32_t a_s32, b_s32;

      // Extract the significand
      // Assume that the sign-bit is already masked off
      // Mask off the significands
      a_s32 = vec_and ((vui32_t) a_mag, sigmask);
      b_s32 = vec_and ((vui32_t) b_mag, sigmask);
      // Assume that exponents are already extracted and merged
      // Compare exponents for denormal, assume finite
      x_norm = (vui32_t) vec_cmpgt ((vui32_t) x_exp, q_zero);
      a_norm = vec_splat (x_norm, VEC_WE_1);
      b_norm = vec_splat (x_norm, VEC_WE_3);
      // For Normal QP insert (hidden) L-bit into significand
      a_sig =  (vui128_t) vec_sel (a_s32, a_norm, hidden);
      b_sig =  (vui128_t) vec_sel (b_s32, b_norm, hidden);
      // Correct exponent for zeros or denormals to E_min
      // will force 0 exponents for zero/denormal results later
      //exp_mask = vec_cmpequd (x_exp, exp_dnrm);
      x_exp = vec_selud ( exp_min, x_exp, (vb64_t) x_norm);
      // Negate sign for subtract, then use add logic
      b_sign = vec_xor (signmask, b_sign);
      // Generation sign difference for signed 0.0
      q_sign = vec_xor (a_sign, b_sign);
      // Precondition the significands before add so the GRX bits
      // are in the least significant 3 bit.
      a_sig = vec_slqi (a_sig, 3);
      b_sig = vec_slqi (b_sig, 3);

      // If sign(vfa) != sign(vfb) will need to:
      // 1) Subtract instead of add significands
      // 2) Generate signed zeros
      diff_sign = (vui32_t) vec_setb_sq ((vi128_t) q_sign);
      // If magnitude(b) >  magnitude(a) will need to swap a/b, later
      a_lt_b = vec_cmpltuq (a_mag, b_mag);

      // Now swap operands a/b if necessary so a has greater magnitude.
      {
	vui128_t a_tmp = a_sig;
	vui128_t b_tmp = b_sig;
	vui64_t x_tmp = vec_swapd (x_exp);

	q_sign = vec_sel (a_sign, b_sign, (vui32_t) a_lt_b);

	x_exp = vec_selud (x_exp, x_tmp, (vb64_t) a_lt_b);
	a_exp = vec_splatd (x_exp, VEC_DW_H);
	b_exp = vec_splatd (x_exp, VEC_DW_L);
	q_exp = a_exp;

	a_sig = vec_seluq (a_tmp, b_tmp, (vb128_t) a_lt_b);
	b_sig = vec_seluq (b_tmp, a_tmp, (vb128_t) a_lt_b);
      }
      // At this point we can assume that:
      // The magnitude (vfa) >= magnitude (vfb)
      // 1) Exponents (a_exp, b_exp) in the range E_min -> E_max
      // 2) a_exp >= b_exp
      // 2a) If a_exp == b_exp then a_sig >= b_sig
      // 2b) If a_exp > b_exp then
      //     shift (b_sig) right by (a_exp - b_exp)
      //     any bits shifted out of b_sig are ORed into the X-bit
      if (vec_cmpud_all_lt (b_exp, a_exp))
	{
	  vui64_t d_exp, l_exp;
	  vui128_t t_sig;
	  vb128_t exp_mask;
	  const vui64_t exp_128 = vec_const64_f128_128();

	  d_exp = vec_subudm (a_exp, b_exp);
	  exp_mask = (vb128_t) vec_cmpltud (d_exp, exp_128);
	  l_exp = vec_subudm (exp_128, d_exp);
	  t_sig = vec_srq (b_sig, (vui128_t) d_exp);
	  x_bits = vec_slq (b_sig, (vui128_t) l_exp);
	  t_sig = vec_seluq ((vui128_t) q_zero, t_sig, exp_mask);
	  x_bits = vec_seluq (b_sig, x_bits, exp_mask);
	  p_odd = vec_addcuq (x_bits, (vui128_t) q_ones);
	  b_sig = (vui128_t) vec_or ((vui32_t) t_sig, (vui32_t) p_odd);
	}

      // If operands have the same sign then s_sig = a_sig + b_sig
      // Otherwise s_sig = a_sig - b_sig
      add_sig = vec_adduqm (a_sig, b_sig);
      sub_sig = vec_subuqm (a_sig, b_sig);
      s_sig = vec_seluq (add_sig, sub_sig, (vb128_t) diff_sign);

      if (__builtin_expect (vec_cmpuq_all_eq (s_sig, (vui128_t) q_zero), 0))
	{ // Special case of both zero with different sign
	  q_sign = vec_sel (a_sign, (vui32_t) q_zero, diff_sign);
	  return vec_xfer_vui32t_2_bin128 (q_sign);
	}

      // Issolate CL bits from significand too simplify the compare
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      vui8_t t_sig = vec_splat ((vui8_t) s_sig, 14);
#else
      vui8_t t_sig = vec_splat ((vui8_t) s_sig, 1);
#endif
//      if (vec_cmpuq_all_gt (s_sig, (vui128_t) sigov))
	if (vec_all_gt (t_sig, t_sig_C))
	{ // Check for carry and adjust
	  p_odd = (vui128_t) vec_and ((vui32_t) s_sig, (vui32_t) xbitmask);
	  s_sig = vec_srqi (s_sig, 1);
	  s_sig = (vui128_t) vec_or ((vui32_t) s_sig, (vui32_t) p_odd);
	  q_exp = vec_addudm (q_exp, exp_one);
	}
      else // if (vec_cmpuq_all_le (s_sig, (vui128_t) sigovt))
	  if (vec_all_le (t_sig, t_sig_L))
	{
	  // Or the significand is below normal range.
	  // This can happen with subtraction.
	  vui64_t c_exp, d_exp;
	  vui128_t c_sig;
	  const vui64_t exp_12 = vec_splat_u64 (12);

	  c_sig = vec_clzq (s_sig);
	  c_exp = vec_splatd ((vui64_t) c_sig, VEC_DW_L);
	  // The IR has 12 leading zeros that should not effect the shift count.
	  c_exp = vec_subudm (c_exp, exp_12);
	  d_exp = vec_subudm (q_exp, (vui64_t) exp_min);
	  d_exp = vec_minud (c_exp, d_exp);
	  {
	    vb64_t nrm_mask = vec_cmpgtsd ((vi64_t) q_exp, (vi64_t) exp_min);
	    vb64_t exp_mask = vec_cmpgtud (q_exp, c_exp);

	    c_sig = vec_slq (s_sig, (vui128_t) d_exp);
	    q_exp = vec_subudm (q_exp, d_exp);
	    exp_mask = (vb64_t) vec_and ((vui32_t) exp_mask, (vui32_t) nrm_mask);
	    q_exp = vec_selud (exp_dnrm,  q_exp, exp_mask);
	    s_sig = vec_seluq (s_sig, c_sig, (vb128_t) nrm_mask);
	  }
	}
      // Round to odd from low order GRX-bits
      p_tmp = (vui128_t) vec_and ((vui32_t) s_sig, (vui32_t) grx_mask);
      p_odd = vec_addcuq (p_tmp, (vui128_t) q_ones);
      q_sig = vec_srqi (s_sig, 3);
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, (vui32_t) p_odd);
      // Check for exponent overflow -> __FLT128_MAX__
      if (__builtin_expect ((vec_cmpud_all_ge ( q_exp, exp_naninf)), 0))
	{
	  // return maximum finite exponent and significand
	  const vui32_t f128_max = CONST_VINT128_W(0x7ffeffff, -1, -1, -1);
	  vui32_t f128_smax = vec_or ((vui32_t) f128_max, q_sign);
	  return vec_xfer_vui32t_2_bin128 (f128_smax);
	}
      // Merge sign, significand, and exponent into final result
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
      result = vec_xsiexpqp (q_sig, q_exp);
      return result;
    }
  else // One or both operands are NaN or Infinity
    {
      //const vui32_t q_nan = CONST_VINT128_W(0x00008000, 0, 0, 0);
      vui32_t q_nan = vec_mask128_f128Qbit ();
      // One or both operands are NaN
      if (vec_all_isnanf128 (vfa))
	{
	  // vfa is NaN, Convert vfa to QNaN and return
	  vui32_t vf128 = vec_or_bin128_2_vui32t (vfa, q_nan);
	  return vec_xfer_vui32t_2_bin128 (vf128);
	}
      else if (vec_all_isnanf128 (vfb))
	{
	  // vfb is NaN, Convert vfb to QNaN and return
	  vui32_t vf128 = vec_or_bin128_2_vui32t (vfb, q_nan);
	  return vec_xfer_vui32t_2_bin128 (vf128);
	}
      else  // Or one or both operands are Infinity
	{
	  a_exp = vec_splatd (x_exp, VEC_DW_H);
	  // b_exp = vec_splatd (x_exp, VEC_DW_L);
	  if (vec_cmpud_all_eq (x_exp, exp_naninf)
	      && vec_cmpud_all_eq ((vui64_t) a_sign, (vui64_t) b_sign))
	    { // Both operands infinity and opposite sign
	      // Inifinty - Infinity (same sign) is Default Quiet NaN
	      return vec_const_nanf128 ();
	    }
	  else
	    { // Either both operands infinity and same sign
	      // Or one infinity and one finite
	      if (vec_cmpud_any_eq (a_exp, exp_naninf))
		{
		  // return infinity
		  return vfa;
		}
	      else
		{
		  // return infinity
		  return vec_negf128(vfb);
		}
	    }
	}
    }
#endif
  return result;
}

/** \brief VSX Scalar Convert Double-Precision to Quad-Precision format.
 *
 *  The left most double-precision element of vector f64 is converted
 *  to quad-precision format.
 *
 *  For POWER9 use the xscvdpqp instruction.
 *  For POWER8 and earlier use vector instruction generated by PVECLIB
 *  operations.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to Signaling NaN and setting the FPSCR.
 *  However if the hardware target includes the xscvdpqp instruction,
 *  the implementation may use that.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |   ?   | 2/cycle  |
 *  |power9   |   3   | 2/cycle  |
 *
 *  @param f64 a vector double. The left most element is converted.
 *  @return a __binary128 value.
 */
__binary128
static inline vec_xscvdpqp (vf64_t f64)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 7)
#if defined (__FLOAT128__) && (__GNUC__ > 9)
  // earlier GCC versions generate extra data moves for this.
  result = f64[VEC_DW_H];
#else
  // No extra data moves here.
  __asm__(
      "xscvdpqp %0,%1"
      : "=v" (result)
      : "v" (f64)
      : );
#endif
#elif  defined (_ARCH_PWR8)
  vui64_t d_exp, d_sig, q_exp;
  vui128_t q_sig;
  vui32_t q_sign;
  const vui64_t exp_delta = (vui64_t) CONST_VINT64_DW ( (0x3fff - 0x3ff), 0 );
  const vui64_t d_naninf = (vui64_t) CONST_VINT64_DW ( 0x7ff, 0 );
  const vui64_t d_denorm = (vui64_t) CONST_VINT64_DW ( 0, 0 );
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);


  f64[VEC_DW_L] = 0.0; // clear the right most element to zero.
  // Extract the exponent, significand, and sign bit.
  d_exp = vec_xvxexpdp (f64);
  d_sig = vec_xvxsigdp (f64);
  q_sign = vec_and ((vui32_t) f64, signmask);
  // The extract sig operation has already tested for finite/subnormal.
  // So avoid testing isfinite/issubnormal again by simply testing
  // the extracted exponent.
  if (__builtin_expect (!vec_cmpud_all_eq (d_exp, d_naninf), 1))
    {
      if (__builtin_expect (!vec_cmpud_all_eq (d_exp, d_denorm), 1))
	{
	  q_sig = vec_srqi ((vui128_t) d_sig, 4);
	  q_exp = vec_addudm (d_exp, exp_delta);
	}
      else
	{
	  if (vec_cmpud_all_eq (d_sig, d_denorm))
	    {
	      q_sig = (vui128_t) d_sig;
	      q_exp = (vui64_t) d_exp;
	    }
	  else
	    { // Must be subnormal but we need to produce a normal QP.
	      // So need to adjust the quad exponent by the f64 denormal
	      // exponent (-1023) and any leading '0's in the f64 sig.
	      // There will be at least 12.
	      vui64_t q_denorm = (vui64_t) CONST_VINT64_DW ( (0x3fff - (1023 - 12)), 0 );
	      vui64_t f64_clz;
	      f64_clz = vec_clzd (d_sig);
	      d_sig = vec_vsld (d_sig, f64_clz);
	      q_exp = vec_subudm (q_denorm, f64_clz);
	      q_sig = vec_srqi ((vui128_t) d_sig, 15);
	    }
	}
    }
  else
    { // isinf or isnan.
      q_sig = vec_srqi ((vui128_t) d_sig, 4);
      q_exp = (vui64_t) CONST_VINT64_DW (0x7fff, 0);
    }
  // Copy Sign-bit to QP significand before insert.
  q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
  // Insert exponent into significand to complete conversion to QP
  result = vec_xsiexpqp (q_sig, q_exp);
#else
  result = f64[VEC_DW_H];
#endif
  return result;
}

/** \brief VSX Scalar Convert with round Quad-Precision to Double-Precision
 *  (using round to odd).
 *
 *  The quad-precision element of vector f128 is converted
 *  to double-precision.
 *  The Floating point value is rounded to odd before conversion.
 *  The result is placed in doubleword element 0
 *  while element 1 is set to zero.
 *
 *  For POWER9 use the xscvqpdpo instruction.
 *  For POWER8 and earlier use vector instructions generated by PVECLIB
 *  operations.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to setting the FPSCR.
 *  However if the hardware target includes the xscvqpdpo instruction,
 *  the implementation may use that.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |   ?   | 1/cycle  |
 *  |power9   |   12  | 1/cycle  |
 *
 *  @param f128 128-bit vector treated as a scalar __binary128.
 *  @return a vector unsigned long long value.
 */
static inline vf64_t
vec_xscvqpdpo (__binary128 f128)
{
  vf64_t result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 7)
#if defined (__FLOAT128__) && (__GNUC__ > 9)
  // GCC runtime does not convert/round directly from __float128 to
  // vector double. So convert scalar double then copy to vector double.
  result = (vf64_t) { 0.0, 0.0 };
  result [VEC_DW_H] = __builtin_truncf128_round_to_odd (f128);
#else
  // No extra data moves here.
  __asm__(
      "xscvqpdpo %0,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#endif
#else //  defined (_ARCH_PWR8)
  vui64_t d_exp, d_sig, x_exp;
  vui64_t q_exp;
  vui128_t q_sig;
  vui32_t q_sign;
  const vui128_t q_zero = { 0 };
  const vui128_t q_ones = (vui128_t) vec_splat_s32 (-1);
  const vui64_t qpdp_delta = (vui64_t) CONST_VINT64_DW ( (0x3fff - 0x3ff), 0 );
  const vui64_t exp_tiny = (vui64_t) CONST_VINT64_DW ( (0x3fff - 1022), (0x3fff - 1022) );
  const vui64_t exp_high = (vui64_t) CONST_VINT64_DW ( (0x3fff + 1023), (0x3fff + 1023));
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  const vui64_t q_naninf = (vui64_t) CONST_VINT64_DW ( 0x7fff, 0x7fff );
  const vui64_t d_naninf = (vui64_t) CONST_VINT64_DW ( 0x7ff, 0 );

  q_exp = vec_xsxexpqp (f128);
  x_exp = vec_splatd (q_exp, VEC_DW_H);
  q_sig = vec_xsxsigqp (f128);
  q_sign = vec_and_bin128_2_vui32t (f128, signmask);
  if (__builtin_expect (!vec_cmpud_all_eq (x_exp, q_naninf), 1))
    {
      if (vec_cmpud_all_ge (x_exp, exp_tiny))
	{ // Greater than or equal to 2**-1022
	  if (vec_cmpud_all_le (x_exp, exp_high))
	    { // Less than or equal to 2**+1023
	      vui64_t d_X;
	      // Convert the significand to double with left shift 4
	      q_sig = vec_slqi ((vui128_t) q_sig, 4);
	      // The GRX round bits are now in bits 64-127 (DW element 1)
	      // For round-to-odd just test for any GRX bits nonzero
	      d_X = (vui64_t) vec_cmpgtud ((vui64_t) q_sig, (vui64_t) q_zero);
	      d_X = vec_mrgald (q_zero, (vui128_t) d_X);
	      d_X = (vui64_t) vec_slqi ((vui128_t) d_X, 1);
	      d_sig = (vui64_t) vec_or ((vui32_t) q_sig, (vui32_t) d_X);
	      d_exp = vec_subudm (q_exp, qpdp_delta);
	    }
	  else
	    { // To high so return infinity OR double max???
	      d_sig = (vui64_t) CONST_VINT64_DW (0x001fffffffffffff, 0);
	      d_exp = (vui64_t) CONST_VINT64_DW (0x7fe, 0);
	    }
	}
      else
	{ // tiny
	  vui64_t d_X;
	  vui64_t q_delta;
	  const vui64_t exp_tinyr = (vui64_t)
	      CONST_VINT64_DW ( (0x3fff-(1022+53)), (0x3fff-(1022+53)));
	  q_delta = vec_subudm (exp_tiny, x_exp);
	  // Set double exp to denormal
	  d_exp = (vui64_t) q_zero;
	  if (vec_cmpud_all_gt (x_exp, exp_tinyr))
	    {
	      // Convert the significand to double with left shift 4
	      // The GRX round bits are now in bits 64-127 (DW element 1)
	      q_sig = vec_slqi ((vui128_t) q_sig, 4);
	      d_sig = (vui64_t) vec_srq (q_sig, (vui128_t) q_delta);
	      // For round-to-odd just test for any nonzero GRX bits.
	      d_X = (vui64_t) vec_cmpgtud ((vui64_t) d_sig, (vui64_t) q_zero);
	      // Generate a low order 0b1 in DW[0]
	      d_X = vec_mrgald (q_zero, (vui128_t) d_X);
	      d_X = (vui64_t) vec_slqi ((vui128_t) d_X, 1);
	      d_sig = (vui64_t) vec_or ((vui32_t) d_sig, (vui32_t) d_X);
	    }
	  else
	    { // tinyr
	      // For round-to-odd just test for any nonzero GRX bits.
	      d_X = (vui64_t) vec_addcuq (q_sig, q_ones);
	      d_sig = (vui64_t) vec_swapd (d_X);
	    }
	}
    }
  else
    { // isinf or isnan.
      const vui64_t q_quiet   = CONST_VINT64_DW (0x0000800000000000, 0);
      vb128_t is_inf;
      vui128_t x_sig;
      is_inf = vec_cmpequq ((vui128_t) q_sig, (vui128_t) q_zero);
      x_sig = (vui128_t) vec_or ((vui32_t) q_sig, (vui32_t) q_quiet);
      q_sig = (vui128_t) vec_sel ((vui32_t)x_sig, (vui32_t)q_sig, (vui32_t)is_inf);
      d_sig = (vui64_t)vec_slqi (q_sig, 4);
      d_exp = d_naninf;
    }

  d_sig [VEC_DW_L] = 0UL;
  d_sig = (vui64_t) vec_or ((vui32_t) d_sig, q_sign);
  result = vec_xviexpdp (d_sig, d_exp);
#endif
  return result;
}

/** \brief VSX Scalar Convert with round to zero Quad-Precision to Unsigned doubleword.
 *
 *  The quad-precision element of vector f128 is converted
 *  to an unsigned doubleword integer.
 *  The Floating point value is rounded toward zero before conversion.
 *  The result is placed in element 0 while element 1 is set to zero.
 *
 *  For POWER9 use the xscvqpudz instruction.
 *  For POWER8 and earlier use vector instructions generated by PVECLIB
 *  operations.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to setting the FPSCR.
 *  However if the hardware target includes the xscvqpudz instruction,
 *  the implementation may use that.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |   ?   | 2/cycle  |
 *  |power9   |   ?   | 2/cycle  |
 *
 *  @param f128 128-bit vector treated as a scalar __binary128.
 *  @return a vector unsigned long long value.
 */
static inline vui64_t
vec_xscvqpudz (__binary128 f128)
{
  vui64_t result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  __asm__(
      "xscvqpudz %0,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#else
  vui64_t q_exp, q_delta, x_exp;
  vui128_t q_sig;
  vb128_t b_sign;
  const vui64_t q_zero = { 0, 0 };
  const vui64_t q_ones = { -1, -1 };
  const vui64_t exp_low = (vui64_t) CONST_VINT64_DW ( 0x3fff, 0x3fff );
  const vui64_t exp_high = (vui64_t) CONST_VINT64_DW ( (0x3fff+64), (0x3fff+64) );
  const vui64_t exp_63 = (vui64_t) CONST_VINT64_DW ( (0x3fff+63), (0x3fff+63) );
  const vui64_t q_naninf = (vui64_t) CONST_VINT64_DW ( 0x7fff, 0x7fff );

  result = q_zero;
  q_exp = vec_xsxexpqp (f128);
  q_sig = vec_xsxsigqp (f128);
  x_exp = vec_splatd (q_exp, VEC_DW_H);
  b_sign = vec_setb_qp (f128);
  if (__builtin_expect (!vec_cmpud_all_eq (x_exp, q_naninf), 1))
    {
      if (vec_cmpud_all_ge (x_exp, exp_low)
       && vec_cmpud_all_eq ((vui64_t)b_sign, (vui64_t)q_zero))
	{ // Greater than or equal to 1.0
	  if (vec_cmpud_all_lt (x_exp, exp_high))
	    { // Less than 2**64-1
	      q_sig = vec_slqi (q_sig, 15);
	      q_delta = vec_subudm (exp_63, x_exp);
	      result = vec_vsrd ((vui64_t) q_sig, q_delta);
	    }
	  else
	    { // set result to 2**64-1
	      result = q_ones;
	    }
	}
      else
	{ // less than 1.0 or negative
	  result = q_zero;
	}
    }
  else
    { // isinf or isnan.
      vb128_t is_inf;
      // Positive Inf returns all ones
      // else NaN or -Infinity returns zero
      is_inf = vec_cmpequq (q_sig, (vui128_t) q_zero);
      // result = ~NaN | (pos & Inf) -> Inf & (pos & Inf) -> pos & Inf
      result = (vui64_t) vec_andc ((vui32_t) is_inf, (vui32_t) b_sign);
    }
  result = vec_mrgahd ((vui128_t) result, (vui128_t) q_zero);
#endif
  return result;
}

/** \brief VSX Scalar Convert with round to zero Quad-Precision to Unsigned Quadword.
 *
 *  The quad-precision element of vector f128 is converted
 *  to an unsigned quadword integer.
 *  The Floating point value is rounded toward zero before conversion.
 *
 *  For POWER10 use the xscvqpuqz instruction.
 *  For POWER9 and earlier use vector instruction generated by PVECLIB
 *  operations.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to setting the FPSCR.
 *  However if the hardware target includes the xscvqpuqz instruction,
 *  the implementation may use that.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |   ?   | 2/cycle  |
 *  |power9   |   ?   | 2/cycle  |
 *
 *  @param f128 128-bit vector treated as a scalar __binary128.
 *  @return a vector unsigned __int128 value.
 */
static inline vui128_t
vec_xscvqpuqz (__binary128 f128)
{
  vui128_t result;
#if defined (_ARCH_PWR10) && (__GNUC__ >= 10)
  __asm__(
      "xscvqpuqz %0,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#else
  vui64_t q_exp, q_delta, x_exp;
  vui128_t q_sig;
  vb128_t b_sign;
  const vui128_t q_zero = { 0 };
  const vui128_t q_ones = (vui128_t) vec_splat_s32 (-1);
  const vui64_t exp_low = (vui64_t) CONST_VINT64_DW ( 0x3fff, 0x3fff );
  const vui64_t exp_high = (vui64_t) CONST_VINT64_DW ( (0x3fff+128), (0x3fff+128) );
  const vui64_t exp_127 = (vui64_t) CONST_VINT64_DW ( (0x3fff+127), (0x3fff+127) );
  const vui64_t q_naninf = (vui64_t) CONST_VINT64_DW ( 0x7fff, 0x7fff );

  result = q_zero;
  q_exp = vec_xsxexpqp (f128);
  q_sig = vec_xsxsigqp (f128);
  x_exp = vec_splatd (q_exp, VEC_DW_H);
  b_sign = vec_setb_qp (f128);
  if (__builtin_expect (!vec_cmpud_all_eq (x_exp, q_naninf), 1))
    {
      if (vec_cmpud_all_ge (x_exp, exp_low)
       && vec_cmpud_all_eq ((vui64_t)b_sign, (vui64_t)q_zero))
	{ // Greater than or equal to 1.0
	  if (vec_cmpud_all_lt (x_exp, exp_high))
	    { // Less than 2**128-1
	      q_sig = vec_slqi (q_sig, 15);
	      q_delta = vec_subudm (exp_127, x_exp);
	      result = vec_srq (q_sig, (vui128_t) q_delta);
	    }
	  else
	    { // set result to 2**128-1
	      result = (vui128_t) q_ones;
	    }
	}
      else
	{ // less than 1.0 or negative
	  result = (vui128_t) q_zero;
	}
    }
  else
    { // isinf or isnan.
      vb128_t is_inf;
      // Positive Inf returns all ones
      // else NaN or -Infinity returns zero
      is_inf = vec_cmpequq (q_sig, (vui128_t) q_zero);
      // result = ~NaN | (pos & Inf) -> Inf & (pos & Inf) -> pos & Inf
      result = (vui128_t) vec_andc ((vui32_t) is_inf, (vui32_t) b_sign);
    }
#endif
  return result;
}

/** \brief VSX Scalar Convert Signed-Doubleword to Quad-Precision format.
 *
 *  The left most signed doubleword element of vector int64 is converted
 *  to quad-precision format.
 *
 *  For POWER9 use the xscvsdqp instruction.
 *  For POWER8 and earlier use vector instruction generated by PVECLIB
 *  operations.
 *
 *
 *  \note At this point we are not trying to comply with PowerISA by
 *  setting any FPSCR bits associated with Quad-Precision convert.
 *  If such is required, FR and/or FI can be set
 *  using the Move To FPSCR Bit 0 (mtfsb0) instruction.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |   ?   | 2/cycle  |
 *  |power9   |   3   | 2/cycle  |
 *
 *  @param int64 a vector signed long long. The left most element is converted.
 *  @return a __binary128 value.
 */
__binary128
static inline vec_xscvsdqp (vi64_t int64)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 7)
#if defined (__FLOAT128__) && (__GNUC__ > 9)
  // earlier GCC versions generate extra data moves for this.
  result = int64[VEC_DW_H];
#else
  // No extra data moves here.
  __asm__(
      "xscvsdqp %0,%1"
      : "=v" (result)
      : "v" (int64)
      : );
#endif
#elif  defined (_ARCH_PWR8)
  vui64_t d_sig, q_exp, d_sign, d_neg;
  vui128_t q_sig;
  vui32_t q_sign;
  const vui64_t d_zero = (vui64_t) CONST_VINT64_DW ( 0, 0 );
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);

  int64[VEC_DW_L] = 0UL; // clear the right most element to zero.

  if (vec_cmpud_all_eq ((vui64_t) int64, d_zero))
    {
      result = vec_xfer_vui64t_2_bin128 (d_zero);
    }
  else
    {
      // We need to produce a normal QP, so we treat the integer like a
      // denormal, then normalize it.
      // Start with the quad exponent bias + 63 then subtract the count
      // leading '0's. The 64-bit magnitude has 1-63 leading '0's
      vui64_t q_expm = (vui64_t) CONST_VINT64_DW ((0x3fff + 63), 0 );
      vui64_t i64_clz;
      // Convert 2s complement to signed magnitude form.
      q_sign = vec_and ((vui32_t) int64, signmask);
      d_neg  = vec_subudm (d_zero, (vui64_t)int64);
      d_sign = (vui64_t) vec_cmpequd ((vui64_t) q_sign, (vui64_t) signmask);
      d_sig = (vui64_t) vec_sel ((vui32_t) int64, (vui32_t) d_neg, (vui32_t) d_sign);
      // Count leading zeros and normalize.
      i64_clz = vec_clzd (d_sig);
      d_sig = vec_vsld (d_sig, i64_clz);
      q_exp = vec_subudm (q_expm, i64_clz);
      q_sig = vec_srqi ((vui128_t) d_sig, 15);
      // Copy Sign-bit to QP significand before insert.
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
      // Insert exponent into significand to complete conversion to QP
      result = vec_xsiexpqp (q_sig, q_exp);
    }
#else
  result = int64[VEC_DW_H];
#endif
  return result;
}

/** \brief VSX Scalar Convert Unsigned-Doubleword to Quad-Precision format.
 *
 *  The left most unsigned doubleword element of vector int64 is converted
 *  to quad-precision format.
 *
 *  For POWER9 use the xscvudqp instruction.
 *  For POWER8 and earlier use vector instruction generated by PVECLIB
 *  operations.
 *
 *  \note At this point we are not trying to comply with PowerISA by
 *  setting any FPSCR bits associated with Quad-Precision convert.
 *  If such is required, FR and/or FI can be set
 *  using the Move To FPSCR Bit 0 (mtfsb0) instruction.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |   ?   | 2/cycle  |
 *  |power9   |   3   | 2/cycle  |
 *
 *  @param int64 a vector unsigned long long. The left most element is converted.
 *  @return a __binary128 value.
 */
__binary128
static inline vec_xscvudqp (vui64_t int64)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 7)
#if defined (__FLOAT128__) && (__GNUC__ > 9)
  // earlier GCC versions generate extra data moves for this.
  result = int64[VEC_DW_H];
#else
  // No extra data moves here.
  __asm__(
      "xscvudqp %0,%1"
      : "=v" (result)
      : "v" (int64)
      : );
#endif
#elif  defined (_ARCH_PWR8)
  vui64_t d_sig, q_exp;
  vui128_t q_sig;
  const vui64_t d_zero = (vui64_t) CONST_VINT64_DW ( 0, 0 );

  int64[VEC_DW_L] = 0UL; // clear the right most element to zero.
  d_sig = int64;
  // Quick test for 0UL as this case requires a special exponent.
  if (vec_cmpud_all_eq (int64, d_zero))
    {
      result = vec_xfer_vui64t_2_bin128 (d_zero);
    }
  else
    { // We need to produce a normal QP, so we treat the integer like a
      // denormal, then normalize it.
      // Start with the quad exponent bias + 63 then subtract the count of
      // leading '0's. The 64-bit sig can have 0-63 leading '0's.
      const vui64_t q_expm = (vui64_t) CONST_VINT64_DW ((0x3fff + 63), 0 );
      vui64_t i64_clz = vec_clzd (int64);
      d_sig = vec_vsld (int64, i64_clz);
      q_exp = vec_subudm (q_expm, i64_clz);
      q_sig = vec_srqi ((vui128_t) d_sig, 15);
      // Insert exponent into significand to complete conversion to QP
      result = vec_xsiexpqp (q_sig, q_exp);
    }
#else
  result = int64[VEC_DW_H];
#endif
  return result;
}

/** \brief VSX Scalar Convert Signed-Quadword to Quad-Precision format.
 *
 *  The signed quadword element of vector int128 is converted
 *  to quad-precision format.
 *  If the conversion is not exact the default rounding mode is
 *  "Round to Nearest Even".
 *
 *  For POWER10 use the xscvuqqp instruction.
 *  POWER9 only supports doubleword converts so use a combination of
 *  two xscvudqp and xsmaddqp instructions.
 *  For POWER8 and earlier use vector instruction generated by PVECLIB
 *  operations.
 *
 *  \note The POWER8 implementation ignores the hardware rounding mode
 *  <B>FPSCR<sub>RN</sub></B>.
 *
 *  \note At this point we are not trying to comply with PowerISA by
 *  setting any FPSCR bits associated with Quad-Precision convert.
 *  If such is required, FPFR, FR and FI can be set
 *  using the Move To FPSCR Bit 0/1 (mtfsb[0|1]) instruction.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |   ?   | 2/cycle  |
 *  |power9   | 44-53 |1/13cycles|
 *
 *  @param int128 a vector signed __int128 which is converted to QP format.
 *  @return a __binary128 value.
 */
__binary128
static inline vec_xscvsqqp (vi128_t int128)
{
  __binary128 result;
#if defined (_ARCH_PWR10)  && (__GNUC__ >= 10)
  __asm__(
      "xscvsqqp %0,%1"
      : "=v" (result)
      : "v" (int128)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  __binary128 hi64, lo64, i_sign;
  __binary128 two64 = 0x1.0p64;
  vui128_t q_sig;
  vui32_t q_sign;
  vui128_t q_neg;
  vb128_t b_sign;
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  // Collect the sign bit of the input value.
  q_sign = vec_and ((vui32_t) int128, signmask);
  // Convert 2s complement to unsigned magnitude form.
  q_neg  = (vui128_t) vec_negsq (int128);
  b_sign = vec_setb_sq (int128);
  q_sig = vec_seluq ((vui128_t) int128, q_neg, b_sign);
  // Generate a signed 0.0 to use with vec_copysignf128
  i_sign = vec_xfer_vui32t_2_bin128 (q_sign);
  // Convert the unsigned int128 magnitude to __binary128
  vui64_t int64 = (vui64_t) q_sig;
  hi64 = int64[VEC_DW_H];
  lo64 = int64[VEC_DW_L];
  result = (hi64 * two64) + lo64;
  // Copy the __int128's sign into the __binary128 result
  result = vec_copysignf128 (i_sign, result);
#elif  defined (_ARCH_PWR8)
  vui64_t q_exp;
  vui128_t q_sig;
  vui128_t q_neg;
  vui32_t q_sign;
  vb128_t b_sign;
  const vui128_t q_zero = (vui128_t) { 0 };
  const vui32_t lowmask = CONST_VINT128_W ( 0, 0, 0, 1);
  const vui32_t signmask = CONST_VINT128_W (0x80000000, 0, 0, 0);
  // Quick test for 0UL as this case requires a special exponent.
  if (vec_cmpuq_all_eq ((vui128_t) int128, q_zero))
    {
      result = vec_xfer_vui128t_2_bin128 (q_zero);
    }
  else
    { // We need to produce a normal QP, so we treat the integer like a
      // denormal, then normalize it.
      // Collect the sign bit of the input value.
      q_sign = vec_and ((vui32_t) int128, signmask);
      // Convert 2s complement to signed magnitude form.
      q_neg  = (vui128_t) vec_negsq (int128);
      b_sign = vec_setb_sq (int128);
      q_sig = vec_seluq ((vui128_t) int128, q_neg, b_sign);
      // Start with the quad exponent bias + 127 then subtract the count of
      // leading '0's. The 128-bit sig can have 0-127 leading '0's.
      vui64_t q_expm = (vui64_t) CONST_VINT64_DW (0, (0x3fff + 127));
      vui64_t i64_clz = (vui64_t) vec_clzq (q_sig);
      q_sig = vec_slq (q_sig, (vui128_t) i64_clz);
      q_exp = vec_subudm (q_expm, i64_clz);
      // This is the part that might require rounding.

      // The Significand (including the L-bit) is right justified in
      // in the high-order 113-bits of q_sig.
      // The guard, round, and sticky (GRX) bits are in the low-order
      // 15 bits.
      // The sticky-bits are the last 13 bits and are logically ORed
      // (or added to 0x1fff) to produce the X-bit.
      //
      // For "Round to Nearest Even".
      // GRX = 0b001 - 0b011; truncate
      // GRX = 0b100 and bit-112 is odd; round up, otherwise truncate
      // GRX = 0b100 - 0b111; round up
      // We can simplify by copying bit-112 and OR it with bit-X
      // Then add 0x3fff to q_sig will generate a carry into bit-112
      // if and only if GRX > 0b100 or (GRX == 0b100) && (bit-112 == 1)
      const vui32_t RXmask = CONST_VINT128_W ( 0, 0, 0, 0x3fff);
      vui128_t q_carry, q_sigc;
      vb128_t qcmask;
      vui32_t q_odd;
      // Isolate bit-112 and OR into GRX bits if q_sig is odd
      q_odd = (vui32_t) vec_srhi ((vui16_t)q_sig, 15);
      q_odd = vec_and (q_odd, lowmask);
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_odd);
      // We add 0x3fff to GRX-bits which may carry into low order sig-bit
      // This may result in a carry out of bit L into bit-C.
      q_carry = vec_addcuq (q_sig, (vui128_t) RXmask);
      q_sig = vec_adduqm (q_sig, (vui128_t) RXmask);
      // Generate a bool mask from the carry to use in the vsel
      qcmask = vec_setb_cyq (q_carry);
      // Two cases; 1) We did carry so shift (double) left 112 bits
      q_sigc = vec_sldqi (q_carry, q_sig, 112);
      // 2) no carry so shift left 15 bits
      q_sig = vec_srqi ((vui128_t) q_sig, 15);
      // Select which based on carry
      q_sig = (vui128_t) vec_sel ((vui32_t) q_sig, (vui32_t) q_sigc, (vui32_t) qcmask);
      // Increment the exponent based on the carry
      q_exp = vec_addudm (q_exp, (vui64_t) q_carry);

      q_exp = vec_swapd (q_exp);
      // Copy Sign-bit to QP significand before insert.
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
      result = vec_xsiexpqp (q_sig, q_exp);
    }
#else
  result = int128[0];
#endif
  return result;
}

/** \brief VSX Scalar Convert Unsigned-Quadword to Quad-Precision format.
 *
 *  The unsigned quadword element of vector int128 is converted
 *  to quad-precision format.
 *  If the conversion is not exact the default rounding mode is
 *  "Round to Nearest Even".
 *
 *  For POWER10 use the xscvuqqp instruction.
 *  POWER9 only supports doubleword converts so use a combination of
 *  two xscvudqp and xsmaddqp instructions.
 *  For POWER8 and earlier use vector instruction generated by PVECLIB
 *  operations.
 *
 *  \note The POWER8 implementation ignores the hardware rounding mode
 *  <B>FPSCR<sub>RN</sub></B>.
 *
 *  \note At this point we are not trying to comply with PowerISA by
 *  setting any FPSCR bits associated with Quad-Precision convert.
 *  If such is required, FPFR, FR and FI can be set
 *  using the Move To FPSCR Bit 0/1 (mtfsb[0|1]) instruction.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |   ?   | 2/cycle  |
 *  |power9   | 38-47 |1/13cycles|
 *
 *  @param int128 a vector unsigned __int128 which is converted to QP format.
 *  @return a __binary128 value.
 */
__binary128
static inline vec_xscvuqqp (vui128_t int128)
{
  __binary128 result;
#if defined (_ARCH_PWR10)  && (__GNUC__ >= 10)
  __asm__(
      "xscvuqqp %0,%1"
      : "=v" (result)
      : "v" (int128)
      : );
#elif defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  vui64_t int64 = (vui64_t) int128;
  __binary128 hi64, lo64;
  __binary128 two64 = 0x1.0p64;
  hi64 = int64[VEC_DW_H];
  lo64 = int64[VEC_DW_L];
  result = (hi64 * two64) + lo64;
#elif  defined (_ARCH_PWR8)
  vui64_t q_exp;
  vui128_t q_sig;
  const vui128_t q_zero = (vui128_t) { 0 };
  const vui32_t lowmask = CONST_VINT128_W ( 0, 0, 0, 1);

  q_sig = int128;
  // Quick test for 0UL as this case requires a special exponent.
  if (vec_cmpuq_all_eq (q_sig, q_zero))
    {
      result = vec_xfer_vui128t_2_bin128 (q_zero);
    }
  else
    { // We need to produce a normal QP, so we treat the integer like a
      // denormal, then normalize it.
      // Start with the quad exponent bias + 127 then subtract the count of
      // leading '0's. The 128-bit sig can have 0-127 leading '0's.
      vui64_t q_expm = (vui64_t) CONST_VINT64_DW (0, (0x3fff + 127));
      vui64_t i64_clz = (vui64_t) vec_clzq (q_sig);
      q_sig = vec_slq (q_sig, (vui128_t) i64_clz);
      q_exp = vec_subudm (q_expm, i64_clz);
      // This is the part that might require rounding.
      // The Significand (including the L-bit) is right justified in
      // in the high-order 113-bits of q_sig.
      // The guard, round, and sticky (GRX) bits are in the low-order
      // 15 bits.
      // The sticky-bits are the last 13 bits and are logically ORed
      // (or added to 0x1fff) to produce the X-bit.
      //
      // For "Round to Nearest Even".
      // GRX = 0b001 - 0b011; truncate
      // GRX = 0b100 and bit-112 is odd; round up, otherwise truncate
      // GRX = 0b100 - 0b111; round up
      // We can simplify by copying bit-112 and OR it with bit-X
      // Then add 0x3fff to q_sig will generate a carry into bit-112
      // if and only if GRX > 0b100 or (GRX == 0b100) && (bit-112 == 1)
      const vui32_t RXmask = CONST_VINT128_W ( 0, 0, 0, 0x3fff);
      vui128_t q_carry, q_sigc;
      vb128_t qcmask;
      vui32_t q_odd;
      // Isolate bit-112 and OR into GRX bits if q_sig is odd
      q_odd = (vui32_t) vec_srhi ((vui16_t)q_sig, 15);
      q_odd = vec_and (q_odd, lowmask);
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_odd);
      // We add 0x3fff to GRX-bits which may carry into low order sig-bit
      // This may result in a carry out of bit L into bit-C.
      q_carry = vec_addcuq (q_sig, (vui128_t) RXmask);
      q_sig = vec_adduqm (q_sig, (vui128_t) RXmask);
      // Generate a bool mask from the carry to use in the vsel
      qcmask = vec_setb_cyq (q_carry);
      // Two cases; 1) We did carry so shift (double) left 112 bits
      q_sigc = vec_sldqi (q_carry, q_sig, 112);
      // 2) no carry so shift left 15 bits
      q_sig = vec_srqi ((vui128_t) q_sig, 15);
      // Select which based on carry
      q_sig = (vui128_t) vec_sel ((vui32_t) q_sig, (vui32_t) q_sigc, (vui32_t) qcmask);
      // Increment the exponent based on the carry
      q_exp = vec_addudm (q_exp, (vui64_t) q_carry);
      q_exp = vec_swapd (q_exp);
      result = vec_xsiexpqp (q_sig, q_exp);
    }
#else
  result = int128[0];
#endif
  return result;
}

/** \brief VSX Scalar Multiply Quad-Precision using round to Odd.
 *
 *  The quad-precision element of vectors vfa and vfb are multiplied
 *  to produce the quad-precision result.
 *  The rounding mode is round to odd.
 *
 *  For POWER9 use the xsmulqpo instruction.
 *  For POWER8 use this soft-float implementation using
 *  vector instruction generated by PVECLIB operations.
 *  For POWER7 and earlier is the compilers soft-float implementation.
 *
 *  \note This operation <I>may not</I> follow the PowerISA
 *  relative to setting the FPSCR.
 *  However if the hardware target includes the xsmulqpo instruction,
 *  the implementation may use that.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 78-84 | 1/cycle  |
 *  |power9   |   24  |1/12 cycle|
 *
 *  @param vfa 128-bit vector treated as a scalar __binary128.
 *  @param vfb 128-bit vector treated as a scalar __binary128.
 *  @return a vector unsigned __int128 value.
 */
static inline __binary128
vec_xsmulqpo (__binary128 vfa, __binary128 vfb)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && (__GNUC__ > 7)
#if defined (__FLOAT128__) && (__GNUC__ > 8)
  // earlier GCC versions generate extra data moves for this.
  result = __builtin_mulf128_round_to_odd (vfa, vfb);
#else
  // No extra data moves here.
  __asm__(
      "xsmulqpo %0,%1,%2"
      : "=v" (result)
      : "v" (vfa), "v" (vfb)
      : );
#endif
  return result;
#else  //_ARCH_PWR8 or _ARCH_PWR7
  vui64_t q_exp, a_exp, b_exp, x_exp;
  vui128_t q_sig, a_sig, b_sig, p_sig_h, p_sig_l, p_odd;
  vui32_t q_sign, a_sign, b_sign;
  vui128_t a_mag, b_mag;
  const vui32_t q_zero = CONST_VINT128_W(0, 0, 0, 0);
  const vui32_t q_ones = CONST_VINT128_W(-1, -1, -1, -1);
  //const vui64_t exp_naninf = (vui64_t) { 0x7fff, 0x7fff };
  const vui64_t exp_naninf = vec_mask64_f128exp ();
  const vui32_t magmask = vec_mask128_f128mag ();

  // Vector extract the exponents from vfa, vfb
  x_exp = vec_xxxexpqpp (vfa, vfb);
  // Mask off sign bits so can use integers for magnitude compare.
  a_mag = (vui128_t) vec_and_bin128_2_vui32t (vfa, magmask);
  b_mag = (vui128_t) vec_and_bin128_2_vui32t (vfb, magmask);
  a_sign = vec_andc_bin128_2_vui32t (vfa, magmask);
  b_sign = vec_andc_bin128_2_vui32t (vfb, magmask);
  q_sign = vec_xor (a_sign, b_sign);

//  if (vec_all_isfinitef128 (vfa) && vec_all_isfinitef128 (vfb))
  if (__builtin_expect (vec_cmpud_all_lt (x_exp, exp_naninf), 1))
    {
      const vui64_t exp_dnrm = (vui64_t) q_zero;
      vui64_t exp_min, exp_one, exp_bias;
      vui128_t p_tmp;
      // const vui64_t exp_min, exp_one = { 1, 1 };
      // exp_min = exp_one = vec_splat_u64 (1);
	{ // Extract the significands and insert the Hidden bit
	  //const vui32_t q_zero = CONST_VINT128_W(0, 0, 0, 0);
	  const vui32_t sigmask = vec_mask128_f128sig ();
	  vui32_t a_s32, b_s32;
	  vui16_t a_e16, b_e16, x_hidden;
	  vb16_t a_norm, b_norm;

	  //const vui32_t hidden = vec_mask128_f128Lbit();
	  x_hidden = vec_splat_u16(1);
	  // Assume that the operands are finite magnitudes
	  // Mask off the significands
	  // Applying sigmask to orignal inputs can save 2 cycles here
	  a_s32 = vec_and_bin128_2_vui32t (vfa, sigmask);
	  b_s32 = vec_and_bin128_2_vui32t (vfb, sigmask);
	  // But still need a/b_mag for exp extract to clear sign-bit
	  // Mask off the exponents in high halfword
	  a_e16 = (vui16_t) vec_andc ((vui32_t) a_mag, sigmask);
	  b_e16 = (vui16_t) vec_andc ((vui32_t) b_mag, sigmask);
	  // Compare exponents for finite i.e. > denomal (q_zero)
	  a_norm = vec_cmpgt (a_e16, (vui16_t) q_zero);
	  b_norm = vec_cmpgt (b_e16, (vui16_t) q_zero);
	  // For Normal QP insert (hidden) L-bit into significand
	  a_sig = (vui128_t) vec_sel ((vui16_t) a_s32, x_hidden, a_norm);
	  b_sig = (vui128_t) vec_sel ((vui16_t) b_s32, x_hidden, b_norm);
	}

      // Precondition the significands before multiply so that the
      // high-order 114-bits (C,L,FRACTION) of the product are right
      // adjusted in p_sig_h. And the Low-order 112-bits are left
      // justified in p_sig_l.
      // Logically this (multiply) step could be moved after the zero
      // test. But this uses a lot of registers and the compiler may
      // see this as register pressure and decide to spill and reload
      // unrelated data around this block.
      // The zero multiply is rare so on average performance is better
      // if we get this started now.
      a_sig = vec_slqi (a_sig, 8);
      b_sig = vec_slqi (b_sig, 8);
      p_sig_l = vec_muludq (&p_sig_h, a_sig, b_sig);

      // check for zero significands in multiply
      if (__builtin_expect (
	  (vec_all_eq((vui32_t ) a_sig, (vui32_t ) q_zero)
	      || vec_all_eq((vui32_t ) b_sig, (vui32_t ) q_zero)),
	  0))
	{ // Multiply by zero, return QP signed zero
	  result = vec_xfer_vui32t_2_bin128 (q_sign);
	  return result;
	}

      // const vui64_t exp_min, exp_one = { 1, 1 };
      exp_min = exp_one = vec_splat_u64 (1);
      //const vui64_t exp_bias = (vui64_t) { 0x3fff, 0x3fff };
      exp_bias = (vui64_t) vec_srhi ((vui16_t) exp_naninf, 1);
	{ // Compute product exponent q_exp
	  // Operand exponents should >= Emin for computation
	  vb64_t exp_mask;
	  exp_mask = vec_cmpequd (x_exp, exp_dnrm);
	  x_exp = vec_selud (x_exp, exp_min, (vb64_t) exp_mask);
	  // sum exponents across x_exp
	  q_exp = vec_addudm (x_exp, vec_swapd (x_exp));
	  // Sum includes 2 x exp_bias, So subtract 1 x exp_bias
	  q_exp = vec_subudm (q_exp, exp_bias);
	}

      // Check for carry; shift right 1 and adjust exp +1
	{
	  vb128_t carry_mask;
	  vui128_t sig_h, sig_l;
	  // Test Carry-bit (greater than L-bit)
	  vui16_t sig_l_mask = vec_splat_u16(1);
	  vui16_t t_sig = vec_splat ((vui16_t) p_sig_h, VEC_HW_H);
	  carry_mask = (vb128_t) vec_cmpgt (t_sig, sig_l_mask);
	  // Shift double quadword right 1 bit
	  p_tmp = vec_sldqi (p_sig_h, p_sig_l, 120);
	  sig_h = vec_srqi (p_sig_h, 1);
	  sig_l = vec_slqi (p_tmp, 7);
	  // Increment the exponent
	  x_exp = vec_addudm (q_exp, exp_one);
	  // Select original or normalized exp/sig
	  p_sig_h = vec_seluq (p_sig_h, sig_h, carry_mask);
	  p_sig_l = vec_seluq (p_sig_l, sig_l, carry_mask);
	  q_exp = vec_selud (q_exp, x_exp, (vb64_t) carry_mask);
	}

      // There are two cases for denormal
      // 1) The sum of unbiased exponents is less the E_min (tiny).
      // 2) The significand is less then 1.0 (C and L-bits are zero).
      //  2a) The exponent is > E_min
      //  2b) The exponent is == E_min
      //
      q_sig = p_sig_h;
      // Check for Tiny exponent
      if (__builtin_expect (
	  (vec_cmpsd_all_lt ((vi64_t) q_exp, (vi64_t) exp_min)), 0))
	{
	  //const vui64_t exp_128 = (vui64_t) { 128, 128 };
	  const vui64_t exp_128 = vec_const64_f128_128 ();
	  const vui64_t too_tiny = (vui64_t
		)
		  { 116, 116 };
	  // const vui32_t xmask = CONST_VINT128_W(0x1fffffff, -1, -1, -1);
	  vui32_t xmask = (vui32_t) vec_srqi ((vui128_t) q_ones, 3);
	  vui32_t tmp;

	  // Intermediate result is tiny, unbiased exponent < -16382
	  //x_exp = vec_subudm ((vui64_t) exp_tiny, q_exp);
	  x_exp = vec_subudm (exp_min, q_exp);

	  if (vec_cmpud_all_gt ((vui64_t) x_exp, too_tiny))
	    {
	      // Intermediate result is too tiny, the shift will
	      // zero the fraction and the GR-bit leaving only the
	      // Sticky bit. The X-bit needs to include all bits
	      // from p_sig_h and p_sig_l
	      p_sig_l = vec_srqi (p_sig_l, 8);
	      p_sig_l = (vui128_t) vec_or ((vui32_t) p_sig_l,
					   (vui32_t) p_sig_h);
	      // generate a carry into bit-2 for any nonzero bits 3-127
	      p_sig_l = vec_adduqm (p_sig_l, (vui128_t) xmask);
	      q_sig = (vui128_t) q_zero;
	      p_sig_l = (vui128_t) vec_andc ((vui32_t) p_sig_l, xmask);
	    }
	  else
	    { // Normal tiny, right shift may loose low order bits
	      // from p_sig_l. So collect any 1-bits below GRX and
	      // OR them into the X-bit, before the right shift.
	      vui64_t l_exp;

	      // Propagate low order bits into the sticky bit
	      // GRX left adjusted in p_sig_l
	      // Issolate bits below GDX (bits 3-128).
	      tmp = vec_and ((vui32_t) p_sig_l, xmask);
	      // generate a carry into bit-2 for any nonzero bits 3-127
	      tmp = (vui32_t) vec_adduqm ((vui128_t) tmp, (vui128_t) xmask);
	      // Or this with the X-bit to propagate any sticky bits into X
	      p_sig_l = (vui128_t) vec_or ((vui32_t) p_sig_l, tmp);
	      p_sig_l = (vui128_t) vec_andc ((vui32_t) p_sig_l, xmask);

	      l_exp = vec_subudm (exp_128, x_exp);
	      p_sig_l = vec_sldq (p_sig_h, p_sig_l, (vui128_t) l_exp);
	      p_sig_h = vec_srq (p_sig_h, (vui128_t) x_exp);
	      q_sig = p_sig_h;
	    }
	  // Set the exponent for denormal
	  q_exp = exp_dnrm;
	}
      // Exponent is not tiny but significand may be denormal
      // Isolate sig CL bits and compare
      vui16_t t_sig = vec_splat ((vui16_t) p_sig_h, VEC_HW_H);
      if (__builtin_expect ((vec_all_eq(t_sig, (vui16_t ) q_zero)), 0))
	{
	  // Is below normal range. This can happen when
	  // multiplying a denormal by a normal.
	  // So try to normalize the significand.
	  //const vui64_t exp_15 = { 15, 15 };
	  const vui64_t exp_15 = vec_splat_u64 (15);
	  vui64_t c_exp, d_exp;
	  vui128_t c_sig;
	  vb64_t exp_mask;
	  c_sig = vec_clzq (p_sig_h);
	  c_exp = vec_splatd ((vui64_t) c_sig, VEC_DW_L);
	  c_exp = vec_subudm (c_exp, exp_15);
	  d_exp = vec_subudm (q_exp, exp_min);
	  d_exp = vec_minud (c_exp, d_exp);
	  exp_mask = vec_cmpgtud (q_exp, c_exp);

	  // Intermediate result <= tiny, unbiased exponent <= -16382
	  if (vec_cmpsd_all_gt ((vi64_t) q_exp, (vi64_t) exp_min))
	    {
	      // Try to normalize the significand.
	      p_sig_h = vec_sldq (p_sig_h, p_sig_l, (vui128_t) d_exp);
	      p_sig_l = vec_slq (p_sig_l, (vui128_t) d_exp);
	      q_sig = p_sig_h;
	      // Compare computed exp to shift count to normalize.
	      //exp_mask = vec_cmpgtud (q_exp, c_exp);
	      q_exp = vec_subudm (q_exp, d_exp);
	      q_exp = vec_selud (exp_dnrm, q_exp, exp_mask);
	    }
	  else
	    { // sig is denormal range (L-bit is 0). Set exp to zero.
	      q_exp = exp_dnrm;
	    }
	}
      // Merge sign early will not effect rounding for this mode
      // q_ssig = vec_or ((vui32_t) q_sig, q_sign);
      // Round to odd from lower product bits
      p_odd = vec_addcuq (p_sig_l, (vui128_t) q_ones);
      q_sig = (vui128_t) vec_or ((vui32_t) q_sig, (vui32_t) p_odd);

      // Check for exponent overflow -> __FLT128_MAX__ (round to odd)
      if (__builtin_expect ((vec_cmpud_all_ge (q_exp, exp_naninf)), 0))
	{
	  // Intermediate result is huge, unbiased exponent > 16383
	  // so return __FLT128_MAX__ with the appropriate sign.
	  const vui32_t f128_max = CONST_VINT128_W(0x7ffeffff, -1, -1, -1);
	  vui32_t f128_smax = vec_or ((vui32_t) f128_max, q_sign);
	  return vec_xfer_vui32t_2_bin128 (f128_smax);
	}
      else // combine sign, exp, and significand for return
	{
	  // Merge sign, significand, and exponent into final result
	  q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
	  vui32_t tmp, t128;
	  // convert DW exp_naninf to QW expmask
	  vui32_t expmask = vec_sld ((vui32_t) exp_naninf, q_zero, 14);
	  // convert q_exp from DW to QW for QP format
	  tmp = vec_sld ((vui32_t) q_exp, q_zero, 14);
	  t128 = vec_sel ((vui32_t) q_sig, tmp, expmask);
	  result = vec_xfer_vui32t_2_bin128 (t128);
	  return result;
	}
    }
  else
    { // One or both operands are NaN or Infinity
      //const vui32_t q_nan = CONST_VINT128_W(0x00008000, 0, 0, 0);
      vui32_t q_nan = vec_mask128_f128Qbit ();
      vui32_t q_inf = vec_mask128_f128exp ();
      // One or both operands are NaN
      if (vec_all_isnanf128 (vfa))
	{
	  // vfa is NaN, Convert vfa to QNaN and return
	  vui32_t vf128 = vec_or_bin128_2_vui32t (vfa, q_nan);
	  return vec_xfer_vui32t_2_bin128 (vf128);
	}
      else if (vec_all_isnanf128 (vfb))
	{
	  // vfb is NaN, Convert vfb to QNaN and return
	  vui32_t vf128 = vec_or_bin128_2_vui32t (vfb, q_nan);
	  return vec_xfer_vui32t_2_bin128 (vf128);
	}
      else  // Or one or both operands are Infinity
	{
	  if (vec_cmpud_all_eq (x_exp, (vui64_t) exp_naninf))
	    {
	      // Infinity x Infinity == signed Infinity
	      q_sig = (vui128_t) q_inf;
	    }
	  else
	    {
	      // One each Infinity/Finite value, check for 0.0
	      if (vec_cmpuq_all_eq (a_mag, (vui128_t) q_zero)
		  || vec_cmpuq_all_eq (b_mag, (vui128_t) q_zero))
		{
		  // Inifinty x Zero is Default Quiet NaN
		  return vec_const_nanf128 ();
		}
	      else // an Infinity and a Nonzero finite number
		{
		  // Return Infinity with product sign.
		  q_sig = (vui128_t) q_inf;
		}
	    }
	  // Merge sign, exp/sig into final result
	  q_sig = (vui128_t) vec_or ((vui32_t) q_sig, q_sign);
	  return vec_xfer_vui128t_2_bin128 (q_sig);
	}
    }
#endif
  return result;
}

/** \brief Scalar Insert Exponent Quad-Precision
 *
 *  Merge the sign (bit 0) and significand (bits 16:127) from sig
 *  with the 15-bit exponent from exp (bits 49:63). The exponent is
 *  moved to bits 1:15 of the final result.
 *  The result is returned as a Quad_precision floating point value.
 *
 *  \note This operation is equivalent to the POWER9 xsiexpqp
 *  instruction, This instruction requires a
 *  POWER9-enabled compiler targeting -mcpu=power9 and is not
 *  available for older compilers nor POWER8 and earlier.
 *  We can't use the built-in scalar_insert_exp because it requires
 *  scalar (GPR) inputs and vec_insert_exp is not defined for
 *  Quad-Precision.
 *  We expect (in context) inputs will be in VRs.
 *  This operation provides implementations for all VSX-enabled
 *  platforms.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  6-8  | 2/cycle  |
 *  |power9   |   2   | 4/cycle  |
 *
 *  @param sig vector __int128 containing the Sign Bit and 112-bit significand.
 *  @param exp vector unsigned long long element 0 containing the 15-bit exponent.
 *  @return a __binary128 value where the exponent bits (1:15) of sig
 *  are replaced from bits 49:63 of exp.
 *
 */
static inline __binary128
vec_xsiexpqp (vui128_t sig, vui64_t exp)
{
  __binary128 result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  __asm__(
      "xsiexpqp %0,%1,%2"
      : "=v" (result)
      : "v" (sig), "v" (exp)
      : );

#else
  vui32_t tmp, t128;
  vui32_t expmask = vec_mask128_f128exp();

  tmp = vec_sld ((vui32_t) exp, (vui32_t) exp, 6);
  t128 =  vec_sel ((vui32_t) sig, tmp, expmask);
  result = vec_xfer_vui32t_2_bin128 (t128);
#endif
  return result;
}

/** \brief Scalar Extract Exponent Quad-Precision
 *
 *  Extract the quad-precision exponent (bits 1:15) and right justify
 *  it to (bits 49:63 of) doubleword 0 of the result vector.
 *  The result is returned as vector long long integer value.
 *
 *  \note This operation is equivalent to the POWER9 xsxexpqp
 *  instruction. This instruction requires a
 *  POWER9-enabled compiler targeting -mcpu=power9 and is not
 *  available for older compilers nor POWER8 and earlier.
 *  We can't use the built-in scalar_extract_exp because it returns
 *  scalar (GPR) results and vec_extract_exp is not defined for
 *  Quad-Precision.
 *  We expect (in context) results are needed in VRs.
 *  This operation provides implementations for all VSX-enabled
 *  platforms.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  8-10 | 2/cycle  |
 *  |power9   |   2   | 4/cycle  |
 *
 *  @param f128 __binary128 scalar value in a vector register.
 *  @return vector unsigned long long element 0 containing the 15-bit
 *  exponent
 *
 */
static inline vui64_t
vec_xsxexpqp (__binary128 f128)
{
  vui64_t result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  __asm__(
      "xsxexpqp %0,%1"
      : "=v" (result)
      : "v" (f128)
      : );

#else
  vui32_t tmp;
  vui32_t expmask = vec_mask128_f128exp();

  tmp = vec_and_bin128_2_vui32t (f128, expmask);
  result = (vui64_t) vec_sld (tmp, tmp, 10);
#endif
  return result;
}

/** \brief Scalar Extract Significand Quad-Precision
 *
 *  Extract the quad-precision significand (bits 16:127) and
 *  restore the implied (hidden) bit (bit 15) if the quad-precition
 *  value is normal (not zero, subnormal, Infinity or NaN).
 *  The result is returned as vector __int128 integer value with
 *  up to 113 bits of significance.
 *
 *  \note This operation is equivalent to the POWER9 xsxsigqp
 *  instruction. This instruction requires a
 *  POWER9-enabled compiler targeting -mcpu=power9 and is not
 *  available for older compilers nor POWER8 and earlier.
 *  We can't use the built-in scalar_extract_sig because it returns
 *  scalar (GPR) results and vec_extract_sig is not defined for
 *  Quad-Precision.
 *  We expect (in context) results are needed in VRs.
 *  This operation provides implementations for all VSX-enabled
 *  platforms.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   | 12-14 | 1/6cycles|
 *  |power9   |   3   | 2/cycle  |
 *
 *  @param f128 __binary128 scalar value in a vector register.
 *  @return vector __int128 containing the significand.
 *
 */
static inline vui128_t
vec_xsxsigqp (__binary128 f128)
{
  vui128_t result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  __asm__(
      "xsxsigqp %0,%1"
      : "=v" (result)
      : "v" (f128)
      : );
#else
  vui32_t t128, tmp, normal;
  const vui32_t zero = CONST_VINT128_W (0, 0, 0, 0);
#if 1
  const vui32_t sigmask = vec_mask128_f128sig();
  const vui32_t expmask = vec_mask128_f128exp();
  const vui32_t hidden = vec_mask128_f128Lbit();
#else
  const vui32_t sigmask = CONST_VINT128_W (0x0000ffff, -1, -1, -1);
  const vui32_t expmask = CONST_VINT128_W (0x7fff0000, 0, 0, 0);
  const vui32_t hidden = CONST_VINT128_W (0x00010000, 0, 0, 0);
#endif

  // Check if f128 is normal. Normal values need the hidden bit
  // restored to the significand. We use a simpler sequence here as
  // vec_isnormalf128 does more then we need.
  tmp = vec_and_bin128_2_vui32t (f128, expmask);
  normal = (vui32_t) vec_nor (vec_cmpeq (tmp, expmask),
		              vec_cmpeq (tmp, zero));
  t128 = vec_and_bin128_2_vui32t (f128, sigmask);
  result = (vui128_t) vec_sel (t128, normal, hidden);
#endif
  return result;
}

/** \brief Vector Extract Exponent Quad-Precision Pair
 *
 *  Extract the quad-precision exponent (bits 1:15),
 *  from each member of the QP Pair,
 *  and right justify the exponents
 *  to (bits 49:63 of) doublewords 0/1 of the result vector.
 *  The result is returned as vector long long integer value.
 *
 *  \note This operation is equivalent to the POWER9 instruction
 *  sequence xsxexpqp/xsxexpqp/xxmrghd.
 *
 *  |processor|Latency|Throughput|
 *  |--------:|:-----:|:---------|
 *  |power8   |  6-8  | 1/cycle  |
 *  |power9   |   5   | 2/cycle  |
 *
 *  @param vfa first __binary128 scalar value in a vector register.
 *  @param vfb first __binary128 scalar value in a vector register.
 *  @return vector unsigned long long elements 0/1 containing
 *  15-bit exponents from vfa/vfb.
 *
 */
static inline vui64_t
vec_xxxexpqpp (__binary128 vfa, __binary128 vfb)
{
  vui64_t result;
#if defined (_ARCH_PWR9) && defined (__FLOAT128__) && (__GNUC__ > 7)
  vui64_t exp_a, exp_b;
  __asm__(
      "xsxexpqp %0,%2;"
      "xsxexpqp %1,%3"
      : "=v" (exp_a), "=v" (exp_b)
      : "v" (vfa), "v" (vfb)
      : );
  result = vec_mrgahd ((vui128_t) exp_a, (vui128_t) exp_b);
#else
  vui32_t tmp, rtmp, exp_mask;
  //const vui32_t expmask = CONST_VINT128_W (0, 0x7fff, 0, 0x7fff);
  exp_mask = (vui32_t) vec_mask64_f128exp();
  tmp = (vui32_t) vec_mrgh_bin128_2_vui64t (vfa, vfb);
  rtmp = vec_sld (tmp, tmp, 10);
  result = (vui64_t) vec_and (rtmp, exp_mask);
#endif
  return result;
}

#endif /* VEC_F128_PPC_H_ */
