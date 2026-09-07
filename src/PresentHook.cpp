#include "PresentHook.h"

#include "Logger.h"
#include "RenderManager.h"

#include <REL/Relocation.h>

namespace PresentHook
{
    namespace
    {
        bool g_installed = false;

        using Present_t =
            HRESULT(WINAPI*)(
                IDXGISwapChain*,
                UINT,
                UINT
            );

        Present_t g_originalPresent = nullptr;

        HRESULT WINAPI PresentThunk(
            IDXGISwapChain* swapChain,
            UINT syncInterval,
            UINT flags)
        {
            return g_originalPresent(
                swapChain,
                syncInterval,
                flags
            );
        }
    }

    bool Install(IDXGISwapChain* swapChain)
    {
        if (g_installed)
            return true;

        if (!swapChain)
        {
            Logger::GetSingleton().Print(
                "PresentHook: swapChain NULL."
            );

            return false;
        }

        auto vtable =
            *reinterpret_cast<std::uintptr_t**>(
                swapChain
            );

        if (!vtable)
        {
            Logger::GetSingleton().Print(
                "PresentHook: vtable NULL."
            );

            return false;
        }

        REL::Relocation<std::uintptr_t> vtableReloc(
            reinterpret_cast<std::uintptr_t>(vtable)
        );

        const auto original =
            vtableReloc.write_vfunc(
                8,
                &PresentThunk
            );

        g_originalPresent =
            reinterpret_cast<Present_t>(
                original
            );

        if (!g_originalPresent)
        {
            Logger::GetSingleton().Print(
                "PresentHook: original Present NULL."
            );

            return false;
        }

        g_installed = true;

        Logger::GetSingleton().Print(
            "PresentHook: INSTALLED."
        );

        return true;
    }

    bool IsInstalled()
    {
        return g_installed;
    }
}