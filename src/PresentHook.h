#pragma once

#include <dxgi.h>

namespace PresentHook
{
    bool Install(IDXGISwapChain* swapChain);

    bool IsInstalled();
}