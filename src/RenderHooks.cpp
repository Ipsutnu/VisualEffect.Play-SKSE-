#include "RenderHooks.h"

#include "Logger.h"
#include "RenderManager.h"
#include "PresentHook.h"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace RenderHooks
{
    namespace
    {
        bool g_installed = false;

        using CreateD3D11_t = void (*)();

        REL::Relocation<CreateD3D11_t> g_originalCreateD3D11;

        struct CreateD3D11Hook
        {
            static void thunk()
            {
                Logger::GetSingleton().Print(
                    "RenderHooks: CreateD3D11 START."
                );

                g_originalCreateD3D11();

                Logger::GetSingleton().Print(
                    "RenderHooks: Skyrim CreateD3D11 completed."
                );

                if (!RenderManager::Init())
                {
                    Logger::GetSingleton().Print(
                        "RenderHooks: RenderManager::Init() FAILED."
                    );

                    return;
                }

                Logger::GetSingleton().Print(
                    "RenderHooks: RenderManager initialized."
                );

            }

        };


        struct RenderUIHook1
        {
            static int64_t thunk(int64_t a_menuManager)
            {
                const auto result = func(a_menuManager);

                if (RenderManager::IsInitialized())
                {
                    RenderManager::Render();

                    if (!PresentHook::IsInstalled())
                    {
                        PresentHook::Install(
                            RenderManager::GetSwapChain()
                        );
                    }
                }

                return result;
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };


        struct RenderUIHook2
        {
            static int64_t thunk(int64_t a_menuManager)
            {
                const auto result = func(a_menuManager);

                if (RenderManager::IsInitialized())
                {
                    RenderManager::Render();
                }

                return result;
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };
    }


    bool Install()
    {
        if (g_installed)
        {
            return true;
        }

        Logger::GetSingleton().Print(
            "RenderHooks: installing..."
        );

        auto& trampoline = SKSE::GetTrampoline();

        SKSE::AllocTrampoline(128);


        // ============================================================
        // D3D INIT
        // ============================================================

        const auto createD3D11 =
            REL::RelocationID(
                75595,
                77226,
                75595
            );

        const auto createD3D11Address =
            createD3D11.address() +
            REL::Relocate(
                0x9,
                0x275,
                0x9
            );

        if (!createD3D11Address)
        {
            Logger::GetSingleton().Print(
                "RenderHooks: CreateD3D11 address is NULL."
            );

            return false;
        }

        g_originalCreateD3D11 =
            trampoline.write_call<5>(
                createD3D11Address,
                CreateD3D11Hook::thunk
            );

        Logger::GetSingleton().Print(
            "RenderHooks: CreateD3D11 hook installed."
        );


        // ============================================================
        // RENDER UI #1
        // ============================================================

        const auto renderUI1 =
            REL::RelocationID(
                35556,
                36555,
                35556
            );

        const auto renderUI1Address =
            renderUI1.address() +
            REL::Relocate(
                0x3AB,
                0x371,
                0x355
            );

        if (!renderUI1Address)
        {
            Logger::GetSingleton().Print(
                "RenderHooks: RenderUI #1 address is NULL."
            );

            return false;
        }

        RenderUIHook1::func =
            trampoline.write_call<5>(
                renderUI1Address,
                RenderUIHook1::thunk
            );

        Logger::GetSingleton().Print(
            "RenderHooks: RenderUI #1 hook installed."
        );


        // ============================================================
        // RENDER UI #2
        // ============================================================

        const auto renderUI2 =
            REL::RelocationID(
                38085,
                39039,
                38085
            );

        std::uintptr_t renderUI2Offset;

        if (REL::Module::IsVR())
        {
            renderUI2Offset = 0x3FC;
        }
        else
        {
            const auto version = REL::Module::get().version();

            const bool isVersion17 =
                version.major() == 1 &&
                version.minor() == 7;

            renderUI2Offset =
                isVersion17
                    ? 0x1A0
                    : 0x19A;
        }

        const auto renderUI2Address =
            renderUI2.address() +
            renderUI2Offset;

        if (!renderUI2Address)
        {
            Logger::GetSingleton().Print(
                "RenderHooks: RenderUI #2 address is NULL."
            );

            return false;
        }

        RenderUIHook2::func =
            trampoline.write_call<5>(
                renderUI2Address,
                RenderUIHook2::thunk
            );

        Logger::GetSingleton().Print(
            "RenderHooks: RenderUI #2 hook installed."
        );


        g_installed = true;

        Logger::GetSingleton().Print(
            "RenderHooks: INSTALLED SUCCESSFULLY."
        );

        return true;
    }
}