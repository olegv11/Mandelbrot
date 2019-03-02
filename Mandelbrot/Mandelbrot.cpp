#include <windows.h>
#include <objidl.h>
#include <string>
#include <gdiplus.h>
#include "Painter.h"
#include <assert.h>

#define WIDTH 1920
#define HEIGHT 1080

alignas(32) static uint32_t g_BackBuffer[WIDTH * HEIGHT];
BITMAPINFO g_BackBufferInfo;
LARGE_INTEGER g_PerformanceFrequency;

#define AVX 1
#define ITERATIONS 128

#if AVX
static CAvxPainter g_Painter;
#else
static CSimplePainter g_Painter(WIDTH, HEIGHT, ITERATIONS);
#endif

static Gdiplus::Bitmap g_mandelbrotBitmap(WIDTH, HEIGHT, PixelFormat32bppRGB);

#pragma comment (lib,"Gdiplus.lib")


VOID DrawFrame(HDC hdc)
{
    TRect mandelbrotRect;
#if 1
    mandelbrotRect.left = 0.0016435;
    mandelbrotRect.width = 0.0000001;
    mandelbrotRect.up = 0.8224686;
    mandelbrotRect.height = 0.0000001;
#else
    mandelbrotRect.left = -2.5;
    mandelbrotRect.width = 3.5;
    mandelbrotRect.up = 1;
    mandelbrotRect.height = 2;
#endif


    LARGE_INTEGER start, end;
    
    QueryPerformanceCounter(&start);
#if AVX
    int verticalDivisions = 4;
    int horizontalDivisions = 8;
    assert(WIDTH % verticalDivisions == 0);
    assert(HEIGHT % horizontalDivisions == 0);

    for (int i = 0; i < verticalDivisions; i++)
    {
        for (int j = 0; j < horizontalDivisions; j++)
        {
            PainterDrawArea area;
            area.width = WIDTH / verticalDivisions;
            area.height = HEIGHT / horizontalDivisions;
            area.stride = WIDTH;
            area.out = &g_BackBuffer[j * WIDTH * HEIGHT / horizontalDivisions + i * WIDTH / verticalDivisions];

            TRect partRect;
            partRect.width = mandelbrotRect.width / verticalDivisions;
            partRect.height = mandelbrotRect.height / horizontalDivisions;
            partRect.left = mandelbrotRect.left + i * partRect.width;
            partRect.up = mandelbrotRect.up - j * partRect.height;

            g_Painter.DrawMandelbrot(ITERATIONS, area, partRect);
        }
    }
#else
    g_Painter.DrawMandelbrot(mandelbrotRect, g_BackBuffer);
#endif
    QueryPerformanceCounter(&end);

    int drawTime = (int)(1000 * (end.QuadPart - start.QuadPart) / g_PerformanceFrequency.QuadPart);
    StretchDIBits(hdc,
        0, 0, WIDTH, HEIGHT,
        0, 0, WIDTH, HEIGHT,
        g_BackBuffer, &g_BackBufferInfo,
        DIB_RGB_COLORS, SRCCOPY);


    Gdiplus::Graphics    graphics(hdc);
    Gdiplus::SolidBrush  brush(Gdiplus::Color(255, 255, 255, 255));
    Gdiplus::FontFamily  fontFamily(L"Times New Roman");
    Gdiplus::Font        font(&fontFamily, 24, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::PointF      pointF(10.0f, 20.0f);

    std::wstring line = L"Draw time: " + std::to_wstring(drawTime) + L"ms";
    graphics.DrawString(line.c_str(), -1, &font, pointF, &brush);

}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, INT iCmdShow)
{
    HWND                hWnd;
    WNDCLASS            wndClass;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;

    // Initialize GDI+.
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hInstance;
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = TEXT("Mandelbrot");

    RegisterClass(&wndClass);

    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    int width = WIDTH;
    int height = HEIGHT;

    g_BackBufferInfo = { 0 };
    g_BackBufferInfo.bmiHeader.biSize = sizeof(g_BackBufferInfo.bmiHeader);
    g_BackBufferInfo.bmiHeader.biWidth = WIDTH;
    g_BackBufferInfo.bmiHeader.biHeight = HEIGHT;
    g_BackBufferInfo.bmiHeader.biPlanes = 1;
    g_BackBufferInfo.bmiHeader.biBitCount = 32;
    g_BackBufferInfo.bmiHeader.biCompression = BI_RGB;

    RECT desiredRect = { 0, 0, width, height };
    AdjustWindowRect(&desiredRect, dwStyle, false);

    hWnd = CreateWindow(
        TEXT("Mandelbrot"),       // window class name
        TEXT("Mandelbrot"),       // window caption
        dwStyle,                  // window style
        CW_USEDEFAULT,            // initial x position
        CW_USEDEFAULT,            // initial y position
        desiredRect.right - desiredRect.left,        // initial x size
        desiredRect.bottom - desiredRect.top,        // initial y size
        NULL,                     // parent window handle
        NULL,                     // window menu handle
        hInstance,                // program instance handle
        NULL);                    // creation parameters

    ShowWindow(hWnd, iCmdShow);
    UpdateWindow(hWnd);

    QueryPerformanceFrequency(&g_PerformanceFrequency);

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        { 
            HDC hdc = GetDC(hWnd);            
            DrawFrame(hdc);
            ReleaseDC(hWnd, hdc);
        }
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}  // WinMain

LRESULT CALLBACK WndProc(HWND hWnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    HDC          hdc;
    PAINTSTRUCT  ps;

    switch (message)
    {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
} // WndProc