/*
 * Copyright (c) 2026 Preguissoso
 *
 * This file is part of VisualEffect.Play(SKSE).
 *
 * The source code is available for viewing and reference purposes only.
 * Modification, redistribution, forking, and creation of derivative
 * works are not permitted without prior written permission.
 *
 * See LICENSE for the full license terms.
 */

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
    static Logger& GetSingleton();

    void Initialize();

    template <typename... Args>
    void Print(const std::string& fmt, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_initialized || !_file.is_open()) {
            return;
        }

        try {
            std::vformat_to(
                std::ostreambuf_iterator<char>(_file),
                fmt,
                std::make_format_args(args...)
            );

            _file << '\n';
            _file.flush();
        }
        catch (const std::exception& e) {
            _file << "[LOGGER ERROR] " << e.what() << '\n';
            _file.flush();
        }
    }

private:
    Logger() = default;

    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream _file;
    std::mutex _mutex;
    bool _initialized = false;
};