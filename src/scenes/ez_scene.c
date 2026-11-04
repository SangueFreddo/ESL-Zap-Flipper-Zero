/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 */
#include "ez_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const ez_on_enter[])(void*) = {
#include "ez_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const ez_on_event[])(void* context, SceneManagerEvent event) = {
#include "ez_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const ez_on_exit[])(void* context) = {
#include "ez_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers ez_scene_handlers = {
    .on_enter_handlers = ez_on_enter,
    .on_event_handlers = ez_on_event,
    .on_exit_handlers = ez_on_exit,
    .scene_num = EzSceneCount,
};
