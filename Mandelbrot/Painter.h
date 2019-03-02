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
    static void DrawMandelbrot(int pixelWidth, int pixelHeight, int maxIterations, TRect mandelbrotRect, uint32_t *out);
};