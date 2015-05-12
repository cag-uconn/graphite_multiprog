#pragma once

#include "pin.H"

int spawnThreadSpawner(const CONTEXT* ctxt);
int callThreadSpawner(const CONTEXT* ctxt);
void setupCarbonSpawnThreadSpawnerStack(const CONTEXT* ctxt);
void setupCarbonThreadSpawnerStack(const CONTEXT* ctxt);
