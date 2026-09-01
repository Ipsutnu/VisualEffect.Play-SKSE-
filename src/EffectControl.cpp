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
#include "EffectControl.h"

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