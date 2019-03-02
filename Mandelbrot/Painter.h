#pragma once
#include <stdint.h>

struct TRect
{
    double left;
    double up;
    double width;
    double height;
};

class CSimplePainter
{
public:
    CSimplePainter(int pixelWidth, int pixelHeight, int maxIterations);
    ~CSimplePainter();
    void DrawMandelbrot(TRect mandelbrotRect, uint32_t *out);

private:
    void CreateLUT();

    int pixelWidth; 
    int pixelHeight;
    int maxIterations;
    uint32_t *colorLUT;
};

class CAvxPainter
{
public:
    CAvxPainter(int pixelWidth, int pixelHeight, int maxIterations);
    ~CAvxPainter();
    void DrawMandelbrot(TRect mandelbrotRect, uint32_t *out);

private:
    int pixelWidth;
    int pixelHeight;
    int maxIterations;

    // The number of colours in the table, excluding the last one (black)
    // There can be at most 15, since we need a dummy black after the last.
    const int colorsInLUT = 4;
};