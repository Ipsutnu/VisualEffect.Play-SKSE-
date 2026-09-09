#include "PCH.h"

#include "EffectDatabase.h"
#include "EffectControl.h"
#include "Logger.h"

class BSReadLockGuard
{
public:
    explicit BSReadLockGuard(RE::BSReadWriteLock& lock) :
        _lock(lock)
    {
        _lock.LockForRead();
    }

    ~BSReadLockGuard()
    {
        _lock.UnlockForRead();
    }

    BSReadLockGuard(const BSReadLockGuard&) = delete;
    BSReadLockGuard& operator=(const BSReadLockGuard&) = delete;

private:
    RE::BSReadWriteLock& _lock;
};

namespace EffectDatabase
{
    float effectDuration = 10.0f;
    //float visualEffectDuration = 5.0f;
    bool showVisualEffects = true;
    bool showEffectShaders = true;
    bool showExplosions = true;
    bool showSpells = true;
    bool showIdles = true;
    bool showImagespaceModifiers = true;

    std::unordered_map<RE::TESForm*, std::string>
        EffectDatabase::g_editorIDCache;

    std::vector<EffectEntry> visualEffects;
    std::vector<EffectEntry> effectShaders;
    std::vector<EffectEntry> explosions;
    std::vector<EffectEntry> spells;
    std::vector<EffectEntry> idles;
    std::vector<EffectEntry> imageSpaceModifiers;

    std::string GetPluginName(RE::TESForm* form)
    {
        if (!form) {
            return "<Unknown>";
        }

        auto* file = form->GetFile();

        if (!file) {
            return "<Unknown>";
        }

        return std::string(file->GetFilename());
    }

    void BuildEditorIDCache()
    {
        g_editorIDCache.clear();

        auto [forms, lock] =
            RE::TESForm::GetAllFormsByEditorID();

        if (!forms)
            return;

        BSReadLockGuard guard(lock.get());

        g_editorIDCache.reserve(forms->size());

        for (const auto& [editorID, form] : *forms)
        {
            if (!form)
                continue;

            if (editorID.empty())
                continue;

            g_editorIDCache.emplace(
                form,
                editorID.c_str());
        }
    }

    std::string GetEditorIDFromForm(
        RE::TESForm* form)
    {
        if (!form)
            return "";

        auto it = g_editorIDCache.find(form);

        if (it != g_editorIDCache.end())
        {
            return it->second;
        }

        return "";
    }

    std::string GetFormName(RE::TESForm* form)
    {
        if (!form)
            return "";

        // Primeiro tenta Editor ID
        const std::string editorID =
            GetEditorIDFromForm(form);

        if (!editorID.empty())
        {
            return editorID;
        }

        // Depois tenta nome normal
        if (const char* name = form->GetName();
            name && name[0] != '\0')
        {
            return name;
        }

        return "";
    }

    RE::Actor* GetEffectTarget()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nullptr;

        auto* crosshair = RE::CrosshairPickData::GetSingleton();
        if (!crosshair)
            return player;

        RE::ObjectRefHandle targetHandle = crosshair->target[0];
        if (!targetHandle)
            return player;

        auto targetPtr = targetHandle.get();
        if (!targetPtr)
            return player;

        // Se o objeto no crosshair for um Actor (NPC), retorna ele
        if (auto* actor = targetPtr->As<RE::Actor>())
            return actor;

        // Se estiver mirando em um objeto comum (baú, porta, etc.) ou no nada, retorna o player
        return player;
    }

    const char* GetSpellTypeName(RE::MagicSystem::SpellType type)
{
    using T = RE::MagicSystem::SpellType;

    switch (type)
    {
    case T::kSpell:          return "Spell";
    case T::kDisease:        return "Disease";
    case T::kPower:          return "Power";
    case T::kLesserPower:    return "Lesser Power";
    case T::kAbility:        return "Ability";
    case T::kPoison:         return "Poison";
    case T::kAddiction:      return "Addiction";
    case T::kVoicePower:          return "Voice";
    case T::kStaffEnchantment:return "Staff Enchantment";
    case T::kScroll:         return "Scroll";
    default:                 return "Unknown";
    }
}

    const char* GetCastingTypeName(
        RE::MagicSystem::CastingType type)
    {
        using T = RE::MagicSystem::CastingType;

        switch (type)
        {
        case T::kConcentration:
            return "Concentration";

        case T::kFireAndForget:
            return "Fire and Forget";

        case T::kConstantEffect:
            return "Constant Effect";

        default:
            return "Unknown";
        }
    }

    const char* GetDeliveryName(
        RE::MagicSystem::Delivery type)
    {
        using T = RE::MagicSystem::Delivery;

        switch (type)
        {
        case T::kSelf:
            return "Self";

        case T::kTouch:
            return "Touch";

        case T::kAimed:
            return "Aimed";

        case T::kTargetActor:
            return "Target";

        case T::kTargetLocation:
            return "Target Location";

        default:
            return "Unknown";
        }
    }

    

    // Invulneravel... nao utilizado, ainda.
    void SetPlayerInvulnerable(bool a_enable)
    {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        auto avOwner = player->As<RE::ActorValueOwner>();
        if (!avOwner) return;

        if (a_enable) {
            // 1. Salva os valores de Base e Atuais da vida, mana e stamina
            EffectControl::g_savedBaseHealth = avOwner->GetBaseActorValue(RE::ActorValue::kHealth);
            EffectControl::g_savedBaseMagicka = avOwner->GetBaseActorValue(RE::ActorValue::kMagicka);
            EffectControl::g_savedBaseStamina = avOwner->GetBaseActorValue(RE::ActorValue::kStamina);

            EffectControl::g_savedHealth = avOwner->GetActorValue(RE::ActorValue::kHealth);
            EffectControl::g_savedMagicka = avOwner->GetActorValue(RE::ActorValue::kMagicka);
            EffectControl::g_savedStamina = avOwner->GetActorValue(RE::ActorValue::kStamina);

            // 2. Eleva o valor Base para 100.000
            avOwner->SetBaseActorValue(RE::ActorValue::kHealth, 100000.0f);
            avOwner->SetBaseActorValue(RE::ActorValue::kMagicka, 100000.0f);
            avOwner->SetBaseActorValue(RE::ActorValue::kStamina, 100000.0f);

            // 3. Restaura os 3 atributos para encher as barras instantaneamente até 100.000
            avOwner->RestoreActorValue(RE::ActorValue::kHealth, 100000.0f);
            avOwner->RestoreActorValue(RE::ActorValue::kMagicka, 100000.0f);
            avOwner->RestoreActorValue(RE::ActorValue::kStamina, 100000.0f);

        } else {
            // 1. Restaura os valores Base originais
            avOwner->SetBaseActorValue(RE::ActorValue::kHealth, EffectControl::g_savedBaseHealth);
            avOwner->SetBaseActorValue(RE::ActorValue::kMagicka, EffectControl::g_savedBaseMagicka);
            avOwner->SetBaseActorValue(RE::ActorValue::kStamina, EffectControl::g_savedBaseStamina);

            // 2. Limpa o dano para resetar a vida atual até o limite máximo restaurado
            avOwner->RestoreActorValue(RE::ActorValue::kHealth, 100000.0f);
            avOwner->RestoreActorValue(RE::ActorValue::kMagicka, 100000.0f);
            avOwner->RestoreActorValue(RE::ActorValue::kStamina, 100000.0f);
        }

        EffectControl::g_isPlayerInvulnerable = a_enable;
    }
    //toggle para o emsmo...
    void TogglePlayerInvulnerable()
    {
        SetPlayerInvulnerable(!EffectControl::g_isPlayerInvulnerable);
    }

    void AddSelectedSpellToPlayer()
    {
        if (!selectedEffect)
            return;

        if (selectedType != EffectType::Spell)
            return;

        RE::Actor* target = GetEffectTarget();

        if (!target) {
            Logger::GetSingleton().Print("Target actor not available");
            return;
        }

        auto* spell =
            selectedEffect->form->As<RE::SpellItem>();

        if (!spell)
            return;

        if (target->HasSpell(spell))
        {
            Logger::GetSingleton().Print(
                "Target actor already has spell: {}",
                selectedEffect->name
            );

            return;
        }

        if (target && spell) 
        {
            if (target->AddSpell(spell)) 
            {
                const char* spellName = spell->GetName();
                const char* targetName = target->GetDisplayFullName(); // Retorna o nome visível do Actor/NPC

                // Fallback caso algum dos nomes venha nulo ou vazio
                if (!spellName || strlen(spellName) == 0) spellName = "Spell";
                if (!targetName || strlen(targetName) == 0) targetName = "Target";

                std::string text = std::format("{} added to {}", spellName, targetName);
                //RE::DebugNotification(text.c_str());
                RE::SendHUDMessage::ShowHUDMessage(text.c_str());
            }
        }

        Logger::GetSingleton().Print(
            "Added spell: {}",
            selectedEffect->name
        );
    }

    void RemoveSelectedSpellFromPlayer()
    {
        if (!selectedEffect)
            return;

        if (selectedType != EffectType::Spell)
            return;

        RE::Actor* target = GetEffectTarget();

        if (!target) {
            Logger::GetSingleton().Print("Target actor not available");
            return;
        }

        auto* spell =
            selectedEffect->form->As<RE::SpellItem>();

        if (!spell)
            return;

        if (!target->HasSpell(spell))
        {
            Logger::GetSingleton().Print(
                "Target actor does not have spell: {}",
                selectedEffect->name
            );

            return;
        }

        if (target && spell) 
        {
            if (target->RemoveSpell(spell)) 
            {
                const char* spellName = spell->GetName();
                const char* targetName = target->GetDisplayFullName(); // Retorna o nome visível do Actor/NPC

                // Fallback caso algum dos nomes venha nulo ou vazio
                if (!spellName || strlen(spellName) == 0) spellName = "Spell";
                if (!targetName || strlen(targetName) == 0) targetName = "Target";

                std::string text = std::format("{} removed from {}", spellName, targetName);
                //RE::DebugNotification(text.c_str());
                RE::SendHUDMessage::ShowHUDMessage(text.c_str());
            }
        }

        //player->RemoveSpell(
        //    spell
        //);

        Logger::GetSingleton().Print(
            "Removed spell: {}",
            selectedEffect->name
        );
    }

    void CopyTextToClipboard(const std::string& text)
    {
        if (text.empty())
            return;

        if (OpenClipboard(nullptr))
        {
            EmptyClipboard();

            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
            if (hMem)
            {
                void* ptr = GlobalLock(hMem);
                if (ptr)
                {
                    std::memcpy(ptr, text.c_str(), text.size() + 1);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_TEXT, hMem);

                    // Notificação de tela
                    EffectControl::copiedText = text;
                    EffectControl::showSubCopiedPopup = true;
                    EffectControl::copiedPopupTime = std::chrono::steady_clock::now();
                }
                else
                {
                    GlobalFree(hMem);
                }
            }

            CloseClipboard();

            Logger::GetSingleton().Print("Copied to clipboard: {}", text);
        }
    }

    void CopySelectedEffectToClipboard()
    {
        if (!selectedEffect)
            return;

        auto* effect =
            selectedEffect;

        std::string text;

        if (EffectControl::copyUseEditorID)
        {
            //procurar o nome/Editor ID
            text = GetEditorIDFromForm(effect->form);

            if (text.empty())
            {
                Logger::GetSingleton().Print(
                    "Could not find Editor ID for form {:08X}",
                    effect->formID
                );

                return;
            }
        }
        else
        {
            char buffer[16];

            std::snprintf(
                buffer,
                sizeof(buffer),
                "%08X",
                effect->formID
            );

            text = buffer;
        }

        if (OpenClipboard(nullptr))
        {
            EmptyClipboard();

            HGLOBAL hMem =
                GlobalAlloc(
                    GMEM_MOVEABLE,
                    text.size() + 1
                );

            if (hMem)
            {
                void* ptr =
                    GlobalLock(hMem);

                if (ptr)
                {
                    std::memcpy(
                        ptr,
                        text.c_str(),
                        text.size() + 1
                    );

                    GlobalUnlock(hMem);

                    SetClipboardData(
                        CF_TEXT,
                        hMem
                    );

                    EffectControl::copiedText =
                        text;

                    EffectControl::showCopiedPopup = true;

                    EffectControl::copiedPopupTime =
                        std::chrono::steady_clock::now();
                }
                else
                {
                    GlobalFree(hMem);
                }
            }

            CloseClipboard();

            Logger::GetSingleton().Print(
                "Copied to clipboard: {}",
                text
            );
        }
    }

    struct IdleData {
        RE::TESIdleForm* form;
        std::string name;
        RE::FormID formID;
        std::string pluginName;
    };

    std::vector<IdleData> cachedIdles;

    // UMA VEZ ao inicializar a janela/menu!
    void BuildIdleDatabaseCache()
    {
        cachedIdles.clear();

        auto [forms, lock] = RE::TESForm::GetAllForms();
        if (!forms)
            return;

        RE::BSReadLockGuard guard(lock);

        cachedIdles.reserve(2000); // Evita realocações contínuas de vetor

        for (auto& [formID, form] : *forms)
        {
            if (!form || !form->Is(RE::FormType::Idle))
                continue;

            auto* idle = form->As<RE::TESIdleForm>();
            if (!idle)
                continue;

            // TESIdleForm usa EditorID para identificação no CK/SKSE
            const char* edid = idle->GetFormEditorID();
            std::string name = (edid && edid[0] != '\0') ? edid : "<No EditorID>";

            cachedIdles.push_back({
                idle,
                std::move(name),
                idle->GetFormID(),
                GetPluginName(idle)
            });
        }
    }

    void ApplySelectedIdle()
    {
        if (!selectedEffect)
            return;

        if (selectedType != EffectType::Idle)
            return;

        auto* player =
            RE::PlayerCharacter::GetSingleton();

        if (!player)
            return;

        auto* idle =
            selectedEffect->form
                ->As<RE::TESIdleForm>();

        if (!idle)
        {
            Logger::GetSingleton().Print(
                "Selected form is not a TESIdleForm"
            );

            return;
        }

        auto* process =
            player->GetActorRuntimeData().currentProcess;

        if (!process)
        {
            Logger::GetSingleton().Print(
                "Player has no AIProcess"
            );

            return;
        }

        const bool result = process->PlayIdle(player, idle, player);

        if (!result)
        {
            Logger::GetSingleton().Print(
                "PlayIdle failed: {}",
                selectedEffect->name
            );

            return;
        }

        EffectControl::idleActive = true;

        EffectControl::idleStartTime =
            std::chrono::steady_clock::now();

        Logger::GetSingleton().Print(
            "Idle applied: {}",
            selectedEffect->name
        );
    }

    void StopPlayerIdle()
    {
        auto* player =
            RE::PlayerCharacter::GetSingleton();

        if (!player)
            return;

        auto* idleStop =
            RE::TESForm::LookupByEditorID<RE::TESIdleForm>(
                "IdleStop_Loose"
            );

        if (!idleStop)
        {
            Logger::GetSingleton().Print(
                "Could not find IdleStop_Loose"
            );

            return;
        }

        auto* process =
            player->GetActorRuntimeData().currentProcess;

        if (!process)
        {
            Logger::GetSingleton().Print(
                "Player has no AIProcess"
            );

            return;
        }

        process->PlayIdle(
            player,
            idleStop,
            nullptr
        );

        EffectControl::idleActive = false;

        Logger::GetSingleton().Print(
            "Idle stopped"
        );
    }

    void UpdateIdle()
    {
        if (!EffectControl::idleActive)
            return;

        const auto now =
            std::chrono::steady_clock::now();

        const float elapsed =
            std::chrono::duration<float>(
                now - EffectControl::idleStartTime
            ).count();

        if (elapsed >= EffectControl::idleDuration)
        {
            StopPlayerIdle();
        }
    }

    void ScheduleIdleUpdate()
    {
        SKSE::GetTaskInterface()->AddTask(
            []()
            {
                UpdateIdle();

                ScheduleIdleUpdate();
            }
        );
    }

    void ApplySelectedImageSpaceModifier()
    {
        if (!selectedEffect)
            return;

        auto* modifier =
            selectedEffect->form->As<RE::TESImageSpaceModifier>();

        if (!modifier)
            return;

        // Se já existe um ImageSpace ativo,
        // para o anterior antes de aplicar o novo.
        if (EffectControl::activeImageSpaceModifier)
        {
            RE::ImageSpaceModifierInstanceForm::Stop(
                EffectControl::activeImageSpaceModifier
            );
        }

        RE::ImageSpaceModifierInstanceForm::Trigger(
            modifier,
            1.0f,
            nullptr
        );

        // GUARDA qual modifier foi realmente aplicado
        EffectControl::activeImageSpaceModifier = modifier;

        EffectControl::imageSpaceRemaining =
            EffectDatabase::effectDuration;

        EffectControl::imageSpaceActive = true;

        Logger::GetSingleton().Print(
            "Applied ImageSpace Modifier {:08X} for {:.2f}s",
            modifier->GetFormID(),
            EffectDatabase::effectDuration
        );
    }

    void StopSelectedImageSpaceModifier()
    {
        auto* modifier =
            EffectControl::activeImageSpaceModifier;

        if (!modifier)
            return;

        RE::ImageSpaceModifierInstanceForm::Stop(
            modifier
        );

        EffectControl::activeImageSpaceModifier = nullptr;
        EffectControl::imageSpaceRemaining = 0.0f;
        EffectControl::imageSpaceActive = false;

        Logger::GetSingleton().Print(
            "Stopped ImageSpace Modifier"
        );
    }

    void UpdateImageSpaceModifier()
    {
        if (!EffectControl::imageSpaceActive)
            return;

        EffectControl::imageSpaceRemaining -=
            ImGui::GetIO().DeltaTime;

        if (EffectControl::imageSpaceRemaining <= 0.0f)
        {
            EffectControl::imageSpaceRemaining = 0.0f;
            EffectControl::imageSpaceActive = false;

            auto* modifier =
                EffectControl::activeImageSpaceModifier;

            if (modifier)
            {
                RE::ImageSpaceModifierInstanceForm::Stop(
                    modifier
                );

                Logger::GetSingleton().Print(
                    "ImageSpace Modifier duration expired {:08X}",
                    modifier->GetFormID()
                );
            }

            // limpa o ponteiro depois do Stop.
            EffectControl::activeImageSpaceModifier = nullptr;
        }
    }

    bool g_canApplyMovementLock = false;

    void ApplyPlayerMovementState()
    {

        if (!g_canApplyMovementLock)
        {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();

        // 1. Verifica se o jogador existe e se o 3D está carregado
        if (!player || !player->Is3DLoaded())
        {
            return;
        }

        auto* controlMap = RE::ControlMap::GetSingleton();
        if (!controlMap)
            return;

        // ToggleControls aceita a flag de movimento e se ela deve estar ativa.
        // Se g_disablePlayerMovement for TRUE, passamos FALSE para desligar os controles.
        controlMap->ToggleControls(
            RE::ControlMap::UEFlag::kMovement,
            !EffectControl::g_disablePlayerMovement,
            true
        );

        Logger::GetSingleton().Print(
            "Player movement applied: {}",
            EffectControl::g_disablePlayerMovement ? "DISABLED" : "ENABLED"
        );
    }

    // O botão da UI chama essa função, que apenas inverte e aplica
    void TogglePlayerMovement()
    {
        EffectControl::g_disablePlayerMovement = !EffectControl::g_disablePlayerMovement;

        

        //ApplyPlayerMovementState();
    }

    //void SetPlayerActivateEnabled(bool a_enable)
    //{
    //    auto* playerControls = RE::PlayerControls::GetSingleton();
    //    if (!playerControls || !playerControls->activateHandler)
    //        return;

        // Se a_enable for true -> disabled = false
        // Se a_enable for false -> disabled = true
    //    playerControls->activateHandler->disabled = !a_enable;
    //}



    void OnOpenImGui()
    {

        auto* player = RE::PlayerCharacter::GetSingleton();

        // 1. Verifica se o jogador existe e se o 3D está carregado
        if (!player || !player->Is3DLoaded())
        {
            return; // Sai se o jogador ainda não existir ou o 3D não tiver carregado
        }

        //if (ModIntegrations::g_isSurvivorPluginLoaded)
        //{

            // 2. Controla o tempo desde que o 3D foi carregado
            //using Clock = std::chrono::steady_clock;
            //static auto loadTime = Clock::now();
            //static bool wasLoaded = false;

            // Se é a primeira vez que detectamos o 3D carregado (ou se resetou)
            //if (!wasLoaded)
            //{
            //loadTime = Clock::now();
            //wasLoaded = true;
            //}

            // Calculamos quantos segundos se passaram desde a detecção do 3D
            //auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - loadTime).count();

            // 3. Se ainda não passaram 30 segundos, bloqueia a execução
            //if (elapsed < 10)
            //{
            //    return;
            //}

            

            //auto* ui = RE::UI::GetSingleton();

            //if ((ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) || (ui && ui->IsApplicationMenuOpen()))
            //{
            //    return;
            //}
        //}

        //g_canApplyMovementLock = true;

        //auto* controlMap = RE::ControlMap::GetSingleton();
        //if (!controlMap) return;

        // Se a flag estiver marcada como true (desativado), bloqueia o movimento
        //if (EffectControl::g_disablePlayerMovement) {
        //    controlMap->ToggleControls(RE::ControlMap::UEFlag::kMovement, false);
        //}
        //EffectControl::g_movementLockActive =
        //EffectControl::g_disablePlayerMovement;

        if (EffectControl::g_savedPlayerActivateState) {
            EffectControl::g_disablePlayerActivate = true;
        }
    }

    void OnCloseImGui()
    {
        //auto* controlMap = RE::ControlMap::GetSingleton();
        //if (!controlMap) return;

        // Garante que o movimento sempre volte ao fechar o menu,
        // sem alterar o valor de EffectControl::g_disablePlayerMovement
        //controlMap->ToggleControls(RE::ControlMap::UEFlag::kMovement, true);
        //EffectControl::g_movementLockActive = false;

        EffectControl::g_disablePlayerActivate = false;
    }

    void TogglePlayerActivate()
    {
        // Inverte a preferência salva do usuário
        EffectControl::g_savedPlayerActivateState = !EffectControl::g_savedPlayerActivateState;

        // Aplica a mudança imediatamente no estado de execução
        EffectControl::g_disablePlayerActivate = EffectControl::g_savedPlayerActivateState;

        Logger::GetSingleton().Print(
            "Player Activate Block: {}",
            EffectControl::g_savedPlayerActivateState ? "ENABLED" : "DISABLED"
        );
    }

    EffectEntry* selectedEffect = nullptr;
    EffectType selectedType = EffectType::None;

    void SelectEffect(
        EffectEntry* effect,
        EffectType type)
    {
        if (!effect)
            return;

        selectedEffect = effect;
        selectedType = type;

        auto& effects = GetEffectsForType(type);

        auto it = std::find_if(
            effects.begin(),
            effects.end(),
            [effect](const EffectEntry& entry)
            {
                return &entry == effect;
            });

        if (it != effects.end())
        {
            EffectControl::selectedIndex =
                static_cast<int>(
                    std::distance(effects.begin(), it)
                );
        }
    }

    void ApplyOrRemoveSpell()
    {
        if (EffectControl::g_isSpellApplied) {
            RemoveSelectedSpellFromPlayer();
            EffectControl::g_isSpellApplied = false;
        } else {
            AddSelectedSpellToPlayer();
            EffectControl::g_isSpellApplied = true;
        }
    }

    void ApplySelectedEffect()
    {
            if (!selectedEffect)
            return;

        // Guarda como RECENTE somente quando realmente for aplicado
        AddRecentEffect(
            selectedEffect,
            selectedType
        );

        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player || !player->Is3DLoaded())
        {
            return; // Sai da função se o jogador não existir
        }
        
        switch (selectedType)
        {
        case EffectType::VisualEffect:
            ApplySelectedVisualEffect();
            break;

        case EffectType::EffectShader:
            ApplySelectedEffectShader();
            break;

        case EffectType::Explosion:
            ApplySelectedExplosion();
            break;

        case EffectType::Spell:
            ApplyOrRemoveSpell();
            break;
        
        case EffectType::Idle:
            ApplySelectedIdle();
            break;

        case EffectType::ImageSpaceModifier:
            ApplySelectedImageSpaceModifier();
            break;

        case EffectType::None:
        default:
            break;
        }
    }

    void ApplySelectedVisualEffect()
    {
        if (!selectedEffect) {
            return;
        }

        if (selectedType != EffectType::VisualEffect) {
            return;
        }

        RE::Actor* target = GetEffectTarget();

        if (!target) {
            Logger::GetSingleton().Print("Target actor not available");
            return;
        }

        auto* visualEffect =
            selectedEffect->form->As<RE::BGSReferenceEffect>();

        if (!visualEffect) {
            Logger::GetSingleton().Print(
                "Failed to cast selected form to BGSReferenceEffect"
            );
            return;
        }

        auto* artObject = visualEffect->data.artObject;

        if (!artObject) {
            Logger::GetSingleton().Print(
                "Visual Effect has no Art Object"
            );
            return;
        }

        auto* effect = target->ApplyArtObject(
            artObject,
            effectDuration
        );

        if (effect)
        {
            EffectControl::g_activeVisualEffects.push_back(effect);
            
            Logger::GetSingleton().Print(
                "EffectShader applied successfully"
            );
        }
        else
        {
            Logger::GetSingleton().Print(
                "Failed to apply Visual Effect"
            );
            return;
        }

        Logger::GetSingleton().Print(
            "Visual Effect applied: %s | Duration: %.2f",
            GetFormName(visualEffect).c_str(),
            effectDuration
        );
    }


    void ApplySelectedExplosion()
    {
        if (!selectedEffect) {
            return;
        }

        if (selectedType != EffectType::Explosion) {
            return;
        }

        RE::Actor* target = GetEffectTarget();

        if (!target) {
            Logger::GetSingleton().Print("Target actor not available");
            return;
        }

        auto* explosion =
            selectedEffect->form->As<RE::BGSExplosion>();

        if (!explosion) {
            Logger::GetSingleton().Print(
                "Failed to cast selected form to BGSExplosion"
            );

            return;
        }

        target->PlaceObjectAtMe(explosion, false);

        Logger::GetSingleton().Print(
            "Explosion placed at player"
        );
    }

    void ApplySelectedEffectShader()
    {
        if (!selectedEffect) {
            return;
        }

        if (selectedType != EffectType::EffectShader) {
            Logger::GetSingleton().Print(
                "Selected effect is not an EffectShader"
            );

            return;
        }

        RE::Actor* target = GetEffectTarget();

        if (!target) {
            Logger::GetSingleton().Print("Target actor not available");
            return;
        }

        auto* shader = selectedEffect->form->As<RE::TESEffectShader>();

        if (!shader) {
            return;
        }

        RE::TESObjectREFR* objeto = target;

        auto* result = objeto->ApplyEffectShader(
            shader,
            effectDuration
        );

        if (result)
        {
            EffectControl::g_activeEffectShaders.push_back(result);
            
            Logger::GetSingleton().Print(
                "EffectShader applied successfully"
            );
        }
        else
        {
            Logger::GetSingleton().Print(
                "ApplyEffectShader returned nullptr"
            );
        }
    }



    bool IsFavorite(
        std::uint32_t formID,
        EffectType type)
    {
        for (const auto& favorite : favorites)
        {
            if (favorite.formID == formID &&
                favorite.type == type)
            {
                return true;
            }
        }

        return false;
    }

    void ToggleFavorite(
        const EffectEntry* effect,
        EffectType type)
    {
        if (!effect)
            return;

        for (auto it = favorites.begin();
            it != favorites.end();
            ++it)
        {
            if (it->formID == effect->formID &&
                it->type == type)
            {
                favorites.erase(it);
                return;
            }
        }

        favorites.push_back({
            effect->formID,
            type,
            effect->name
        });
    }

    void AddRecentEffect(
        const EffectEntry* effect,
        EffectType type)
    {
        if (!effect)
            return;

        // Se já estava nos recentes, remove
        recentEffects.erase(
            std::remove_if(
                recentEffects.begin(),
                recentEffects.end(),
                [&](const FavoriteEffect& recent)
                {
                    return recent.formID == effect->formID &&
                        recent.type == type;
                }),
            recentEffects.end()
        );

        // Coloca o último aplicado no topo
        recentEffects.insert(
            recentEffects.begin(),
            {
                effect->formID,
                type,
                effect->name
            }
        );

        // Máximo de 10
        if (recentEffects.size() > 10)
        {
            recentEffects.resize(10);
        }
    }
    // Funçao auxiliar do menu de favoritos
    EffectEntry* GetEffectByFormID(std::uint32_t formID, EffectType type)
    {
        auto& effects = GetEffectsForType(type);
        
        auto it = std::find_if(
            effects.begin(),
            effects.end(),
            [formID](const EffectEntry& entry)
            {

                return entry.formID == formID; 
            });

        if (it != effects.end())
        {
            return &(*it); // Retorna o ponteiro do EffectEntry encontrado
        }

        return nullptr;
    }
    // Funçao auxiliar do menu principal
    void SelectEffectByFormID(
        std::uint32_t formID,
        EffectType type)
    {
        auto& effects =
            GetEffectsForType(type);

        for (auto& effect : effects)
        {
            if (effect.formID == formID)
            {
                selectedEffect = &effect;
                selectedType = type;

                EffectControl::focusLastEffectOnOpen = true;

                return;
            }
        }
    }

   //==================================================

}
