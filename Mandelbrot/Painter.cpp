#include "Painter.h"
#include <stdlib.h>
#include <assert.h>
#include <xmmintrin.h>
#include <immintrin.h>

#define MAKERGB(r,g,b) ((r) << 16 | (g) << 8 | (b) << 0)

struct TColor
{
    union
    {
        struct
        {
            uint32_t B : 8;
            uint32_t G : 8;
            uint32_t R : 8;
            uint32_t A : 8;
        };
        uint32_t Raw;
    };
};

template <typename T>
T lerp(T a, T b, double t)
{
    return (T)(a + (b - a) * t);
}

__forceinline __m128 lerpAvx(__m128 a, __m128 b, __m128 t)
{
    return _mm_add_ps(a, _mm_mul_ps(_mm_sub_ps(b, a), t));
}


CSimplePainter::CSimplePainter(int pixelWidth, int pixelHeight, int maxIterations)
    : pixelWidth(pixelWidth), pixelHeight(pixelHeight), maxIterations(maxIterations)
{
    colorLUT = (uint32_t *)malloc(6 * sizeof(uint32_t));
    CreateLUT();
}


CSimplePainter::~CSimplePainter()
{
    free(colorLUT);
}

TColor LerpColor(TColor a, TColor b, double t)
{
    TColor result = { 0 };
    result.A = 255;
    result.R = lerp<uint8_t>(a.R, b.R, t);
    result.G = lerp<uint8_t>(a.G, b.G, t);
    result.B = lerp<uint8_t>(a.B, b.B, t);
    return result;
}

void CSimplePainter::CreateLUT()
{
    colorLUT[0] = MAKERGB(0, 0, 255);
    colorLUT[1] = MAKERGB(0, 255, 0);
    colorLUT[2] = MAKERGB(255, 255, 0);
    colorLUT[3] = MAKERGB(255, 0, 0);
    colorLUT[4] = MAKERGB(0, 0, 0);
    // If we've reached the maximum amount of iterations, we'll interpolate between indices 4 and 5,
    // so let's just add a dummy element [5]. We'll interpolate between black and black.
    colorLUT[5] = MAKERGB(0, 0, 0);
}

void CSimplePainter::DrawMandelbrot(TRect mandelbrotRect, uint32_t *out)
{
    double left = mandelbrotRect.left;
    double right = left + mandelbrotRect.width;
    double up = mandelbrotRect.up;
    double down = up - mandelbrotRect.height;

    int32_t quarter = maxIterations / 4;

    for (int32_t j = 0; j < pixelHeight; j++)
    {
        double y0 = up - ((double)j / pixelHeight) * mandelbrotRect.height;
        for (int32_t i = 0; i < pixelWidth; i++)
        {
            double x0 = left + ((double)i / pixelWidth) * mandelbrotRect.width;

            int32_t iteration = 0;
            double x = 0;
            double y = 0;
            while (x * x + y * y <= 4 && iteration < maxIterations)
            {
                double temp = x * x - y * y + x0;
                y = 2 * x * y + y0;
                x = temp;
                iteration++;
            }
            
            int32_t lutIndex = iteration / quarter;
            float t = ((float)iteration / quarter) - lutIndex;
            TColor from, to;
            from.Raw = colorLUT[lutIndex];
            to.Raw = colorLUT[lutIndex + 1];

            out[j * pixelWidth + i] = LerpColor(from, to, t).Raw;
        }
    }
}

CAvxPainter::CAvxPainter(int pixelWidth, int pixelHeight, int maxIterations)
    : pixelWidth(pixelWidth), pixelHeight(pixelHeight), maxIterations(maxIterations)
{
    assert(pixelWidth % 4 == 0);
}


CAvxPainter::~CAvxPainter()
{
}

bool AnyLaneNonZero(__m256i x)
{
    return x.m256i_i64[0] || x.m256i_i64[1] || x.m256i_i64[2] || x.m256i_i64[3];
}

__m256d int64_to_double(__m256i x) {
    x = _mm256_add_epi64(x, _mm256_castpd_si256(_mm256_set1_pd(0x0018000000000000)));
    return _mm256_sub_pd(_mm256_castsi256_pd(x), _mm256_set1_pd(0x0018000000000000));
}

void CAvxPainter::DrawMandelbrot(TRect mandelbrotRect, uint32_t *out)
{
    assert((intptr_t)out % 16 == 0);
    __m256d zero = _mm256_set1_pd(0.0);
    __m128 zero128 = _mm_set1_ps(0);
    __m256i zeroi256 = _mm256_set1_epi32(0);
    __m256d two = _mm256_set1_pd(2.0);
    __m256d four = _mm256_set1_pd(4.0);
    __m128i fouri = _mm_set1_epi32(4);
    __m128i onei = _mm_set1_epi32(1);
    __m128i twofivefive = _mm_set1_epi32(255);

    __m256d maxIter = _mm256_set1_pd((double)maxIterations);
    __m256d quarter = _mm256_set1_pd((double)maxIterations / 4.0);
    __m256i maxIteri = _mm256_set1_epi32(maxIterations);
    
    __m256i increments = _mm256_setr_epi32(1, 0, 1, 0, 1, 0, 1, 0);

    double left = mandelbrotRect.left;
    double right = left + mandelbrotRect.width;
    double up = mandelbrotRect.up;
    double down = up - mandelbrotRect.height;

    __m256d yStep = _mm256_set1_pd(mandelbrotRect.height / pixelHeight);
    __m256d xStep = _mm256_set1_pd(4 * mandelbrotRect.width / pixelWidth);
    __m256d y0 = _mm256_set1_pd(up);
    __m256d xAtLineStart = _mm256_setr_pd(left, 
        left + mandelbrotRect.width / pixelWidth,
        left + 2 * mandelbrotRect.width / pixelWidth,
        left + 3 * mandelbrotRect.width / pixelWidth);

    for (int32_t j = 0; j < pixelHeight; j++)
    {
        uint32_t *pixel = &out[j * pixelWidth];
        __m256d x0 = xAtLineStart;

        for (int32_t i = 0; i < pixelWidth; i += 4)
        {
            int32_t iteration = 0;
            __m256i iterations = zeroi256;
            __m256d x = zero;
            __m256d y = zero;

            __m256d squareX = _mm256_mul_pd(x, x);
            __m256d squareY = _mm256_mul_pd(y, y);
            __m256d square = _mm256_add_pd(squareX, squareY);
            __m256d leqFour = _mm256_cmp_pd(square, four, _CMP_LE_OQ);
            __m256i leqFourI = _mm256_castpd_si256(leqFour);

            while (_mm256_movemask_pd(leqFour) && iteration < maxIterations)
            {
                __m256d temp = _mm256_add_pd(x0, _mm256_sub_pd(squareX, squareY));
                __m256d xy = _mm256_mul_pd(x, y);
                y = _mm256_add_pd(y0, _mm256_add_pd(xy, xy));
                x = temp;
                iterations = _mm256_add_epi32(iterations, _mm256_and_si256(leqFourI, increments));

                squareX = _mm256_mul_pd(x, x);
                squareY = _mm256_mul_pd(y, y);
                square = _mm256_add_pd(squareX, squareY);
                leqFour = _mm256_cmp_pd(square, four, _CMP_LE_OQ);
                leqFourI = _mm256_castpd_si256(leqFour);

                iteration++;
            }

            __m256d iterationsD = int64_to_double(iterations);            
            
            __m256d rat = _mm256_div_pd(iterationsD, quarter);
            __m256d floored = _mm256_floor_pd(rat);

            __m256d td = _mm256_sub_pd(rat, floored);
            __m128 t = _mm256_cvtpd_ps(td);
            __m128i LUTindices = _mm256_cvtpd_epi32(floored);
            
            // Indices four and five are both "black", so let's consider them the same.
            LUTindices = _mm_min_epi32(LUTindices, fouri);
            __m128i nextLUTIndices = _mm_add_epi32(LUTindices, onei);

            __m128 red0 = zero128, red1 = zero128, green0 = zero128, green1 = zero128, blue0 = zero128, blue1 = zero128;

            const float RedLUT[5] = {0, 0, 255, 255, 0};
            const float GreenLUT[5] = { 0, 255, 255, 0, 0 };
            const float BlueLUT[5] = { 255, 0, 0, 0, 0 };

            for (int i = 0; i < 5; i++)
            {
                __m128 currentRed = _mm_set1_ps(RedLUT[i]);
                __m128 currentGreen = _mm_set1_ps(GreenLUT[i]);
                __m128 currentBlue = _mm_set1_ps(BlueLUT[i]);

                __m128i currentIndex = _mm_set1_epi32(i);
                __m128 currentIndexMatch = _mm_castsi128_ps(_mm_cmpeq_epi32(LUTindices, currentIndex));
                __m128 nextIndexMatch = _mm_castsi128_ps(_mm_cmpeq_epi32(nextLUTIndices, currentIndex));

                red0 = _mm_add_ps(red0, _mm_and_ps(currentIndexMatch, currentRed));
                red1 = _mm_add_ps(red1, _mm_and_ps(nextIndexMatch, currentRed));

                green0 = _mm_add_ps(green0, _mm_and_ps(currentIndexMatch, currentGreen));
                green1 = _mm_add_ps(green1, _mm_and_ps(nextIndexMatch, currentGreen));

                blue0 = _mm_add_ps(blue0, _mm_and_ps(currentIndexMatch, currentBlue));
                blue1 = _mm_add_ps(blue1, _mm_and_ps(nextIndexMatch, currentBlue));
            }            

            __m128 red = lerpAvx(red0, red1, t);
            __m128 green = lerpAvx(green0, green1, t);
            __m128 blue = lerpAvx(blue0, blue1, t);

            __m128i redi = _mm_cvtps_epi32(red);
            __m128i greeni = _mm_cvtps_epi32(green);
            __m128i bluei = _mm_cvtps_epi32(blue);

            __m128i rpix = _mm_slli_epi32(redi, 16);
            __m128i gpix = _mm_slli_epi32(greeni, 8);
            __m128i bpix = bluei;
            __m128i apix = _mm_slli_epi32(twofivefive, 24);

            __m128i result = _mm_or_si128(_mm_or_si128(rpix, gpix), _mm_or_si128(bpix, apix));
            
            _mm_store_si128((__m128i*)pixel, result);
            pixel += 4;

            x0 = _mm256_add_pd(x0, xStep);
            
        }
        y0 = _mm256_sub_pd(y0, yStep);
    }
}