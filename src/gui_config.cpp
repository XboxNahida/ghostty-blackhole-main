// gui_config.cpp  ImGui config panel with preset editing
#include "gui_config.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstring>
#include <cstdio>

static const DiskPreset DEFAULT_PRESETS[8] = {
    {5500, 1.50f, 0.35f, 1.8f, 8.0f, 0.90f, 0.60f, 2.5f, 2.2f, 1.6f, 7.0f, 5.0f, 1.40f, 0.0f},
    {4500, 1.52f, 0.10f, 2.2f, 7.0f, 0.85f, 0.35f, 2.0f, 1.4f, 0.5f, 7.0f, 5.0f, 1.20f, 0.0f},
    {3800, 0.55f,-0.30f, 2.2f, 6.0f, 0.45f, 0.90f, 3.5f, 1.6f, 0.4f, 3.0f, 2.5f, 1.10f, 0.0f},
    {6500, 0.30f, 0.00f, 3.0f,10.0f, 0.50f, 0.80f, 2.5f, 1.0f, 1.1f, 7.0f, 5.0f, 1.00f, 0.0f},
    {15000,1.30f, 0.35f, 3.0f,14.0f, 0.35f, 1.00f, 4.0f, 1.2f, 1.3f, 8.0f, 5.0f, 0.80f, 0.0f},
    {18000,1.05f, 0.55f, 3.0f,16.0f, 0.30f, 1.00f, 5.0f, 1.0f, 1.5f, 9.0f, 6.0f, 0.75f, 0.0f},
    {5500, 1.50f, 0.35f, 1.8f, 8.0f, 0.00f, 1.00f, 2.5f, 0.0f, 1.6f, 7.0f, 5.0f, 1.00f, 0.6f},
    {5500, 1.50f, 0.35f, 1.8f, 8.0f, 0.90f, 0.60f, 2.5f, 2.2f, 1.6f, 7.0f, 5.0f, 1.40f, 0.0f},
};

void InitDefaultPresets(BlackholeConfig& cfg) {
    cfg.presetCount = 8;
    for (int i = 0; i < 8; i++) cfg.presets[i] = DEFAULT_PRESETS[i];
}

static const char* PRESET_NAMES[8] = {
    "Inferno", "Gargantua", "M87* Donut", "Face-on Ember",
    "Quasar", "Blazar", "Pure Lens", "Inferno 2"
};

bool GUI_ShowConfigPanel(BlackholeConfig& cfg) {
    if (cfg.presetCount == 0) InitDefaultPresets(cfg);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    GLFWwindow* win = glfwCreateWindow(900, 620, "Black Hole Config", nullptr, nullptr);
    if (!win) return true;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    static const ImWchar ranges[] = { 0x0020, 0x00FF, 0x4E00, 0x9FFF, 0 };
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 16.0f, nullptr, ranges);
    io.Fonts->Build();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    char presetName[16][64] = {};
    for (int i = 0; i < cfg.presetCount && i < 8; i++)
        strcpy(presetName[i], PRESET_NAMES[i]);
    for (int i = 8; i < cfg.presetCount; i++)
        snprintf(presetName[i], 64, "Custom %d", i + 1);

    int selPreset = 0;
    const char* modes[] = { "始终显示", "空闲检测" };
    bool done = false;

    while (!glfwWindowShouldClose(win) && !done) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(900, 620));
        ImGui::Begin("黑洞设置", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        // Left panel: preset list + mode
        ImGui::BeginChild("left", ImVec2(260, 560), true);
        ImGui::TextColored(ImVec4(0.4f,0.7f,1.0f,1.0f), "轮播预设");

        // Mode
        ImGui::Separator();
        ImGui::Text("模式:"); ImGui::SameLine();
        ImGui::Combo("##mode", &cfg.mode, modes, 2);
        ImGui::Checkbox("自定义轮播", &cfg.useCustomPresets);
        if (cfg.mode == 1) {
            ImGui::SliderInt("超时(秒)", &cfg.idleSec, 10, 1800);
        }

        ImGui::Separator();
        ImGui::Text("预设列表 (%d个)", cfg.presetCount);

        for (int i = 0; i < cfg.presetCount; i++) {
            char label[64];
            snprintf(label, 64, "%d. %s", i+1, presetName[i]);
            if (ImGui::Selectable(label, selPreset == i))
                selPreset = i;
        }

        ImGui::Spacing();
        if (ImGui::Button("+ 添加", ImVec2(80,25)) && cfg.presetCount < 16) {
            cfg.presets[cfg.presetCount] = cfg.presets[selPreset];
            snprintf(presetName[cfg.presetCount], 64, "Custom %d", cfg.presetCount+1);
            cfg.presetCount++;
        }
        ImGui::SameLine();
        if (ImGui::Button("- 删除", ImVec2(80,25)) && cfg.presetCount > 1) {
            for (int i = selPreset; i < cfg.presetCount - 1; i++) {
                cfg.presets[i] = cfg.presets[i+1];
                strcpy(presetName[i], presetName[i+1]);
            }
            cfg.presetCount--;
            if (selPreset >= cfg.presetCount) selPreset = cfg.presetCount - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("上移", ImVec2(40,25)) && selPreset > 0) {
            DiskPreset tmp = cfg.presets[selPreset];
            cfg.presets[selPreset] = cfg.presets[selPreset-1];
            cfg.presets[selPreset-1] = tmp;
            char tn[64]; strcpy(tn, presetName[selPreset]);
            strcpy(presetName[selPreset], presetName[selPreset-1]);
            strcpy(presetName[selPreset-1], tn);
            selPreset--;
        }
        ImGui::SameLine();
        if (ImGui::Button("下移", ImVec2(40,25)) && selPreset < cfg.presetCount-1) {
            DiskPreset tmp = cfg.presets[selPreset];
            cfg.presets[selPreset] = cfg.presets[selPreset+1];
            cfg.presets[selPreset+1] = tmp;
            char tn[64]; strcpy(tn, presetName[selPreset]);
            strcpy(presetName[selPreset], presetName[selPreset+1]);
            strcpy(presetName[selPreset+1], tn);
            selPreset++;
        }

        if (ImGui::Button("恢复默认", ImVec2(120,30))) {
            InitDefaultPresets(cfg);
            for (int i = 0; i < 8; i++) strcpy(presetName[i], PRESET_NAMES[i]);
            cfg.presetCount = 8;
            selPreset = 0;
        }
        ImGui::EndChild();

        // Right panel: edit selected preset
        ImGui::SameLine();
        ImGui::BeginChild("right", ImVec2(620, 560), true);

        DiskPreset& p = cfg.presets[selPreset];
        char pname[64];
        snprintf(pname, 64, "编辑: %s", presetName[selPreset]);
        ImGui::TextColored(ImVec4(0.4f,0.7f,1.0f,1.0f), "%s", pname);

        // Name editor
        ImGui::InputText("名称", presetName[selPreset], 64);
        ImGui::Separator();

        // Parameter grid (2 columns)
        ImGui::SliderFloat("色温 (K)",  &p.temp,  1500, 40000, "%.0f");
        ImGui::SliderFloat("盘面倾角",  &p.incl,  0.0f, 1.57f, "%.2f");
        ImGui::SliderFloat("盘面旋转",  &p.roll, -1.57f, 1.57f, "%.2f");
        ImGui::SliderFloat("内半径",    &p.inner, 1.6f, 10.0f, "%.1f");
        ImGui::SliderFloat("外半径",    &p.outer, 3.0f, 30.0f, "%.1f");
        ImGui::SliderFloat("不透明度",  &p.opac,  0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("多普勒",    &p.dopp,  0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("光束指数",  &p.beam,  1.0f, 8.0f, "%.1f");
        ImGui::SliderFloat("亮度增益",  &p.gain,  0.0f, 6.0f, "%.2f");
        ImGui::SliderFloat("条纹对比度",&p.contr, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("缠绕紧度",  &p.wind,  1.0f, 15.0f, "%.1f");
        ImGui::SliderFloat("旋转速度",  &p.speed,-10.0f, 10.0f, "%.1f");
        ImGui::SliderFloat("曝光度",    &p.expo,  0.2f, 3.0f, "%.2f");
        ImGui::SliderFloat("星空亮度",  &p.star,  0.0f, 2.0f, "%.3f");

        ImGui::EndChild();

        // Bottom bar
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::SetCursorPosX(350);
        if (ImGui::Button("启  动", ImVec2(180, 45))) {
            cfg.confirmed = true;
            done = true;
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1.0f), "  ESC 退出");

        ImGui::End();
        ImGui::Render();

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);

        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    return cfg.confirmed;
}