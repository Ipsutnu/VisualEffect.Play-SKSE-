#include "PCH.h"
#include "EffectControl.h"
//reseta o estado do efeito
namespace EffectControl
{
    void ResetRuntimeState()
    {
        // Fecha o menu
        g_showTestWindow = false;

        // Limpa seleção
        EffectDatabase::selectedEffect = nullptr;
        EffectDatabase::selectedType = EffectType::None;

    }
}
