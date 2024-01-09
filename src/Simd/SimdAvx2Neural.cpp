/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2022 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdMemory.h"
#include "Simd/SimdExtract.h"
#include "Simd/SimdStore.h"
#include "Simd/SimdStream.h"
#include "Simd/SimdBase.h"
#include "Simd/SimdNeural.h"
#include "Simd/SimdPow.h"
#include "Simd/SimdExp.h"

namespace Simd
{
#ifdef SIMD_AVX2_ENABLE    
    namespace Avx2
    {
        template <bool align> SIMD_INLINE void AdaptiveGradientUpdate(const float* delta, const __m256& norm, const __m256& alpha, const __m256& epsilon, float* gradient, float* weight)
        {
            __m256 d = _mm256_mul_ps(Load<align>(delta), norm);
            __m256 _gradient = _mm256_add_ps(Load<align>(gradient), _mm256_mul_ps(d, d));
            Store<align>(gradient, _gradient);
            Store<align>(weight, _mm256_sub_ps(Load<align>(weight), _mm256_mul_ps(_mm256_mul_ps(alpha, d), _mm256_rsqrt_ps(_mm256_add_ps(_gradient, epsilon)))));
        }

        template <bool align> SIMD_INLINE void AdaptiveGradientUpdate(const float* delta, size_t offset, const __m256& norm, const __m256& alpha, const __m256& epsilon, float* gradient, float* weight)
        {
            AdaptiveGradientUpdate<align>(delta + offset, norm, alpha, epsilon, gradient + offset, weight + offset);
        }

        template <bool align> void NeuralAdaptiveGradientUpdate(const float* delta, size_t size, size_t batch, const float* alpha, const float* epsilon, float* gradient, float* weight)
        {
            if (align)
                assert(Aligned(delta) && Aligned(gradient) && Aligned(weight));

            size_t partialAlignedSize = AlignLo(size, F);
            size_t fullAlignedSize = AlignLo(size, QF);
            const float norm = (float)(1.0 / batch);
            __m256 _norm = _mm256_set1_ps(norm);
            __m256 _alpha = _mm256_set1_ps(*alpha);
            __m256 _epsilon = _mm256_set1_ps(*epsilon);
            size_t i = 0;
            if (partialAlignedSize)
            {
                if (fullAlignedSize)
                {
                    for (; i < fullAlignedSize; i += QF)
                    {
                        AdaptiveGradientUpdate<align>(delta, i + F * 0, _norm, _alpha, _epsilon, gradient, weight);
                        AdaptiveGradientUpdate<align>(delta, i + F * 1, _norm, _alpha, _epsilon, gradient, weight);
                        AdaptiveGradientUpdate<align>(delta, i + F * 2, _norm, _alpha, _epsilon, gradient, weight);
                        AdaptiveGradientUpdate<align>(delta, i + F * 3, _norm, _alpha, _epsilon, gradient, weight);
                    }
                }
                for (; i < partialAlignedSize; i += F)
                    AdaptiveGradientUpdate<align>(delta, i, _norm, _alpha, _epsilon, gradient, weight);
            }
            for (; i < size; ++i)
                Base::AdaptiveGradientUpdate(delta, i, norm, *alpha, *epsilon, gradient, weight);
        }

        void NeuralAdaptiveGradientUpdate(const float* delta, size_t size, size_t batch, const float* alpha, const float* epsilon, float* gradient, float* weight)
        {
            if (Aligned(delta) && Aligned(gradient) && Aligned(weight))
                NeuralAdaptiveGradientUpdate<true>(delta, size, batch, alpha, epsilon, gradient, weight);
            else
                NeuralAdaptiveGradientUpdate<false>(delta, size, batch, alpha, epsilon, gradient, weight);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE void AddVector(const float* src, float* dst)
        {
            Store<align>(dst, _mm256_add_ps(Load<align>(dst), Load<align>(src)));
        }

        template <bool align> SIMD_INLINE void AddVector(const float* src, size_t aligned, size_t partial, size_t full, float* dst)
        {
            size_t i = 0;
            for (; i < aligned; i += QF)
            {
                AddVector<align>(src + i + F * 0, dst + i + F * 0);
                AddVector<align>(src + i + F * 1, dst + i + F * 1);
                AddVector<align>(src + i + F * 2, dst + i + F * 2);
                AddVector<align>(src + i + F * 3, dst + i + F * 3);
            }
            for (; i < partial; i += F)
                AddVector<align>(src + i, dst + i);
            for (; i < full; ++i)
                dst[i] += src[i];
        }

        void NeuralAddVector(const float* src, size_t size, float* dst)
        {
            size_t aligned = AlignLo(size, QF);
            size_t partial = AlignLo(size, F);
            if (Aligned(src) && Aligned(dst))
                AddVector<true>(src, aligned, partial, size, dst);
            else
                AddVector<false>(src, aligned, partial, size, dst);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE void AddValue(const __m256& value, float* dst)
        {
            Store<align>(dst, _mm256_add_ps(Load<align>(dst), value));
        }

        template <bool align> SIMD_INLINE void AddValue(const float* value, float* dst, size_t aligned, size_t partial, size_t full)
        {
            size_t i = 0;
            if (partial)
            {
                __m256 _value = _mm256_set1_ps(value[0]);
                for (; i < aligned; i += QF)
                {
                    AddValue<align>(_value, dst + i + F * 0);
                    AddValue<align>(_value, dst + i + F * 1);
                    AddValue<align>(_value, dst + i + F * 2);
                    AddValue<align>(_value, dst + i + F * 3);
                }
                for (; i < partial; i += F)
                    AddValue<align>(_value, dst + i);
            }
            for (; i < full; ++i)
                dst[i] += value[0];
        }

        void NeuralAddValue(const float* value, float* dst, size_t size)
        {
            size_t aligned = AlignLo(size, QF);
            size_t partial = AlignLo(size, F);
            if (Aligned(dst))
                AddValue<true>(value, dst, aligned, partial, size);
            else
                AddValue<false>(value, dst, aligned, partial, size);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool inversion> __m128i Invert(__m128i value);

        template <> __m128i Invert<true>(__m128i value)
        {
            return _mm_sub_epi8(Sse41::K_INV_ZERO, value);
        }

        template <> __m128i Invert<false>(__m128i value)
        {
            return value;
        }

        template <bool inversion, bool align, bool stream> void Convert(const uint8_t * src, const __m256 & _1_255, float * dst)
        {
            __m128i _src = Invert<inversion>(_mm_loadl_epi64((__m128i*)src));
            Avx::Stream<align, stream>(dst, _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_src)), _1_255));
        }

        template <bool inversion, bool align, bool stream> void NeuralConvert(const uint8_t * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride)
        {
            assert(width >= F);
            if (align)
                assert(Aligned(dst) && Aligned(dstStride));

            size_t alignedWidth = AlignLo(width, F);
            __m256 _1_255 = _mm256_set1_ps(1.0f / 255.0f);

            for (size_t row = 0; row < height; ++row)
            {
                for (size_t col = 0; col < alignedWidth; col += F)
                    Convert<inversion, align, stream>(src + col, _1_255, dst + col);
                if (width != alignedWidth)
                    Convert<inversion, false, stream>(src + width - F, _1_255, dst + width - F);
                src += srcStride;
                dst += dstStride;
            }
            if (stream)
                _mm_mfence();
        }

        template <bool inversion> void NeuralConvert(const uint8_t * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride)
        {
            if (Aligned(src) && Aligned(srcStride) && Aligned(dst) && Aligned(dstStride))
            {
                if (width*height * sizeof(float) >= STREAM_SIZE_MIN)
                    NeuralConvert<inversion, true, true>(src, srcStride, width, height, dst, dstStride);
                else
                    NeuralConvert<inversion, true, false>(src, srcStride, width, height, dst, dstStride);
            }
            else
                NeuralConvert<inversion, false, false>(src, srcStride, width, height, dst, dstStride);
        }

        void NeuralConvert(const uint8_t * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride, int inversion)
        {
            if (inversion)
                NeuralConvert<true>(src, srcStride, width, height, dst, dstStride);
            else
                NeuralConvert<false>(src, srcStride, width, height, dst, dstStride);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE void NeuralProductSum(const float * a, const float * b, size_t offset, __m256 & sum)
        {
            __m256 _a = Load<align>(a + offset);
            __m256 _b = Load<align>(b + offset);
            sum = _mm256_fmadd_ps(_a, _b, sum);
        }

        template <bool align> SIMD_INLINE void NeuralProductSum(const float * a, const float * b, size_t size, float * sum)
        {
            if (align)
                assert(Aligned(a) && Aligned(b));

            *sum = 0;
            size_t partialAlignedSize = AlignLo(size, F);
            size_t fullAlignedSize = AlignLo(size, QF);
            size_t i = 0;
            if (partialAlignedSize)
            {
                __m256 sums[4] = { _mm256_setzero_ps(), _mm256_setzero_ps(), _mm256_setzero_ps(), _mm256_setzero_ps() };
                if (fullAlignedSize)
                {
                    for (; i < fullAlignedSize; i += QF)
                    {
                        NeuralProductSum<align>(a, b, i + F * 0, sums[0]);
                        NeuralProductSum<align>(a, b, i + F * 1, sums[1]);
                        NeuralProductSum<align>(a, b, i + F * 2, sums[2]);
                        NeuralProductSum<align>(a, b, i + F * 3, sums[3]);
                    }
                    sums[0] = _mm256_add_ps(_mm256_add_ps(sums[0], sums[1]), _mm256_add_ps(sums[2], sums[3]));
                }
                for (; i < partialAlignedSize; i += F)
                    NeuralProductSum<align>(a, b, i, sums[0]);
                *sum += Avx::ExtractSum(sums[0]);
            }
            for (; i < size; ++i)
                *sum += a[i] * b[i];
        }

        void NeuralProductSum(const float * a, const float * b, size_t size, float * sum)
        {
            if (Aligned(a) && Aligned(b))
                NeuralProductSum<true>(a, b, size, sum);
            else
                NeuralProductSum<false>(a, b, size, sum);
        }

        //-------------------------------------------------------------------------------------------------

        void NeuralAddVectorMultipliedByValue(const float * src, size_t size, const float * value, float * dst)
        {
            size_t aligned = AlignLo(size, QF);
            size_t partial = AlignLo(size, F);
            if (Aligned(src) && Aligned(dst))
                AddMultiplied<true>(src, aligned, partial, size, *value, dst);
            else
                AddMultiplied<false>(src, aligned, partial, size, *value, dst);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE void NeuralDerivativeSigmoid(const float* src, size_t size, const float* slope, float* dst)
        {
            size_t alignedSize = Simd::AlignLo(size, F);
            __m256 _slope = _mm256_set1_ps(*slope);
            __m256 _1 = _mm256_set1_ps(1.0f);
            size_t i = 0;
            for (; i < alignedSize; i += F)
            {
                __m256 _src = Load<align>(src + i);
                __m256 _dst = Load<align>(dst + i);
                Store<align>(dst + i, _mm256_mul_ps(_mm256_mul_ps(_dst, _slope), _mm256_mul_ps(_mm256_sub_ps(_1, _src), _src)));
            }
            for (; i < size; ++i)
                dst[i] *= slope[0] * Base::DerivativeSigmoid(src[i]);
        }

        void NeuralDerivativeSigmoid(const float* src, size_t size, const float* slope, float* dst)
        {
            if (Aligned(src) && Aligned(dst))
                NeuralDerivativeSigmoid<true>(src, size, slope, dst);
            else
                NeuralDerivativeSigmoid<false>(src, size, slope, dst);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE void NeuralDerivativeTanh(const float* src, size_t size, const float* slope, float* dst)
        {
            size_t alignedSize = Simd::AlignLo(size, F);
            __m256 _slope = _mm256_set1_ps(*slope);
            __m256 _1 = _mm256_set1_ps(1.0f);
            size_t i = 0;
            for (; i < alignedSize; i += F)
            {
                __m256 _src = Load<align>(src + i);
                __m256 _dst = Load<align>(dst + i);
                Store<align>(dst + i, _mm256_mul_ps(_mm256_mul_ps(_dst, _slope), _mm256_sub_ps(_1, _mm256_mul_ps(_src, _src))));
            }
            for (; i < size; ++i)
                dst[i] *= slope[0] * Base::DerivativeTanh(src[i]);
        }

        void NeuralDerivativeTanh(const float* src, size_t size, const float* slope, float* dst)
        {
            if (Aligned(src) && Aligned(dst))
                NeuralDerivativeTanh<true>(src, size, slope, dst);
            else
                NeuralDerivativeTanh<false>(src, size, slope, dst);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> void NeuralDerivativeRelu(const float* src, size_t size, const float* slope, float* dst)
        {
            float s = slope[0];
            __m256 _0 = _mm256_set1_ps(0.0f);
            __m256 _1 = _mm256_set1_ps(1.0f);
            __m256 _s = _mm256_set1_ps(s);
            size_t alignedSize = Simd::AlignLo(size, F);
            size_t i = 0;
            for (; i < alignedSize; i += F)
            {
                __m256 mask = _mm256_cmp_ps(Load<align>(src + i), _0, _CMP_GT_OS);
                __m256 _dst = Load<align>(dst + i);
                Store<align>(dst + i, _mm256_mul_ps(_mm256_blendv_ps(_s, _1, mask), _dst));
            }
            for (; i < size; ++i)
                dst[i] *= src[i] > 0 ? 1.0f : s;
        }

        void NeuralDerivativeRelu(const float* src, size_t size, const float* slope, float* dst)
        {
            if (Aligned(src) && Aligned(dst))
                NeuralDerivativeRelu<true>(src, size, slope, dst);
            else
                NeuralDerivativeRelu<false>(src, size, slope, dst);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE void NeuralRoughSigmoid(const float* src, size_t size, const float* slope, float* dst)
        {
            size_t alignedSize = Simd::AlignLo(size, F);
            __m256 _slope = _mm256_set1_ps(*slope);
            __m256 _0 = _mm256_set1_ps(-0.0f);
            __m256 _1 = _mm256_set1_ps(1.0f);
            __m256 _a = _mm256_set1_ps(0.5417f);
            __m256 _b = _mm256_set1_ps(0.1460f);
            size_t i = 0;
            for (; i < alignedSize; i += F)
            {
                __m256 _src = Load<align>(src + i);
                __m256 x = _mm256_andnot_ps(_0, _mm256_mul_ps(_src, _slope));
                __m256 x2 = _mm256_mul_ps(x, x);
                __m256 x4 = _mm256_mul_ps(x2, x2);
                __m256 series = _mm256_add_ps(_mm256_add_ps(_1, x), _mm256_add_ps(_mm256_mul_ps(x2, _a), _mm256_mul_ps(x4, _b)));
                __m256 mask = _mm256_cmp_ps(_src, _0, _CMP_GT_OS);
                __m256 exp = _mm256_blendv_ps(series, _mm256_rcp_ps(series), mask);
                __m256 sigmoid = _mm256_rcp_ps(_mm256_add_ps(_1, exp));
                Store<align>(dst + i, sigmoid);
            }
            for (; i < size; ++i)
                dst[i] = Base::RoughSigmoid(src[i] * slope[0]);
        }

        void NeuralRoughSigmoid(const float* src, size_t size, const float* slope, float* dst)
        {
            if (Aligned(src) && Aligned(dst))
                NeuralRoughSigmoid<true>(src, size, slope, dst);
            else
                NeuralRoughSigmoid<false>(src, size, slope, dst);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE void NeuralRoughSigmoid2(const float * src, const __m256 & k, const __m256 & o, const __m256 & m, float * dst)
        {
            __m256 _src = Load<align>(src);
            __m256 e1 = _mm256_max_ps(m, _mm256_fmadd_ps(_src, k, o));
            __m256 e2 = _mm256_mul_ps(e1, e1);
            __m256 e4 = _mm256_mul_ps(e2, e2);
            __m256 e8 = _mm256_mul_ps(e4, e4);
            __m256 e16 = _mm256_mul_ps(e8, e8);
            __m256 e32 = _mm256_mul_ps(e16, e16);
            __m256 e64 = _mm256_mul_ps(e32, e32);
            __m256 sigmoid = _mm256_rcp_ps(_mm256_fmadd_ps(e64, e64, o));
            Store<align>(dst, sigmoid);
        }

        template <bool align> SIMD_INLINE void NeuralRoughSigmoid2(const float * src, size_t size, const float * slope, float * dst)
        {
            size_t partialAlignedSize = Simd::AlignLo(size, F);
            size_t fullAlignedSize = Simd::AlignLo(size, QF);
            __m256 _k = _mm256_set1_ps(-(*slope)*0.0078125f);
            __m256 _1 = _mm256_set1_ps(1.0f);
            __m256 _05 = _mm256_set1_ps(0.5f);
            size_t i = 0;
            for (; i < fullAlignedSize; i += QF)
            {
                NeuralRoughSigmoid2<align>(src + i + 0 * F, _k, _1, _05, dst + i + 0 * F);
                NeuralRoughSigmoid2<align>(src + i + 1 * F, _k, _1, _05, dst + i + 1 * F);
                NeuralRoughSigmoid2<align>(src + i + 2 * F, _k, _1, _05, dst + i + 2 * F);
                NeuralRoughSigmoid2<align>(src + i + 3 * F, _k, _1, _05, dst + i + 3 * F);
            }
            for (; i < partialAlignedSize; i += F)
                NeuralRoughSigmoid2<align>(src + i, _k, _1, _05, dst + i);
            for (; i < size; ++i)
                dst[i] = Base::RoughSigmoid2(src[i] * slope[0]);
        }

        void NeuralRoughSigmoid2(const float * src, size_t size, const float * slope, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                NeuralRoughSigmoid2<true>(src, size, slope, dst);
            else
                NeuralRoughSigmoid2<false>(src, size, slope, dst);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE void NeuralRoughTanh(const float* src, size_t size, const float* slope, float* dst)
        {
            size_t alignedSize = Simd::AlignLo(size, F);
            __m256 _slope = _mm256_set1_ps(*slope);
            __m256 _0 = _mm256_set1_ps(-0.0f);
            __m256 _1 = _mm256_set1_ps(1.0f);
            __m256 _a = _mm256_set1_ps(0.5658f);
            __m256 _b = _mm256_set1_ps(0.1430f);
            size_t i = 0;
            for (; i < alignedSize; i += F)
            {
                __m256 _src = Load<align>(src + i);
                __m256 x = _mm256_andnot_ps(_0, _mm256_mul_ps(_src, _slope));
                __m256 x2 = _mm256_mul_ps(x, x);
                __m256 x4 = _mm256_mul_ps(x2, x2);
                __m256 pe = _mm256_add_ps(_mm256_add_ps(_1, x), _mm256_add_ps(_mm256_mul_ps(x2, _a), _mm256_mul_ps(x4, _b)));
                __m256 ne = _mm256_rcp_ps(pe);
                __m256 absTanh = _mm256_mul_ps(_mm256_sub_ps(pe, ne), _mm256_rcp_ps(_mm256_add_ps(pe, ne)));
                __m256 tanh = _mm256_xor_ps(absTanh, _mm256_and_ps(_0, _mm256_cmp_ps(_0, _src, _CMP_GT_OS)));
                Store<align>(dst + i, tanh);
            }
            for (; i < size; ++i)
                dst[i] = Base::RoughTanh(src[i] * slope[0]);
        }

        void NeuralRoughTanh(const float* src, size_t size, const float* slope, float* dst)
        {
            if (Aligned(src) && Aligned(dst))
                NeuralRoughTanh<true>(src, size, slope, dst);
            else
                NeuralRoughTanh<false>(src, size, slope, dst);
        }

        //-------------------------------------------------------------------------------------------------

        template<bool align> void NeuralPow(const float * src, size_t size, const float * exponent, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(dst));

            float e = exponent[0];
            size_t alignedSize = AlignLo(size, F);
            __m256 _e = _mm256_set1_ps(e);
            Pow pow;
            size_t i = 0;
            for (; i < alignedSize; i += F)
                Store<align>(dst + i, pow(Load<align>(src + i), _e));
            for (; i < size; ++i)
                dst[i] = Base::Pow(src[i], e);
        }

        void NeuralPow(const float * src, size_t size, const float * exponent, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                NeuralPow<true>(src, size, exponent, dst);
            else
                NeuralPow<false>(src, size, exponent, dst);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE __m256 Pooling1x1Max3x1Body(const float * src)
        {
            return _mm256_max_ps(_mm256_max_ps(Load<false>(src - 1), Load<align>(src)), Load<false>(src + 1));
        }

        template <bool align> SIMD_INLINE void Pooling1x1Max3x3Body(const float * src, size_t stride, float * dst)
        {
            __m256 src0 = Pooling1x1Max3x1Body<align>(src - stride);
            __m256 src1 = Pooling1x1Max3x1Body<align>(src);
            __m256 src2 = Pooling1x1Max3x1Body<align>(src + stride);
            Store<align>(dst, _mm256_max_ps(_mm256_max_ps(src0, src1), src2));
        }

        template <bool align> SIMD_INLINE void Pooling1x1Max3x2Body(const float * src, size_t stride, float * dst)
        {
            __m256 src0 = Pooling1x1Max3x1Body<align>(src);
            __m256 src1 = Pooling1x1Max3x1Body<align>(src + stride);
            Store<align>(dst, _mm256_max_ps(src0, src1));
        }

        __m256i K32_PERMUTE_NOSE = SIMD_MM256_SETR_EPI32(0, 0, 1, 2, 3, 4, 5, 6);

        template <bool align> SIMD_INLINE __m256 Pooling1x1Max3x1Nose(const float * src)
        {
            __m256 src1 = Load<align>(src);
            __m256 src0 = _mm256_permutevar8x32_ps(src1, K32_PERMUTE_NOSE);
            __m256 src2 = Load<false>(src + 1);
            return _mm256_max_ps(_mm256_max_ps(src0, src1), src2);
        }

        template <bool align> SIMD_INLINE void Pooling1x1Max3x3Nose(const float * src, size_t stride, float * dst)
        {
            __m256 src0 = Pooling1x1Max3x1Nose<align>(src - stride);
            __m256 src1 = Pooling1x1Max3x1Nose<align>(src);
            __m256 src2 = Pooling1x1Max3x1Nose<align>(src + stride);
            Store<align>(dst, _mm256_max_ps(_mm256_max_ps(src0, src1), src2));
        }
        template <bool align> SIMD_INLINE void Pooling1x1Max3x2Nose(const float * src, size_t stride, float * dst)
        {
            __m256 src0 = Pooling1x1Max3x1Nose<align>(src);
            __m256 src1 = Pooling1x1Max3x1Nose<align>(src + stride);
            Store<align>(dst, _mm256_max_ps(src0, src1));
        }

        __m256i K32_PERMUTE_TAIL = SIMD_MM256_SETR_EPI32(1, 2, 3, 4, 5, 6, 7, 7);

        template <bool align> SIMD_INLINE __m256 Pooling1x1Max3x1Tail(const float * src)
        {
            __m256 src0 = Load<false>(src - 1);
            __m256 src1 = Load<align>(src);
            __m256 src2 = _mm256_permutevar8x32_ps(src1, K32_PERMUTE_TAIL);
            return _mm256_max_ps(_mm256_max_ps(src0, src1), src2);
        }

        template <bool align> SIMD_INLINE void Pooling1x1Max3x3Tail(const float * src, size_t stride, float * dst)
        {
            __m256 src0 = Pooling1x1Max3x1Tail<align>(src - stride);
            __m256 src1 = Pooling1x1Max3x1Tail<align>(src);
            __m256 src2 = Pooling1x1Max3x1Tail<align>(src + stride);
            Store<align>(dst, _mm256_max_ps(_mm256_max_ps(src0, src1), src2));
        }
        template <bool align> SIMD_INLINE void Pooling1x1Max3x2Tail(const float * src, size_t stride, float * dst)
        {
            __m256 src0 = Pooling1x1Max3x1Tail<align>(src);
            __m256 src1 = Pooling1x1Max3x1Tail<align>(src + stride);
            Store<align>(dst, _mm256_max_ps(src0, src1));
        }

        template <bool align> void NeuralPooling1x1Max3x3(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride)
        {
            assert(width > F && height > 1);

            size_t alignedWidth = AlignHi(width, F) - F;
            height -= 1;

            Pooling1x1Max3x2Nose<align>(src, srcStride, dst);
            for (size_t col = F; col < alignedWidth; col += F)
                Pooling1x1Max3x2Body<align>(src + col, srcStride, dst + col);
            Pooling1x1Max3x2Tail<false>(src + width - F, srcStride, dst + width - F);

            for (size_t row = 1; row < height; ++row)
            {
                src += srcStride;
                dst += dstStride;
                Pooling1x1Max3x3Nose<align>(src, srcStride, dst);
                for (size_t col = F; col < alignedWidth; col += F)
                    Pooling1x1Max3x3Body<align>(src + col, srcStride, dst + col);
                Pooling1x1Max3x3Tail<false>(src + width - F, srcStride, dst + width - F);
            }

            dst += dstStride;
            Pooling1x1Max3x2Nose<align>(src, srcStride, dst);
            for (size_t col = F; col < alignedWidth; col += F)
                Pooling1x1Max3x2Body<align>(src + col, srcStride, dst + col);
            Pooling1x1Max3x2Tail<false>(src + width - F, srcStride, dst + width - F);
        }

        void NeuralPooling1x1Max3x3(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride)
        {
            if (Aligned(src) && Aligned(srcStride, F) && Aligned(dst) && Aligned(dstStride, F))
                NeuralPooling1x1Max3x3<true>(src, srcStride, width, height, dst, dstStride);
            else
                NeuralPooling1x1Max3x3<false>(src, srcStride, width, height, dst, dstStride);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE __m256 Pooling2x2Max2x2(const float* src, size_t stride)
        {
            __m256 lo = _mm256_max_ps(Load<align>(src + 0), Load<align>(src + stride + 0));
            __m256 hi = _mm256_max_ps(Load<align>(src + F), Load<align>(src + stride + F));
            __m256 _lo = _mm256_permute2f128_ps(lo, hi, 0x20);
            __m256 _hi = _mm256_permute2f128_ps(lo, hi, 0x31);
            return _mm256_max_ps(_mm256_shuffle_ps(_lo, _hi, 0x88), _mm256_shuffle_ps(_lo, _hi, 0xDD));
        }

        template <bool align> SIMD_INLINE __m256 Pooling2x2Max2(const float* src)
        {
            __m256 lo = Load<align>(src + 0);
            __m256 hi = Load<align>(src + F);
            __m256 _lo = _mm256_permute2f128_ps(lo, hi, 0x20);
            __m256 _hi = _mm256_permute2f128_ps(lo, hi, 0x31);
            return _mm256_max_ps(_mm256_shuffle_ps(_lo, _hi, 0x88), _mm256_shuffle_ps(_lo, _hi, 0xDD));
        }

        template <bool align> void NeuralPooling2x2Max2x2(const float* src, size_t srcStride, size_t width, size_t height, float* dst, size_t dstStride)
        {
            size_t heightEven = Simd::AlignLo(height, 2);
            size_t widthEven = Simd::AlignLo(width, 2);
            size_t alignedWidth = AlignLo(width, DF);
            for (size_t row = 0; row < heightEven; row += 2)
            {
                for (size_t col = 0; col < alignedWidth; col += DF)
                    Store<align>(dst + (col >> 1), Pooling2x2Max2x2<align>(src + col, srcStride));
                if (widthEven - alignedWidth)
                {
                    size_t col = widthEven - DF;
                    Store<false>(dst + (col >> 1), Pooling2x2Max2x2<false>(src + col, srcStride));
                }
                if (width - widthEven)
                    dst[widthEven >> 1] = Simd::Max(src[widthEven], src[widthEven + srcStride]);
                src += 2 * srcStride;
                dst += dstStride;
            }
            if (height - heightEven)
            {
                for (size_t col = 0; col < alignedWidth; col += DF)
                    Store<align>(dst + (col >> 1), Pooling2x2Max2<align>(src + col));
                if (widthEven - alignedWidth)
                {
                    size_t col = widthEven - DF;
                    Store<false>(dst + (col >> 1), Pooling2x2Max2<false>(src + col));
                }
                if (width - widthEven)
                    dst[widthEven >> 1] = src[widthEven];
            }
        }

        void NeuralPooling2x2Max2x2(const float* src, size_t srcStride, size_t width, size_t height, float* dst, size_t dstStride)
        {
            if (Aligned(src) && Aligned(srcStride, F) && Aligned(dst) && Aligned(dstStride, F))
                NeuralPooling2x2Max2x2<true>(src, srcStride, width, height, dst, dstStride);
            else
                NeuralPooling2x2Max2x2<false>(src, srcStride, width, height, dst, dstStride);
        }

        //-------------------------------------------------------------------------------------------------

        SIMD_INLINE float Max2(const float * src)
        {
            return Simd::Max(src[0], src[1]);
        }

        SIMD_INLINE float Max2x2(const float * src, size_t stride)
        {
            return Simd::Max(Max2(src), Max2(src + stride));
        }

        SIMD_INLINE float Max2x3(const float * src, size_t stride)
        {
            return Simd::Max(Max2(src), Simd::Max(Max2(src + stride), Max2(src + 2 * stride)));
        }

        template <bool align> SIMD_INLINE __m256 Pooling2x2Max1x3(const float * src, size_t stride)
        {
            return _mm256_max_ps(_mm256_max_ps(Load<align>(src), Load<align>(src + stride)), Load<align>(src + 2 * stride));
        }

        SIMD_INLINE __m256 PermuteFor2x2(__m256 a)
        {
            return _mm256_castsi256_ps(_mm256_permute4x64_epi64(_mm256_castps_si256(a), 0xD8));
        }

        template <bool align> SIMD_INLINE __m256 Pooling2x2Max3x3(const float * src, size_t stride)
        {
            __m256 _01234567 = Pooling2x2Max1x3<align>(src, stride);
            __m256 _89abcdef = Pooling2x2Max1x3<align>(src + F, stride);
            __m256 _456789ab = _mm256_permute2f128_ps(_01234567, _89abcdef, 0x21);
            __m256 _12345678 = Alignr<1>(_01234567, _456789ab);
            __m256 _9abcdefg = Pooling2x2Max1x3<false>(src + F + 1, stride);
            __m256 _028a46ce = _mm256_shuffle_ps(_01234567, _89abcdef, 0x88);
            __m256 _139b57df = _mm256_shuffle_ps(_01234567, _89abcdef, 0xDD);
            __m256 _24ac68eg = _mm256_shuffle_ps(_12345678, _9abcdefg, 0xDD);
            return PermuteFor2x2(_mm256_max_ps(_mm256_max_ps(_028a46ce, _139b57df), _24ac68eg));
        }

        template <bool align> SIMD_INLINE __m256 Pooling2x2Max1x2(const float * src, size_t stride)
        {
            return _mm256_max_ps(Load<align>(src), Load<align>(src + stride));
        }

        template <bool align> SIMD_INLINE __m256 Pooling2x2Max3x2(const float * src, size_t stride)
        {
            __m256 _01234567 = Pooling2x2Max1x2<align>(src, stride);
            __m256 _89abcdef = Pooling2x2Max1x2<align>(src + F, stride);
            __m256 _456789ab = _mm256_permute2f128_ps(_01234567, _89abcdef, 0x21);
            __m256 _12345678 = Alignr<1>(_01234567, _456789ab);
            __m256 _9abcdefg = Pooling2x2Max1x2<false>(src + F + 1, stride);
            __m256 _028a46ce = _mm256_shuffle_ps(_01234567, _89abcdef, 0x88);
            __m256 _139b57df = _mm256_shuffle_ps(_01234567, _89abcdef, 0xDD);
            __m256 _24ac68eg = _mm256_shuffle_ps(_12345678, _9abcdefg, 0xDD);
            return PermuteFor2x2(_mm256_max_ps(_mm256_max_ps(_028a46ce, _139b57df), _24ac68eg));
        }

        template <bool align> void NeuralPooling2x2Max3x3(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride)
        {
            height -= 1;
            width -= 1;
            size_t heightEven = Simd::AlignLo(height, 2);
            size_t widthEven = Simd::AlignLo(width, 2);
            size_t alignedWidth = AlignLo(width, DF);
            for (size_t row = 0; row < heightEven; row += 2)
            {
                for (size_t col = 0; col < alignedWidth; col += DF)
                    Store<align>(dst + (col >> 1), Pooling2x2Max3x3<align>(src + col, srcStride));
                if (widthEven - alignedWidth)
                {
                    size_t col = widthEven - DF;
                    Store<false>(dst + (col >> 1), Pooling2x2Max3x3<false>(src + col, srcStride));
                }
                if (width - widthEven)
                    dst[widthEven >> 1] = Max2x3(src + widthEven, srcStride);
                src += 2 * srcStride;
                dst += dstStride;
            }
            if (height - heightEven)
            {
                for (size_t col = 0; col < alignedWidth; col += DF)
                    Store<align>(dst + (col >> 1), Pooling2x2Max3x2<align>(src + col, srcStride));
                if (widthEven - alignedWidth)
                {
                    size_t col = widthEven - DF;
                    Store<false>(dst + (col >> 1), Pooling2x2Max3x2<false>(src + col, srcStride));
                }
                if (width - widthEven)
                    dst[widthEven >> 1] = Max2x2(src + widthEven, srcStride);
            }
        }

        void NeuralPooling2x2Max3x3(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride)
        {
            if (Aligned(src) && Aligned(srcStride, F) && Aligned(dst) && Aligned(dstStride, F))
                NeuralPooling2x2Max3x3<true>(src, srcStride, width, height, dst, dstStride);
            else
                NeuralPooling2x2Max3x3<false>(src, srcStride, width, height, dst, dstStride);
        }

        //-------------------------------------------------------------------------------------------------

        template <bool align> SIMD_INLINE void UpdateWeights(const float* x, const __m256& a, const __m256& b, float* d, float* w)
        {
            __m256 _d = _mm256_add_ps(_mm256_mul_ps(a, Load<align>(d)), _mm256_mul_ps(b, Load<align>(x)));
            Store<align>(d, _d);
            Store<align>(w, _mm256_add_ps(Load<align>(w), _d));
        }

        template <bool align> SIMD_INLINE void UpdateWeights(const float* x, size_t offset, const __m256& a, const __m256& b, float* d, float* w)
        {
            UpdateWeights<align>(x + offset, a, b, d + offset, w + offset);
        }

        template <bool align> SIMD_INLINE void NeuralUpdateWeights(const float* x, size_t size, const float& a, const float& b, float* d, float* w)
        {
            if (align)
                assert(Aligned(x) && Aligned(d) && Aligned(w));

            size_t partialAlignedSize = AlignLo(size, F);
            size_t fullAlignedSize = AlignLo(size, QF);
            __m256 _a = _mm256_set1_ps(a);
            __m256 _b = _mm256_set1_ps(b);
            size_t i = 0;
            if (partialAlignedSize)
            {
                if (fullAlignedSize)
                {
                    for (; i < fullAlignedSize; i += QF)
                    {
                        UpdateWeights<align>(x, i + F * 0, _a, _b, d, w);
                        UpdateWeights<align>(x, i + F * 1, _a, _b, d, w);
                        UpdateWeights<align>(x, i + F * 2, _a, _b, d, w);
                        UpdateWeights<align>(x, i + F * 3, _a, _b, d, w);
                    }
                }
                for (; i < partialAlignedSize; i += F)
                    UpdateWeights<align>(x, i, _a, _b, d, w);
            }
            for (; i < size; ++i)
                Base::UpdateWeights(x, i, a, b, d, w);
        }

        void NeuralUpdateWeights(const float* x, size_t size, const float* a, const float* b, float* d, float* w)
        {
            if (Aligned(x) && Aligned(d) && Aligned(w))
                NeuralUpdateWeights<true>(x, size, *a, *b, d, w);
            else
                NeuralUpdateWeights<false>(x, size, *a, *b, d, w);
        }
    }
#endif// SIMD_AVX2_ENABLE
}
