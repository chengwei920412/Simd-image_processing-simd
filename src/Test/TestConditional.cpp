/*
* Simd Library Tests.
*
* Copyright (c) 2011-2014 Yermalayeu Ihar.
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
#include "Test/TestUtils.h"
#include "Test/TestPerformance.h"
#include "Test/TestData.h"
#include "Test/Test.h"

namespace Test
{
    namespace
    {
        struct FuncC
        {
            typedef void (*FuncPtr)(const uint8_t * src, size_t stride, size_t width, size_t height, 
                uint8_t value, SimdCompareType compareType, uint32_t * count);

            FuncPtr func;
            std::string description;

            FuncC(const FuncPtr & f, const std::string & d) : func(f), description(d) {}

            void Call(const View & src, uint8_t value, SimdCompareType compareType, uint32_t & count) const
            {
                TEST_PERFORMANCE_TEST(description);
                func(src.data, src.stride, src.width, src.height, value, compareType, &count);
            }
        };
    }

#define ARGS_C(width, height, type, function1, function2) \
    width, height, type, \
    FuncC(function1.func, function1.description + CompareTypeDescription(type)), \
    FuncC(function2.func, function2.description + CompareTypeDescription(type))

#define FUNC_C(function) \
    FuncC(function, std::string(#function))

    bool ConditionalCountAutoTest(int width, int height, SimdCompareType type, const FuncC & f1, const FuncC & f2)
    {
        bool result = true;

        std::cout << "Test " << f1.description << " & " << f2.description << " [" << width << ", " << height << "]." << std::endl;

        View src(width, height, View::Gray8, NULL, TEST_ALIGN(width));
        FillRandom(src);

        uint8_t value = 127;
        uint32_t c1, c2;

        TEST_EXECUTE_AT_LEAST_MIN_TIME(f1.Call(src, value, type, c1));

        TEST_EXECUTE_AT_LEAST_MIN_TIME(f2.Call(src, value, type, c2));

        TEST_CHECK_VALUE(c);

        return result;
    }

    bool ConditionalCountAutoTest(const FuncC & f1, const FuncC & f2)
    {
        bool result = true;

        for(SimdCompareType type = SimdCompareEqual; type <= SimdCompareLesserOrEqual && result; type = SimdCompareType(type + 1))
        {
            result = result && ConditionalCountAutoTest(ARGS_C(W, H, type, f1, f2));
            result = result && ConditionalCountAutoTest(ARGS_C(W + 1, H - 1, type, f1, f2));
            result = result && ConditionalCountAutoTest(ARGS_C(W - 1, H + 1, type, f1, f2));
        }

        return result;
    }

    bool ConditionalCountAutoTest()
    {
        bool result = true;

        result = result && ConditionalCountAutoTest(FUNC_C(Simd::Base::ConditionalCount), FUNC_C(SimdConditionalCount));

#ifdef SIMD_SSE2_ENABLE
        if(Simd::Sse2::Enable)
            result = result && ConditionalCountAutoTest(FUNC_C(Simd::Sse2::ConditionalCount), FUNC_C(SimdConditionalCount));
#endif 

#ifdef SIMD_AVX2_ENABLE
        if(Simd::Avx2::Enable)
            result = result && ConditionalCountAutoTest(FUNC_C(Simd::Avx2::ConditionalCount), FUNC_C(SimdConditionalCount));
#endif 

#ifdef SIMD_VSX_ENABLE
        if(Simd::Vsx::Enable)
            result = result && ConditionalCountAutoTest(FUNC_C(Simd::Vsx::ConditionalCount), FUNC_C( SimdConditionalCount));
#endif 

        return result;
    }

    namespace 
    {
        struct FuncS
        {
            typedef void (*FuncPtr)(const uint8_t * src, size_t srcStride, size_t width, size_t height, 
                const uint8_t * mask, size_t maskStride, uint8_t value, SimdCompareType compareType, uint64_t * sum);

            FuncPtr func;
            std::string description;

            FuncS(const FuncPtr & f, const std::string & d) : func(f), description(d) {}

            void Call(const View & src, const View & mask, uint8_t value, SimdCompareType compareType, uint64_t & sum) const
            {
                TEST_PERFORMANCE_TEST(description);
                func(src.data, src.stride, src.width, src.height, mask.data, mask.stride, value, compareType, &sum);
            }
        };
    }

#define ARGS_S1(width, height, type, function1, function2) \
    width, height, type, \
    FuncS(function1.func, function1.description + CompareTypeDescription(type)), \
    FuncS(function2.func, function2.description + CompareTypeDescription(type))

#define ARGS_S2(function1, function2) \
    FuncS(function1, std::string(#function1)), FuncS(function2, std::string(#function2))

    bool ConditionalSumAutoTest(int width, int height, SimdCompareType type, const FuncS & f1, const FuncS & f2)
    {
        bool result = true;

        std::cout << "Test " << f1.description << " & " << f2.description << " [" << width << ", " << height << "]." << std::endl;

        View src(width, height, View::Gray8, NULL, TEST_ALIGN(width));
        FillRandom(src);
        View mask(width, height, View::Gray8, NULL, TEST_ALIGN(width));
        FillRandom(mask);

        uint8_t value = 127;
        uint64_t s1, s2;

        TEST_EXECUTE_AT_LEAST_MIN_TIME(f1.Call(src, mask, value, type, s1));

        TEST_EXECUTE_AT_LEAST_MIN_TIME(f2.Call(src, mask, value, type, s2));

        TEST_CHECK_VALUE(s);

        return result;
    }

    bool ConditionalSumAutoTest(const FuncS & f1, const FuncS & f2)
    {
        bool result = true;

        for(SimdCompareType type = SimdCompareEqual; type <= SimdCompareLesserOrEqual && result; type = SimdCompareType(type + 1))
        {
            result = result && ConditionalSumAutoTest(ARGS_S1(W, H, type, f1, f2));
            result = result && ConditionalSumAutoTest(ARGS_S1(W + 1, H - 1, type, f1, f2));
            result = result && ConditionalSumAutoTest(ARGS_S1(W - 1, H + 1, type, f1, f2));
        }

        return result;
    }

    bool ConditionalSumAutoTest()
    {
        bool result = true;

        result = result && ConditionalSumAutoTest(ARGS_S2(Simd::Base::ConditionalSum, SimdConditionalSum));

#if defined(SIMD_SSE2_ENABLE) && defined(SIMD_AVX2_ENABLE)
        if(Simd::Sse2::Enable && Simd::Avx2::Enable)
            result = result && ConditionalSumAutoTest(ARGS_S2(Simd::Avx2::ConditionalSum, Simd::Sse2::ConditionalSum));
#endif 

        return result;
    }

    bool ConditionalSquareSumAutoTest()
    {
        bool result = true;

        result = result && ConditionalSumAutoTest(ARGS_S2(Simd::Base::ConditionalSquareSum, SimdConditionalSquareSum));

#if defined(SIMD_SSE2_ENABLE) && defined(SIMD_AVX2_ENABLE)
        if(Simd::Sse2::Enable && Simd::Avx2::Enable)
            result = result && ConditionalSumAutoTest(ARGS_S2(Simd::Avx2::ConditionalSquareSum, Simd::Sse2::ConditionalSquareSum));
#endif 

        return result;
    }

    bool ConditionalSquareGradientSumAutoTest()
    {
        bool result = true;

        result = result && ConditionalSumAutoTest(ARGS_S2(Simd::Base::ConditionalSquareGradientSum, SimdConditionalSquareGradientSum));

#if defined(SIMD_SSE2_ENABLE) && defined(SIMD_AVX2_ENABLE)
        if(Simd::Sse2::Enable && Simd::Avx2::Enable)
            result = result && ConditionalSumAutoTest(ARGS_S2(Simd::Avx2::ConditionalSquareGradientSum, Simd::Sse2::ConditionalSquareGradientSum));
#endif 

        return result;
    }

    //-----------------------------------------------------------------------

    bool ConditionalCountDataTest(bool create, int width, int height, SimdCompareType type, const FuncC & f)
    {
        bool result = true;

        Data data(f.description);

        std::cout << (create ? "Create" : "Verify") << " test " << f.description << " [" << width << ", " << height << "]." << std::endl;

        View src(width, height, View::Gray8, NULL, TEST_ALIGN(width));
        uint8_t value = 127;
        uint32_t c1, c2;

        if(create)
        {
            FillRandom(src);

            TEST_SAVE(src);

            f.Call(src, value, type, c1);

            TEST_SAVE(c1);
        }
        else
        {
            TEST_LOAD(src);

            TEST_LOAD(c1);

            f.Call(src, value, type, c2);

            TEST_SAVE(c2);

            TEST_CHECK_VALUE(c);
        }

        return result;
    }

    bool ConditionalCountDataTest(bool create)
    {
        bool result = true;

        FuncC f = FUNC_C(SimdConditionalCount);
        for(SimdCompareType type = SimdCompareEqual; type <= SimdCompareLesserOrEqual && result; type = SimdCompareType(type + 1))
        {
            result = result && ConditionalCountDataTest(create, DW, DH, type, FuncC(f.func, f.description + CompareTypeDataDescription(type)));
        }

        return result;
    }
}
