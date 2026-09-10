#include "template_engine.h"

static const TemplateSlotDefinition s_templates[TEMPLATE_COUNT] = {
  // 0: TEMPLATE_HERO_6_GRID
  {
    TEMPLATE_HERO_6_GRID,
    "Hero 6-Grid",
    6,
    {
      {{4, 44, 232, 78}, SIZE_HERO},    // Slot 0: Hero below the 44px header
      {{4, 126, 114, 60}, SIZE_MEDIUM}, // Slot 1: Mid Left
      {{122, 126, 114, 60}, SIZE_MEDIUM}, // Slot 2: Mid Right
      {{4, 190, 74, 54}, SIZE_SMALL},   // Slot 3: Bottom Left
      {{83, 190, 74, 54}, SIZE_SMALL},  // Slot 4: Bottom Center
      {{162, 190, 74, 54}, SIZE_SMALL}  // Slot 5: Bottom Right
    },
    true,
    {4, 248, 232, 50}
  },

  // 1: TEMPLATE_4_GRID (not used by any default page; kept for future custom layouts)
  {
    TEMPLATE_4_GRID,
    "4-Grid Symmetric",
    4,
    {
      {{4, 44, 114, 114}, SIZE_MEDIUM},
      {{122, 44, 114, 114}, SIZE_MEDIUM},
      {{4, 164, 114, 134}, SIZE_MEDIUM},
      {{122, 164, 114, 134}, SIZE_MEDIUM}
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
      {{4, 44, 114, 58}, SIZE_SMALL},
      {{122, 44, 114, 58}, SIZE_SMALL},
      {{4, 106, 232, 192}, SIZE_LARGE}
    },
    false,
    {0, 0, 0, 0}
  },

  // 3: TEMPLATE_8_GRID (not used by any default page; kept for future custom layouts)
  {
    TEMPLATE_8_GRID,
    "8-Grid Pro View",
    8,
    {
      {{4, 44, 114, 48}, SIZE_SMALL},   {{122, 44, 114, 48}, SIZE_SMALL},
      {{4, 96, 114, 64}, SIZE_SMALL},   {{122, 96, 114, 64}, SIZE_SMALL},
      {{4, 164, 114, 64}, SIZE_SMALL},  {{122, 164, 114, 64}, SIZE_SMALL},
      {{4, 232, 114, 64}, SIZE_SMALL},  {{122, 232, 114, 64}, SIZE_SMALL}
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
      {{4, 44, 232, 258}, SIZE_FULL}
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
