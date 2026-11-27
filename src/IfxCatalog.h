#pragma once

#include "EffectCatalog.h"

int getIfxEffectCount();
const EffectInfo& getIfxEffectInfo (int index);
std::unique_ptr<IEngine> createIfxEngine (int index);
std::unique_ptr<IEngine> createIfxDepopEngine (int index);
