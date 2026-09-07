#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

namespace RenderManager
{
    // ============================================================
    // INITIALIZATION
    // ============================================================

    bool Init();
    void Shutdown();

    bool IsInitialized();

    // ============================================================
    // RENDER
    // ============================================================

    void Render();

    // ============================================================
    // SCREENSHOT
    // ============================================================

    void ProcessScreenshot(IDXGISwapChain* swapChain);

    void CaptureDebugBackbuffer(
        IDXGISwapChain* swapChain,
        const char* name
    );

    bool IsScreenshotRequested();

    // ============================================================
    // D3D
    // ============================================================

    ID3D11Device* GetDevice();

    ID3D11DeviceContext* GetContext();

    IDXGISwapChain* GetSwapChain();

    HWND GetWindow();
}