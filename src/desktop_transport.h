#pragma once
#include <cstdio>

struct DesktopTransport;
DesktopTransport* DesktopTransport_Create(int width, int height, FILE* log);
// 4 像素参数：rh/incl/roll/fullRh；中心UV/temp/inner；outer/opac/dopp/beam；gain/contr/wind/speed。
bool DesktopTransport_Render(DesktopTransport* transport, unsigned program, unsigned desktop,
                             int width, int height, double seconds);
unsigned DesktopTransport_Texture(const DesktopTransport* transport);
unsigned DesktopTransport_StateTexture(const DesktopTransport* transport);
float DesktopTransport_FormulaMix(const DesktopTransport* transport);
void DesktopTransport_Destroy(DesktopTransport*& transport);
