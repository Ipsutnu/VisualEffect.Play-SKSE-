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

#include "PCH.h"
#include "Logger.h"

#include "EffectDatabase.h"
#include "EffectControl.h"

inline bool g_postLoadGame = false;

bool g_showTestWindow = false;
bool g_imguiInitialized = false;
HWND g_gameWindow = nullptr;
bool g_takeScreenshot = false;

static float globalAlpha = 0.0f; // Opacidade (0.0f = invisível, 1.0f = visível)
float fadeSpeed = 5.0f;          // Velocidade do Fade (aumente para ficar mais rápido)

//int g_toggleKey = 'G';
int g_toggleKey = VK_F3;
int g_movementToggleKey = VK_F4; // F4

char g_openMenuKeyBuffer[32] = "F3";
char g_movementKeyBuffer[32] = "F4";

static bool g_UpArrowDown = false;
static bool g_DownArrowDown = false;

std::string g_themeToDelete;
bool g_showDeleteThemePopup = false;

ImVec2 g_keyConfigPopupPos;

bool g_showKeyConfig = false;

std::string g_screenshotNotification;

float g_screenshotNotificationTime = 0.0f;

int g_tabFocusIndex = 0;
bool g_requestTabFocus = false;

WNDPROC g_originalWndProc = nullptr;

std::unordered_map<std::string, ImGuiStyle> g_themes;

std::string g_currentThemeName = "Default";

bool g_showCreateThemePopup = false;
char g_newThemeName[64] = "";

bool g_themeDirty = false;

bool g_mouseEnabled = false;

static const EffectDatabase::EffectEntry* lastSelectedEffect = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class InputHandler : public RE::BSTEventSink<RE::InputEvent*>
{
public:
    static InputHandler* GetSingleton()
    {
        static InputHandler instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>*) override
    {
        if (!g_showTestWindow)
            return RE::BSEventNotifyControl::kContinue;
        
        if (!a_event || !*a_event)
            return RE::BSEventNotifyControl::kContinue;

        auto* ui = RE::UI::GetSingleton();

        // Se o Skyrim está em MenuMode, cancela bloqueio de input
        if (ui && ui->IsItemMenuOpen())
        {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* userEvents = RE::UserEvents::GetSingleton();

        // ================================================================
        // PLAYER / MOVEMENT LOCK
        // ================================================================

        auto* player = RE::PlayerCharacter::GetSingleton();

        const bool blockMovement =
            EffectControl::g_disablePlayerMovement &&
            player &&
            player->Is3DLoaded();

        for (auto* event = *a_event; event; event = event->next)
        {
            // ============================================================
            // 1. MOUSE MOVE
            // ============================================================

            if (EffectControl::g_disablePlayerMovement &&
                event->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove)
            {
                auto* mouseMove = static_cast<RE::MouseMoveEvent*>(event);

                mouseMove->mouseInputX = 0;
                mouseMove->mouseInputY = 0;

                continue;
            }

            // ============================================================
            // 2. BUTTON EVENT
            // ============================================================

            auto* buttonEvent = event->AsButtonEvent();
            if (!buttonEvent)
                continue;

            // ============================================================
            // 2A. ATIVAÇÃO / E
            // ============================================================

            if (buttonEvent->QUserEvent() == userEvents->activate)
            {
                if (EffectControl::g_disablePlayerActivate)
                {
                    if (buttonEvent->IsDown())
                    {
                        // EffectDatabase::ApplySelectedEffect();
                    }

                    buttonEvent->GetRuntimeData().value = 0.0f;
                    buttonEvent->GetRuntimeData().heldDownSecs = 0.0f;
                    buttonEvent->SetUserEvent("");
                    buttonEvent->SetIDCode(0xFF);
                }
            }

            // ============================================================
            // 2B. TAB / ENTER / ESC
            // ============================================================

            if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                g_showTestWindow)
            {
                const uint32_t keyId = buttonEvent->GetIDCode();

                // TAB / ENTER
                if (keyId == 0x0F ||
                    keyId == 0x1C ||
                    keyId == 0x9C)
                {
                    buttonEvent->GetRuntimeData().value = 0.0f;
                    buttonEvent->GetRuntimeData().heldDownSecs = 0.0f;
                    buttonEvent->SetUserEvent("");
                    buttonEvent->SetIDCode(0xFF);
                }

                // ESC
                if (keyId == 0x01)
                {
                    if (buttonEvent->IsDown())
                    {
                        g_showTestWindow = false;
                        SaveConfig();
                        EffectDatabase::OnCloseImGui();
                    }

                    buttonEvent->GetRuntimeData().value = 0.0f;
                    buttonEvent->GetRuntimeData().heldDownSecs = 0.0f;
                    buttonEvent->SetUserEvent("");
                    buttonEvent->SetIDCode(0xFF);
                }
            }

            // ============================================================
            // 2C. SCREENSHOT
            // ============================================================

            if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                buttonEvent->GetIDCode() == 0xB7)
            {
                if (buttonEvent->IsDown())
                {
                    g_takeScreenshot = true;
                }

                buttonEvent->GetRuntimeData().value = 0.0f;
                buttonEvent->GetRuntimeData().heldDownSecs = 0.0f;
                buttonEvent->SetUserEvent("");
                buttonEvent->SetIDCode(0xFF);
            }

            // ============================================================
            // 2D. MOVEMENT TOGGLE
            // ============================================================

            static bool movementKeyWasDown = false;

            if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kKeyboard)
            {
                const bool movementKeyDown =
                    (GetAsyncKeyState(g_movementToggleKey) & 0x8000) != 0;

                if (movementKeyDown && !movementKeyWasDown)
                {
                    EffectDatabase::TogglePlayerMovement();

                    // Consome a tecla de toggle
                    buttonEvent->GetRuntimeData().value = 0.0f;
                    buttonEvent->GetRuntimeData().heldDownSecs = 0.0f;
                    buttonEvent->SetUserEvent("");
                    buttonEvent->SetIDCode(0xFF);
                }

                movementKeyWasDown = movementKeyDown;
            }

            // ============================================================
            // 3. BLOQUEIO DE MOVIMENTO
            // ============================================================

            if (!blockMovement)
                continue;

            bool shouldBlock = false;

            // ------------------------------------------------------------
            // TECLADO
            // ------------------------------------------------------------

            if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kKeyboard)
            {
                const auto& userEvent = buttonEvent->QUserEvent();

                if (userEvent == "Forward" ||
                    userEvent == "Back" ||
                    userEvent == "Left" ||
                    userEvent == "Right" ||
                    userEvent == "Strafe Left" ||
                    userEvent == "Strafe Right" ||
                    userEvent == "Jump" ||
                    userEvent == "Sprint" ||
                    userEvent == "Sneak")
                {
                    shouldBlock = true;
                }
            }

            // ------------------------------------------------------------
            // MOUSE
            // ------------------------------------------------------------

            else if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kMouse)
            {
                const uint32_t mouseButton = buttonEvent->GetIDCode();

                // 0 = esquerdo
                // 1 = direito
                // 8 e 9 scrolls

                if (mouseButton == 0 || mouseButton == 1 || mouseButton == 8 || mouseButton == 9)
                {
                    shouldBlock = true;
                }
            }

            // ------------------------------------------------------------
            // CONSUME EVENT
            // ------------------------------------------------------------

            if (shouldBlock)
            {
                buttonEvent->GetRuntimeData().value = 0.0f;
                buttonEvent->GetRuntimeData().heldDownSecs = 0.0f;
                buttonEvent->SetUserEvent("");
                buttonEvent->SetIDCode(0xFF);
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

void RegisterInputSink()
{
    auto* deviceManager = RE::BSInputDeviceManager::GetSingleton();
    if (!deviceManager)
        return;

    // Registra o nosso sink
    deviceManager->AddEventSink(InputHandler::GetSingleton());

    // Reordena os sinks para colocar o NOSSO no topo (índice 0)
    // Isso garante que tratamos a tecla 'E' ANTES do PlayerControls do Skyrim
    auto& sinks = deviceManager->sinks;

    for (RE::BSTArray<RE::BSTEventSink<RE::InputEvent*>*>::size_type i = 0;
        i < sinks.size();
        ++i)
    {
        if (sinks[i] == InputHandler::GetSingleton())
        {
            std::swap(sinks[i], sinks[0]);
            break;
        }
    }
}

std::filesystem::path GetThemeDirectory()
{
    return std::filesystem::path("Data")
        / "SKSE"
        / "Plugins"
        / "Themes";
}

std::vector<EffectDatabase::EffectEntry>& GetEffectsForType(
    EffectType type)
{
    switch (type)
    {
    case EffectType::VisualEffect:
        return EffectDatabase::visualEffects;

    case EffectType::EffectShader:
        return EffectDatabase::effectShaders;

    case EffectType::Explosion:
        return EffectDatabase::explosions;

    case EffectType::Spell:
        return EffectDatabase::spells;

    case EffectType::Idle:
        return EffectDatabase::idles;

    case EffectType::ImageSpaceModifier:
        return EffectDatabase::imageSpaceModifiers;

    default:
        return EffectDatabase::visualEffects;
    }
}

void IncreaseEffectDuration()
{
    EffectDatabase::effectDuration += 1.0f;

    if (EffectDatabase::effectDuration > 300.0f)
        EffectDatabase::effectDuration = 300.0f;
}

void DecreaseEffectDuration()
{
    EffectDatabase::effectDuration -= 1.0f;

    if (EffectDatabase::effectDuration < 0.1f)
        EffectDatabase::effectDuration = 0.1f;
}

void UpdateEffectControl()
{
    if (!EffectControl::eHeld)
        return;

    auto now = std::chrono::steady_clock::now();

    if (now - EffectControl::lastApplyTime >=
        std::chrono::seconds(1))
    {
        EffectDatabase::ApplySelectedEffect();

        EffectControl::lastApplyTime = now;
    }
}



namespace ProcessInputQueueHook
{
    using func_t =
        void(
            RE::BSTEventSource<RE::InputEvent*>*,
            RE::InputEvent* const*
        );

    REL::Relocation<func_t> originalFunction;
    
    void ProcessKeyboardEvent(RE::ButtonEvent* button)
    {
        if (!button)
            return;

        ImGuiIO& io = ImGui::GetIO();



        const int id = button->GetIDCode();
        const bool down = button->IsDown();
        const bool isPressed = button->IsPressed();

        ImGuiKey key = ImGuiKey_None;

        switch (id)
        {
        case 200:
            //key = ImGuiKey_UpArrow;
            g_UpArrowDown = isPressed;
            io.AddKeyEvent(ImGuiKey_UpArrow, g_UpArrowDown);
            break;

        case 208:
            //key = ImGuiKey_DownArrow;
            g_DownArrowDown = isPressed;
            io.AddKeyEvent(ImGuiKey_DownArrow, g_DownArrowDown);
            break;

        case 203:
            key = ImGuiKey_LeftArrow;
            break;

        case 205:
            key = ImGuiKey_RightArrow;
            break;

        case 14:
            key = ImGuiKey_Backspace;
            break;

        case 15:
        {
            if (down && g_showTestWindow)
            {
                g_tabFocusIndex++;

                // 1. Se foi para o SelectedEffectItem (1) mas NÃO tem efeito selecionado -> pula para favorites (3)
                if (g_tabFocusIndex == static_cast<int>(TabFocus::SelectedEffectItem) && !lastSelectedEffect)
                {
                    g_tabFocusIndex = static_cast<int>(TabFocus::Favorites);
                }

                // 2. Se o próximo índice for o Inspector (2), mas NÃO existir selectedEffect
                if (g_tabFocusIndex == static_cast<int>(TabFocus::Inspector) && !EffectDatabase::selectedEffect)
                {
                    g_tabFocusIndex = static_cast<int>(TabFocus::Favorites); // Pula direto para o 2
                }
                
                // 3. Se o próximo índice for o Favorites (4), mas NÃO existir favorites
                if (g_tabFocusIndex == static_cast<int>(TabFocus::Favorites) && EffectDatabase::favorites.empty())
                {
                    g_tabFocusIndex = static_cast<int>(TabFocus::MainButton); // Pula direto para o config
                }


                // Wrap-around (se ultrapassar o último foco, volta para o primeiro)
                if (g_tabFocusIndex >= 5)
                {
                    g_tabFocusIndex = 0;
                }

                g_requestTabFocus = true;
            }
            break;
        }

        case 28:
            key = ImGuiKey_Enter;
            break;

        case 18:
            key = ImGuiKey_E;
            break;

        case 46:  key = ImGuiKey_C; break; // Scancode 46 = 'C'
        case 47:  key = ImGuiKey_V; break; // Scancode 47 = 'V'
        case 45:  key = ImGuiKey_X; break; // Scancode 45 = 'X'
        case 30:  key = ImGuiKey_A; break; // Scancode 30 = 'A'

        case 42: // Left Shift
            io.AddKeyEvent(ImGuiKey_LeftShift, down);
            break;

        case 54: // Right Shift
            io.AddKeyEvent(ImGuiKey_RightShift, down);
            break;

        case 29:
            io.AddKeyEvent(ImGuiKey_LeftCtrl, down);
            break;

        //case 56:
        //    io.AddKeyEvent(ImGuiKey_LeftAlt, down);
        //    break;

        default:
            break;
        }

        io.AddKeyEvent(ImGuiMod_Shift, (::GetKeyState(VK_SHIFT) & 0x8000) != 0);
        io.AddKeyEvent(ImGuiMod_Ctrl,  (::GetKeyState(VK_CONTROL) & 0x8000) != 0);
        //io.AddKeyEvent(ImGuiMod_Alt,   (::GetKeyState(VK_MENU) & 0x8000) != 0);
        
        if (key != ImGuiKey_None)
        {
            io.AddKeyEvent(key, down);
        }

        // ------------------------------------------------------------
        // Texto
        // ------------------------------------------------------------

        if (!down)
            return;

        BYTE keyboardState[256]{};

        // 1. Pega o estado global do teclado do Windows
        if (!GetKeyboardState(keyboardState))
            return;

        // 2. Garante que os estados de Shift e CapsLock reflitam o GetKeyState atual
        // Isso resolve falhas onde o GetKeyboardState não capturou o frame exato do Shift
        keyboardState[VK_SHIFT]   = (::GetKeyState(VK_SHIFT) & 0x8000) ? 0x80 : 0x00;
        keyboardState[VK_LSHIFT]  = (::GetKeyState(VK_LSHIFT) & 0x8000) ? 0x80 : 0x00;
        keyboardState[VK_RSHIFT]  = (::GetKeyState(VK_RSHIFT) & 0x8000) ? 0x80 : 0x00;
        keyboardState[VK_CAPITAL] = static_cast<BYTE>(::GetKeyState(VK_CAPITAL) & 0x0001);

        UINT virtualKey = MapVirtualKeyW(
            static_cast<UINT>(id),
            MAPVK_VSC_TO_VK_EX
        );

        wchar_t buffer[8]{};

        int result = ToUnicodeEx(
            virtualKey,
            static_cast<UINT>(id),
            keyboardState, // Passa o buffer ajustado com o Shift forçado se estiver pressionado
            buffer,
            static_cast<int>(std::size(buffer)),
            0,
            GetKeyboardLayout(0)
        );

        if (result > 0)
        {
            for (int i = 0; i < result; ++i)
            {
                io.AddInputCharacter(
                    static_cast<unsigned int>(buffer[i])
                );
            }
        }
    }

    void ProcessMouseEvent(RE::ButtonEvent* button)
    {
        if (!button) {
            return;
        }

        auto& io = ImGui::GetIO();

        const auto id = button->GetIDCode();
        const bool pressed = button->IsPressed();

        switch (id)
        {
        case 0:
            // Left Mouse
            io.AddMouseButtonEvent(0, pressed);
            break;

        case 1:
            // Right Mouse
            io.AddMouseButtonEvent(1, pressed);
            break;

        case 2:
            // Middle Mouse
            io.AddMouseButtonEvent(2, pressed);
            break;

        case 8:
            // Mouse Wheel Up
            if (pressed) {
                // Usa a intensidade do scroll (button->Value()) ou 1.0f padrão
                float delta = button->Value() > 0.0f ? button->Value() : 1.0f;
                io.AddMouseWheelEvent(0.0f, delta);
            }
            break;

        case 9:
            // Mouse Wheel Down
            if (pressed) {
                // Valor negativo para scroll para baixo
                float delta = button->Value() > 0.0f ? button->Value() : 1.0f;
                io.AddMouseWheelEvent(0.0f, -delta);
            }
            break;

        default:
            break;
        }

        Logger::GetSingleton().Print(
            "IMGUI MOUSE [{}] [{}]",
            id,
            pressed
        );
    }
    


    void thunk(
        RE::BSTEventSource<RE::InputEvent*>* dispatcher,
        RE::InputEvent* const* events)
    {
        // Nunca tente processar uma lista inexistente.
        if (!dispatcher || !events)
        {
            return;
        }

        RE::InputEvent* first = *events;

        // Não há eventos.
        if (!first)
        {
            originalFunction(dispatcher, events);
            return;
        }

        // ============================================================
        // Se o menu não está aberto:
        // não mexemos na lista, apenas passamos para o Skyrim.
        // ============================================================

        if (!g_showTestWindow)
        {
            originalFunction(dispatcher, events);
            return;
        }

        // ============================================================
        // Processa eventos mas NÃO MODIFICA a lista
        // ============================================================

        RE::InputEvent* current = first;

        while (current)
        {
            // ========================================================
            // BUTTON EVENT
            // ========================================================

            if (auto* button = current->AsButtonEvent())
            {
                // MOUSE
                if (button->GetDevice() == RE::INPUT_DEVICE::kMouse)
                {
                    ProcessMouseEvent(button);
                    // Apenas processa, NÃO remove
                }

                // KEYBOARD
                else if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard)
                {
                    ProcessKeyboardEvent(button);

                    ImGuiIO& io = ImGui::GetIO();

                    if (!io.WantTextInput)
                    {

                        const bool ctrlDown =
                            (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                            
                        switch (button->GetIDCode())
                        {
                        case 203: // LEFT
                            if (button->IsPressed())
                                DecreaseEffectDuration();
                            break;

                        case 205: // RIGHT
                            if (button->IsPressed())
                                IncreaseEffectDuration();
                            break;

                        case 46: // ctrl C quando fora da caixa de texto
                            if (button->IsDown() && ctrlDown)
                            {
                                EffectDatabase::CopySelectedEffectToClipboard();
                            }
                            break;

                        case 18: // E
                            if (button->IsDown())
                            {
                                EffectControl::eHeld = true;
                                EffectDatabase::ApplySelectedEffect();
                                EffectControl::lastApplyTime = 
                                    std::chrono::steady_clock::now();
                            }
                            else if (button->IsUp())
                            {
                                EffectControl::eHeld = false;
                            }
                            break;
                        }
                    }
                    //else if (io.WantTextInput)
                    //{
                    //    const bool ctrlDown =
                    //        (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

                    //    switch (button->GetIDCode())
                    //    {
                    //    case 46: // C
                    //        if (button->IsDown() && ctrlDown)
                    //        {
                            // nada
                    //        }
                    //        break;
                    //    case 47: // V
                    //        if (button->IsDown() && ctrlDown)
                    //        {
                            // nada
                    //        }
                    //        break;
                    //    }
                    //}
                }
            }

            current = current->next;
        }

        // ============================================================
        // Passa a lista ORIGINAL para o Skyrim (sem modificações)
        // ============================================================

        originalFunction(dispatcher, events);
    }

    void Install()
    {
        SKSE::AllocTrampoline(14);

        auto& trampoline = SKSE::GetTrampoline();

        originalFunction =
            trampoline.write_call<5>(
                REL::RelocationID(
                    67315,
                    68617,
                    67315
                ).address()
                +
                REL::Relocate(
                    0x7B,
                    0x7B,
                    0x81
                ),
                thunk
            );
    }
}

LRESULT CALLBACK WndProcHook(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    if (g_showTestWindow && g_imguiInitialized)
    {

        if (msg == WM_CHAR)
        {
            Logger::GetSingleton().Print(
                "WM_CHAR: {}",
                static_cast<int>(wParam)
            );
        }

        ImGui_ImplWin32_WndProcHandler(
            hWnd,
            msg,
            wParam,
            lParam
        );

        ImGuiIO& io = ImGui::GetIO();

        if (io.WantCaptureMouse)
        {
            switch (msg)
            {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                return 0;
            }
        }

        
        if (io.WantCaptureKeyboard)
        {
            switch (msg)
            {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_SYSCHAR:
                return 0;
            }
        }
    }

    return CallWindowProc(
        g_originalWndProc,
        hWnd,
        msg,
        wParam,
        lParam
    );
}

bool InitializeWndProcHook()
{
    Logger::GetSingleton().Print(
        "WndProc: InitializeWndProcHook called"
    );

    Logger::GetSingleton().Print(
        "WndProc: HWND = {}",
        reinterpret_cast<std::uintptr_t>(g_gameWindow)
    );

    if (!g_gameWindow)
    {
        Logger::GetSingleton().Print(
            "WndProc: game window is null"
        );

        return false;
    }

    if (g_originalWndProc)
    {
        Logger::GetSingleton().Print(
            "WndProc: already installed"
        );

        return true;
    }

    g_originalWndProc =
        reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(
                g_gameWindow,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(WndProcHook)
            )
        );

    if (!g_originalWndProc)
    {
        Logger::GetSingleton().Print(
            "WndProc: SetWindowLongPtr FAILED"
        );

        return false;
    }

    Logger::GetSingleton().Print(
        "WndProc: hook installed successfully"
    );

    return true;
}




namespace EffectDatabase
{

    char searchBuffer[256]{};

    std::string g_searchLower;

    std::map<std::string, std::vector<EffectEntry*>> GroupByPlugin(
        const std::vector<EffectEntry>& effects)
    {
        std::map<std::string, std::vector<EffectEntry*>> groups;

        for (auto& effect : effects)
        {
            groups[effect.modName].push_back(
                const_cast<EffectEntry*>(&effect)
            );
        }

        return groups;
    }


    // Busca sem alocar memória nova
    inline bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty()) return true;
        if (haystack.size() < needle.size()) return false;

        auto it = std::search(
            haystack.begin(), haystack.end(),
            needle.begin(), needle.end(),
            [](char ch1, char ch2) {
                return std::tolower(static_cast<unsigned char>(ch1)) == 
                    std::tolower(static_cast<unsigned char>(ch2));
            }
        );

        return it != haystack.end();
    }

    bool MatchesSearch(const EffectEntry& effect)
    {
        // 1. Se a busca estiver vazia, aceita tudo imediatamente
        if (g_searchLower.empty())
            return true;

        // 2. Busca no Nome do Efeito
        if (ContainsCaseInsensitive(effect.name, g_searchLower))
            return true;

        // 3. Busca no Nome do Mod
        if (ContainsCaseInsensitive(effect.modName, g_searchLower))
            return true;

        // 4. Busca no FormID Hexadecimal (Formatado na stack, sem alocar memória no heap)
        char formIDBuf[16];
        std::snprintf(formIDBuf, sizeof(formIDBuf), "%08X", effect.formID);
        if (ContainsCaseInsensitive(formIDBuf, g_searchLower))
            return true;

        // 5. Busca no EditorID (Só executa se as buscas anteriores falharem)
        if (effect.form)
        {
            const std::string editorID = EffectDatabase::GetEditorIDFromForm(effect.form);
            if (!editorID.empty() && ContainsCaseInsensitive(editorID, g_searchLower))
                return true;
        }

        return false;
    }

    bool HasMatchingEffects(
        const std::vector<EffectDatabase::EffectEntry*>& effects)
    {
        for (auto* effect : effects)
        {
            if (effect && MatchesSearch(*effect))
                return true;
        }

        return false;
    }

    void DrawPluginGroup(
        const std::string& modName,
        const std::vector<EffectEntry*>& effects,
        EffectType type)
    {
        // Texto que aparece na interface continua exatamente igual
        int matchCount = 0;

        for (auto* effect : effects)
        {
            if (effect && MatchesSearch(*effect))
                ++matchCount;
        }

        if (matchCount == 0)
            return;

        std::string header =
            modName +
            " (" +
            std::to_string(matchCount) +
            ")";

        // ---------------------------------------------------------
        // ID INTERNO DO IMGUI
        //
        // O nome visível continua sendo apenas "ccbgssse019..."
        // mas o ID agora também depende da categoria.
        //
        // Assim:
        //
        // VISUAL EFFECTS
        //   ccbgssse019...
        //
        // EXPLOSIONS
        //   ccbgssse019...
        //
        // são dois IDs diferentes para o ImGui.
        // ---------------------------------------------------------

        ImGui::PushID(static_cast<int>(type));
        ImGui::PushID(modName.c_str());

        // Header visual continua mostrando somente o nome do plugin
        if (ImGui::TreeNode(header.c_str()))
        {
            for (auto* effect : effects)
            {
                if (!effect) {
                    continue;
                }

                if (!MatchesSearch(*effect)) {
                    continue;
                }

                if (!HasMatchingEffects(effects))
                    return;

                // -------------------------------------------------
                // ID do efeito
                //
                // Categoria + plugin + FormID
                // -------------------------------------------------

                

                ImGui::PushID(static_cast<int>(effect->formID));

                bool isSelected =
                    selectedEffect == effect &&
                    selectedType == type;

                if (isSelected)
                {
                    lastSelectedEffect = effect;
                }

                if (isSelected &&
                    EffectControl::focusLastEffectOnOpen)
                {
                    ImGui::SetKeyboardFocusHere();
                    g_tabFocusIndex = static_cast<int>(TabFocus::SelectedEffectItem);
                    EffectControl::focusLastEffectOnOpen = false;
                }

                // Captura o foco
                if (effect == lastSelectedEffect && g_requestTabFocus && g_tabFocusIndex == static_cast<int>(TabFocus::SelectedEffectItem))
                {
                    ImGui::SetKeyboardFocusHere();
                    ImGui::SetScrollHereY(0.5f);
                    
                    g_requestTabFocus = false;
                }

                if (ImGui::Selectable(
                        effect->name.c_str(),
                        isSelected))
                {
                    EffectDatabase::SelectEffect(
                        effect,
                        type
                    );
                }

                // Navegação por teclado/gamepad do próprio ImGui
                if (ImGui::IsItemFocused())
                {
                    if (!isSelected)
                    {
                        EffectDatabase::SelectEffect(
                            effect,
                            type
                        );
                    }
                }

                ImGui::SameLine();

                ImGui::TextDisabled(
                    "%08X",
                    effect->formID
                );

                ImGui::PopID();
            }

            ImGui::TreePop();
        }

        ImGui::PopID(); // modName
        ImGui::PopID(); // type
    }
    
    


    
    
    void DrawEffectBrowser()
    {
        ImGui::BeginChild(
            "EffectBrowser",
            ImVec2(0, 0),
            true
        );

        ImGui::Text(
            "EFFECT LIBRARY"
        );

        ImGui::Separator();

        if (g_requestTabFocus &&
            g_tabFocusIndex == static_cast<int>(TabFocus::Effects))
        {
            ImGui::SetWindowFocus("EffectBrowser");    
            ImGui::SetKeyboardFocusHere();

            g_requestTabFocus = false;
        }

        if (EffectControl::focusSearchOnOpen)
        {
           ImGui::SetKeyboardFocusHere();
            EffectControl::focusSearchOnOpen = false;
            g_tabFocusIndex = static_cast<int>(TabFocus::Effects);
        }
        

        // 1. Campo de busca
        ImGui::PushItemWidth(200.0f);
        bool textChanged = ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer));
        ImGui::PopItemWidth();

        ImGui::SameLine(0.0f, 10.0f);

        // 2. RadioButton pequeno usado como botão de gatilho (sempre passa false)
        if (ImGui::RadioButton("##PasteRadio", false))
        {
            if (const char* clipboard = ImGui::GetClipboardText())
            {
                strncpy_s(searchBuffer, sizeof(searchBuffer), clipboard, _TRUNCATE);
                textChanged = true; // Força a atualização do g_searchLower
            }
        }

        // Tooltip para o usuário saber o que a bolinha faz ao passar o mouse por cima
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Paste from clipboard");
        }

        //ImGui::SameLine();
        //ImGui::Text("Search"); // Rótulo ao lado

        // 3. Processa a string se houve mudança (digitada ou colada)
        if (textChanged)
        {
            g_searchLower = searchBuffer;
            std::transform(g_searchLower.begin(), g_searchLower.end(), g_searchLower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }

        // ------------------------------------------------------------
        // Trava: Detecta se o usuário clicou fora do campo de busca
        // ------------------------------------------------------------
        bool clickedOutsideField = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered();

        if (clickedOutsideField)
        {
            // Se clicou fora, reseta o estado e impede que a caixa de texto seja reativada
            EffectControl::searchOpenedWithFocus = false;
        }
        else if (ImGui::IsItemFocused())
        {
            // Se chegou aqui porque o menu acabou de abrir,
            // apenas mantém o foco no Search.
            if (EffectControl::searchOpenedWithFocus)
            {
                // Não ativa a edição.
            }
            else if (!ImGui::IsItemActive())
            {
                // Chegou ao Search pela navegação (setas do teclado):
                // entra automaticamente em edição.
                ImGui::ActivateItemByID(ImGui::GetItemID());
            }
        }
        else
        {
            // Saiu do Search.
            // Da próxima vez que voltar por ↑/↓,
            // poderá entrar automaticamente em edição.
            EffectControl::searchOpenedWithFocus = false;
        }

        ImGui::Spacing();

        ImGui::Checkbox(
            "Spells",
            &showSpells
        );

        ImGui::Checkbox(
            "Visual Effects",
            &showVisualEffects
        );

        //ImGui::SameLine();

        ImGui::Checkbox(
            "Effect Shaders",
            &showEffectShaders
        );

        ImGui::Checkbox(
            "Explosions",
            &showExplosions
        );

        //ImGui::SameLine();

        

        ImGui::Checkbox(
            "ImageSpace",
            &showImagespaceModifiers
        );

        

        //ImGui::SameLine();

        //ImGui::Checkbox(
        //    "Idles",
        //    &showIdles
        //);

        
        if (showSpells)
        {
            ImGui::Separator();

            ImGui::Text("SPELLS");

            auto groups =
                GroupByPlugin(spells);

            for (auto& [modName, effects] : groups)
            {
                DrawPluginGroup(
                    modName,
                    effects,
                    EffectType::Spell
                );
            }
        }
        

        if (showVisualEffects)
        {
            ImGui::Separator();

            ImGui::Text(
                "VISUAL EFFECTS"
            );

            auto groups =
                GroupByPlugin(visualEffects);

            for (auto& [modName, effects] : groups)
            {
                DrawPluginGroup(
                    modName,
                    effects,
                    EffectType::VisualEffect
                );
            }
        }

        if (showEffectShaders)
        {
            ImGui::Separator();

            ImGui::Text(
                "EFFECT SHADERS"
            );

            auto groups =
                GroupByPlugin(effectShaders);

            for (auto& [modName, effects] : groups)
            {
                DrawPluginGroup(
                    modName,
                    effects,
                    EffectType::EffectShader
                );
            }
        }

        

        if (showExplosions)
        {
            ImGui::Separator();

            ImGui::Text(
                "EXPLOSIONS"
            );

            auto groups =
                GroupByPlugin(explosions);

            for (auto& [modName, effects] : groups)
            {
                DrawPluginGroup(
                    modName,
                    effects,
                    EffectType::Explosion
                );
            }
        }

        

        if (showImagespaceModifiers)
        {
            ImGui::Separator();

            ImGui::Text("IMAGESPACE");

            auto groups =
                GroupByPlugin(imageSpaceModifiers);

            for (auto& [modName, effects] : groups)
            {
                DrawPluginGroup(
                    modName,
                    effects,
                    EffectType::ImageSpaceModifier
                );
            }
        }

        //---------------------------------------------------------------------------------------------
        // Separador caso nao tenha nenhum menu selecionado
        //----------------------------------------------------------------------------------

        

        //if (showIdles)
        //{
        //    ImGui::Separator();

        //    ImGui::Text("IDLES");

        //    auto groups =
        //        GroupByPlugin(idles);

        //    for (auto& [modName, effects] : groups)
        //    {
        //        DrawPluginGroup(
        //            modName,
        //            effects,
        //            EffectType::Idle
        //        );
        //    }
        //}

        if (!showVisualEffects && !showEffectShaders && !showExplosions && !showSpells && showIdles && !showImagespaceModifiers)
        {
            ImGui::Separator();
        }

        ImGui::EndChild();
    }

    struct EffectTypeInfo {
        const char* tag;
        ImU32 color;
    };

    EffectTypeInfo GetEffectTypeInfo(EffectType type)
        {
            switch (type)
            {
            case EffectType::EffectShader:
                return { "EFSH", IM_COL32(237, 27, 76, 255) };   // Rosa-roxeado

            case EffectType::VisualEffect:
                return { "VSEF", IM_COL32(168, 34, 107, 255) };  // Roxo-rosa

            case EffectType::Explosion:
                return { "EXPL", IM_COL32(242, 106, 68, 255) };  // Laranjado

            case EffectType::ImageSpaceModifier:
                return { "IMAD", IM_COL32(46, 150, 153, 255) };  // Azul marinho - Verde

            case EffectType::Spell:
                return { "SPEL", IM_COL32(246, 219, 104, 255) };  // Amarelo claro

            default:
                return { "UNK",  IM_COL32(250, 250, 250, 255) };
            }
        }

    void DrawEffectInspector()
    {
        ImGui::BeginChild(
            "Inspector",
            ImVec2(0, 0),
            true
        );

        ImGui::Text(
            "INSPECTOR"
        );

        if (g_requestTabFocus &&
            g_tabFocusIndex == static_cast<int>(TabFocus::Inspector))
        {
            ImGui::SetKeyboardFocusHere();
            g_requestTabFocus = false;
        }

        if (selectedEffect)
        {
            const bool isFavorite =
                EffectDatabase::IsFavorite(
                    selectedEffect->formID,
                    selectedType
                );

            ImGui::SameLine(
                isFavorite ? 145.0f : 95.0f, // 1º argumento: offset_from_start_x
                0.0f                        // 2º argumento: spacing_w
            );

            ImGui::PushStyleColor(
                ImGuiCol_Text,
                isFavorite
                    ? IM_COL32(255, 225, 0, 255)
                    : IM_COL32(120, 120, 120, 255)
            );

            if (ImGui::Button(
                    isFavorite ? "FAVORITES" : "Add to Favorites"))
            {
                EffectDatabase::ToggleFavorite(
                    selectedEffect,
                    selectedType
                );
            }

            ImGui::PopStyleColor();

        }

        ImGui::Separator();

        if (!selectedEffect)
        {
            ImGui::TextDisabled(
                "Select an effect."
            );

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::TextDisabled(
                "CONTROLS:"
            );

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::TextDisabled(
                "Activate - to apply effect to player,"
            );
            ImGui::TextDisabled(
                "aim an NPC, and effect will be applied to it"
            );
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::TextDisabled(
                "^ v  moves through itens"
            );
            ImGui::TextDisabled(
                "Tab  moves through menu"
            );

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::TextDisabled(
                "< >  increases or decreases duration effect"
            );
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::TextDisabled(
                "While menu is opened:"
            );
            ImGui::Spacing();
            ImGui::TextDisabled(
                "   PrintScr - captures screen and menu"
            );
            ImGui::TextDisabled(
                "   Ctrl+C - selected effect to clipboard"
            );


            ImGui::EndChild();
            return;
        }

        ImGui::Text("Form ID:");
        ImGui::SameLine();

        ImGui::PushStyleColor(
            ImGuiCol_Text,
            IM_COL32(255, 0, 127, 255)
        );

        ImGui::Text(
            "%08X",
            selectedEffect->formID
        );

        ImGui::PopStyleColor();

        ImGui::Spacing();


        ImGui::PushStyleColor(
            ImGuiCol_Text,
            IM_COL32(0, 255, 255, 255)
        );

        ImGui::Text(
            "%s",
            selectedEffect->name.c_str()
        );

        ImGui::PopStyleColor();

        ImGui::Spacing();

        

        

        ImGui::Text(
            "Plugin: %s",
            selectedEffect->modName.c_str()
        );

        const char* typeName = "Unknown";

        switch (selectedType)
        {
        case EffectType::VisualEffect:
            typeName = "Visual Effect";
            break;

        case EffectType::EffectShader:
            typeName = "Effect Shader";
            break;

        case EffectType::Explosion:
            typeName = "Explosion";
            break;

        case EffectType::Spell:
            typeName = "Spell";
            break;

        case EffectType::Idle:
            typeName = "Idle";
            break;
        
        case EffectType::ImageSpaceModifier:
            typeName = "ImageSpace Modifier";
            break;

        default:
            break;
        }

        ImGui::Text(
            "Type:"
        );


        ImGui::SameLine();
        
        EffectTypeInfo typeInfo = GetEffectTypeInfo(selectedType);

        ImGui::PushStyleColor(ImGuiCol_Text, typeInfo.color);
        ImGui::Text("%s", typeInfo.tag);
        ImGui::PopStyleColor();

        ImGui::SameLine();

        ImGui::Text(
            "%s",
            typeName
        );
            
        ImGui::Separator();
        ImGui::Text("Copy to clipboard:");

        if (ImGui::RadioButton(
                "FORM ID",
                !EffectControl::copyUseEditorID))
        {
            EffectControl::copyUseEditorID = false;
            

            EffectDatabase::CopySelectedEffectToClipboard();
            EffectControl::copiedType =
                EffectControl::CopiedType::FormID;
        }

        ImGui::SameLine();

        if (ImGui::RadioButton(
                "EDITOR ID",
                EffectControl::copyUseEditorID))
        {
            EffectControl::copyUseEditorID = true;

            EffectDatabase::CopySelectedEffectToClipboard();
            EffectControl::copiedType =
                EffectControl::CopiedType::EditorID;
        }

        

        

        //ImGui::SameLine();

        //if (ImGui::Button("Copy to Clipboard"))
        //{
        //    CopySelectedEffectToClipboard();
        //}

        ImGui::Separator();

        

        if (selectedType == EffectType::EffectShader)
        {
            ImGui::Text(
                "PLAYER"
            );

            ImGui::InputFloat(
                "Duration",
                &effectDuration,
                0.5f,
                5.0f,
                "%.2f"
            );

            if (effectDuration < 0.0f)
                effectDuration = 0.0f;

            ImGui::Spacing();

            if (ImGui::Button(
                    "APPLY TO PLAYER",
                    ImVec2(-1, 40)))
            {
                //ApplySelectedEffectShader();
                ApplySelectedEffect();
            }
        }
        else if (selectedType == EffectType::VisualEffect)
        {
            ImGui::Text("PLAYER");

            ImGui::InputFloat(
                "Duration",
                &effectDuration,
                0.5f,
                5.0f,
                "%.2f"
            );

            if (effectDuration < 0.0f)
                effectDuration = 0.0f;

            ImGui::Spacing();

            if (ImGui::Button(
                    "APPLY TO PLAYER",
                    ImVec2(-1, 40)))
            {
                //ApplySelectedVisualEffect();
                ApplySelectedEffect();
            }
        }
        else if (selectedType == EffectType::Explosion)
        {
            ImGui::Text(
                "PLAYER POSITION"
            );

            ImGui::Spacing();

            if (ImGui::Button(
                    "PLACE EXPLOSION",
                    ImVec2(-1, 40)))
            {
                //ApplySelectedExplosion();
                ApplySelectedEffect();
            }
        }
        else if (selectedEffect &&
                selectedType == EffectType::Spell)
        {
            auto* spell =
                selectedEffect->form->As<RE::SpellItem>();

            if (!spell)
            {
                ImGui::Text("Invalid SpellItem");
                return;
            }

            // =========================================================
            // ACTIONS
            // =========================================================

            if (ImGui::Button("ADD SPELL", ImVec2(-1, 40)))
            {
                //EffectDatabase::AddSelectedSpellToPlayer();
                ApplySelectedEffect();
            }

            //ImGui::SameLine();

            if (ImGui::Button("REMOVE SPELL", ImVec2(-1, 40)))
            {
                EffectDatabase::RemoveSelectedSpellFromPlayer();
            }

            ImGui::Separator();

            
            // =========================================================
            // BASIC INFORMATION
            // =========================================================

            if (ImGui::CollapsingHeader(
                    "Basic Information",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {
                // =========================================================
                // NAME
                // =========================================================

                ImGui::Text("Name:");
                ImGui::SameLine();

                const char* name = spell->GetName();

                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    IM_COL32(50, 180, 80, 255)
                );

                ImGui::Text(
                    "%s",
                    (name && name[0] != '\0')
                        ? name
                        : "<None>"
                );

                ImGui::PopStyleColor();

                // =========================================================
                // EDITOR ID
                // =========================================================

                ImGui::Text("EditorID:");
                ImGui::SameLine();

                std::string editorID =
                    GetEditorIDFromForm(spell);

                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    IM_COL32(0, 255, 255, 255)
                );

                ImGui::Text(
                    "%s",
                    editorID.empty()
                        ? "<None>"
                        : editorID.c_str()
                );

                ImGui::PopStyleColor();

                //Form

                ImGui::Text(
                    "Form ID: %08X",
                    spell->GetFormID()
                );

                // =========================================================
                // PLUGIN
                // =========================================================

                ImGui::Text(
                    "Plugin: %s",
                    GetPluginName(spell).c_str()
                );
            }

            // =========================================================
            // SPELL DATA / SPIT
            // =========================================================

            if (ImGui::CollapsingHeader(
                    "Spell Data",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text(
                    "Spell Type: %s",
                    GetSpellTypeName(
                        spell->data.spellType
                    )
                );

                ImGui::Text(
                    "Casting Type: %s",
                    GetCastingTypeName(
                        spell->data.castingType
                    )
                );

                ImGui::Text(
                    "Delivery: %s",
                    GetDeliveryName(
                        spell->data.delivery
                    )
                );

                ImGui::Text(
                    "Charge Time: %.3f",
                    spell->data.chargeTime
                );

                ImGui::Text(
                    "Cast Duration: %.3f",
                    spell->data.castDuration
                );

                ImGui::Text(
                    "Range: %.3f",
                    spell->data.range
                );

                ImGui::Text(
                    "Cost Override: %d",
                    spell->data.costOverride
                );

                ImGui::Text(
                    "Calculated Cost: %.2f",
                    spell->CalculateMagickaCost(
                        RE::PlayerCharacter::GetSingleton()
                    )
                );

                ImGui::Text(
                    "Fixed Cast Duration: %.3f",
                    spell->GetFixedCastDuration()
                );

                ImGui::Text(
                    "Associated Skill: %d",
                    static_cast<int>(
                        spell->GetAssociatedSkill()
                    )
                );

                if (spell->data.castingPerk)
                {
                    auto* perk =
                        spell->data.castingPerk;

                    ImGui::Text(
                        "Casting Perk: %s",
                        perk->GetName()
                            ? perk->GetName()
                            : "<None>"
                    );

                    ImGui::Text(
                        "Casting Perk FormID: %08X",
                        perk->GetFormID()
                    );
                }
                else
                {
                    ImGui::Text(
                        "Casting Perk: None"
                    );
                }
            }

            // =========================================================
            // FLAGS
            // =========================================================

            if (ImGui::CollapsingHeader("Spell Flags"))
            {
                const auto flags =
                    spell->data.flags.underlying();

                ImGui::Text(
                    "Raw Flags: %08X",
                    flags
                );

                ImGui::BulletText(
                    "Cost Override: %s",
                    (flags & (1u << 0))
                        ? "Yes"
                        : "No"
                );

                ImGui::BulletText(
                    "Food Item: %s",
                    (flags & (1u << 1))
                        ? "Yes"
                        : "No"
                );

                ImGui::BulletText(
                    "Extend Duration: %s",
                    (flags & (1u << 3))
                        ? "Yes"
                        : "No"
                );

                ImGui::BulletText(
                    "PC Start Spell: %s",
                    (flags & (1u << 17))
                        ? "Yes"
                        : "No"
                );

                ImGui::BulletText(
                    "Instant Cast: %s",
                    (flags & (1u << 18))
                        ? "Yes"
                        : "No"
                );

                ImGui::BulletText(
                    "Ignore LOS: %s",
                    (flags & (1u << 19))
                        ? "Yes"
                        : "No"
                );

                ImGui::BulletText(
                    "Ignore Resistance: %s",
                    (flags & (1u << 20))
                        ? "Yes"
                        : "No"
                );

                ImGui::BulletText(
                    "No Absorb: %s",
                    (flags & (1u << 21))
                        ? "Yes"
                        : "No"
                );

                ImGui::BulletText(
                    "No Dual Cast Mods: %s",
                    (flags & (1u << 23))
                        ? "Yes"
                        : "No"
                );
            }

            // =========================================================
            // MAGIC EFFECTS
            // =========================================================

            if (ImGui::CollapsingHeader(
                    "Magic Effects",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {


                ImGui::Text("Effect Count:");
                ImGui::SameLine();

                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    IM_COL32(255, 0, 127, 255)
                );

                ImGui::Text(
                    "%zu",
                    spell->effects.size()
                );

                ImGui::PopStyleColor();




                ImGui::Separator();

                for (decltype(spell->effects)::size_type i = 0;
                    i < spell->effects.size();
                    ++i)
                {
                    auto* effect =
                        spell->effects[i];

                    if (!effect)
                        continue;

                    ImGui::PushID(
                        static_cast<int>(i)
                    );

                    // -------------------------------------------------
                    // Effect name
                    // -------------------------------------------------

                    std::string label;

                    if (effect->baseEffect)
                    {
                        const char* effectName =
                            effect->baseEffect->GetName();

                        label = std::format(
                            "{}##Effect{}",
                            effectName
                                ? effectName
                                : "<Unnamed Effect>",
                            i
                        );
                    }
                    else
                    {
                        label = std::format(
                            "Effect {}",
                            i + 1
                        );
                    }

                    bool effectOpen = ImGui::TreeNodeEx(
                        std::format("##Effect{}", i).c_str(),
                        ImGuiTreeNodeFlags_SpanAvailWidth
                    );

                    ImGui::SameLine();

                    ImGui::PushStyleColor(
                        ImGuiCol_Text,
                        IM_COL32(255, 0, 127, 255)
                    );

                    ImGui::Text(
                        "%s",
                        label.c_str()
                    );

                    ImGui::PopStyleColor();

                    if (effectOpen)
                    {
                        // =================================================
                        // EFFECT ITEM
                        // =================================================

                        if (ImGui::CollapsingHeader(
                                "Effect Item",
                                ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Text(
                                "Magnitude: %.3f",
                                effect->effectItem.magnitude
                            );

                            ImGui::Text(
                                "Duration: %u",
                                effect->effectItem.duration
                            );

                            ImGui::Text(
                                "Area: %u",
                                effect->effectItem.area
                            );

                            ImGui::Text(
                                "Cost: %.3f",
                                effect->cost
                            );
                        }

                        // =================================================
                        // EFFECT SETTING
                        // =================================================

                        if (effect->baseEffect)
                        {
                            auto* setting =
                                effect->baseEffect;

                            if (ImGui::CollapsingHeader(
                                    "Magic Effect",
                                    ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                ImGui::Text("Name:");
                                ImGui::SameLine();

                                const char* effectName =
                                    setting->GetName();

                                ImGui::PushStyleColor(
                                    ImGuiCol_Text,
                                    IM_COL32(50, 180, 80, 255)
                                );

                                ImGui::Text(
                                    "%s",
                                    (effectName && effectName[0] != '\0')
                                        ? effectName
                                        : "<None>"
                                );

                                ImGui::PopStyleColor();

                                // --- EditorID ---
                                ImGui::Text("EditorID:");
                                ImGui::SameLine();

                                std::string effectEditorID = GetEditorIDFromForm(setting);

                                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 255, 255));
                                ImGui::Text("%s", effectEditorID.empty() ? "<None>" : effectEditorID.c_str());
                                ImGui::PopStyleColor();

                                


                                // --- Form ID ---
                                char formIdStr[16];
                                std::snprintf(formIdStr, sizeof(formIdStr), "%08X", setting->GetFormID());

                                ImGui::Text("Form ID:");
                                ImGui::SameLine();

                                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
                                ImGui::Text("%s", formIdStr);
                                ImGui::PopStyleColor();

                                if (!effectEditorID.empty())
                                {
                                    //ImGui::SameLine();
                                    ImGui::PushID(setting); // Evita duplicidade de ID no ImGui caso esteja num loop
                                    if (ImGui::Button("Copy Editor ID##SubEditorID"))
                                    {
                                        CopyTextToClipboard(effectEditorID);
                                        EffectControl::copiedTypeSub =
                                            EffectControl::SubCopiedType::SubEditorID;
                                    }
                                    ImGui::PopID();
                                }

                                ImGui::SameLine();
                                
                                ImGui::PushID(setting);
                                if (ImGui::Button("Copy Form ID##SubFormID"))
                                {
                                    CopyTextToClipboard(formIdStr);
                                    EffectControl::copiedTypeSub =
                                        EffectControl::SubCopiedType::SubFormID;
                                }
                                ImGui::PopID();

                                ImGui::Text(
                                    "Archetype: %d",
                                    static_cast<int>(
                                        setting->data.archetype
                                    )
                                );

                                ImGui::Text(
                                    "Associated Skill: %d",
                                    static_cast<int>(
                                        setting->data.associatedSkill
                                    )
                                );

                                ImGui::Text(
                                    "Minimum Skill: %d",
                                    setting->data.minimumSkill
                                );

                                ImGui::Text(
                                    "Resist Variable: %d",
                                    static_cast<int>(
                                        setting->data.resistVariable
                                    )
                                );

                                ImGui::Text(
                                    "Primary AV: %d",
                                    static_cast<int>(
                                        setting->data.primaryAV
                                    )
                                );

                                ImGui::Text(
                                    "Secondary AV: %d",
                                    static_cast<int>(
                                        setting->data.secondaryAV
                                    )
                                );

                                ImGui::Text(
                                    "Base Cost: %.3f",
                                    setting->data.baseCost
                                );

                                ImGui::Text(
                                    "Skill Usage Mult: %.3f",
                                    setting->data.skillUsageMult
                                );

                                ImGui::Text(
                                    "Spellmaking Area: %d",
                                    setting->data.spellmakingArea
                                );

                                ImGui::Text(
                                    "Spellmaking Charge Time: %.3f",
                                    setting->data.spellmakingChargeTime
                                );

                                ImGui::Text(
                                    "Casting Type: %s",
                                    GetCastingTypeName(
                                        setting->data.castingType
                                    )
                                );

                                ImGui::Text(
                                    "Delivery: %s",
                                    GetDeliveryName(
                                        setting->data.delivery
                                    )
                                );
                            }

                            // =================================================
                            // EFFECT FLAGS
                            // =================================================

                            if (ImGui::CollapsingHeader(
                                    "Effect Flags"))
                            {
                                const auto flags =
                                    setting->data.flags.underlying();

                                ImGui::Text(
                                    "Raw Flags: %08X",
                                    flags
                                );

                                ImGui::BulletText(
                                    "Hostile: %s",
                                    (flags & (1u << 0))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "Recover: %s",
                                    (flags & (1u << 1))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "Detrimental: %s",
                                    (flags & (1u << 2))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "Snap To NavMesh: %s",
                                    (flags & (1u << 3))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "No Hit Event: %s",
                                    (flags & (1u << 4))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "Dispel With Keywords: %s",
                                    (flags & (1u << 8))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "No Duration: %s",
                                    (flags & (1u << 9))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "No Magnitude: %s",
                                    (flags & (1u << 10))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "No Area: %s",
                                    (flags & (1u << 11))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "FX Persist: %s",
                                    (flags & (1u << 12))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "Gory Visuals: %s",
                                    (flags & (1u << 14))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "Hide In UI: %s",
                                    (flags & (1u << 15))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "No Recast: %s",
                                    (flags & (1u << 17))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "Power Affects Magnitude: %s",
                                    (flags & (1u << 21))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "Power Affects Duration: %s",
                                    (flags & (1u << 22))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "Painless: %s",
                                    (flags & (1u << 26))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "No Hit Effect: %s",
                                    (flags & (1u << 27))
                                        ? "Yes" : "No"
                                );

                                ImGui::BulletText(
                                    "No Death Dispel: %s",
                                    (flags & (1u << 28))
                                        ? "Yes" : "No"
                                );
                            }

                            // =================================================
                            // VISUAL / GAMEPLAY REFERENCES
                            // =================================================

                            if (ImGui::CollapsingHeader(
                                    "Effect References"))
                            {
                                if (setting->data.projectileBase)
                                {
                                    ImGui::Text(
                                        "Projectile: %08X",
                                        setting->data.projectileBase->GetFormID()
                                    );
                                }
                                else
                                {
                                    ImGui::Text(
                                        "Projectile: None"
                                    );
                                }

                                if (setting->data.explosion)
                                {
                                    ImGui::Text(
                                        "Explosion: %08X",
                                        setting->data.explosion->GetFormID()
                                    );
                                }
                                else
                                {
                                    ImGui::Text(
                                        "Explosion: None"
                                    );
                                }

                                if (setting->data.effectShader)
                                {
                                    ImGui::Text(
                                        "Effect Shader: %08X",
                                        setting->data.effectShader->GetFormID()
                                    );
                                }

                                if (setting->data.enchantShader)
                                {
                                    ImGui::Text(
                                        "Enchant Shader: %08X",
                                        setting->data.enchantShader->GetFormID()
                                    );
                                }

                                if (setting->data.castingArt)
                                {
                                    ImGui::Text(
                                        "Casting Art: %08X",
                                        setting->data.castingArt->GetFormID()
                                    );
                                }

                                if (setting->data.hitEffectArt)
                                {
                                    ImGui::Text(
                                        "Hit Effect Art: %08X",
                                        setting->data.hitEffectArt->GetFormID()
                                    );
                                }

                                if (setting->data.enchantEffectArt)
                                {
                                    ImGui::Text(
                                        "Enchant Effect Art: %08X",
                                        setting->data.enchantEffectArt->GetFormID()
                                    );
                                }

                                if (setting->data.hitVisuals)
                                {
                                    ImGui::Text(
                                        "Hit Visuals: %08X",
                                        setting->data.hitVisuals->GetFormID()
                                    );
                                }

                                if (setting->data.enchantVisuals)
                                {
                                    ImGui::Text(
                                        "Enchant Visuals: %08X",
                                        setting->data.enchantVisuals->GetFormID()
                                    );
                                }

                                if (setting->data.imageSpaceMod)
                                {
                                    ImGui::Text(
                                        "Image Space Modifier: %08X",
                                        setting->data.imageSpaceMod->GetFormID()
                                    );
                                }

                                if (setting->data.perk)
                                {
                                    ImGui::Text(
                                        "Perk: %08X",
                                        setting->data.perk->GetFormID()
                                    );
                                }
                            }
                        }

                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }
            }

            // =========================================================
            // MAGIC ITEM INFO
            // =========================================================

            if (ImGui::CollapsingHeader("Magic Item"))
            {
                ImGui::Text(
                    "Hostile Effects: %d",
                    spell->hostileCount
                );

                ImGui::Text(
                    "Largest Area: %d",
                    spell->GetLargestArea()
                );

                ImGui::Text(
                    "Longest Duration: %u",
                    spell->GetLongestDuration()
                );

                ImGui::Text(
                    "Permanent: %s",
                    spell->IsPermanent()
                        ? "Yes"
                        : "No"
                );

                ImGui::Text(
                    "Valid: %s",
                    spell->IsValid()
                        ? "Yes"
                        : "No"
                );

                ImGui::Text(
                    "Two Handed: %s",
                    spell->IsTwoHanded()
                        ? "Yes"
                        : "No"
                );

                ImGui::Text(
                    "Is Poison: %s",
                    spell->IsPoison()
                        ? "Yes"
                        : "No"
                );

                ImGui::Text(
                    "Is Food: %s",
                    spell->IsFood()
                        ? "Yes"
                        : "No"
                );

                ImGui::Text(
                    "Ignores Resistance: %s",
                    spell->IgnoresResistance()
                        ? "Yes"
                        : "No"
                );

                ImGui::Text(
                    "Ignore LOS: %s",
                    spell->IgnoreLOS()
                        ? "Yes"
                        : "No"
                );
            }
        }
        else if (selectedEffect &&
            selectedType == EffectType::Idle)
        {
            ImGui::InputFloat(
                "Idle Duration",
                &EffectControl::idleDuration,
                0.5f,
                1.0f,
                "%.1f"
            );

            if (ImGui::Button("PLAY IDLE", ImVec2(-1, 40)))
            {
                EffectDatabase::ApplySelectedIdle();
            }

            //ImGui::SameLine();

            if (ImGui::Button("STOP IDLE", ImVec2(-1, 40)))
            {
                EffectDatabase::StopPlayerIdle();
            }
        }

        else if (selectedEffect &&
            selectedType == EffectType::ImageSpaceModifier)
        {

            ImGui::InputFloat(
                "Duration",
                &EffectDatabase::effectDuration,
                0.5f,
                1.0f,
                "%.1f s"
            );

            if (EffectDatabase::effectDuration < 0.1f)
                EffectDatabase::effectDuration = 0.1f;
                
            if (ImGui::Button(
                    "APPLY",
                    ImVec2(-1, 40)))
            {
                //EffectDatabase::ApplySelectedImageSpaceModifier();
                ApplySelectedEffect();
            }

            //ImGui::SameLine();

            if (ImGui::Button(
                    "STOP",
                    ImVec2(-1, 40)))
            {
                EffectDatabase::StopSelectedImageSpaceModifier();
            }

            if (EffectControl::imageSpaceActive)
            {
                ImGui::SameLine();

                ImGui::Text(
                    "%.1f s",
                    EffectControl::imageSpaceRemaining
                );
            }

        }


        ImGui::EndChild();
    }

    enum class FavDockState {
        Free,           // Livre pela tela
        Right_Top,      // Direita / Topo  -> Cresce p/ BAIXO (Vermelho)
        Right_Bottom,   // Direita / Base  -> Cresce p/ CIMA  (Verde)
        Left_Top,       // Esquerda / Topo -> Cresce p/ BAIXO (Vermelho)
        Left_Bottom,    // Esquerda / Base -> Cresce p/ CIMA  (Verde)
        Top_Center,     // Acima da Main   -> Cresce p/ CIMA  (Verde)
        Bottom_Center   // Abaixo da Main  -> Cresce p/ BAIXO (Vermelho)
    };

    static FavDockState currentDock = FavDockState::Right_Top;

    
        
    void DrawFavoritesWindow(const ImVec2& mainPos, const ImVec2& mainSize)
    {
        float snapThreshold = 200.0f; // Distância em pixels para ativar o "ímã"
        float snapThresholdBottom = 30.0f; // "ímã" da caixa bottom

        // -------------------------------------------------------------
        // 1. APLICAR POSIÇÃO APENAS SE NÃO ESTIVER ARRASTANDO
        // -------------------------------------------------------------
        // Se estiver desacoplado (Free) ou se o usuário estiver arrastando com o mouse,
        // NÃO forçamos o SetNextWindowPos para permitir a movimentação fluida.
        static bool isDraggingFav = false; // Guarda se a janela FAVORITES especificamente está sendo arrastada

        // Atualiza a posição apenas se estiver acoplado E não estiver arrastando a própria janela Favorites
        if (currentDock != FavDockState::Free && !isDraggingFav)
        {
            ImVec2 targetPos;
            ImVec2 pivot;

            switch (currentDock)
            {
            case FavDockState::Right_Top:
                targetPos = ImVec2(mainPos.x + mainSize.x, mainPos.y);
                pivot     = ImVec2(0.0f, 0.0f);
                break;

            case FavDockState::Right_Bottom:
                targetPos = ImVec2(mainPos.x + mainSize.x, mainPos.y + mainSize.y);
                pivot     = ImVec2(0.0f, 1.0f);
                break;

            case FavDockState::Left_Top:
                targetPos = ImVec2(mainPos.x, mainPos.y);
                pivot     = ImVec2(1.0f, 0.0f);
                break;

            case FavDockState::Left_Bottom:
                targetPos = ImVec2(mainPos.x, mainPos.y + mainSize.y);
                pivot     = ImVec2(1.0f, 1.0f);
                break;

            case FavDockState::Top_Center:
                targetPos = ImVec2(mainPos.x, mainPos.y);
                pivot     = ImVec2(0.0f, 1.0f);
                break;

            case FavDockState::Bottom_Center:
                targetPos = ImVec2(mainPos.x, mainPos.y + mainSize.y);
                pivot     = ImVec2(0.0f, 0.0f);
                break;

            default:
                break;
            }

            ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always, pivot);
        }

        ImGuiWindowFlags favFlags = ImGuiWindowFlags_None;

        //No ImGui, o título da janela no ImGui::Begin() serve tanto como o texto visível no cabeçalho
        //quanto como a ID única interna daquela janela no motor do ImGui.

        //Se você passar uma string vazia "" ou espaços " ", o ImGui pode ter problemas de colisão de ID.
        //A forma correta e recomendada pelo ImGui para ocultar ou deixar um título em branco é usar o separador de ID ###.

        //A Solução Nativa: ###
        //Escreva apenas os três jogo da velha seguidos de um identificador único qualquer:

        ImGui::Begin("###FavoriteWindow", nullptr, favFlags);

        ImVec2 currentFavPos  = ImGui::GetWindowPos();
        ImVec2 currentFavSize = ImGui::GetWindowSize();

        isDraggingFav = ImGui::IsWindowFocused() && ImGui::IsMouseDragging(ImGuiMouseButton_Left);

        // -------------------------------------------------------------
        // 2. DETECTAR ARRASTE E CALCULAR SNAP
        // -------------------------------------------------------------
        // Verifica se a janela de favoritos está focada e se o usuário está arrastando
        if (isDraggingFav)
        {
            float favX = currentFavPos.x;
            float favY = currentFavPos.y;

            // Lógica para desengatar das âncoras se o mouse se mover
            FavDockState newDock = FavDockState::Free;

            // Âncoras do lado Direito
            if (std::abs(favX - (mainPos.x + mainSize.x)) < snapThreshold)
            {
                if (std::abs(favY - mainPos.y) < snapThreshold)
                    newDock = FavDockState::Right_Top;
                else if (std::abs((favY + currentFavSize.y) - (mainPos.y + mainSize.y)) < snapThreshold)
                    newDock = FavDockState::Right_Bottom;
            }
            // Âncoras do lado Esquerdo
            else if (std::abs((favX + currentFavSize.x) - mainPos.x) < snapThreshold)
            {
                if (std::abs(favY - mainPos.y) < snapThreshold)
                    newDock = FavDockState::Left_Top;
                else if (std::abs((favY + currentFavSize.y) - (mainPos.y + mainSize.y)) < snapThreshold)
                    newDock = FavDockState::Left_Bottom;
            }
            // Âncora Superior
            else if (std::abs((favY + currentFavSize.y) - mainPos.y) < snapThreshold &&
                    std::abs(favX - mainPos.x) < snapThreshold)
            {
                newDock = FavDockState::Top_Center;
            }
            // Âncora Inferior
            else if (std::abs(favY - (mainPos.y + mainSize.y)) < snapThreshold &&
                    std::abs(favX - mainPos.x) < snapThreshold)
            {
                newDock = FavDockState::Bottom_Center;
            }

            currentDock = newDock;
        }

        // -------------------------------------------------------------
        // 3. ÍMÃ DE REDIMENSIONAMENTO (SNAP DE ALTURA)
        // -------------------------------------------------------------
        // Se a janela está sendo redimensionada pelo usuário
        if (ImGui::IsWindowFocused() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            // Caso esteja ancorado no Topo (Right_Top ou Left_Top)
            if (currentDock == FavDockState::Right_Top || currentDock == FavDockState::Left_Top)
            {
                // Onde a borda inferior do Favoritos DEVERIA estar para alinhar com a Main Window
                float targetBottomY = mainPos.y + mainSize.y;
                float currentBottomY = currentFavPos.y + currentFavSize.y;

                // Se a borda inferior do Favoritos estiver perto do fundo da janela principal
                if (std::abs(currentBottomY - targetBottomY) < snapThresholdBottom)
                {
                    float targetHeight = mainSize.y; // Força a mesma altura da Main Window
                    ImGui::SetWindowSize(ImVec2(currentFavSize.x, targetHeight));
                }
            }
        }

        // =========================================================
        // RECENT
        // =========================================================

        ImGui::Text("RECENT");
        ImGui::Separator();

        ImGui::PushID("Recent");

        for (std::size_t i = 0; i < recentEffects.size(); ++i)
        {
            auto& recent = recentEffects[i];

            ImGui::PushID(static_cast<int>(i));

            EffectEntry* entryPtr =
                EffectDatabase::GetEffectByFormID(
                    recent.formID,
                    recent.type
                );

            bool isSelected =
                selectedEffect == entryPtr &&
                selectedType == recent.type;

            // ---------------------------------------------
            // Foco inicial
            // ---------------------------------------------

            if (isSelected &&
                EffectControl::focusLastEffectOnOpen)
            {
                ImGui::SetKeyboardFocusHere();
                g_tabFocusIndex = static_cast<int>(TabFocus::Favorites);
                EffectControl::focusLastEffectOnOpen = false;
            }

            // ---------------------------------------------
            // SELECTABLE
            // ---------------------------------------------

            ImGui::PushStyleColor(
                ImGuiCol_Text,
                IM_COL32(150, 150, 150, 255)
            );

            bool clicked =
                ImGui::Selectable(
                    recent.name.c_str(),
                    isSelected
                );

            // IMPORTANTE:
            // IsItemFocused / IsItemClicked precisam vir
            // imediatamente depois do Selectable.

            bool focused =
                ImGui::IsItemFocused();

            bool mouseClicked =
                ImGui::IsItemClicked(ImGuiMouseButton_Left);

            ImGui::PopStyleColor();

            // ---------------------------------------------
            // Seleção por clique
            // ---------------------------------------------

            if ((clicked || mouseClicked) && entryPtr)
            {
                EffectDatabase::SelectEffect(
                    entryPtr,
                    recent.type
                );
            }

            // ---------------------------------------------
            // Navegação por teclado
            // ---------------------------------------------

            if (focused && entryPtr)
            {
                if (!isSelected)
                {
                    EffectDatabase::SelectEffect(
                        entryPtr,
                        recent.type
                    );
                }
            }

            // ---------------------------------------------
            // TAG
            // ---------------------------------------------

            ImGui::SameLine(0.0f, 8.0f);

            EffectTypeInfo typeInfo =
                GetEffectTypeInfo(recent.type);

            ImGui::PushStyleColor(
                ImGuiCol_Text,
                typeInfo.color
            );

            ImGui::Text(
                "%s",
                typeInfo.tag
            );

            ImGui::PopStyleColor();

            ImGui::PopID();
        }

        ImGui::PopID();

        // =========================================================
        // FAVORITES
        // =========================================================

        ImGui::Spacing();

        ImGui::Text("FAVORITES");
        ImGui::Separator();

        ImGui::PushID("Favorites");

        for (std::size_t i = 0; i < favorites.size(); ++i)
        {
            auto& favorite = favorites[i];
       

            ImGui::PushID(static_cast<int>(i));


            // 1. OBTÉM O PONTEIRO EffectEntry* CORRESPONDENTE
            EffectEntry* entryPtr = EffectDatabase::GetEffectByFormID(favorite.formID, favorite.type);

            // 2. VERIFICA SE ESTÁ SELECIONADO
            bool isSelected = selectedEffect == entryPtr && 
                            selectedType == favorite.type;

            // 3. SELEÇÃO/FOCO INICIAL DE TECLADO
            if (isSelected && EffectControl::focusLastEffectOnOpen)
            {
                ImGui::SetKeyboardFocusHere();
                g_tabFocusIndex = static_cast<int>(TabFocus::Favorites);
                EffectControl::focusLastEffectOnOpen = false;
            }

            // -----------------------------------------------------
            // RENDERIZAÇÃO
            // -----------------------------------------------------

            // 1. DETERMINA A TAG E A COR COM BASE NO TIPO DO EFEITO
            EffectTypeInfo typeInfo = GetEffectTypeInfo(favorite.type);

            // 2. DESENHA A TAG COLORIDA PRIMEIRO (na esquerda)
            ImGui::PushStyleColor(ImGuiCol_Text, typeInfo.color);
            ImGui::Text("%s", typeInfo.tag);
            ImGui::PopStyleColor();

            // 3. COLOCA O NOME DO FAVORITO NA MESMA LINHA (Com espaço de 8px)
            ImGui::SameLine(0.0f, 8.0f);

            // 1. OBTÉM O TEMPO ATUAL DO IMGUI
            float time = static_cast<float>(ImGui::GetTime());

            // 2. CALCULA UMA FREQUÊNCIA E UM DESLOCAMENTO DE FASE ÚNICOS PARA CADA ITEM (i)


            // Quanto MENOR o número base, MAIS LENTO fica o fade.
            // Exemplo anterior: 1.8f (Rápido)
            // Exemplo atual:    0.4f (Bem lento e suave)
            float baseSpeed = 0.6f;

            // Multiplicamos 'i' por números primos/quebrados para os ciclos nunca se alinharem
            float speed  = baseSpeed + std::fmod(i * 0.17f, 0.3f); // Velocidades levemente diferentes



            float offset = i * 2.399f;                         // Desloca o início do ciclo

            // Onda senoidal calculada entre 0.0 (cinza) e 1.0 (branco)
            float wave = (std::sin(time * speed + offset) + 1.0f) * 0.5f;

            // Interpola o tom de cinza (120 = cinza escuro/médio, 255 = branco puro)
            std::uint8_t colorValue = static_cast<std::uint8_t>(120.0f + (wave * 135.0f));

            // -----------------------------------------------------
            // RENDERIZAÇÃO COM COR DINÂMICA
            // -----------------------------------------------------
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(colorValue, colorValue, colorValue, 255));

            if (g_requestTabFocus &&
                g_tabFocusIndex == static_cast<int>(TabFocus::Favorites))
            {
                ImGui::SetKeyboardFocusHere();
                g_requestTabFocus = false;
            }

            if (ImGui::Selectable(favorite.name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (entryPtr)
                {
                    EffectDatabase::SelectEffect(entryPtr, favorite.type);
                }
            }

            ImGui::PopStyleColor();

            // 4. DETECTA A NAVEGAÇÃO POR TECLADO
            if (ImGui::IsItemFocused())
            {
                if (!isSelected && entryPtr)
                {
                    EffectDatabase::SelectEffect(entryPtr, favorite.type);
                }
            }

            // -----------------------------------------------------
            // Clique normal
            // -----------------------------------------------------

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                EffectDatabase::SelectEffectByFormID(
                    favorite.formID,
                    favorite.type
                );
            }

            // -----------------------------------------------------
            // COMEÇOU O DRAG
            // -----------------------------------------------------

            if (ImGui::IsItemActive() &&
                ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                EffectControl::draggingFavorite = true;
                EffectControl::draggedFavoriteIndex =
                    static_cast<int>(i);
            }

            // -----------------------------------------------------
            // Soltou o mouse
            // -----------------------------------------------------

            if (EffectControl::draggingFavorite &&
                !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                EffectControl::draggingFavorite = false;
                EffectControl::draggedFavoriteIndex = -1;
            }

            // -----------------------------------------------------
            // Se estamos arrastando outro favorito
            // -----------------------------------------------------

            if (EffectControl::draggingFavorite &&
                EffectControl::draggedFavoriteIndex >= 0 &&
                EffectControl::draggedFavoriteIndex !=
                    static_cast<int>(i))
            {
                if (ImGui::IsItemHovered(
                        ImGuiHoveredFlags_RectOnly))
                {
                    const int from =
                        EffectControl::draggedFavoriteIndex;

                    const int to =
                        static_cast<int>(i);

                    if (from != to)
                    {
                        std::swap(
                            favorites[from],
                            favorites[to]
                        );

                        EffectControl::draggedFavoriteIndex =
                            to;
                    }
                }
            }

            // -----------------------------------------------------
            // Visual do item sendo arrastado
            // -----------------------------------------------------

            if (EffectControl::draggingFavorite &&
                EffectControl::draggedFavoriteIndex ==
                    static_cast<int>(i))
            {
                ImGui::SameLine();

                ImGui::TextDisabled(
                    "< drag >"
                );
            }

            ImGui::PopID();
        }

        ImGui::PopID();

        ImGui::End();
    }

    void Scan()
    {
        visualEffects.clear();
        effectShaders.clear();
        explosions.clear();
        spells.clear();
        idles.clear();
        imageSpaceModifiers.clear();

        auto* dataHandler =
            RE::TESDataHandler::GetSingleton();

        if (!dataHandler)
        {
            Logger::GetSingleton().Print(
                "EffectDatabase: TESDataHandler == nullptr"
            );

            return;
        }

        // ============================================================
        // VISUAL EFFECTS
        // ============================================================

        auto& visualArray =
            dataHandler->GetFormArray<RE::BGSReferenceEffect>();

        for (auto* form : visualArray)
        {
            if (!form)
                continue;

            std::string name = GetFormName(form);
            //std::string modName = GetPluginName(form);

            if (name.empty())
                name = "<No Name>";

            visualEffects.push_back({
                form,
                name,
                form->GetFormID(),
                GetPluginName(form)
            });
        }

        // ============================================================
        // EFFECT SHADERS
        // ============================================================

        auto& shaderArray =
            dataHandler->GetFormArray<RE::TESEffectShader>();

        for (auto* form : shaderArray)
        {
            if (!form)
                continue;

            std::string name = GetFormName(form);
            //std::string modName = GetPluginName(form);

            if (name.empty())
                name = "<No Name>";

            effectShaders.push_back({
                form,
                name,
                form->GetFormID(),
                GetPluginName(form)
            });
        }

        // ============================================================
        // EXPLOSION SHADERS
        // ============================================================

        auto& explosionArray =
            dataHandler->GetFormArray<RE::BGSExplosion>();

        for (auto* form : explosionArray)
        {
            if (!form)
                continue;

            std::string name = GetFormName(form);
            //std::string modName = GetPluginName(form);

            if (name.empty())
                name = "<No Name>";

            explosions.push_back({
                form,
                name,
                form->GetFormID(),
                GetPluginName(form)
            });
        }

        // ============================================================
        // SPELLS
        // ============================================================
        auto& spellArray =
            dataHandler->GetFormArray<RE::SpellItem>();

        for (auto* form : spellArray)
        {
            if (!form)
                continue;

            std::string name = GetFormName(form);

            if (name.empty())
                name = "<No Name>";

            spells.push_back({
                form,
                name,
                form->GetFormID(),
                GetPluginName(form)
            });
        }

        // ============================================================
        // IDLES
        // ============================================================

        //auto [forms, lock] = RE::TESForm::GetAllForms();

        //if (!forms)
        //    return;

        //RE::BSReadLockGuard guard(lock);

        //for (auto& [formID, form] : *forms)
        //{
        //    if (!form || !form->Is(RE::FormType::Idle))
        //        continue;

        //    auto* idle = form->As<RE::TESIdleForm>();

        //    if (!idle)
        //        continue;

        //    std::string name = GetFormName(idle);

        //    if (name.empty())
        //        name = "<No Name>";
        //    idles.push_back({
        //        idle,
        //        name,
        //        idle->GetFormID(),
        //        GetPluginName(idle)
        //    });
        //}

        //EffectDatabase::BuildIdleDatabaseCache();

        // -------------------------------------------------
        // IMAGE SPACE MODIFIERS
        // -------------------------------------------------

        auto& imageSpaceArray =
            dataHandler->GetFormArray<RE::TESImageSpaceModifier>();

        for (auto* form : imageSpaceArray)
        {
            if (!form)
                continue;

            std::string name = GetFormName(form);

            if (name.empty())
                name = "<No Name>";

            imageSpaceModifiers.push_back({
                form,
                name,
                form->GetFormID(),
                GetPluginName(form)
            });
        }

        // ============================================================
        // ============================================================
        // ============================================================

        Logger::GetSingleton().Print(
            "EffectDatabase: Scanned"
        );
    }

    bool DrawColorSetting(
        const char* name,
        ImVec4& color)
    {
        bool changed = ImGui::ColorEdit4(
            name,
            reinterpret_cast<float*>(&color),
            ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_DisplayRGB);

        return changed;
    }

    void SaveStyleToFile(
        const ImGuiStyle& style,
        const std::filesystem::path& path)
    {
        std::ofstream file(path);

        if (!file.is_open())
            return;

        // -------------------------
        // Layout
        // -------------------------

        file << "[Style]\n";

        file << "WindowPadding="
            << style.WindowPadding.x << ","
            << style.WindowPadding.y << "\n";

        file << "WindowRounding="
            << style.WindowRounding << "\n";

        file << "FramePadding="
            << style.FramePadding.x << ","
            << style.FramePadding.y << "\n";

        file << "FrameRounding="
            << style.FrameRounding << "\n";

        file << "ItemSpacing="
            << style.ItemSpacing.x << ","
            << style.ItemSpacing.y << "\n";

        file << "ItemInnerSpacing="
            << style.ItemInnerSpacing.x << ","
            << style.ItemInnerSpacing.y << "\n";

        file << "IndentSpacing="
            << style.IndentSpacing << "\n";

        file << "ScrollbarSize="
            << style.ScrollbarSize << "\n";

        file << "ScrollbarRounding="
            << style.ScrollbarRounding << "\n";

        file << "GrabMinSize="
            << style.GrabMinSize << "\n";

        file << "GrabRounding="
            << style.GrabRounding << "\n";


        // -------------------------
        // Colors
        // -------------------------

        file << "\n[Colors]\n";

        for (int i = 0; i < ImGuiCol_COUNT; ++i)
        {
            const ImVec4& c = style.Colors[i];

            file << i << "="
                << c.x << ","
                << c.y << ","
                << c.z << ","
                << c.w << "\n";
        }
    }

    void SaveCurrentTheme()
    {
        const auto dir = GetThemeDirectory();

        std::filesystem::create_directories(dir);

        SaveStyleToFile(
            ImGui::GetStyle(),
            dir / "__CurrentTheme.ini");
    }

    void CreateTheme(
        const std::string& name)
    {
        if (name.empty())
            return;

        const auto dir =
            GetThemeDirectory();

        std::filesystem::create_directories(dir);

        const auto path =
            dir / (name + ".ini");

        SaveStyleToFile(
            ImGui::GetStyle(),
            path);

        g_themes[name] =
            ImGui::GetStyle();

        g_currentThemeName = name;

        SaveCurrentTheme();
    }

    bool LoadStyleFromFile(
        ImGuiStyle& style,
        const std::filesystem::path& path)
    {
        std::ifstream file(path);

        if (!file.is_open())
            return false;

        std::string section;
        std::string line;

        while (std::getline(file, line))
        {
            if (line.empty())
                continue;

            if (line.front() == '[' &&
                line.back() == ']')
            {
                section = line.substr(
                    1,
                    line.size() - 2);

                continue;
            }

            const auto equal = line.find('=');

            if (equal == std::string::npos)
                continue;

            std::string key =
                line.substr(0, equal);

            std::string value =
                line.substr(equal + 1);

            if (section == "Colors")
            {
                try
                {
                    int index = std::stoi(key);

                    if (index < 0 ||
                        index >= ImGuiCol_COUNT)
                    {
                        continue;
                    }

                    float values[4];

                    if (sscanf_s(
                            value.c_str(),
                            "%f,%f,%f,%f",
                            &values[0],
                            &values[1],
                            &values[2],
                            &values[3]) == 4)
                    {
                        style.Colors[index] =
                            ImVec4(
                                values[0],
                                values[1],
                                values[2],
                                values[3]);
                    }
                }
                catch (...)
                {
                }

                continue;
            }

            if (section != "Style")
                continue;

            try
            {
                if (key == "WindowRounding")
                    style.WindowRounding =
                        std::stof(value);

                else if (key == "FrameRounding")
                    style.FrameRounding =
                        std::stof(value);

                else if (key == "IndentSpacing")
                    style.IndentSpacing =
                        std::stof(value);

                else if (key == "ScrollbarSize")
                    style.ScrollbarSize =
                        std::stof(value);

                else if (key == "ScrollbarRounding")
                    style.ScrollbarRounding =
                        std::stof(value);

                else if (key == "GrabMinSize")
                    style.GrabMinSize =
                        std::stof(value);

                else if (key == "GrabRounding")
                    style.GrabRounding =
                        std::stof(value);

                else if (key == "WindowPadding")
                    sscanf_s(
                        value.c_str(),
                        "%f,%f",
                        &style.WindowPadding.x,
                        &style.WindowPadding.y);

                else if (key == "FramePadding")
                    sscanf_s(
                        value.c_str(),
                        "%f,%f",
                        &style.FramePadding.x,
                        &style.FramePadding.y);

                else if (key == "ItemSpacing")
                    sscanf_s(
                        value.c_str(),
                        "%f,%f",
                        &style.ItemSpacing.x,
                        &style.ItemSpacing.y);

                else if (key == "ItemInnerSpacing")
                    sscanf_s(
                        value.c_str(),
                        "%f,%f",
                        &style.ItemInnerSpacing.x,
                        &style.ItemInnerSpacing.y);
            }
            catch (...)
            {
            }
        }

        return true;
    }

    

    void LoadCurrentTheme()
    {
        const auto path =
            GetThemeDirectory() /
            "__CurrentTheme.ini";

        ImGuiStyle& style =
            ImGui::GetStyle();

        if (LoadStyleFromFile(style, path))
        {
            Logger::GetSingleton().Print(
                "Current theme loaded");

            return;
        }

        Logger::GetSingleton().Print(
            "No current theme found. Using DefaultTheme.");
    }

    void LoadDefaultTheme()
    {
        const auto path =
            GetThemeDirectory() /
            "DefaultTheme.ini";

        ImGuiStyle& style =
            ImGui::GetStyle();

        if (LoadStyleFromFile(style, path))
        {
            Logger::GetSingleton().Print(
                "Default theme loaded: {}",
                path.string());

            return;
        }

        Logger::GetSingleton().Print(
            "DefaultTheme.ini not found. Using ImGui default style.");
    }

    void DeleteTheme(const std::string& name)
    {
        if (name.empty())
            return;

        // Temas protegidos
        if (name == "DefaultTheme" ||
            name == "__CurrentTheme")
        {
            return;
        }

        const auto path =
            GetThemeDirectory() /
            (name + ".ini");

        std::error_code ec;

        if (std::filesystem::remove(path, ec))
        {
            Logger::GetSingleton().Print(
                "Theme deleted: {}",
                name);
        }
        else
        {
            Logger::GetSingleton().Print(
                "Failed to delete theme: {} ({})",
                name,
                ec.message());

            return;
        }

        // Remove do cache em memória
        g_themes.erase(name);

        // Se era o tema atual
        if (g_currentThemeName == name)
        {
            g_currentThemeName = "DefaultTheme";

            LoadDefaultTheme();
            SaveCurrentTheme();
        }
    }

    void LoadThemes()
    {
        g_themes.clear();

        const auto dir =
            GetThemeDirectory();

        std::filesystem::create_directories(dir);

        for (const auto& entry :
            std::filesystem::directory_iterator(dir))
        {
            if (!entry.is_regular_file())
                continue;

            if (entry.path().extension() != ".ini")
                continue;

            if (entry.path().filename() ==
                "__CurrentTheme.ini")
            {
                continue;
            }

            std::string name =
                entry.path().stem().string();

            ImGuiStyle style;

            // começa com o estilo atual como base
            style = ImGui::GetStyle();

            if (LoadStyleFromFile(
                    style,
                    entry.path()))
            {
                g_themes[name] = style;
            }
        }

        
    }

    void LoadNamedTheme(
        const std::string& name)
    {
        const auto path =
            GetThemeDirectory() /
            (name + ".ini");

        if (!LoadStyleFromFile(
                ImGui::GetStyle(),
                path))
        {
            return;
        }

        g_themes[name] =
            ImGui::GetStyle();

        g_currentThemeName = name;

        SaveCurrentTheme();
    }

    void DrawThemeCustomizer()
    {
        ImGuiStyle* style = &ImGui::GetStyle();

        if (ImGui::CollapsingHeader("Style"))
        {
            ImGui::Text("Layout");
            ImGui::Separator();

            if (ImGui::DragFloat2(
                "WindowPadding",
                reinterpret_cast<float*>(&style->WindowPadding),
                0.5f,
                0.0f,
                100.0f))
            {
                g_themeDirty = true;
            }

            if (ImGui::DragFloat(
                "WindowRounding",
                &style->WindowRounding,
                0.1f,
                0.0f,
                30.0f))
            {
                g_themeDirty = true;
            }

            if (ImGui::DragFloat2(
                "FramePadding",
                reinterpret_cast<float*>(&style->FramePadding),
                0.5f,
                0.0f,
                50.0f))
            {
                g_themeDirty = true;
            }

            if (ImGui::DragFloat(
                "FrameRounding",
                &style->FrameRounding,
                0.1f,
                0.0f,
                30.0f))
            {
                g_themeDirty = true;
            }

            if (ImGui::DragFloat2(
                "ItemSpacing",
                reinterpret_cast<float*>(&style->ItemSpacing),
                0.5f,
                0.0f,
                50.0f))
            {
                g_themeDirty = true;
            }

            if (ImGui::DragFloat2(
                "ItemInnerSpacing",
                reinterpret_cast<float*>(&style->ItemInnerSpacing),
                0.5f,
                0.0f,
                50.0f))
            {
                g_themeDirty = true;
            }

            if (ImGui::DragFloat(
                "IndentSpacing",
                &style->IndentSpacing,
                0.5f,
                0.0f,
                100.0f))
            {
                g_themeDirty = true;
            }

            if (ImGui::DragFloat(
                "ScrollbarSize",
                &style->ScrollbarSize,
                0.5f,
                1.0f,
                50.0f))
            {
                g_themeDirty = true;
            }

            if (ImGui::DragFloat(
                "ScrollbarRounding",
                &style->ScrollbarRounding,
                0.1f,
                0.0f,
                30.0f))
            {
                g_themeDirty = true;
            }

            if (ImGui::DragFloat(
                "GrabMinSize",
                &style->GrabMinSize,
                0.5f,
                1.0f,
                50.0f))
            {
                g_themeDirty = true;
            }

            if (ImGui::DragFloat(
                "GrabRounding",
                &style->GrabRounding,
                0.1f,
                0.0f,
                30.0f))
            {
                g_themeDirty = true;
            }
        }

        if (ImGui::CollapsingHeader("Colors"))
        {
            ImGui::SeparatorText("Text");

            if (DrawColorSetting(
                "Text",
                style->Colors[ImGuiCol_Text]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "TextDisabled",
                style->Colors[ImGuiCol_TextDisabled]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "TextSelectedBg",
                style->Colors[ImGuiCol_TextSelectedBg]))
            {
                g_themeDirty = true;
            }


            ImGui::SeparatorText("Windows");

            if (DrawColorSetting(
                "WindowBg",
                style->Colors[ImGuiCol_WindowBg]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "ChildBg",
                style->Colors[ImGuiCol_ChildBg]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "PopupBg",
                style->Colors[ImGuiCol_PopupBg]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "Border",
                style->Colors[ImGuiCol_Border]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "BorderShadow",
                style->Colors[ImGuiCol_BorderShadow]))
            {
                g_themeDirty = true;
            }


            ImGui::SeparatorText("Frame");

            if (DrawColorSetting(
                "FrameBg",
                style->Colors[ImGuiCol_FrameBg]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "FrameBgHovered",
                style->Colors[ImGuiCol_FrameBgHovered]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "FrameBgActive",
                style->Colors[ImGuiCol_FrameBgActive]))
            {
                g_themeDirty = true;
            }


            ImGui::SeparatorText("Title");

            if (DrawColorSetting(
                "TitleBg",
                style->Colors[ImGuiCol_TitleBg]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "TitleBgCollapsed",
                style->Colors[ImGuiCol_TitleBgCollapsed]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "TitleBgActive",
                style->Colors[ImGuiCol_TitleBgActive]))
            {
                g_themeDirty = true;
            }


            ImGui::SeparatorText("Menu");

            if (DrawColorSetting(
                "MenuBarBg",
                style->Colors[ImGuiCol_MenuBarBg]))
            {
                g_themeDirty = true;
            }


            ImGui::SeparatorText("Scrollbar");

            if (DrawColorSetting(
                "ScrollbarBg",
                style->Colors[ImGuiCol_ScrollbarBg]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "ScrollbarGrab",
                style->Colors[ImGuiCol_ScrollbarGrab]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "ScrollbarGrabHovered",
                style->Colors[ImGuiCol_ScrollbarGrabHovered]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "ScrollbarGrabActive",
                style->Colors[ImGuiCol_ScrollbarGrabActive]))
            {
                g_themeDirty = true;
            }


            ImGui::SeparatorText("Controls");

            if (DrawColorSetting(
                "CheckMark",
                style->Colors[ImGuiCol_CheckMark]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "SliderGrab",
                style->Colors[ImGuiCol_SliderGrab]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "SliderGrabActive",
                style->Colors[ImGuiCol_SliderGrabActive]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "Button",
                style->Colors[ImGuiCol_Button]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "ButtonHovered",
                style->Colors[ImGuiCol_ButtonHovered]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "ButtonActive",
                style->Colors[ImGuiCol_ButtonActive]))
            {
                g_themeDirty = true;
            }


            ImGui::SeparatorText("Headers");

            if (DrawColorSetting(
                "Header",
                style->Colors[ImGuiCol_Header]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "HeaderHovered",
                style->Colors[ImGuiCol_HeaderHovered]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "HeaderActive",
                style->Colors[ImGuiCol_HeaderActive]))
            {
                g_themeDirty = true;
            }


            ImGui::SeparatorText("Resize");

            if (DrawColorSetting(
                "ResizeGrip",
                style->Colors[ImGuiCol_ResizeGrip]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "ResizeGripHovered",
                style->Colors[ImGuiCol_ResizeGripHovered]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "ResizeGripActive",
                style->Colors[ImGuiCol_ResizeGripActive]))
            {
                g_themeDirty = true;
            }


            ImGui::SeparatorText("Plots");

            if (DrawColorSetting(
                "PlotLines",
                style->Colors[ImGuiCol_PlotLines]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "PlotLinesHovered",
                style->Colors[ImGuiCol_PlotLinesHovered]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "PlotHistogram",
                style->Colors[ImGuiCol_PlotHistogram]))
            {
                g_themeDirty = true;
            }

            if (DrawColorSetting(
                "PlotHistogramHovered",
                style->Colors[ImGuiCol_PlotHistogramHovered]))
            {
                g_themeDirty = true;
            }
        }

        if (ImGui::CollapsingHeader("Themes"))
        {
            if (ImGui::Button("+", ImVec2(30, 0)))
            {
                g_newThemeName[0] = '\0';

                g_showCreateThemePopup = true;
            }

            ImGui::SameLine();

            ImGui::Text(
                "Current: %s",
                g_currentThemeName.c_str());

            if (g_showCreateThemePopup)
            {
                ImGui::OpenPopup("Create Theme");

                g_showCreateThemePopup = false;
            }

            if (ImGui::BeginPopup(
                    "Create Theme"))
            {
                ImGui::Text("Theme name");

                ImGui::InputText(
                    "##ThemeName",
                    g_newThemeName,
                    sizeof(g_newThemeName));

                bool enterPressed =
                    ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                    ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);

                if (enterPressed &&
                    g_newThemeName[0] != '\0')
                {
                    CreateTheme(g_newThemeName);
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere(-1);
                }

                if (ImGui::Button("Create"))
                {
                    std::string name =
                        g_newThemeName;

                    if (!name.empty())
                    {
                        CreateTheme(name);

                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            ImGui::Spacing();

            for (const auto& [name, themeStyle] : g_themes)
            {
                ImGui::PushID(name.c_str());

                std::string buttonID =
                    name + "##ThemeButton";

                if (ImGui::Button(buttonID.c_str()))
                {
                    LoadNamedTheme(name);
                }

                // Não desenha o X para o tema protegido
                if (name != "DefaultTheme")
                {
                    ImGui::SameLine(0.0f, 4.0f);

                    if (ImGui::SmallButton("X"))
                    {
                        g_themeToDelete = name;
                        g_showDeleteThemePopup = true;
                    }
                }

                ImGui::PopID();
            }
        }

        if (g_themeDirty &&
            !ImGui::IsAnyItemActive())
        {
            SaveCurrentTheme();

            g_themeDirty = false;
        }
    }

    void DrawPopUps()
    {
        ImDrawList* drawList =
            ImGui::GetForegroundDrawList();

        if (!drawList)
            return;

        // =========================================================
        // COPIED POPUP
        // =========================================================

        if (EffectControl::showCopiedPopup)
        {
            const auto now =
                std::chrono::steady_clock::now();

            const float elapsed =
                std::chrono::duration<float>(
                    now - EffectControl::copiedPopupTime
                ).count();

            if (elapsed >= 1.5f)
            {
                EffectControl::showCopiedPopup = false;
            }
            else
            {
                ImGuiIO& io = ImGui::GetIO();

                const ImVec2 pos(
                    io.MousePos.x + 15.0f,
                    io.MousePos.y + 15.0f
                );

                // ---------------------------------------------
                // Texto
                // ---------------------------------------------

                const std::string prefix = "Copied: ";
                const std::string& value =
                    EffectControl::copiedText;

                ImFont* font = ImGui::GetFont();
                const float fontSize = ImGui::GetFontSize();

                const ImVec2 prefixSize =
                    ImGui::CalcTextSize(prefix.c_str());

                const ImVec2 valueSize =
                    ImGui::CalcTextSize(value.c_str());

                const float paddingX = 10.0f;
                const float paddingY = 7.0f;

                const float width =
                    prefixSize.x +
                    valueSize.x +
                    paddingX * 2.0f;

                const float height =
                    std::max(prefixSize.y, valueSize.y) +
                    paddingY * 2.0f;

                // ---------------------------------------------
                // Fundo
                // ---------------------------------------------

                drawList->AddRectFilled(
                    pos,
                    ImVec2(
                        pos.x + width,
                        pos.y + height
                    ),
                    IM_COL32(25, 25, 25, 235),
                    5.0f
                );

                // Borda
                drawList->AddRect(
                    pos,
                    ImVec2(
                        pos.x + width,
                        pos.y + height
                    ),
                    IM_COL32(60, 60, 60, 255), //cinzaescuro
                    5.0f
                );

                // ---------------------------------------------
                // "Copied:"
                // ---------------------------------------------

                const ImVec2 textPos(
                    pos.x + paddingX,
                    pos.y + paddingY
                );

                drawList->AddText(
                    font,
                    fontSize,
                    textPos,
                    IM_COL32(255, 255, 255, 255),
                    prefix.c_str()
                );

                // ---------------------------------------------
                // Valor copiado
                // ---------------------------------------------

                ImU32 copiedColor;

                if (EffectControl::copiedType ==
                    EffectControl::CopiedType::FormID)
                {
                    copiedColor =
                        IM_COL32(255, 0, 127, 255);
                }
                else
                {
                    copiedColor =
                        IM_COL32(0, 255, 255, 255);
                }

                drawList->AddText(
                    font,
                    fontSize,
                    ImVec2(
                        textPos.x + prefixSize.x,
                        textPos.y
                    ),
                    copiedColor,
                    value.c_str()
                );
            }
        }

        //==========================================================
        // SUBCOPY POPUP
        //==========================================================
        if (EffectControl::showSubCopiedPopup)
        {
            const auto now =
                std::chrono::steady_clock::now();

            const float elapsed =
                std::chrono::duration<float>(
                    now - EffectControl::copiedPopupTime
                ).count();

            if (elapsed >= 1.5f)
            {
                EffectControl::showSubCopiedPopup = false;
            }
            else
            {
                ImGuiIO& io = ImGui::GetIO();

                const ImVec2 pos(
                    io.MousePos.x + 15.0f,
                    io.MousePos.y + 15.0f
                );

                // ---------------------------------------------
                // Texto
                // ---------------------------------------------

                const std::string prefix = "Copied: ";
                const std::string& value =
                    EffectControl::copiedText;

                ImFont* font = ImGui::GetFont();
                const float fontSize = ImGui::GetFontSize();

                const ImVec2 prefixSize =
                    ImGui::CalcTextSize(prefix.c_str());

                const ImVec2 valueSize =
                    ImGui::CalcTextSize(value.c_str());

                const float paddingX = 10.0f;
                const float paddingY = 7.0f;

                const float width =
                    prefixSize.x +
                    valueSize.x +
                    paddingX * 2.0f;

                const float height =
                    std::max(prefixSize.y, valueSize.y) +
                    paddingY * 2.0f;

                // ---------------------------------------------
                // Fundo
                // ---------------------------------------------

                drawList->AddRectFilled(
                    pos,
                    ImVec2(
                        pos.x + width,
                        pos.y + height
                    ),
                    IM_COL32(25, 25, 25, 235),
                    5.0f
                );

                // Borda
                drawList->AddRect(
                    pos,
                    ImVec2(
                        pos.x + width,
                        pos.y + height
                    ),
                    IM_COL32(60, 60, 60, 255), //cinzaescuro
                    5.0f
                );

                // ---------------------------------------------
                // "Copied:"
                // ---------------------------------------------

                const ImVec2 textPos(
                    pos.x + paddingX,
                    pos.y + paddingY
                );

                drawList->AddText(
                    font,
                    fontSize,
                    textPos,
                    IM_COL32(255, 255, 255, 255),
                    prefix.c_str()
                );

                // ---------------------------------------------
                // Valor copiado
                // ---------------------------------------------

                ImU32 copiedColor;

                if (EffectControl::copiedTypeSub ==
                    EffectControl::SubCopiedType::SubFormID)
                {
                    copiedColor =
                        IM_COL32(255, 255, 0, 255);
                }
                else
                {
                    copiedColor =
                        IM_COL32(0, 255, 255, 255);
                }

                drawList->AddText(
                    font,
                    fontSize,
                    ImVec2(
                        textPos.x + prefixSize.x,
                        textPos.y
                    ),
                    copiedColor,
                    value.c_str()
                );
            }
        }

        // =========================================================
        // SCREENSHOT NOTIFICATION
        // =========================================================

        if (g_screenshotNotificationTime > 0.0f)
        {
            g_screenshotNotificationTime -=
                ImGui::GetIO().DeltaTime;

            if (g_screenshotNotificationTime > 0.0f)
            {
                ImGuiIO& io = ImGui::GetIO();

                const ImVec2 mousePos = io.MousePos;

                const ImVec2 pos(
                    mousePos.x + 18.0f,
                    mousePos.y + 18.0f
                );

                ImFont* font = ImGui::GetFont();
                const float fontSize = ImGui::GetFontSize();

                const float paddingX = 10.0f;
                const float paddingY = 7.0f;

                // -------------------------------------------------
                // Monta as linhas
                // -------------------------------------------------

                const std::string notification =
                    g_screenshotNotification;

                const std::size_t datePos =
                    notification.find("Date: ");

                const std::size_t localPos =
                    notification.find("Local: ");

                std::string dateText;

                if (datePos != std::string::npos)
                {
                    const std::size_t dateStart =
                        datePos + 6;

                    const std::size_t dateEnd =
                        notification.find('\n', dateStart);

                    dateText =
                        notification.substr(
                            dateStart,
                            dateEnd == std::string::npos
                                ? std::string::npos
                                : dateEnd - dateStart
                        );
                }

                std::string localPath;

                if (localPos != std::string::npos)
                {
                    const std::size_t pathStart =
                        localPos + 7;

                    localPath =
                        notification.substr(pathStart);
                }

                // -------------------------------------------------
                // Mede o conteúdo
                // -------------------------------------------------

                const ImVec2 savedSize =
                    ImGui::CalcTextSize("Saved!");

                const ImVec2 dateLabelSize =
                    ImGui::CalcTextSize("Date: ");

                const ImVec2 dateValueSize =
                    ImGui::CalcTextSize(dateText.c_str());

                const ImVec2 localLabelSize =
                    ImGui::CalcTextSize("Local: ");

                // Mede o caminho inteiro para determinar a largura.
                const ImVec2 pathSize =
                    ImGui::CalcTextSize(localPath.c_str());

                float contentWidth =
                    std::max(
                        savedSize.x,
                        dateLabelSize.x + dateValueSize.x
                    );

                contentWidth =
                    std::max(
                        contentWidth,
                        localLabelSize.x + pathSize.x
                    );

                const float lineHeight =
                    ImGui::GetTextLineHeight();

                const float contentHeight =
                    lineHeight * 3.0f;

                const float width =
                    contentWidth +
                    paddingX * 2.0f;

                const float height =
                    contentHeight +
                    paddingY * 2.0f;

                // -------------------------------------------------
                // Fundo
                // -------------------------------------------------

                drawList->AddRectFilled(
                    pos,
                    ImVec2(
                        pos.x + width,
                        pos.y + height
                    ),
                    IM_COL32(25, 25, 25, 220),
                    5.0f
                );

                drawList->AddRect(
                    pos,
                    ImVec2(
                        pos.x + width,
                        pos.y + height
                    ),
                    IM_COL32(60, 60, 60, 255),
                    5.0f
                );

                // -------------------------------------------------
                // Saved!
                // -------------------------------------------------

                ImVec2 textPos(
                    pos.x + paddingX,
                    pos.y + paddingY
                );

                drawList->AddText(
                    font,
                    fontSize,
                    textPos,
                    IM_COL32(255, 255, 255, 255),
                    "Saved!"
                );

                // -------------------------------------------------
                // Date:
                // -------------------------------------------------

                textPos.y += lineHeight;

                drawList->AddText(
                    font,
                    fontSize,
                    textPos,
                    IM_COL32(255, 255, 255, 255),
                    "Date: "
                );

                textPos.x += dateLabelSize.x;

                drawList->AddText(
                    font,
                    fontSize,
                    textPos,
                    IM_COL32(255, 255, 255, 255),
                    dateText.c_str()
                );

                // -------------------------------------------------
                // Local:
                // -------------------------------------------------

                textPos.x = pos.x + paddingX;
                textPos.y += lineHeight;

                drawList->AddText(
                    font,
                    fontSize,
                    textPos,
                    IM_COL32(255, 255, 255, 255),
                    "Local: "
                );

                textPos.x += localLabelSize.x;

                // -------------------------------------------------
                // Caminho
                //
                // Disco       = amarelo
                // '\'         = amarelo
                // Pastas      = branco
                // Arquivo     = amarelo
                // -------------------------------------------------

                std::string pathString = localPath;

                std::size_t pathIndex = 0; // Renomeado para evitar conflito com ImVec2 pos
                bool firstPart = true;

                while (pathIndex < pathString.size())
                {
                    const std::size_t slash =
                        pathString.find('\\', pathIndex);

                    const std::size_t end =
                        slash == std::string::npos
                            ? pathString.size()
                            : slash;

                    std::string part =
                        pathString.substr(
                            pathIndex,
                            end - pathIndex
                        );

                    if (!part.empty())
                    {
                        const bool isDrive =
                            firstPart &&
                            part.size() == 2 &&
                            part[1] == ':';

                        const bool isLastPart =
                            slash == std::string::npos;

                        const ImU32 color =
                            (isDrive || isLastPart)
                                ? IM_COL32(255, 255, 0, 255)
                                : IM_COL32(255, 255, 255, 255);

                        drawList->AddText(
                            font,
                            fontSize,
                            textPos,
                            color,
                            part.c_str()
                        );

                        textPos.x +=
                            ImGui::CalcTextSize(
                                part.c_str()
                            ).x;

                        firstPart = false;
                    }

                    if (slash != std::string::npos)
                    {
                        const char separator[] = "\\";

                        drawList->AddText(
                            font,
                            fontSize,
                            textPos,
                            IM_COL32(255, 255, 0, 255),
                            separator
                        );

                        textPos.x +=
                            ImGui::CalcTextSize(separator).x;

                        pathIndex = slash + 1;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }

    

    void ApplyPurpleTheme()
    {
        ImGuiStyle * style = &ImGui::GetStyle();
 
	style->WindowPadding = ImVec2(15, 15);
	style->WindowRounding = 5.0f;
	style->FramePadding = ImVec2(5, 5);
	style->FrameRounding = 4.0f;
	style->ItemSpacing = ImVec2(12, 8);
	style->ItemInnerSpacing = ImVec2(8, 6);
	style->IndentSpacing = 25.0f;
	style->ScrollbarSize = 15.0f;
	style->ScrollbarRounding = 9.0f;
	style->GrabMinSize = 5.0f;
	style->GrabRounding = 3.0f;
 
	style->Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
	style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
	style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
	style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
	style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
    }

    bool menuOpen = false;

    void DrawMenu()
    {
        
        //UpdateIdle();
        EffectDatabase::UpdateImageSpaceModifier();
        


        if (!g_showTestWindow) {
            return;
        }

        
        //EffectDatabase::ApplyPlayerMovementState();
        EffectDatabase::OnOpenImGui();


        //RE::PlayerCharacter* player =
        //RE::PlayerCharacter::GetSingleton();

        //if (!player){
        //    return;
        //}

        //if (player->IsDead())
        //{
        //    g_showTestWindow = false;
        //    return;
        //}

        ImGui::SetNextWindowSize(
            ImVec2(1100.0f, 700.0f),
            ImGuiCond_FirstUseEver
        );

        bool wasOpen = g_showTestWindow;

        ImGui::Begin(
            "Effect Debugger",
            &g_showTestWindow
        );

        if (wasOpen && !g_showTestWindow)
        {
            SaveConfig();
            EffectDatabase::OnCloseImGui();
        }

        //ImGui::Text(
        //    "VisualEffect / EffectShader Debugger"
        //);

        //ImGui::SameLine();

        //if (ImGui::Button("Rescan"))
        //{
        //    Scan();
        //}

        ImVec2 mainPos  = ImGui::GetWindowPos();
        ImVec2 mainSize = ImGui::GetWindowSize();

        //ImGui::SameLine();

        if (EffectControl::g_disablePlayerMovement)
        {
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                IM_COL32(200, 60, 60, 255)
            );
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                IM_COL32(230, 80, 80, 255)
            );
            ImGui::PushStyleColor(
                ImGuiCol_ButtonActive,
                IM_COL32(170, 45, 45, 255)
            );
        }
        else
        {
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                IM_COL32(50, 180, 80, 255)
            );
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                IM_COL32(70, 210, 100, 255)
            );
            ImGui::PushStyleColor(
                ImGuiCol_ButtonActive,
                IM_COL32(40, 150, 65, 255)
            );
        }

        if (ImGui::Button(
                EffectControl::g_disablePlayerMovement
                    ? "Move OFF"
                    : "Move ON"))
        {
            EffectDatabase::TogglePlayerMovement();
            //EffectDatabase::ApplyPlayerMovementState();
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        if (EffectControl::g_savedPlayerActivateState)
        {
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                IM_COL32(200, 60, 60, 255)
            );
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                IM_COL32(230, 80, 80, 255)
            );
            ImGui::PushStyleColor(
                ImGuiCol_ButtonActive,
                IM_COL32(170, 45, 45, 255)
            );
        }
        else
        {
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                IM_COL32(50, 180, 80, 255)
            );
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                IM_COL32(70, 210, 100, 255)
            );
            ImGui::PushStyleColor(
                ImGuiCol_ButtonActive,
                IM_COL32(40, 150, 65, 255)
            );
        }

        if (ImGui::Button(
                EffectControl::g_savedPlayerActivateState
                    ? "Activate OFF"
                    : "Activate ON"))
        {
            EffectDatabase::TogglePlayerActivate();
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        if (ImGui::Button("Screenshot"))
        {
            g_takeScreenshot = true;
        }

        ImGui::SameLine();

        //ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.5f, 0.2f, 0.8f, 1.0f));
        //ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.3f, 0.9f, 1.0f));
        //ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.4f, 0.1f, 0.7f, 1.0f));

        if (g_requestTabFocus &&
            g_tabFocusIndex == static_cast<int>(TabFocus::MainButton))
        {
            ImGui::SetKeyboardFocusHere();
            g_requestTabFocus = false;
        }

        if (ImGui::Button("Config"))
        {
            g_keyConfigPopupPos = ImGui::GetMousePos();
            OpenKeyConfig();
        }

        //ImGui::PopStyleColor(3);

        ImGui::Separator();

        if (ImGui::BeginTable(
            "MainLayout",
            2, 
            ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV
        ))
        {
            // 2. Entre na Coluna 0 ANTES de desenhar os Favoritos
            //ImGui::TableNextColumn();
            DrawFavoritesWindow(mainPos, mainSize);

            // 3. Coluna 1
            ImGui::TableNextColumn();
            DrawEffectBrowser();

            // 4. Coluna 2
            ImGui::TableNextColumn();
            DrawEffectInspector();

            // 5. Só chama EndTable() se BeginTable() retornar true!
            ImGui::EndTable();
        }
        

        ImGui::End();
        
        

        if (g_showKeyConfig)
        {
            ImGui::OpenPopup("Key Configuration");
            g_showKeyConfig = false;
        }

        if (ImGui::BeginPopup(
                "Key Configuration",
                ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Keyboard Configuration");
            ImGui::Separator();

            ImGui::Text("Open Menu Key");

            ImGui::InputText(
                "##OpenMenuKey",
                g_openMenuKeyBuffer,
                sizeof(g_openMenuKeyBuffer)
            );

            ImGui::Spacing();

            ImGui::Text("Toggle Movement Key");

            ImGui::InputText(
                "##MovementToggleKey",
                g_movementKeyBuffer,
                sizeof(g_movementKeyBuffer)
            );

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            DrawThemeCustomizer();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
            {
                ToUpperBuffer(g_openMenuKeyBuffer);
                ToUpperBuffer(g_movementKeyBuffer);

                g_toggleKey =
                    ParseKey(g_openMenuKeyBuffer);

                g_movementToggleKey =
                    ParseKey(g_movementKeyBuffer);

                SaveConfig();

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button(
                    "Cancel",
                    ImVec2(120.0f, 0.0f)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (g_showDeleteThemePopup)
        {
            ImGui::OpenPopup("Delete Theme");
            g_showDeleteThemePopup = false;
        }

        if (ImGui::BeginPopup(
                "Delete Theme",
                ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(
                "Delete theme \"%s\"?",
                g_themeToDelete.c_str());

            ImGui::Spacing();

            if (ImGui::Button(
                    "Delete",
                    ImVec2(100.0f, 0.0f)))
            {
                DeleteTheme(g_themeToDelete);

                g_themeToDelete.clear();

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button(
                    "Cancel",
                    ImVec2(100.0f, 0.0f)))
            {
                g_themeToDelete.clear();

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        DrawPopUps();


    }

}


namespace
{
    using Present_t = HRESULT(__stdcall*)(
        IDXGISwapChain*,
        UINT,
        UINT
    );

    Present_t g_originalPresent = nullptr;


    
    bool g_lastGState = false;

    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_context = nullptr;
    ID3D11RenderTargetView* g_renderTarget = nullptr;

    void ResetKeyboardStateOnMenuToggle()
    {
        ImGuiIO& io = ImGui::GetIO();

        g_UpArrowDown = false;
        g_DownArrowDown = false;

        io.AddKeyEvent(ImGuiKey_UpArrow, false);
        io.AddKeyEvent(ImGuiKey_DownArrow, false);
        io.AddKeyEvent(ImGuiKey_LeftArrow, false);
        io.AddKeyEvent(ImGuiKey_RightArrow, false);

        //io.ClearEventsQueue();
    }


    // ============================================================
    // G KEY
    // ============================================================

    void CheckGKey()
    {

        //auto* player = RE::PlayerCharacter::GetSingleton();

        //if (!player || !player->Is3DLoaded())
        //{
        //    return; // Sai da função se o jogador não existir
        //}

        // Verifica se a tecla configurada em g_toggleKey é uma tecla de função (F1 até F24)
        const bool isFunctionKey = (g_toggleKey >= VK_F1 && g_toggleKey <= VK_F24);

        if (!isFunctionKey && ImGui::GetIO().WantTextInput)
        {
            return;
        }

            const bool currentG =
                 (GetAsyncKeyState(g_toggleKey) & 0x8000) != 0;

            if (currentG && !g_lastGState)
            {

                const bool wasOpen = g_showTestWindow;
                
                g_showTestWindow = !g_showTestWindow;

                if (!wasOpen && g_showTestWindow)
                {
                    if (EffectControl::firstOpen)
                    {
                        EffectControl::focusSearchOnOpen = true;
                        EffectControl::firstOpen = false;
                    }
                    else if (EffectDatabase::selectedEffect)
                    {
                        EffectControl::focusLastEffectOnOpen = true;
                    }
                    else if (!EffectDatabase::selectedEffect)
                    {
                        EffectControl::focusSearchOnOpen = true;
                    }

                }

                if (g_showTestWindow)
                {   
                    
                    ResetKeyboardStateOnMenuToggle();
                    
                }
                else
                {
                    SaveConfig();
                    EffectDatabase::OnCloseImGui();
                }

                Logger::GetSingleton().Print(
                    "Effect Debugger: G PRESSED -> {}",
                    g_showTestWindow
                        ? "OPEN"
                        : "CLOSED"
                );
            }

            g_lastGState = currentG;
    
        
    }

    // ============================================================
    // IMGUI INITIALIZATION
    // ============================================================

    bool InitializeImGui(IDXGISwapChain* swapChain)
    {
        if (g_imguiInitialized)
            return true;

        if (!swapChain)
            return false;

        DXGI_SWAP_CHAIN_DESC desc{};

        if (FAILED(swapChain->GetDesc(&desc)))
        {
            Logger::GetSingleton().Print(
                "ImGui: GetDesc failed"
            );

            return false;
        }

        // PEGAMOS A JANELA DO SKYRIM
        g_gameWindow = desc.OutputWindow;

        Logger::GetSingleton().Print(
            "ImGui: Game HWND = {}",
            reinterpret_cast<std::uintptr_t>(g_gameWindow)
        );

        if (!g_gameWindow)
        {
            Logger::GetSingleton().Print(
                "ImGui: OutputWindow is NULL"
            );

            return false;
        }

        if (FAILED(
            swapChain->GetDevice(
                __uuidof(ID3D11Device),
                reinterpret_cast<void**>(&g_device)
            )))
        {
            return false;
        }

        g_device->GetImmediateContext(&g_context);

        ID3D11Texture2D* backBuffer = nullptr;

        if (FAILED(
            swapChain->GetBuffer(
                0,
                __uuidof(ID3D11Texture2D),
                reinterpret_cast<void**>(&backBuffer)
            )))
        {
            return false;
        }

        HRESULT hr =
            g_device->CreateRenderTargetView(
                backBuffer,
                nullptr,
                &g_renderTarget
            );

        backBuffer->Release();

        if (FAILED(hr))
            return false;

        IMGUI_CHECKVERSION();

        ImGui::CreateContext();
        
            
        ImGuiIO& io = ImGui::GetIO();

        

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        //io.ConfigDebugHighlightIdConflicts = false;
        io.KeyRepeatDelay = 0.30f;
        io.KeyRepeatRate = 0.06f;

        //ImGui::StyleColorsDark();
        //ImGuiStyle& style = ImGui::GetStyle();
        

        //EffectDatabase::ApplyPurpleTheme();
        //    EffectDatabase::LoadThemes();
        //EffectDatabase::LoadCurrentTheme();

        EffectDatabase::LoadDefaultTheme();

        EffectDatabase::LoadThemes();

        EffectDatabase::LoadCurrentTheme();

        if (!InitializeWndProcHook())
        {
            Logger::GetSingleton().Print(
                "ImGui: WndProc hook FAILED"
            );

            //ImGui_ImplDX11_Shutdown();
            //ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            return false;
        }

        if (!ImGui_ImplWin32_Init(g_gameWindow))
        {
            Logger::GetSingleton().Print(
                "ImGui: Win32 Init FAILED"
            );

            ImGui::DestroyContext();
            return false;
        }

        

        if (!ImGui_ImplDX11_Init(
            g_device,
            g_context))
        {
            Logger::GetSingleton().Print(
                "ImGui: DX11 Init FAILED"
            );

            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            return false;
        }

        // AGORA SIM: g_gameWindow já existe
        

        g_imguiInitialized = true;

        Logger::GetSingleton().Print(
            "ImGui: initialized successfully"
        );

        return true;
    }


    // ============================================================
    // TEST WINDOW
    // ============================================================

    void DrawTestWindow()
    {
        if (!g_showTestWindow)
            return;

        ImGui::SetNextWindowSize(
            ImVec2(400.0f, 200.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::Begin(
            "Effect Debugger TEST",
            &g_showTestWindow
        );

        ImGui::Text(
            "SUCCESS!"
        );

        ImGui::Separator();

        ImGui::Text(
            "Present Hook is working."
        );

        ImGui::Text(
            "G key was detected."
        );

        ImGui::Text(
            "ImGui is rendering."
        );

        //if (ImGui::Button("Close"))
        //{
        //    g_showTestWindow = false;

        //    SaveConfig();
        //    EffectDatabase::OnCloseImGui();
        //}

        ImGui::End();
    }


    // ============================================================
    // PRESENT HOOK
    // ============================================================

    HRESULT __stdcall PresentHook(
        IDXGISwapChain* swapChain,
        UINT syncInterval,
        UINT flags)
    {
        static bool logged = false;

        if (!logged)
        {
            logged = true;
            Logger::GetSingleton().Print("Effect Debugger: PRESENT HOOK RUNNING");
        }

        if (!swapChain)
        {
            return g_originalPresent(swapChain, syncInterval, flags);
        }

        // Inicializar se ainda não foi feito
        if (!g_imguiInitialized)
        {
            InitializeImGui(swapChain);
        }

        // Garante que o contexto, dispositivo e render target são 100% válidos antes de tocar no ImGui
        if (g_imguiInitialized && g_device && g_context && g_renderTarget)
        {
            // 1. Inicia o Frame do ImGui com segurança
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // 2. Trata Inputs de Mouse se a janela estiver visível
            if (g_showTestWindow)
            {
                POINT point{};
                if (GetCursorPos(&point))
                {
                    if (g_gameWindow)
                    {
                        ScreenToClient(g_gameWindow, &point);
                        ImGui::GetIO().AddMousePosEvent(
                            static_cast<float>(point.x),
                            static_cast<float>(point.y)
                        );
                    }
                }
            }

            ImGui::GetIO().MouseDrawCursor = g_showTestWindow;

            const float deltaTime = ImGui::GetIO().DeltaTime;

            // Fading do Menu
            if (g_showTestWindow)
            {
                globalAlpha += deltaTime * fadeSpeed;
                if (globalAlpha > 1.0f) globalAlpha = 1.0f;
            }
            else
            {
                globalAlpha -= deltaTime * fadeSpeed;
                if (globalAlpha < 0.0f) globalAlpha = 0.0f;
            }

            // Renderiza o menu apenas se o Alpha for maior que zero para economizar draw calls
            if (globalAlpha > 0.0f)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, globalAlpha);
                EffectDatabase::DrawMenu();
                ImGui::PopStyleVar();
            }

            // 3. Conclui o cálculo de vértices do ImGui
            ImGui::Render();

            // 4. Salva o RenderTarget anterior do Skyrim para NÃO corromper os outros mods (OAR/IED)
            ID3D11RenderTargetView* oldRenderTargetViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
            ID3D11DepthStencilView* oldDepthStencilView = nullptr;
            g_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRenderTargetViews, &oldDepthStencilView);

            // Define o nosso RenderTarget do ImGui
            g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            // RESTAURA o RenderTarget do jogo/outros mods
            g_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRenderTargetViews, oldDepthStencilView);

            // Libera os ponteiros de com obtidos pelo OMGetRenderTargets
            for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            {
                if (oldRenderTargetViews[i]) oldRenderTargetViews[i]->Release();
            }
            if (oldDepthStencilView) oldDepthStencilView->Release();

            // 5. Screenshot Logic
            if (g_takeScreenshot)
            {
                g_takeScreenshot = false;
                ID3D11Texture2D* backBuffer = nullptr;

                const HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

                if (SUCCEEDED(hr) && backBuffer)
                {
                    const auto path = MakeScreenshotPath();
                    if (!path.empty())
                    {
                        const bool saved = SaveTextureToPNG(g_device, g_context, backBuffer, path);
                        if (saved)
                        {
                            SYSTEMTIME time{};
                            GetLocalTime(&time);
                            char dateTime[64]{};
                            sprintf_s(dateTime, "%02d/%02d/%04d %02d:%02d:%02d",
                                time.wDay, time.wMonth, time.wYear,
                                time.wHour, time.wMinute, time.wSecond);

                            g_screenshotNotification = std::string("Saved!\nDate: ") + dateTime + "\nLocal: " + path.string();
                            g_screenshotNotificationTime = 5.0f;
                        }
                        else
                        {
                            g_screenshotNotification = "Error";
                            g_screenshotNotificationTime = 3.0f;
                        }
                    }
                    else
                    {
                        g_screenshotNotification = "Error: no folder.";
                        g_screenshotNotificationTime = 3.0f;
                    }
                    backBuffer->Release();
                }
                else
                {
                    g_screenshotNotification = "Error in backbuffer.";
                    g_screenshotNotificationTime = 3.0f;
                }
            }
        }

        return g_originalPresent(swapChain, syncInterval, flags);
    }


    // ============================================================
    // NOVO PRESENT HOOK
    // ============================================================

    bool InitializePresentHook()
    {
        Logger::GetSingleton().Print(
            "Effect Debugger: Initializing DX11 Present hook..."
        );

        WNDCLASSEXA wc{};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "EffectDebuggerDummy";

        const ATOM atom = RegisterClassExA(&wc);

        if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            Logger::GetSingleton().Print(
                "Effect Debugger: RegisterClassExA FAILED: {}",
                static_cast<unsigned>(GetLastError())
            );

            return false;
        }

        HWND hwnd = CreateWindowExA(
            0,
            wc.lpszClassName,
            "EffectDebuggerDummy",
            WS_OVERLAPPEDWINDOW,
            0,
            0,
            100,
            100,
            nullptr,
            nullptr,
            wc.hInstance,
            nullptr
        );

        if (!hwnd)
        {
            Logger::GetSingleton().Print(
                "Effect Debugger: CreateWindow FAILED: {}",
                static_cast<unsigned>(GetLastError())
            );

            return false;
        }

        DXGI_SWAP_CHAIN_DESC swapDesc{};
        swapDesc.BufferCount = 1;
        swapDesc.BufferDesc.Width = 100;
        swapDesc.BufferDesc.Height = 100;
        swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.OutputWindow = hwnd;
        swapDesc.SampleDesc.Count = 1;
        swapDesc.Windowed = TRUE;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        IDXGISwapChain* swapChain = nullptr;

        D3D_FEATURE_LEVEL featureLevel{};

        Logger::GetSingleton().Print(
            "Effect Debugger: Creating temporary DX11 device..."
        );

        const HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &swapDesc,
            &swapChain,
            &device,
            &featureLevel,
            &context
        );

        if (FAILED(hr))
        {
            Logger::GetSingleton().Print(
                "Effect Debugger: D3D11CreateDeviceAndSwapChain FAILED: {:08X}",
                static_cast<unsigned>(hr)
            );

            if (context)
                context->Release();

            if (device)
                device->Release();

            if (swapChain)
                swapChain->Release();

            DestroyWindow(hwnd);

            if (atom)
            {
                UnregisterClassA(
                    wc.lpszClassName,
                    wc.hInstance
                );
            }

            return false;
        }

        if (!swapChain)
        {
            Logger::GetSingleton().Print(
                "Effect Debugger: swapChain is NULL"
            );

            if (context)
                context->Release();

            if (device)
                device->Release();

            DestroyWindow(hwnd);

            if (atom)
            {
                UnregisterClassA(
                    wc.lpszClassName,
                    wc.hInstance
                );
            }

            return false;
        }

        //
        // IDXGISwapChain::Present = vtable[8]
        //
        void** vtable =
            *reinterpret_cast<void***>(swapChain);

        if (!vtable)
        {
            Logger::GetSingleton().Print(
                "Effect Debugger: SwapChain vtable is NULL"
            );

            if (context)
                context->Release();

            if (device)
                device->Release();

            swapChain->Release();

            DestroyWindow(hwnd);

            if (atom)
            {
                UnregisterClassA(
                    wc.lpszClassName,
                    wc.hInstance
                );
            }

            return false;
        }

        void* presentAddress = vtable[8];

        Logger::GetSingleton().Print(
            "Effect Debugger: Present address = {}",
            presentAddress
        );

        if (!presentAddress)
        {
            Logger::GetSingleton().Print(
                "Effect Debugger: Present address is NULL"
            );

            if (context)
                context->Release();

            if (device)
                device->Release();

            swapChain->Release();

            DestroyWindow(hwnd);

            if (atom)
            {
                UnregisterClassA(
                    wc.lpszClassName,
                    wc.hInstance
                );
            }

            return false;
        }

        //
        // IMPORTANT:
        // Não tenta criar o hook novamente.
        //
        Logger::GetSingleton().Print(
            "Effect Debugger: BEFORE MH_CreateHook"
        );

        MH_STATUS status = MH_CreateHook(
            presentAddress,
            reinterpret_cast<void*>(&PresentHook),
            reinterpret_cast<void**>(&g_originalPresent)
        );

        Logger::GetSingleton().Print(
            "Effect Debugger: AFTER MH_CreateHook = {}",
            static_cast<int>(status)
        );

        if (status != MH_OK)
        {
            Logger::GetSingleton().Print(
                "Effect Debugger: MH_CreateHook FAILED: {}",
                static_cast<int>(status)
            );

            if (context)
                context->Release();

            if (device)
                device->Release();

            swapChain->Release();

            DestroyWindow(hwnd);

            if (atom)
            {
                UnregisterClassA(
                    wc.lpszClassName,
                    wc.hInstance
                );
            }

            return false;
        }

        Logger::GetSingleton().Print(
            "Effect Debugger: BEFORE MH_EnableHook"
        );

        status = MH_EnableHook(presentAddress);

        Logger::GetSingleton().Print(
            "Effect Debugger: AFTER MH_EnableHook = {}",
            static_cast<int>(status)
        );

        if (status != MH_OK)
        {
            Logger::GetSingleton().Print(
                "Effect Debugger: MH_EnableHook FAILED: {}",
                static_cast<int>(status)
            );

            MH_RemoveHook(presentAddress);

            if (context)
                context->Release();

            if (device)
                device->Release();

            swapChain->Release();

            DestroyWindow(hwnd);

            if (atom)
            {
                UnregisterClassA(
                    wc.lpszClassName,
                    wc.hInstance
                );
            }

            return false;
        }

        if (context)
            context->Release();

        if (device)
            device->Release();

        swapChain->Release();

        DestroyWindow(hwnd);

        if (atom)
        {
            UnregisterClassA(
                wc.lpszClassName,
                wc.hInstance
            );
        }

        Logger::GetSingleton().Print(
            "Effect Debugger: PRESENT HOOK INITIALIZED"
        );

        return true;
    }


    // ============================================================
    // MINHOOK
    // ============================================================

    bool InitializeMinHook()
    {
        static bool initialized = false;

        if (initialized)
            return true;

        MH_STATUS status =
            MH_Initialize();

        if (status == MH_OK ||
            status == MH_ERROR_ALREADY_INITIALIZED)
        {
            initialized = true;

            Logger::GetSingleton().Print(
                "Effect Debugger: MINHOOK INITIALIZED"
            );

            return true;
        }

        Logger::GetSingleton().Print(
            "Effect Debugger: MINHOOK FAILED: {}",
            static_cast<int>(status)
        );

        return false;
    }
}



void VerifySurvivalMode()
{
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (dataHandler)
    {
        // Substitua "x.esl" pelo nome exato do arquivo do mod
        const auto* modInfo = dataHandler->LookupLoadedModByName("ccQDRSSE001-SurvivalMode.esl");

        if (modInfo)
        {
            ModIntegrations::g_isSurvivorPluginLoaded = true;
            //SKSE::log::info("Plugin x.esl detectado com sucesso! Recursos ativados.");
        }
        else
        {
            ModIntegrations::g_isSurvivorPluginLoaded = false;
            //SKSE::log::info("Plugin x.esl nao encontrado. Recursos desativados.");
        }
    }
}

// ================================================================
// SKSE LOAD
// ================================================================

SKSEPluginLoad(
    const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    auto* messaging =
        SKSE::GetMessagingInterface();

    if (!messaging)
    {
        return false;
    }

    messaging->RegisterListener(
        [](SKSE::MessagingInterface::Message* message)
        {
            if (!message)
                return;

            switch (message->type)
            {
            case SKSE::MessagingInterface::kDataLoaded:

                Logger::GetSingleton().Initialize();
                
                Logger::GetSingleton().Print(
                    "SKSEPluginLoad called"
                );

                RegisterInputSink();

                EffectDatabase::BuildEditorIDCache();

                EffectDatabase::Scan();
                
                ProcessInputQueueHook::Install();
                LoadConfig();

                //VerifySurvivalMode();
                
            case SKSE::MessagingInterface::kPreLoadGame:
            
                //EffectControl::ResetRuntimeState();
            

            case SKSE::MessagingInterface::kPostLoadGame:
            {
                Logger::GetSingleton().Print(
                    "Effect Debugger: SAVE LOADED"
                );

                static bool hooksInitialized = false;

                if (!hooksInitialized)
                {
                    if (!InitializeMinHook())
                    {
                        Logger::GetSingleton().Print(
                            "Effect Debugger: MinHook initialization FAILED"
                        );

                        hooksInitialized = true;
                        break;
                    }

                    if (!InitializePresentHook())
                    {
                        Logger::GetSingleton().Print(
                            "Effect Debugger: Present hook initialization FAILED"
                        );

                        // Não deixa uma falha do Present Hook derrubar o Skyrim.
                        hooksInitialized = true;

                        break;
                    }

                    hooksInitialized = true;

                    Logger::GetSingleton().Print(
                        "Effect Debugger: READY - PRESS G"
                    );
                }

                break;
            }

            case SKSE::MessagingInterface::kPostLoad:

                g_postLoadGame = true;

                Logger::GetSingleton().Print(
                    "POST LOAD GAME TRUE"
                );

                break;
            }
            
        }
    );

    return true;
}