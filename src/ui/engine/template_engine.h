#ifndef OPENCYCLO_UI_ENGINE_TEMPLATE_ENGINE_H
#define OPENCYCLO_UI_ENGINE_TEMPLATE_ENGINE_H

#include "widget_types.h"

enum LayoutTemplateId : uint8_t {
  TEMPLATE_HERO_6_GRID = 0,
  TEMPLATE_4_GRID,
  TEMPLATE_2_GRID_CHART,
  TEMPLATE_8_GRID,
  TEMPLATE_FULL_CONTAINER,
  TEMPLATE_COUNT
};

struct TemplateSlotDefinition {
  LayoutTemplateId id;
  const char* name;
  uint8_t max_slots;
  Rect slot_rects[8];
  bool has_action_button;
  Rect action_button_rect;
};

const TemplateSlotDefinition& getTemplateDefinition(LayoutTemplateId templateId);

#endif // OPENCYCLO_UI_ENGINE_TEMPLATE_ENGINE_H
