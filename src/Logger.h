#pragma once

#include "pch.h"

std::filesystem::path GetPicturesFolder();
std::filesystem::path MakeScreenshotPath();
bool SaveTextureToPNG(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* source, const std::filesystem::path& path);
int ParseKey(const std::string& key);
void LoadConfig();
void SaveConfig();
void OpenKeyConfig();
void ToUpperBuffer(char* buffer);

namespace ModIntegrations
{
    // Flag global que indica se o plugin SurvivalMode está ativo no jogo
    inline bool g_isSurvivorPluginLoaded = false;
}

class Logger
{
public:
    static void Initialize();

    // Mantido por compatibilidade com as chamadas do seu código atual
    // ex: Logger::GetSingleton().Print("mensagem {}", arg1);
    static Logger& GetSingleton()
    {
        static Logger instance;
        return instance;
    }

    template <typename... Args>
    void Print(fmt::format_string<Args...> a_fmt, Args&&... a_args)
    {
        spdlog::info(a_fmt, std::forward<Args>(a_args)...);
    }

private:
    Logger() = default;
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};