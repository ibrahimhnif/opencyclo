#include "template_engine.h"

static const TemplateSlotDefinition s_templates[TEMPLATE_COUNT] = {
  // 0: TEMPLATE_HERO_6_GRID
  {
    TEMPLATE_HERO_6_GRID,
    "Hero 6-Grid",
    6,
    {
      {4, 28, 232, 94},   // Slot 0: Hero
      {4, 126, 114, 60},  // Slot 1: Mid Left
      {122, 126, 114, 60},// Slot 2: Mid Right
      {4, 190, 74, 54},   // Slot 3: Bottom Left
      {83, 190, 74, 54},  // Slot 4: Bottom Center
      {162, 190, 74, 54}  // Slot 5: Bottom Right
    },
    true,
    {4, 248, 232, 50}    // Action Button
  },

  // 1: TEMPLATE_4_GRID
  {
    TEMPLATE_4_GRID,
    "4-Grid Symmetric",
    4,
    {
      {4, 28, 114, 130},  // Slot 0: Top Left
      {122, 28, 114, 130},// Slot 1: Top Right
      {4, 164, 114, 134}, // Slot 2: Bottom Left
      {122, 164, 114, 134}// Slot 3: Bottom Right
    },
    false,
    {0, 0, 0, 0}
  },

  // 2: TEMPLATE_2_GRID_CHART
  {
    TEMPLATE_2_GRID_CHART,
    "2-Grid + Chart",
    3,
    {
      {4, 28, 114, 74},   // Slot 0: Top Left
      {122, 28, 114, 74}, // Slot 1: Top Right
      {4, 106, 232, 192}  // Slot 2: Chart Area
    },
    false,
    {0, 0, 0, 0}
  },

  // 3: TEMPLATE_8_GRID
  {
    TEMPLATE_8_GRID,
    "8-Grid Pro View",
    8,
    {
      {4, 28, 114, 64},   {122, 28, 114, 64},  // Row 1
      {4, 96, 114, 64},   {122, 96, 114, 64},  // Row 2
      {4, 164, 114, 64},  {122, 164, 114, 64}, // Row 3
      {4, 232, 114, 64},  {122, 232, 114, 64}  // Row 4
    },
    false,
    {0, 0, 0, 0}
  },

  // 4: TEMPLATE_FULL_CONTAINER
  {
    TEMPLATE_FULL_CONTAINER,
    "Full Container",
    1,
    {
      {4, 28, 232, 274}   // Slot 0: Full Screen Box
    },
    false,
    {0, 0, 0, 0}
  }
};

const TemplateSlotDefinition& getTemplateDefinition(LayoutTemplateId templateId) {
  if (templateId < TEMPLATE_COUNT) {
    return s_templates[templateId];
  }
  return s_templates[TEMPLATE_FULL_CONTAINER];
}
