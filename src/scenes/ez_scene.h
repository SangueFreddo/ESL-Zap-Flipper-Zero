/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) EzScene##id,
typedef enum {
#include "ez_scene_config.h"
    EzSceneCount,
} EzScene;
#undef ADD_SCENE

extern const SceneManagerHandlers ez_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "ez_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "ez_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "ez_scene_config.h"
#undef ADD_SCENE
