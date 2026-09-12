#include "formula_texture.h"
#include <cwchar>
#include <cstring>

GLuint CreateFormulaTexture() {
    constexpr int width = 1024, height = 1024;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return 0;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HFONT font = CreateFontW(-31, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Cambria Math");
    if (!bitmap || !bits || !font) {
        if (font) DeleteObject(font);
        if (bitmap) DeleteObject(bitmap);
        DeleteDC(dc);
        return 0;
    }
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    HGDIOBJ oldFont = SelectObject(dc, font);
    std::memset(bits, 0, width * height * 4);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(245, 245, 245));
    const wchar_t* equations[] = {
        L"G\u03bc\u03bd + \u039b g\u03bc\u03bd = 8\u03c0G T\u03bc\u03bd / c\u2074",
        L"E = mc\u00b2     r\u209b = 2GM / c\u00b2",
        L"i\u210f \u2202\u03a8/\u2202t = \u0124\u03a8",
        L"S = k\u0299 c\u00b3 A / 4G\u210f",
        L"\u2207\u00b7E = \u03c1/\u03b5\u2080     \u2207\u00b7B = 0",
        L"\u0394x \u0394p \u2265 \u210f/2",
        L"R\u03bc\u03bd - \u00bd R g\u03bc\u03bd = \u03ba T\u03bc\u03bd",
        L"e\u2071\u03c0 + 1 = 0    \u222b f(x) dx",
        L"ds\u00b2 = -c\u00b2dt\u00b2 + dx\u00b2",
        L"T\u2095 = \u210fc\u00b3 / 8\u03c0GMk\u0299",
        L"\u03a9\u00b2 = GM / r\u00b3",
        L"\u2202\u03c1/\u2202t + \u2207\u00b7(\u03c1v) = 0"
    };
    for (int row = 0; row < 20; ++row) {
        for (int column = 0; column < 2; ++column) {
            const wchar_t* text = equations[(row * 5 + column * 7) % 12];
            TextOutW(dc, column * 512 + 12 + (row % 3) * 9, row * 51 + 7,
                     text, static_cast<int>(std::wcslen(text)));
        }
    }
    GdiFlush();
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bits);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // 纹理名字非零不代表显存分配成功，失败时让调用方退回基础效果。
    if (glGetError() != GL_NO_ERROR) {
        if (texture) glDeleteTextures(1, &texture);
        texture = 0;
    }
    SelectObject(dc, oldFont);
    SelectObject(dc, oldBitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return texture;
}
