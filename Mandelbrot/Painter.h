#pragma once
#include <stdint.h>

struct TRect
{
    double left;
    double up;
    double width;
    double height;
};

struct PainterDrawArea
{
    uint32_t *out;
    int width;
    int height;
    int stride;
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
    static void DrawMandelbrot(int maxIterations, PainterDrawArea drawArea, TRect mandelbrotRect);
};