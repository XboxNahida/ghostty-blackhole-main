#pragma once

#include <cstdio>

struct BloomRenderer;

BloomRenderer* Bloom_Create(int width, int height, FILE* debugLog);
bool Bloom_BeginScene(BloomRenderer* bloom, int width, int height, bool enabled);
void Bloom_EndScene(BloomRenderer* bloom, int width, int height, bool enabled);
void Bloom_Destroy(BloomRenderer*& bloom);
