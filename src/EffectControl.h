#pragma once

#include <chrono>

namespace EffectControl
{   
    enum class CopiedType
    {
        FormID,
        EditorID
    };

    enum class SubCopiedType
    {
        SubFormID,
        SubEditorID
    };

    inline CopiedType copiedType =
        CopiedType::FormID;

    inline SubCopiedType copiedTypeSub =
        SubCopiedType::SubFormID;

    inline bool g_disablePlayerMovement = false;

    inline bool g_disablePlayerActivate = false;
    inline bool g_savedPlayerActivateState = false;

    inline std::vector<RE::ModelReferenceEffect*> g_activeVisualEffects;
    inline std::vector<RE::ShaderReferenceEffect*> g_activeEffectShaders;

    // Visual Effect atualmente selecionado
    inline int selectedIndex = 0;
    inline bool draggingFavorite = false;
    inline int draggedFavoriteIndex = -1;

    // Duração do efeito
    inline float duration = 1.0f;

    // E está sendo segurado?
    inline bool eHeld = false;

    inline bool focusSearch = false;
    inline bool focusSearchOnOpen = false;
    inline bool searchOpenedWithFocus = false;

    inline bool firstOpen = true;
    inline bool focusLastEffectOnOpen = false;

    inline bool copyUseEditorID = false;

    inline bool g_isPlayerInvulnerable = false;

    inline bool showCopiedPopup = false;
    inline bool showSubCopiedPopup = false;
    inline std::string copiedText;
    inline std::chrono::steady_clock::time_point copiedPopupTime;

    // Guarda os valores de base e atuais antes de ativar o God Mode
    inline float g_savedBaseHealth = 100.0f;
    inline float g_savedBaseMagicka = 100.0f;
    inline float g_savedBaseStamina = 100.0f;

    inline float g_savedHealth = 100.0f;
    inline float g_savedMagicka = 100.0f;
    inline float g_savedStamina = 100.0f;

    inline float imageSpaceDuration = 5.0f;
    inline float imageSpaceRemaining = 0.0f;
    inline bool imageSpaceActive = false;
    inline RE::TESImageSpaceModifier* activeImageSpaceModifier = nullptr;


    inline bool g_isSpellApplied = false;

    inline float idleDuration = 5.0f;

    inline bool idleActive = false;

    inline std::chrono::steady_clock::time_point idleStartTime;

    inline std::chrono::steady_clock::time_point lastApplyTime =
        std::chrono::steady_clock::now();

        void ResetRuntimeState();

}