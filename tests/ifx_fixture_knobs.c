/* Plain C access to libifx's fixture knob tables for the C++ catalog test. */
#include "ifx_effect.h"

#include <string.h>

unsigned ifx_fixture_knob_count(const char *name);
int ifx_fixture_knob(const char *name, unsigned index, int *min, int *max, int *def);

#define DECLARE(effect) extern const ifx_effect ifx_effect_##effect;
IFX_EFFECTS(DECLARE)
#undef DECLARE

static const ifx_effect *find(const char *name) {
#define FIND(effect)                                                                               \
  if (strcmp(name, #effect) == 0) {                                                                \
    return &ifx_effect_##effect;                                                                   \
  }
  IFX_EFFECTS(FIND)
#undef FIND
  return NULL;
}

unsigned ifx_fixture_knob_count(const char *name) {
  const ifx_effect *effect = find(name);
  return effect != NULL ? effect->knobs : 0u;
}

int ifx_fixture_knob(const char *name, unsigned index, int *min, int *max, int *def) {
  const ifx_effect *effect = find(name);
  if (effect == NULL || index >= effect->knobs) {
    return -1;
  }
  *min = effect->knob[index].min;
  *max = effect->knob[index].max;
  *def = effect->knob[index].def;
  return 0;
}
