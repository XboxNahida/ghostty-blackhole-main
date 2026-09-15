#pragma once

#include <cstdio>

struct BloomRenderer;
struct RippleState;

BloomRenderer* Bloom_Create(int width, int height, FILE* debugLog);
bool Bloom_BeginScene(BloomRenderer* bloom, int width, int height, bool enabled);
void Bloom_EndScene(BloomRenderer* bloom, int width, int height, bool enabled,
                    const RippleState* ripples = nullptr, double now = 0.0, bool applyBloom = true);
void Bloom_Destroy(BloomRenderer*& bloom);
