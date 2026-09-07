#pragma once

enum class EffectType
{
    None,
    VisualEffect,
    EffectShader,
    Explosion,
    Spell,
    Idle,
    ImageSpaceModifier
};

enum class TabFocus
{
    Effects = 0,
    SelectedEffectItem,
    Inspector,
    Favorites,
    MainButton    
};

namespace EffectDatabase
{
    struct EffectEntry
    {
        RE::TESForm* form = nullptr;
        std::string name;
        RE::FormID formID = 0;
        std::string modName;
        //std::string editorID;
    };

    struct FavoriteEffect
    {
        std::uint32_t formID;
        EffectType type;
        std::string name;
    };

    inline std::vector<FavoriteEffect> favorites;
    inline std::vector<FavoriteEffect> recentEffects;

    extern std::vector<EffectEntry> visualEffects;
    extern std::vector<EffectEntry> effectShaders;
    extern std::vector<EffectEntry> explosions;
    extern std::vector<EffectEntry> spells;
    extern std::vector<EffectEntry> idles;
    extern std::vector<EffectEntry> imageSpaceModifiers;

    void SelectEffect(
        EffectEntry* effect,
        EffectType type
    );

    void ToggleFavorite(
        const EffectEntry* effect,
        EffectType type);

    bool IsFavorite(
        std::uint32_t formID,
        EffectType type);

    void AddRecentEffect(
        const EffectEntry* effect,
        EffectType type);
    

    EffectEntry* GetEffectByFormID(std::uint32_t formID, EffectType type);

    void SelectEffectByFormID(
        std::uint32_t formID,
        EffectType type);
    
    void ApplyPlayerMovementState();

    extern std::unordered_map<RE::TESForm*, std::string> g_editorIDCache;
    void BuildEditorIDCache();

    std::string GetPluginName(RE::TESForm* form);
    std::string GetEditorIDFromForm(RE::TESForm* target);
    std::string GetFormName(RE::TESForm* form);

    void ApplySelectedImageSpaceModifier();
    void StopSelectedImageSpaceModifier();
    void UpdateImageSpaceModifier();
    
    void TogglePlayerMovement();
    void TogglePlayerActivate();
    void SetPlayerActivateEnabled(bool a_enable);

    void OnOpenImGui();
    void OnCloseImGui();

   // std::map<std::string, std::vector<EffectEntry*>> GroupByPlugin();


    extern EffectEntry* selectedEffect;
    extern EffectType selectedType;

    extern float effectDuration;
    extern float visualEffectDuration;
    extern bool showVisualEffects;
    extern bool showEffectShaders;
    extern bool showExplosions;
    extern bool showSpells;
    extern bool showIdles;
    extern bool showImagespaceModifiers;


    void CopyTextToClipboard(const std::string& text);

    void BuildIdleDatabaseCache();

    const char* GetSpellTypeName(RE::MagicSystem::SpellType type);
    const char* GetCastingTypeName(RE::MagicSystem::CastingType type);
    const char* GetDeliveryName(RE::MagicSystem::Delivery type);

    void SetPlayerInvulnerable(bool a_enable);
    void TogglePlayerInvulnerable();

    void AddSelectedSpellToPlayer();
    void RemoveSelectedSpellFromPlayer();
    void CopySelectedEffectToClipboard();

    void ApplySelectedIdle();
    void StopPlayerIdle();
    void UpdateIdle();
    void ScheduleIdleUpdate();

    void SelectVisualEffect(int index);
    void ApplySelectedEffect();

    // Aplicações específicas
    void ApplySelectedVisualEffect();
    void ApplySelectedEffectShader();
    void ApplySelectedExplosion();
}