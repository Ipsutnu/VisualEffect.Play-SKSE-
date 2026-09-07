#include "pch.h"
#include "EffectDatabase.h"
#include "Logger.h"

#include <shlobj.h>
#include <filesystem>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

bool ParseBool(const std::string& value)
{
    return
        value == "true" ||
        value == "TRUE" ||
        value == "True" ||
        value == "1" ||
        value == "yes" ||
        value == "YES";
}

std::filesystem::path GetConfigPath()
{
    // Obtém a pasta 'Data/SKSE/Plugins' diretamente via CommonLibSSE
    // Caso a pasta não exista no disco, o filesystem cria a estrutura.
    auto path = SKSE::log::log_directory(); 
    if (path) {
        // SKSE::log::log_directory() geralmente aponta para "My Games/Skyrim Special Edition/SKSE"
        // Para salvar diretamente na pasta do jogo Data/SKSE/Plugins:
        return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "VisualEffectPlay.ini";
    }

    // Fallback relativo seguro
    return std::filesystem::path("Data") / "SKSE" / "Plugins" / "VisualEffectPlay.ini";
}

void LoadConfig()
{
    const auto path = GetConfigPath();

    std::ifstream file(path);

    if (!file.is_open())
    {
        return;
    }

    std::string section;
    std::string line;

    EffectDatabase::favorites.clear();

    while (std::getline(file, line))
    {
        // Remove espaços no início/fim
        const auto first =
            line.find_first_not_of(" \t");

        if (first == std::string::npos)
        {
            continue;
        }

        const auto last =
            line.find_last_not_of(" \t");

        line = line.substr(
            first,
            last - first + 1);

        // Comentários
        if (line[0] == ';' ||
            line[0] == '#')
        {
            continue;
        }

        // Seção
        if (line.front() == '[' &&
            line.back() == ']')
        {
            section =
                line.substr(
                    1,
                    line.size() - 2);

            continue;
        }

        //if (section != "General")
        //{
        //    continue;
        //}

        const auto equal =
            line.find('=');

        if (equal == std::string::npos)
        {
            continue;
        }

        std::string key =
            line.substr(0, equal);

        std::string value =
            line.substr(equal + 1);

        // Remove espaços
        const auto keyFirst =
            key.find_first_not_of(" \t");

        const auto keyLast =
            key.find_last_not_of(" \t");

        if (keyFirst != std::string::npos)
        {
            key = key.substr(
                keyFirst,
                keyLast - keyFirst + 1);
        }

        const auto valueFirst =
            value.find_first_not_of(" \t");

        const auto valueLast =
            value.find_last_not_of(" \t");

        if (valueFirst != std::string::npos)
        {
            value = value.substr(
                valueFirst,
                valueLast - valueFirst + 1);
        }

        if (section == "General")
        {
            if (key == "ToggleKey")
            {
                if (!value.empty())
                {
                    g_toggleKey = ParseKey(value);

                    Logger::GetSingleton().Print(
                        "ToggleKey: '{}' -> {}",
                        value,
                        g_toggleKey);
                }
            }
            else if (key == "MovementToggleKey")
            {
                if (!value.empty())
                {
                    g_movementToggleKey = ParseKey(value);

                    Logger::GetSingleton().Print(
                        "MovementToggleKey: '{}' -> {}",
                        value,
                        g_movementToggleKey);
                }
            }
        }
        else if (section == "Internal")
        {
            const bool state = ParseBool(value);

            if (key == "Spells")
            {
                EffectDatabase::showSpells = state;
            }
            else if (key == "VisualEffect")
            {
                EffectDatabase::showVisualEffects = state;
            }
            else if (key == "EffectShader")
            {
                EffectDatabase::showEffectShaders = state;
            }
            else if (key == "Explosion")
            {
                EffectDatabase::showExplosions = state;
            }
            else if (key == "ImageSpace")
            {
                EffectDatabase::showImagespaceModifiers = state;
            }
            else if (key == "FormID")
            {
                EffectControl::copyUseEditorID = !state;
                if (EffectControl::copyUseEditorID)
                {
                    EffectControl::copiedType =
                        EffectControl::CopiedType::EditorID;
                }
                else if (!EffectControl::copyUseEditorID)
                {
                    EffectControl::copiedType =
                        EffectControl::CopiedType::FormID;
                }
            }
            else if (key == "MovementKeyState")
            {
                EffectControl::g_disablePlayerMovement = !state;
                    
            }
            else if (key == "FontScale")
            {
                try {
                    float scale = std::stof(value);
                    
                    // Garante que o valor lido esteja em um limite seguro (ex: entre 0.5 e 2.5)
                    if (scale >= 0.5f && scale <= 2.5f) {
                        ImGui::GetIO().FontGlobalScale = scale;
                    }
                }
                catch (...) {
                    Logger::GetSingleton().Print("Erro ao ler FontScale do INI. Usando padrao (1.0).");
                    ImGui::GetIO().FontGlobalScale = 1.0f;
                }
            }
            
        }
        else if (section == "Favorites")
        {
            // Procura os dois delimitadores '|' na string "FormID|Type|Name"
            const size_t pipe1 = value.find('|');
            
            if (pipe1 != std::string::npos)
            {
                const size_t pipe2 = value.find('|', pipe1 + 1);
                
                if (pipe2 != std::string::npos)
                {
                    std::string formIDStr = value.substr(0, pipe1);
                    std::string typeStr = value.substr(pipe1 + 1, pipe2 - pipe1 - 1);
                    std::string nameStr = value.substr(pipe2 + 1);

                    try
                    {
                        // Converte a string Hex para uint32_t (FormID)
                        RE::FormID parsedID = std::stoul(formIDStr, nullptr, 16);
                        
                        // Converte o tipo para int e faz cast pro seu enum
                        int parsedType = std::stoi(typeStr);

                        // Recria o objeto e adiciona na lista
                        EffectDatabase::favorites.push_back({
                            parsedID,
                            static_cast<EffectType>(parsedType), // Ajuste pro seu Enum
                            nameStr
                        });
                    }
                    catch (const std::exception&)
                    {
                        // Se falhar no stoul/stoi (corrompido), ignora essa linha
                        continue; 
                    }
                }
            }
        }
        
    }
}

void SaveConfig()
{
    const auto path = GetConfigPath();

    std::ofstream file(path);

    if (!file.is_open())
    {
        Logger::GetSingleton().Print(
            "Failed to save VisualEffectPlay.ini"
        );

        return;
    }

    // ============================================================
    // GENERAL
    // ============================================================

    file << "[General]\n";
    file << "ToggleKey="
         << g_openMenuKeyBuffer
         << "\n";

    file << "MovementToggleKey="
         << g_movementKeyBuffer
         << "\n";

    file << "\n";

    // ============================================================
    // INTERNAL
    // ============================================================

    file << "[Internal]\n";

    file << "Spells="
         << (EffectDatabase::showSpells ? "true" : "false")
         << "\n";

    file << "VisualEffect="
         << (EffectDatabase::showVisualEffects ? "true" : "false")
         << "\n";

    file << "EffectShader="
         << (EffectDatabase::showEffectShaders ? "true" : "false")
         << "\n";

    file << "Explosion="
         << (EffectDatabase::showExplosions ? "true" : "false")
         << "\n";

    file << "ImageSpace="
         << (EffectDatabase::showImagespaceModifiers ? "true" : "false")
         << "\n";

    file << "FormID="
         << (!EffectControl::copyUseEditorID
             ? "true"
             : "false")
         << "\n";

    file << "MovementKeyState="
         << (!EffectControl::g_disablePlayerMovement
             ? "true"
             : "false")
         << "\n";

    file << "FontScale=" << ImGui::GetIO().FontGlobalScale << "\n";

    // ============================================================
    // FAVORITES
    // ============================================================
    
    file << "\n[Favorites]\n";
    
    for (std::size_t i = 0; i < EffectDatabase::favorites.size(); ++i)
    {
        char buffer[512];
        
        // Serializa como "00012F56|1|Nome do Efeito"
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%08X|%d|%s",
            EffectDatabase::favorites[i].formID,
            static_cast<int>(EffectDatabase::favorites[i].type), // Substitua pelo nome do seu Enum de tipo se necessário
            EffectDatabase::favorites[i].name.c_str()
        );

        file << "Item" << i << "=" << buffer << "\n";
    }

    file.close();

    Logger::GetSingleton().Print(
        "Config saved: ToggleKey={} MovementToggleKey={} "
        "Spells={} VisualEffect={} EffectShader={} "
        "Explosion={} ImageSpace={} FormID={}",
        g_openMenuKeyBuffer,
        g_movementKeyBuffer,
        EffectDatabase::showSpells,
        EffectDatabase::showVisualEffects,
        EffectDatabase::showEffectShaders,
        EffectDatabase::showExplosions,
        EffectDatabase::showImagespaceModifiers,
        !EffectControl::copyUseEditorID
    );
}

std::string KeyToString(int key)
{
    // F1 - F24
    if (key >= VK_F1 && key <= VK_F24)
    {
        return "F" + std::to_string(
            key - VK_F1 + 1);
    }

    // Letras A-Z
    if (key >= 'A' && key <= 'Z')
    {
        return std::string(
            1,
            static_cast<char>(key));
    }

    // Números 0-9
    if (key >= '0' && key <= '9')
    {
        return std::string(
            1,
            static_cast<char>(key));
    }

    // Teclas especiais
    if (key == VK_INSERT)
        return "INSERT";

    if (key == VK_DELETE)
        return "DELETE";

    if (key == VK_HOME)
        return "HOME";

    if (key == VK_END)
        return "END";

    if (key == VK_PRIOR)
        return "PAGEUP";

    if (key == VK_NEXT)
        return "PAGEDOWN";

    if (key == VK_UP)
        return "UP";

    if (key == VK_DOWN)
        return "DOWN";

    if (key == VK_LEFT)
        return "LEFT";

    if (key == VK_RIGHT)
        return "RIGHT";

    if (key == VK_SPACE)
        return "SPACE";

    if (key == VK_TAB)
        return "TAB";

    if (key == VK_RETURN)
        return "ENTER";

    if (key == VK_ESCAPE)
        return "ESC";

    return "UNKNOWN";
}

void ToUpperBuffer(char* buffer)
{
    if (!buffer)
        return;

    for (char* p = buffer; *p; ++p)
    {
        *p = static_cast<char>(
            std::toupper(
                static_cast<unsigned char>(*p)
            )
        );
    }
}

void OpenKeyConfig()
{
    std::strncpy(
        g_openMenuKeyBuffer,
        KeyToString(g_toggleKey).c_str(),
        sizeof(g_openMenuKeyBuffer) - 1
    );

    g_openMenuKeyBuffer[
        sizeof(g_openMenuKeyBuffer) - 1
    ] = '\0';

    std::strncpy(
        g_movementKeyBuffer,
        KeyToString(g_movementToggleKey).c_str(),
        sizeof(g_movementKeyBuffer) - 1
    );

    g_movementKeyBuffer[
        sizeof(g_movementKeyBuffer) - 1
    ] = '\0';

    g_showKeyConfig = true;
}

int ParseKey(const std::string& key)
{
    std::string k = key;

    // Uppercase
    for (char& c : k)
    {
        c = static_cast<char>(
            std::toupper(
                static_cast<unsigned char>(c)));
    }

    // Letras A-Z
    if (k.size() == 1 &&
        k[0] >= 'A' &&
        k[0] <= 'Z')
    {
        return k[0];
    }

    // Números 0-9
    if (k.size() == 1 &&
        k[0] >= '0' &&
        k[0] <= '9')
    {
        return k[0];
    }

    // F1 - F24
    if (k[0] == 'F' &&
        k.size() <= 3)
    {
        try
        {
            const int number =
                std::stoi(k.substr(1));

            if (number >= 1 &&
                number <= 24)
            {
                return VK_F1 + (number - 1);
            }
        }
        catch (...)
        {
        }
    }

    // Teclas especiais
    if (k == "INSERT")
        return VK_INSERT;

    if (k == "DELETE" || k == "DEL")
        return VK_DELETE;

    if (k == "HOME")
        return VK_HOME;

    if (k == "END")
        return VK_END;

    if (k == "PAGEUP")
        return VK_PRIOR;

    if (k == "PAGEDOWN")
        return VK_NEXT;

    if (k == "UP")
        return VK_UP;

    if (k == "DOWN")
        return VK_DOWN;

    if (k == "LEFT")
        return VK_LEFT;

    if (k == "RIGHT")
        return VK_RIGHT;

    if (k == "SPACE")
        return VK_SPACE;

    if (k == "TAB")
        return VK_TAB;

    if (k == "ENTER")
        return VK_RETURN;

    if (k == "ESC" || k == "ESCAPE")
        return VK_ESCAPE;

    // Se inválida, volta para G
    return VK_F3;
}

std::filesystem::path GetPicturesFolder()
{
    PWSTR path = nullptr;

    // KF_FLAG_CREATE garante que o Windows crie o diretório 'Pictures' se ele não existir
    HRESULT hr = SHGetKnownFolderPath(
        FOLDERID_Pictures,
        KF_FLAG_CREATE,
        nullptr,
        &path);

    if (FAILED(hr) || !path)
    {
        return {};
    }

    std::filesystem::path result(path);

    CoTaskMemFree(path);

    return result;
}

std::filesystem::path MakeScreenshotPath()
{
    auto pictures = GetPicturesFolder();

    if (pictures.empty())
    {
        return {};
    }

    std::filesystem::path folder = pictures / "Skyrim Screenshots";

    std::error_code ec;
    std::filesystem::create_directories(folder, ec);

    if (ec)
    {
        return {};
    }

    // Tempo atual com precisão de milissegundos
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
    localtime_s(&localTime, &time);

    std::ostringstream filename;
    filename
        << "Skyrim_"
        << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S")
        << "_" << std::setfill('0') << std::setw(3) << ms.count() // Adiciona milissegundos
        << ".png";

    return folder / filename.str();
}

bool SaveTextureToPNG(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* source,
    const std::filesystem::path& path)
{
    if (!device || !context || !source)
        return false;

    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);

    Logger::GetSingleton().Print(
        "Screenshot: SaveTexture format={}",
        static_cast<int>(desc.Format));

    // ============================================================
    // STAGING
    // ============================================================

    D3D11_TEXTURE2D_DESC stagingDesc = desc;

    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    ID3D11Texture2D* staging = nullptr;

    HRESULT hr = device->CreateTexture2D(
        &stagingDesc,
        nullptr,
        &staging);

    if (FAILED(hr) || !staging)
    {
        Logger::GetSingleton().Print(
            "Screenshot: CreateTexture2D staging FAILED hr=0x{:08X}",
            static_cast<unsigned>(hr));

        return false;
    }

    // ============================================================
    // GPU -> CPU
    // ============================================================

    context->CopyResource(staging, source);

    D3D11_MAPPED_SUBRESOURCE mapped{};

    hr = context->Map(
        staging,
        0,
        D3D11_MAP_READ,
        0,
        &mapped);

    if (FAILED(hr))
    {
        Logger::GetSingleton().Print(
            "Screenshot: Map FAILED hr=0x{:08X}",
            static_cast<unsigned>(hr));

        staging->Release();
        return false;
    }

    Logger::GetSingleton().Print(
        "Screenshot: RowPitch={}",
        mapped.RowPitch);

    // ============================================================
    // CONVERTE PARA BGRA8
    // ============================================================

    const UINT width = desc.Width;
    const UINT height = desc.Height;

    const UINT rowBytes = width * 4;

    std::vector<BYTE> pixels(
        static_cast<size_t>(rowBytes) * height);

    const BYTE* src =
        static_cast<const BYTE*>(mapped.pData);

    for (UINT y = 0; y < height; ++y)
    {
        const BYTE* srcRow =
            src + static_cast<size_t>(y) * mapped.RowPitch;

        BYTE* dstRow =
            pixels.data() + static_cast<size_t>(y) * rowBytes;

        for (UINT x = 0; x < width; ++x)
        {
            BYTE r = 0;
            BYTE g = 0;
            BYTE b = 0;
            BYTE a = 255;

            // ====================================================
            // R10G10B10A2
            // ====================================================

            if (desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
            {
                const uint32_t packed =
                    reinterpret_cast<const uint32_t*>(srcRow)[x];

                const uint32_t r10 =
                    packed & 0x3FF;

                const uint32_t g10 =
                    (packed >> 10) & 0x3FF;

                const uint32_t b10 =
                    (packed >> 20) & 0x3FF;

                const uint32_t a2 =
                    (packed >> 30) & 0x3;

                r = static_cast<BYTE>(
                    (r10 * 255 + 511) / 1023);

                g = static_cast<BYTE>(
                    (g10 * 255 + 511) / 1023);

                b = static_cast<BYTE>(
                    (b10 * 255 + 511) / 1023);

                a = static_cast<BYTE>(
                    (a2 * 255 + 1) / 3);
            }

            // ====================================================
            // R8G8B8A8
            // ====================================================

            else if (
                desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
                desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
            {
                const BYTE* p =
                    srcRow + x * 4;

                r = p[0];
                g = p[1];
                b = p[2];
                a = p[3];
            }

            // ====================================================
            // B8G8R8A8
            // ====================================================

            else if (
                desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
            {
                const BYTE* p =
                    srcRow + x * 4;

                b = p[0];
                g = p[1];
                r = p[2];
                a = p[3];
            }

            else
            {
                context->Unmap(staging, 0);
                staging->Release();

                Logger::GetSingleton().Print(
                    "Screenshot: UNSUPPORTED DXGI format {}",
                    static_cast<int>(desc.Format));

                return false;
            }

            // WIC = BGRA
            const size_t offset =
                static_cast<size_t>(x) * 4;

            dstRow[offset + 0] = b;
            dstRow[offset + 1] = g;
            dstRow[offset + 2] = r;
            dstRow[offset + 3] = a;
        }
    }

    // ============================================================
    // WIC
    // ============================================================

    IWICImagingFactory* factory = nullptr;

    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));

    if (FAILED(hr) || !factory)
    {
        Logger::GetSingleton().Print(
            "Screenshot: WIC factory FAILED.");

        context->Unmap(staging, 0);
        staging->Release();

        return false;
    }

    IWICBitmapEncoder* encoder = nullptr;

    hr = factory->CreateEncoder(
        GUID_ContainerFormatPng,
        nullptr,
        &encoder);

    if (FAILED(hr) || !encoder)
    {
        factory->Release();

        context->Unmap(staging, 0);
        staging->Release();

        return false;
    }

    IWICStream* stream = nullptr;

    hr = factory->CreateStream(&stream);

    if (FAILED(hr) || !stream)
    {
        encoder->Release();
        factory->Release();

        context->Unmap(staging, 0);
        staging->Release();

        return false;
    }

    const std::wstring widePath =
        path.wstring();

    hr = stream->InitializeFromFilename(
        widePath.c_str(),
        GENERIC_WRITE);

    if (FAILED(hr))
    {
        stream->Release();
        encoder->Release();
        factory->Release();

        context->Unmap(staging, 0);
        staging->Release();

        return false;
    }

    hr = encoder->Initialize(
        stream,
        WICBitmapEncoderNoCache);

    if (FAILED(hr))
    {
        stream->Release();
        encoder->Release();
        factory->Release();

        context->Unmap(staging, 0);
        staging->Release();

        return false;
    }

    IWICBitmapFrameEncode* frame = nullptr;

    hr = encoder->CreateNewFrame(
        &frame,
        nullptr);

    if (FAILED(hr) || !frame)
    {
        stream->Release();
        encoder->Release();
        factory->Release();

        context->Unmap(staging, 0);
        staging->Release();

        return false;
    }

    hr = frame->Initialize(nullptr);

    if (SUCCEEDED(hr))
    {
        hr = frame->SetSize(
            width,
            height);
    }

    WICPixelFormatGUID format =
        GUID_WICPixelFormat32bppBGRA;

    if (SUCCEEDED(hr))
    {
        hr = frame->SetPixelFormat(&format);
    }

    if (SUCCEEDED(hr))
    {
        hr = frame->WritePixels(
            height,
            rowBytes,
            static_cast<UINT>(pixels.size()),
            pixels.data());
    }

    if (SUCCEEDED(hr))
        hr = frame->Commit();

    if (SUCCEEDED(hr))
        hr = encoder->Commit();

    // ============================================================
    // CLEANUP
    // ============================================================

    frame->Release();
    stream->Release();
    encoder->Release();
    factory->Release();

    context->Unmap(staging, 0);
    staging->Release();

    Logger::GetSingleton().Print(
        "Screenshot: WIC result hr=0x{:08X}",
        static_cast<unsigned>(hr));

    return SUCCEEDED(hr);
}

static std::filesystem::path GetLogDirectory()
{
    wchar_t* buffer = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &buffer))) {
        std::filesystem::path path(buffer);
        CoTaskMemFree(buffer);

        path /= "My Games";
        path /= "Skyrim Special Edition";
        path /= "SKSE";
        return path;
    }
    return {};
}

void Logger::Initialize()
{
    auto directory = GetLogDirectory();
    if (directory.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        return;
    }

    auto logPath = directory / "VisualEffectPlay.log";

    // Reescreve o log a cada nova inicialização do jogo
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
    auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

    auto logger = std::make_shared<spdlog::logger>(
        "global", 
        spdlog::sinks_init_list{ fileSink, msvcSink }
    );

    // Registra como padrão (NÃO chame spdlog::register_logger aqui!)
    spdlog::set_default_logger(logger);

    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::info); // Força flush automático para INFO, WARN e ERROR

    // Garante que o spdlog descarregue o buffer para o disco a cada 1 segundo (sem travar as threads do jogo)
    spdlog::flush_every(std::chrono::seconds(1));

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    spdlog::info("========================================");
    spdlog::info("VisualEffect.Play(SKSE) - START");
    spdlog::info("========================================");
}