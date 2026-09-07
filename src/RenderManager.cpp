#include "RenderManager.h"

#include "EffectDatabase.h"
//#include "PresentHook.h"
#include "Logger.h"

#include <RE/Skyrim.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <vector>
#include <cstdint>
#include <algorithm>

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

namespace RenderManager
{
    namespace
    {
        ID3D11Device* g_device = nullptr;

        ID3D11DeviceContext* g_context = nullptr;

        IDXGISwapChain* g_swapChain = nullptr;

        HWND g_gameWindow = nullptr;

        bool g_initialized = false;

        float g_globalAlpha = 0.0f;

        constexpr float g_fadeSpeed = 5.0f;

        //newrender
        std::vector<std::uint8_t> g_uiPixels;
        UINT g_uiWidth = 0;
        UINT g_uiHeight = 0;
        bool g_uiCaptured = false;
    }


    bool CopyTextureToBGRA8(
        ID3D11Texture2D* source,
        std::vector<std::uint8_t>& output,
        UINT& width,
        UINT& height)
    {
        if (!source || !g_device || !g_context)
            return false;

        D3D11_TEXTURE2D_DESC desc{};
        source->GetDesc(&desc);

        width = desc.Width;
        height = desc.Height;

        if (desc.SampleDesc.Count != 1)
        {
            Logger::GetSingleton().Print(
                "Screenshot: MSAA texture not supported."
            );

            return false;
        }

        D3D11_TEXTURE2D_DESC stagingDesc{};

        stagingDesc.Width = desc.Width;
        stagingDesc.Height = desc.Height;
        stagingDesc.MipLevels = 1;
        stagingDesc.ArraySize = 1;
        stagingDesc.Format = desc.Format;
        stagingDesc.SampleDesc.Count = 1;
        stagingDesc.SampleDesc.Quality = 0;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MiscFlags = 0;

        ID3D11Texture2D* staging = nullptr;

        HRESULT hr =
            g_device->CreateTexture2D(
                &stagingDesc,
                nullptr,
                &staging
            );

        if (FAILED(hr) || !staging)
        {
            Logger::GetSingleton().Print(
                "Screenshot: CreateTexture2D staging FAILED. hr=0x{:08X}",
                static_cast<unsigned>(hr)
            );

            return false;
        }

        g_context->CopyResource(
            staging,
            source
        );

        D3D11_MAPPED_SUBRESOURCE mapped{};

        hr =
            g_context->Map(
                staging,
                0,
                D3D11_MAP_READ,
                0,
                &mapped
            );

        if (FAILED(hr))
        {
            Logger::GetSingleton().Print(
                "Screenshot: Map FAILED. hr=0x{:08X}",
                static_cast<unsigned>(hr)
            );

            staging->Release();
            return false;
        }

        output.resize(
            static_cast<size_t>(width) *
            static_cast<size_t>(height) *
            4
        );

        for (UINT y = 0; y < height; ++y)
        {
            const auto* src =
                static_cast<const std::uint8_t*>(
                    mapped.pData
                ) +
                static_cast<size_t>(y) *
                mapped.RowPitch;

            auto* dst =
                output.data() +
                static_cast<size_t>(y) *
                width *
                4;

            for (UINT x = 0; x < width; ++x)
            {
                std::uint8_t r = 0;
                std::uint8_t g = 0;
                std::uint8_t b = 0;
                std::uint8_t a = 255;

                switch (desc.Format)
                {
                case DXGI_FORMAT_R10G10B10A2_UNORM:
                {
                    const auto pixel =
                        reinterpret_cast<const std::uint32_t*>(src)[x];

                    const std::uint32_t rr =
                        (pixel >> 0) & 0x3FF;

                    const std::uint32_t gg =
                        (pixel >> 10) & 0x3FF;

                    const std::uint32_t bb =
                        (pixel >> 20) & 0x3FF;

                    r = static_cast<std::uint8_t>(
                        (rr * 255) / 1023
                    );

                    g = static_cast<std::uint8_t>(
                        (gg * 255) / 1023
                    );

                    b = static_cast<std::uint8_t>(
                        (bb * 255) / 1023
                    );

                    // O jogo é considerado opaco.
                    a = 255;

                    break;
                }

                case DXGI_FORMAT_R8G8B8A8_UNORM:
                case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                {
                    const auto* pixel =
                        src + x * 4;

                    r = pixel[0];
                    g = pixel[1];
                    b = pixel[2];
                    a = pixel[3];

                    break;
                }

                case DXGI_FORMAT_B8G8R8A8_UNORM:
                case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                {
                    const auto* pixel =
                        src + x * 4;

                    b = pixel[0];
                    g = pixel[1];
                    r = pixel[2];
                    a = pixel[3];

                    break;
                }

                default:
                    g_context->Unmap(staging, 0);

                    staging->Release();

                    Logger::GetSingleton().Print(
                        "Screenshot: Unsupported format {}",
                        static_cast<int>(desc.Format)
                    );

                    return false;
                }

                // Saída sempre BGRA
                dst[x * 4 + 0] = b;
                dst[x * 4 + 1] = g;
                dst[x * 4 + 2] = r;
                dst[x * 4 + 3] = a;
            }
        }

        g_context->Unmap(
            staging,
            0
        );

        staging->Release();

        return true;
    }

    bool Init()
    {
        if (g_initialized)
        {
            return true;
        }


        /*
         * ============================================================
         * SKYRIM RENDERER
         * ============================================================
         */

        auto* renderer =
            RE::BSGraphics::Renderer::GetSingleton();

        if (!renderer)
        {
            Logger::GetSingleton().Print(
                "RenderManager: Renderer == nullptr."
            );

            return false;
        }


        auto& runtimeData =
            renderer->GetRuntimeData();


        /*
         * ============================================================
         * DEVICE
         * ============================================================
         */

        g_device =
            reinterpret_cast<ID3D11Device*>(
                runtimeData.forwarder
            );


        /*
         * ============================================================
         * CONTEXT
         * ============================================================
         */

        g_context =
            reinterpret_cast<ID3D11DeviceContext*>(
                runtimeData.context
            );


        /*
         * ============================================================
         * SWAPCHAIN
         * ============================================================
         */

        g_swapChain =
            reinterpret_cast<IDXGISwapChain*>(
                runtimeData.renderWindows[0].swapChain
            );


        if (!g_device)
        {
            Logger::GetSingleton().Print(
                "RenderManager: Device == nullptr."
            );

            return false;
        }

        if (!g_context)
        {
            Logger::GetSingleton().Print(
                "RenderManager: Context == nullptr."
            );

            return false;
        }

        if (!g_swapChain)
        {
            Logger::GetSingleton().Print(
                "RenderManager: SwapChain == nullptr."
            );

            return false;
        }


        /*
         * ============================================================
         * WINDOW
         * ============================================================
         */

        DXGI_SWAP_CHAIN_DESC desc{};

        if (FAILED(
            g_swapChain->GetDesc(&desc)))
        {
            Logger::GetSingleton().Print(
                "RenderManager: SwapChain::GetDesc() FAILED."
            );

            return false;
        }

        g_gameWindow =
            desc.OutputWindow;


        if (!g_gameWindow)
        {
            Logger::GetSingleton().Print(
                "RenderManager: Game HWND == nullptr."
            );

            return false;
        }

        
        /*
         * ============================================================
         * BACKBUFFER
         * ============================================================
         */

        //ID3D11Texture2D* backBuffer = nullptr;

        //const HRESULT bufferResult =
        //    g_swapChain->GetBuffer(
        //        0,
        //        IID_PPV_ARGS(&backBuffer)
        //    );


        //if (FAILED(bufferResult) ||
        //    !backBuffer)
        //{
        //    Logger::GetSingleton().Print(
        //        "RenderManager: GetBuffer() FAILED."
        //    );

        //    return false;
        //}


        /*
         * ============================================================
         * RENDER TARGET
         * ============================================================
         */

        //const HRESULT targetResult =
        //    g_device->CreateRenderTargetView(
        //        backBuffer,
        //        nullptr,
        //        &g_renderTarget
        //    );


        //backBuffer->Release();


        //if (FAILED(targetResult) ||
        //    !g_renderTarget)
        //{
        //    Logger::GetSingleton().Print(
        //        "RenderManager: CreateRenderTargetView() FAILED."
        //    );

        //    return false;
        //}


        /*
         * ============================================================
         * IMGUI
         * ============================================================
         */

        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        ImGuiIO& io =
            ImGui::GetIO();


        io.ConfigFlags |=
            ImGuiConfigFlags_NavEnableKeyboard;

        io.ConfigFlags |=
            ImGuiConfigFlags_NavEnableGamepad;

        io.ConfigFlags |=
            ImGuiConfigFlags_NoMouseCursorChange;

        io.KeyRepeatDelay = 0.30f;

        io.KeyRepeatRate = 0.06f;


        /*
         * ============================================================
         * SEU TEMA
         * ============================================================
         */

        EffectDatabase::LoadDefaultTheme();

        EffectDatabase::LoadThemes();

        EffectDatabase::LoadCurrentTheme();


        /*
         * ============================================================
         * WIN32 BACKEND
         * ============================================================
         *
         * NÃO instalamos WndProc hook aqui.
         *
         * OAR também não usa ImGui_ImplWin32_WndProcHandler
         * no WndProc para alimentar a interface.
         */

        if (!ImGui_ImplWin32_Init(g_gameWindow))
        {
            Logger::GetSingleton().Print(
                "RenderManager: ImGui_ImplWin32_Init() FAILED."
            );



            ImGui::DestroyContext();

            return false;
        }


        /*
         * ============================================================
         * DX11 BACKEND
         * ============================================================
         */

        if (!ImGui_ImplDX11_Init(
            g_device,
            g_context))
        {
            Logger::GetSingleton().Print(
                "RenderManager: ImGui_ImplDX11_Init() FAILED."
            );

            ImGui_ImplWin32_Shutdown();
      
            ImGui::DestroyContext();

            return false;
        }


        g_initialized = true;


        Logger::GetSingleton().Print(
            "RenderManager: INITIALIZED."
        );

        Logger::GetSingleton().Print(
            "RenderManager: HWND = {:X}",
            reinterpret_cast<std::uintptr_t>(
                g_gameWindow
            )
        );

        Logger::GetSingleton().Print(
            "RenderManager: Device = {:X}",
            reinterpret_cast<std::uintptr_t>(
                g_device
            )
        );

        Logger::GetSingleton().Print(
            "RenderManager: Context = {:X}",
            reinterpret_cast<std::uintptr_t>(
                g_context
            )
        );

        Logger::GetSingleton().Print(
            "RenderManager: SwapChain = {:X}",
            reinterpret_cast<std::uintptr_t>(
                g_swapChain
            )
        );


        return true;
    }


    void Render()
    {
        if (!g_initialized)
        {
            return;
        }

        if (!g_device || !g_context || !g_swapChain) {
			Logger::GetSingleton().Print("Render Error: D3D11 resources null! device: {}, context: {}, swapChain: {}", 
				(void*)g_device, (void*)g_context, (void*)g_swapChain);
			return;
		}

        if (!ImGui::GetCurrentContext())
		{
			static bool logged = false;

			if (!logged)
			{
				Logger::GetSingleton().Print(
					"RenderManager: ERROR - ImGui context is NULL!"
				);

				logged = true;
			}

			return;
		}
		//-------------------------------------------------
		// bora colocar mais debugs
		//----------------------------------------------------
		static bool lastShowState = false;

		if (lastShowState != g_showTestWindow)
		{
			Logger::GetSingleton().Print(
				"RenderManager: g_showTestWindow = {}",
				g_showTestWindow ? "TRUE" : "FALSE"
			);

			lastShowState = g_showTestWindow;
		}
			
        /*
         * ============================================================
         * NEW FRAME
         * ============================================================
         */

        ImGui_ImplDX11_NewFrame();

        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();

		static bool lastShowState2 = false;

		if (lastShowState2 != g_showTestWindow)
		{
			Logger::GetSingleton().Print(
				"RenderManager: NewFrame OK, showWindow={}",
				g_showTestWindow ? "TRUE" : "FALSE"
			);

			lastShowState2 = g_showTestWindow;
		}

        /*
         * ============================================================
         * MOUSE
         * ============================================================
         */

        if (g_showTestWindow &&
            g_gameWindow)
        {
            POINT point{};

            if (GetCursorPos(&point))
            {
                if (ScreenToClient(
                    g_gameWindow,
                    &point))
                {
                    ImGui::GetIO().AddMousePosEvent(
                        static_cast<float>(point.x),
                        static_cast<float>(point.y)
                    );
                }
            }
        }


        ImGui::GetIO().MouseDrawCursor =
            g_showTestWindow;


        /*
         * ============================================================
         * DELTA TIME
         * ============================================================
         */

        ImGuiIO& io =
            ImGui::GetIO();

        const float deltaTime =
            io.DeltaTime > 0.0f
                ? io.DeltaTime
                : 1.0f / 60.0f;


        /*
         * ============================================================
         * FADE
         * ============================================================
         */

        if (g_showTestWindow)
        {
            g_globalAlpha +=
                deltaTime * g_fadeSpeed;

            if (g_globalAlpha > 1.0f)
            {
                g_globalAlpha = 1.0f;
            }
        }
        else
        {
            g_globalAlpha -=
                deltaTime * g_fadeSpeed;

            if (g_globalAlpha < 0.0f)
            {
                g_globalAlpha = 0.0f;
            }
        }


        /*
         * ============================================================
         * DRAW
         * ============================================================
         */

        if (g_globalAlpha > 0.0f)
        {
            ImGui::PushStyleVar(
                ImGuiStyleVar_Alpha,
                g_globalAlpha
            );

            EffectDatabase::DrawMenu();

            ImGui::PopStyleVar();
        }


        /*
         * ============================================================
         * RENDER
         * ============================================================
         */

        ImGui::Render();


        ImGui_ImplDX11_RenderDrawData(
            ImGui::GetDrawData()
        );

        //Gente do céu funçao da pqp caraio de directx
        //ProcessScreenshot();
        if (g_takeScreenshot)
        {
            ProcessScreenshot(g_swapChain);
        }
        
    }

    void ProcessScreenshot(IDXGISwapChain* swapChain)
    {
        if (!g_initialized)
            return;

        if (!g_takeScreenshot)
            return;

        if (!g_device || !g_context || !swapChain)
            return;

        // Consumimos o pedido agora.
        g_takeScreenshot = false;

        // ============================================================
        // 1. CAPTURA O IMGUI
        // ============================================================

        ID3D11RenderTargetView* currentRTV = nullptr;

        g_context->OMGetRenderTargets(
            1,
            &currentRTV,
            nullptr
        );

        if (!currentRTV)
        {
            Logger::GetSingleton().Print(
                "Screenshot: No current RenderTargetView."
            );

            g_screenshotNotification =
                "Error: no UI render target.";

            g_screenshotNotificationTime = 3.0f;

            return;
        }

        ID3D11Resource* uiResource = nullptr;

        currentRTV->GetResource(
            &uiResource
        );

        ID3D11Texture2D* uiTexture = nullptr;

        HRESULT hr =
            uiResource->QueryInterface(
                IID_PPV_ARGS(&uiTexture)
            );

        uiResource->Release();
        currentRTV->Release();

        if (FAILED(hr) || !uiTexture)
        {
            Logger::GetSingleton().Print(
                "Screenshot: UI texture QueryInterface FAILED."
            );

            g_screenshotNotification =
                "Error: no UI texture.";

            g_screenshotNotificationTime = 3.0f;

            return;
        }

        UINT uiWidth = 0;
        UINT uiHeight = 0;

        std::vector<std::uint8_t> uiPixels;

        const bool uiCaptured =
            CopyTextureToBGRA8(
                uiTexture,
                uiPixels,
                uiWidth,
                uiHeight
            );

        uiTexture->Release();

        if (!uiCaptured)
        {
            Logger::GetSingleton().Print(
                "Screenshot: UI capture FAILED."
            );

            g_screenshotNotification =
                "Error: UI capture failed.";

            g_screenshotNotificationTime = 3.0f;

            return;
        }

        Logger::GetSingleton().Print(
            "Screenshot: UI captured {}x{}.",
            uiWidth,
            uiHeight
        );

        // ============================================================
        // 2. PEGA O BACKBUFFER DO JOGO
        // ============================================================

        ID3D11Texture2D* gameTexture = nullptr;

        hr =
            swapChain->GetBuffer(
                0,
                IID_PPV_ARGS(&gameTexture)
            );

        if (FAILED(hr) || !gameTexture)
        {
            Logger::GetSingleton().Print(
                "Screenshot: GetBuffer(0) FAILED. hr=0x{:08X}",
                static_cast<unsigned>(hr)
            );

            g_screenshotNotification =
                "Error: no backbuffer.";

            g_screenshotNotificationTime = 3.0f;

            return;
        }

        D3D11_TEXTURE2D_DESC gameDesc{};
        gameTexture->GetDesc(&gameDesc);

        Logger::GetSingleton().Print(
            "SCREENSHOT GAME: {}x{} Format={} Samples={} Bind={} Misc={}",
            gameDesc.Width,
            gameDesc.Height,
            static_cast<int>(gameDesc.Format),
            gameDesc.SampleDesc.Count,
            gameDesc.BindFlags,
            gameDesc.MiscFlags
        );

        UINT gameWidth = 0;
        UINT gameHeight = 0;

        std::vector<std::uint8_t> gamePixels;

        const bool gameCaptured =
            CopyTextureToBGRA8(
                gameTexture,
                gamePixels,
                gameWidth,
                gameHeight
            );

        gameTexture->Release();

        if (!gameCaptured)
        {
            Logger::GetSingleton().Print(
                "Screenshot: GAME capture FAILED."
            );

            g_screenshotNotification =
                "Error: game capture failed.";

            g_screenshotNotificationTime = 3.0f;

            return;
        }

        Logger::GetSingleton().Print(
            "Screenshot: GAME captured {}x{}.",
            gameWidth,
            gameHeight
        );

        // ============================================================
        // 3. VERIFICA TAMANHO
        // ============================================================

        if (gameWidth != uiWidth ||
            gameHeight != uiHeight)
        {
            Logger::GetSingleton().Print(
                "Screenshot: SIZE MISMATCH Game={}x{} UI={}x{}",
                gameWidth,
                gameHeight,
                uiWidth,
                uiHeight
            );

            g_screenshotNotification =
                "Error: size mismatch.";

            g_screenshotNotificationTime = 3.0f;

            return;
        }

        // ============================================================
        // 4. FUNDE GAME + IMGUI
        // ============================================================

        std::vector<std::uint8_t> finalPixels;

        finalPixels.resize(
            static_cast<size_t>(gameWidth) *
            static_cast<size_t>(gameHeight) *
            4
        );

        const size_t pixelCount =
            static_cast<size_t>(gameWidth) *
            static_cast<size_t>(gameHeight);

        for (size_t i = 0; i < pixelCount; ++i)
        {
            const size_t p = i * 4;

            const std::uint32_t uiAlpha =
                uiPixels[p + 3];

            const std::uint32_t inverseAlpha =
                255 - uiAlpha;

            // ========================================================
            // O BACKBUFFER DO IMGUI É PREMULTIPLIED
            //
            // Portanto:
            //
            // final = UI + Game * (1 - alpha)
            //
            // e NÃO:
            //
            // final = UI * alpha + Game * (1 - alpha)
            // ========================================================

            finalPixels[p + 0] =
                static_cast<std::uint8_t>(
                    std::min(
                        255u,
                        static_cast<std::uint32_t>(
                            uiPixels[p + 0]
                        ) +
                        (
                            static_cast<std::uint32_t>(
                                gamePixels[p + 0]
                            ) *
                            inverseAlpha
                        ) / 255
                    )
                );

            finalPixels[p + 1] =
                static_cast<std::uint8_t>(
                    std::min(
                        255u,
                        static_cast<std::uint32_t>(
                            uiPixels[p + 1]
                        ) +
                        (
                            static_cast<std::uint32_t>(
                                gamePixels[p + 1]
                            ) *
                            inverseAlpha
                        ) / 255
                    )
                );

            finalPixels[p + 2] =
                static_cast<std::uint8_t>(
                    std::min(
                        255u,
                        static_cast<std::uint32_t>(
                            uiPixels[p + 2]
                        ) +
                        (
                            static_cast<std::uint32_t>(
                                gamePixels[p + 2]
                            ) *
                            inverseAlpha
                        ) / 255
                    )
                );

            finalPixels[p + 3] = 255;
        }

        // ============================================================
        // 5. CRIA TEXTURA FINAL BGRA8
        // ============================================================

        D3D11_TEXTURE2D_DESC finalDesc{};

        finalDesc.Width = gameWidth;
        finalDesc.Height = gameHeight;
        finalDesc.MipLevels = 1;
        finalDesc.ArraySize = 1;
        finalDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        finalDesc.SampleDesc.Count = 1;
        finalDesc.SampleDesc.Quality = 0;
        finalDesc.Usage = D3D11_USAGE_DEFAULT;
        finalDesc.BindFlags = 0;
        finalDesc.CPUAccessFlags = 0;
        finalDesc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA finalData{};

        finalData.pSysMem =
            finalPixels.data();

        finalData.SysMemPitch =
            gameWidth * 4;

        ID3D11Texture2D* finalTexture = nullptr;

        hr =
            g_device->CreateTexture2D(
                &finalDesc,
                &finalData,
                &finalTexture
            );

        if (FAILED(hr) || !finalTexture)
        {
            Logger::GetSingleton().Print(
                "Screenshot: Create final texture FAILED. hr=0x{:08X}",
                static_cast<unsigned>(hr)
            );

            g_screenshotNotification =
                "Error: final texture.";

            g_screenshotNotificationTime = 3.0f;

            return;
        }

        // ============================================================
        // 6. PATH
        // ============================================================

        const auto path =
            MakeScreenshotPath();

        if (path.empty())
        {
            Logger::GetSingleton().Print(
                "Screenshot: MakeScreenshotPath() returned EMPTY."
            );

            g_screenshotNotification =
                "Error: no folder.";

            g_screenshotNotificationTime = 3.0f;

            finalTexture->Release();

            return;
        }

        // ============================================================
        // 7. SALVA
        // ============================================================

        Logger::GetSingleton().Print(
            "Screenshot: Saving COMBINED image to {}",
            path.string()
        );

        const bool saved =
            SaveTextureToPNG(
                g_device,
                g_context,
                finalTexture,
                path
            );

        finalTexture->Release();

        // ============================================================
        // 8. RESULTADO
        // ============================================================

        if (saved)
        {
            SYSTEMTIME time{};
            GetLocalTime(&time);

            char dateTime[64]{};

            sprintf_s(
                dateTime,
                "%02d/%02d/%04d %02d:%02d:%02d",
                time.wDay,
                time.wMonth,
                time.wYear,
                time.wHour,
                time.wMinute,
                time.wSecond
            );

            g_screenshotNotification =
                std::string("Saved!\nDate: ") +
                dateTime +
                "\nLocal: " +
                path.string();

            g_screenshotNotificationTime = 5.0f;

            Logger::GetSingleton().Print(
                "Screenshot: COMBINED SaveTextureToPNG SUCCESS."
            );
        }
        else
        {
            g_screenshotNotification =
                "Error";

            g_screenshotNotificationTime = 3.0f;

            Logger::GetSingleton().Print(
                "Screenshot: COMBINED SaveTextureToPNG FAILED."
            );
        }
    }

    void Shutdown()
    {
        if (!g_initialized)
        {
            return;
        }


        if (ImGui::GetCurrentContext())
        {
            ImGui_ImplDX11_Shutdown();

            ImGui_ImplWin32_Shutdown();

            ImGui::DestroyContext();
        }


        g_device = nullptr;

        g_context = nullptr;

        g_swapChain = nullptr;

        g_gameWindow = nullptr;

        g_initialized = false;

        g_globalAlpha = 0.0f;


        Logger::GetSingleton().Print(
            "RenderManager: shutdown."
        );
    }


    bool IsInitialized()
    {
        return g_initialized;
    }


    ID3D11Device* GetDevice()
    {
        return g_device;
    }


    ID3D11DeviceContext* GetContext()
    {
        return g_context;
    }


    IDXGISwapChain* GetSwapChain()
    {
        return g_swapChain;
    }


    HWND GetWindow()
    {
        return g_gameWindow;
    }


    void RequestScreenshot()
    {
        g_takeScreenshot = true;
    }
}