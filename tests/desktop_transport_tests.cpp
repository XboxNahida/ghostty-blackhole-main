#define GLFW_INCLUDE_NONE
#include <windows.h>
#include <GLFW/glfw3.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#include "../src/bloom_renderer.cpp"
#pragma GCC diagnostic pop
#include "../src/desktop_transport.h"
#include "../src/fragment_shader_builder.h"
#include "../src/formula_texture.h"
#include <vector>
#include <cmath>
#include <fstream>

void* Win32GL_GetProcAddress(const char* name) { return reinterpret_cast<void*>(glfwGetProcAddress(name)); }
int main(int argc,char** argv) {
    const bool angleSequence=argc>1 && std::string(argv[1])=="--angles";
    const bool narrowSequence=argc>1 && std::string(argv[1])=="--narrow-scenes";
    const bool sequence=angleSequence || narrowSequence || (argc>1 && std::string(argv[1])=="--sequence");
    const bool presetFrames=argc>1 && std::string(argv[1])=="--presets";
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    GLFWwindow* window=glfwCreateWindow(800,500,"Transport GPU test",nullptr,nullptr);
    if (!window) return 2;
    glfwMakeContextCurrent(window);
    BloomRenderer* bloom=Bloom_Create(800,500,stderr);
    if (!bloom) return 3;
    GLuint program=CreateProgram(*bloom,R"GLSL(#version 330 core
uniform float testRadius;
uniform float testInclination = 1.05;
uniform float testRoll = -0.35;
uniform int uTransportPass;
out vec4 color;
void main() { color=gl_FragCoord.x<1.0 ? vec4(testRadius,testInclination,testRoll,0.02) : vec4(0.35,0.45,0,1); }
)GLSL");
    if (!program) return 4;
    auto uniform1f=reinterpret_cast<PFNGLUNIFORM1FPROC>(glfwGetProcAddress("glUniform1f"));
    DesktopTransport* flow=DesktopTransport_Create(800,500,stderr);
    if (!flow) return 5;
    GLuint desktop=0;
    glGenTextures(1,&desktop);
    glBindTexture(GL_TEXTURE_2D,desktop);
    std::vector<unsigned char> pixels(800*500*4,255);
    for (int y=0;y<500;++y) for (int x=0;x<800;++x) {
        size_t p=static_cast<size_t>(y*800+x)*4;
        pixels[p]=static_cast<unsigned char>(x*255/799);
        pixels[p+1]=static_cast<unsigned char>(y*255/499);
        pixels[p+2]=100;
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,800,500,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    auto render=[&](float radius,double dt) {
        bloom->gl.UseProgram(program);
        uniform1f(bloom->gl.GetUniformLocation(program,"testRadius"),radius);
        return DesktopTransport_Render(flow,program,desktop,800,500,dt);
    };
    auto read=[&]() {
        glBindTexture(GL_TEXTURE_2D,DesktopTransport_Texture(flow));
        std::vector<float> values(800*500*4);
        glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,values.data());
        return values;
    };
    if (!render(0.0f,0.0)) return 6;
    auto initial=read();
    for (int y=0;y<500;++y) for (int x=0;x<800;++x) {
        size_t p=static_cast<size_t>(y*800+x)*4;
        size_t source=static_cast<size_t>((499-y)*800+x)*4;
        for (int c=0;c<3;++c) if (!std::isfinite(initial[p+c]) || std::abs(initial[p+c]-pixels[source+c]/255.0f)>0.008f) return 7;
        if (initial[p+3]<0.1f) return 8;
    }
    // 几何必须保留三维投影，不能把阴影内部的前景材料强行撑成圆环。
    glBindTexture(GL_TEXTURE_2D,DesktopTransport_StateTexture(flow));
    GLint stateWidth=0,stateHeight=0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&stateWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&stateHeight);
    std::vector<float> savedState(static_cast<size_t>(stateWidth)*stateHeight*4);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,savedState.data());
    auto terminalState=savedState;
    for (int y=0;y<stateHeight;++y) for (int x=0;x<stateWidth;++x) {
        size_t p=static_cast<size_t>(y*stateWidth+x)*4;
        float angle=6.283185307f*x/(stateWidth-1);
        float radius=0.08f*(0.4f+0.4f*y/(stateHeight-1));
        terminalState[p]=0.35f*1.6f+radius*std::cos(angle);
        terminalState[p+1]=0.55f+radius*std::sin(angle);
        terminalState[p+2]=0; terminalState[p+3]=1;
    }
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,stateWidth,stateHeight,GL_RGBA,GL_FLOAT,terminalState.data());
    if (!render(0.08f,0.0)) return 36;
    auto terminalImage=read();
    float closest=100,furthest=0;
    for (int y=0;y<500;++y) for (int x=0;x<800;++x) {
        if (terminalImage[static_cast<size_t>(y*800+x)*4+3]<0.001f) continue;
        float dx=(x+0.5f)/500.0f-0.56f,dy=(y+0.5f)/500.0f-0.55f;
        float radius=std::sqrt(dx*dx+dy*dy)/0.08f;
        closest=std::min(closest,radius); furthest=std::max(furthest,radius);
    }
    std::printf("Terminal material projected radius: %.4f .. %.4f shadow radii\n",closest,furthest);
    if (closest<0.39f || closest>0.43f || furthest<0.77f || furthest>0.81f) return 37;
    glBindTexture(GL_TEXTURE_2D,DesktopTransport_StateTexture(flow));
    // 部分捕获后缩到零半径，死顶点仍必须按相对中心坐标解释。
    for (int y=0;y<stateHeight/2;++y) for (int x=0;x<stateWidth;++x) {
        size_t p=static_cast<size_t>(y*stateWidth+x)*4;
        terminalState[p]-=0.56f; terminalState[p+1]-=0.55f; terminalState[p+3]=0;
    }
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,stateWidth,stateHeight,GL_RGBA,GL_FLOAT,terminalState.data());
    if (!render(0.0f,0.0)) return 38;
    auto zeroRadius=read();
    int visible=0;
    for (int y=0;y<500;++y) for (int x=0;x<800;++x) {
        if (zeroRadius[static_cast<size_t>(y*800+x)*4+3]<0.001f) continue;
        ++visible;
        float dx=(x+0.5f)/500.0f-0.56f,dy=(y+0.5f)/500.0f-0.55f;
        if (dx*dx+dy*dy>0.01f) {std::fprintf(stderr,"FAIL captured vertex uses absolute coordinates at zero radius\n"); return 39;}
    }
    if (visible<100) return 40;
    if (!render(0.08f,0.0)) return 41;
    auto boundaryReference=read();
    float mixedInner=100.0f;
    for (int y=0;y<500;++y) for (int x=0;x<800;++x) {
        if (boundaryReference[static_cast<size_t>(y*800+x)*4+3]<0.001f) continue;
        float dx=(x+0.5f)/500.0f-0.56f,dy=(y+0.5f)/500.0f-0.55f;
        mixedInner=std::min(mixedInner,std::sqrt(dx*dx+dy*dy)/0.08f);
    }
    std::printf("Mixed live/captured inner boundary: %.4f shadow radii\n",mixedInner);
    if (mixedInner<0.39f || mixedInner>0.43f) {std::fprintf(stderr,"FAIL interpolated alive cuts boundary early\n");return 46;}
    // 已捕获材料的旧方位不能影响存活边界；模拟多圈差速后冻结端点交替朝向。
    for (int y=0;y<stateHeight/2;++y) for (int x=0;x<stateWidth;++x) {
        size_t p=static_cast<size_t>(y*stateWidth+x)*4;
        if (x%2==0) { terminalState[p]=-terminalState[p]; terminalState[p+1]=-terminalState[p+1]; }
    }
    glBindTexture(GL_TEXTURE_2D,DesktopTransport_StateTexture(flow));
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,stateWidth,stateHeight,GL_RGBA,GL_FLOAT,terminalState.data());
    if (!render(0.08f,0.0)) return 42;
    auto boundaryTwisted=read();
    int changedBoundary=0;
    for (size_t p=0;p<boundaryTwisted.size();p+=4) {
        if (std::abs(boundaryTwisted[p]-boundaryReference[p])>0.01f
            || std::abs(boundaryTwisted[p+3]-boundaryReference[p+3])>0.01f) ++changedBoundary;
    }
    std::printf("Captured endpoint sensitivity: %d pixels\n",changedBoundary);
    if (changedBoundary>20) {std::fprintf(stderr,"FAIL frozen endpoints produce boundary teeth\n"); return 43;}
    // 独立三维薄环：投影短轴随 cos(倾角) 缩短，滚转 90 度后交换长短轴。
    for (int pose=0;pose<3;++pose) {
        float incl=pose==0 ? 0.0f : 1.2f, roll=pose==2 ? 1.570796327f : 0.0f;
        bloom->gl.UseProgram(program);
        uniform1f(bloom->gl.GetUniformLocation(program,"testInclination"),incl);
        uniform1f(bloom->gl.GetUniformLocation(program,"testRoll"),roll);
        for (int y=0;y<stateHeight;++y) for (int x=0;x<stateWidth;++x) {
            size_t p=static_cast<size_t>(y*stateWidth+x)*4;
            float angle=6.283185307f*x/(stateWidth-1);
            float radius=0.08f*(0.8f+0.4f*y/(stateHeight-1));
            float a=radius*std::cos(angle),b=radius*std::sin(angle);
            terminalState[p]=0.56f+std::cos(roll)*a+std::sin(roll)*std::cos(incl)*b;
            terminalState[p+1]=0.55f-std::sin(roll)*a+std::cos(roll)*std::cos(incl)*b;
            terminalState[p+2]=-std::sin(incl)*b; terminalState[p+3]=1;
        }
        glBindTexture(GL_TEXTURE_2D,DesktopTransport_StateTexture(flow));
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,stateWidth,stateHeight,GL_RGBA,GL_FLOAT,terminalState.data());
        if (!render(0.08f,0.0)) return 47;
        auto tilted=read();
        float extentX=0,extentY=0;
        for (int y=0;y<500;++y) for (int x=0;x<800;++x) {
            if (tilted[static_cast<size_t>(y*800+x)*4+3]<0.001f) continue;
            extentX=std::max(extentX,std::abs((x+0.5f)/500.0f-0.56f)/0.08f);
            extentY=std::max(extentY,std::abs((y+0.5f)/500.0f-0.55f)/0.08f);
        }
        float expectedX=1.2f*(pose==2 ? std::cos(incl) : 1.0f);
        float expectedY=1.2f*(pose==2 ? 1.0f : std::cos(incl));
        std::printf("Tilted ring pose=%d extents=%.4f/%.4f expected=%.4f/%.4f\n",pose,extentX,extentY,expectedX,expectedY);
        if (std::abs(extentX-expectedX)>0.03f || std::abs(extentY-expectedY)>0.03f) return 48;
    }
    bloom->gl.UseProgram(program);
    uniform1f(bloom->gl.GetUniformLocation(program,"testInclination"),0.0f);
    uniform1f(bloom->gl.GetUniformLocation(program,"testRoll"),0.0f);
    // 小黑洞附近只收薄流带离盘高度；同样归一化位置的大黑洞维持原高度。
    for (int sizeCase=0;sizeCase<2;++sizeCase) {
        float rh=sizeCase==0 ? 0.02f : 0.10f;
        auto elevated=savedState;
        for (size_t p=0;p<elevated.size();p+=4) elevated[p+2]=rh*0.5f;
        glBindTexture(GL_TEXTURE_2D,DesktopTransport_StateTexture(flow));
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,stateWidth,stateHeight,GL_RGBA,GL_FLOAT,elevated.data());
        if (!render(rh,0)) return 54;
        auto image=read();
        int probeX=static_cast<int>((0.56f+rh)*500);
        float thickness=(image[static_cast<size_t>(275*800+probeX)*4+3]-0.5f)*8.0f/rh;
        std::printf("Size dependent stream height small=%d height/rh=%.4f\n",sizeCase==0,thickness);
        if ((sizeCase==0 && (thickness<0.12f || thickness>0.25f)) || (sizeCase==1 && std::abs(thickness-0.5f)>0.01f)) return 55;
    }
    // 小洞左右入流截面都应收窄，不能只压深度却留下左侧宽扇面。
    auto ribbon=savedState;
    for (int y=0;y<stateHeight;++y) for (int x=0;x<stateWidth;++x) {
        size_t p=static_cast<size_t>(y*stateWidth+x)*4;
        ribbon[p]=0.56f+0.04f*(static_cast<float>(x)/(stateWidth-1)-0.5f);
        ribbon[p+1]=0.55f+0.04f*(static_cast<float>(y)/(stateHeight-1)-0.5f);
        ribbon[p+2]=0;
    }
    glBindTexture(GL_TEXTURE_2D,DesktopTransport_StateTexture(flow));
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,stateWidth,stateHeight,GL_RGBA,GL_FLOAT,ribbon.data());
    if (!render(0.02f,0)) return 61;
    auto narrowImage=read();
    int widths[2]={};
    for (int side=0;side<2;++side) for (int y=0;y<500;++y)
        if (narrowImage[static_cast<size_t>(y*800+(side==0 ? 274 : 285))*4+3]>0) ++widths[side];
    std::printf("Two-sided stream widths left/right=%d/%d pixels\n",widths[0],widths[1]);
    if (widths[0]<4 || widths[0]>11 || widths[1]<4 || widths[1]>11 || std::abs(widths[0]-widths[1])>1) return 62;
    // 覆盖旧版未触及的 5rh 左右外段，并检查 14rh 外完整桌面不受影响。
    // 正视与近侧视都测屏幕轮廓，避免仅 Z 压缩也能误通过。
    for (int pose=0;pose<2;++pose) {
        uniform1f(bloom->gl.GetUniformLocation(program,"testInclination"),pose==0 ? 0.0f : 1.5f);
        for (int y=0;y<stateHeight;++y) for (int x=0;x<stateWidth;++x) {
            size_t p=static_cast<size_t>(y*stateWidth+x)*4;
            ribbon[p]=0.56f+0.60f*(static_cast<float>(x)/(stateWidth-1)-0.5f);
            ribbon[p+1]=0.55f+0.08f*(static_cast<float>(y)/(stateHeight-1)-0.5f);
            ribbon[p+2]=0; ribbon[p+3]=1;
        }
        glBindTexture(GL_TEXTURE_2D,DesktopTransport_StateTexture(flow));
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,stateWidth,stateHeight,GL_RGBA,GL_FLOAT,ribbon.data());
        if (!render(0.02f,0)) return 63;
        auto outerImage=read();
        int spans[4]={};
        const int columns[4]={230,330,140,420};
        for (int c=0;c<4;++c) for (int y=0;y<500;++y)
            if (outerImage[static_cast<size_t>(y*800+columns[c])*4+3]>0) ++spans[c];
        std::printf("Outer stream pose=%d left/right=%d/%d far=%d/%d pixels\n",pose,spans[0],spans[1],spans[2],spans[3]);
        if (spans[0]<8 || spans[0]>34 || spans[1]<8 || spans[1]>34 ||
            std::abs(spans[0]-spans[1])>1 || spans[2]!=40 || spans[3]!=40) return 64;
    }
    uniform1f(bloom->gl.GetUniformLocation(program,"testInclination"),0.0f);
    glBindTexture(GL_TEXTURE_2D,DesktopTransport_StateTexture(flow));
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,stateWidth,stateHeight,GL_RGBA,GL_FLOAT,savedState.data());
    // 近盘应由热辐射决定颜色，远处仍完整保留桌面颜色。
    std::vector<float> thermalImages[2];
    for (int sample=0;sample<2;++sample) {
        auto solid=pixels;
        for (size_t p=0;p<solid.size();p+=4) {
            solid[p]=sample==0 ? 255 : 0;
            solid[p+1]=0; solid[p+2]=sample==0 ? 0 : 255;
        }
        glBindTexture(GL_TEXTURE_2D,desktop);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,800,500,GL_RGBA,GL_UNSIGNED_BYTE,solid.data());
        if (!render(0.08f,0.0)) return 44;
        thermalImages[sample]=read();
    }
    double nearDifference=0,nearGreen=0,farDifference=0;
    int nearCount=0,farCount=0;
    for (int y=0;y<500;++y) for (int x=0;x<800;++x) {
        size_t p=static_cast<size_t>(y*800+x)*4;
        if (thermalImages[0][p+3]<0.001f) continue;
        float dx=(x+0.5f)/500.0f-0.56f,dy=(y+0.5f)/500.0f-0.55f;
        float radius=std::sqrt(dx*dx+dy*dy)/0.08f;
        double delta=std::abs(thermalImages[0][p]-thermalImages[1][p])+std::abs(thermalImages[0][p+2]-thermalImages[1][p+2]);
        if (radius>1.08f && radius<1.25f) {nearDifference+=delta;nearGreen+=thermalImages[0][p+1];++nearCount;}
        if (radius>6.0f) {farDifference+=delta;++farCount;}
    }
    std::printf("Thermal source-color difference near=%.5f far=%.5f green=%.5f\n",nearDifference/std::max(nearCount,1),farDifference/std::max(farCount,1),nearGreen/std::max(nearCount,1));
    if (nearCount<100 || farCount<100 || nearDifference/nearCount>0.01 || nearGreen/nearCount<0.25 || nearGreen/nearCount>1.0 || farDifference/farCount<1.9) return 45;
    const size_t warmProbe=static_cast<size_t>(275*800+350)*4;
    const auto& warm=thermalImages[1];
    std::printf("Approaching disk warm RGB=%.4f/%.4f/%.4f\n",warm[warmProbe],warm[warmProbe+1],warm[warmProbe+2]);
    if (warm[warmProbe]<warm[warmProbe+1] || warm[warmProbe+1]<warm[warmProbe+2]) return 51;
    // 相同屏幕半径的离盘桌面不能被照成白球，热化主轴必须跟着滚转。
    for (int pose=0;pose<2;++pose) {
        bloom->gl.UseProgram(program);
        uniform1f(bloom->gl.GetUniformLocation(program,"testInclination"),1.2f);
        uniform1f(bloom->gl.GetUniformLocation(program,"testRoll"),pose==0 ? 0.0f : 1.570796327f);
        if (!render(0.08f,0.0)) return 49;
        auto hot=read();
        float alongX=hot[static_cast<size_t>(275*800+360)*4+1];
        float alongY=hot[static_cast<size_t>(355*800+280)*4+1];
        std::printf("Thermal disk confinement pose=%d greenX=%.4f greenY=%.4f\n",pose,alongX,alongY);
        float onDisk=pose==0 ? alongX : alongY, offDisk=pose==0 ? alongY : alongX;
        if (onDisk<0.20f || offDisk>0.05f) return 50;
    }
    bloom->gl.UseProgram(program);
    uniform1f(bloom->gl.GetUniformLocation(program,"testInclination"),1.05f);
    uniform1f(bloom->gl.GetUniformLocation(program,"testRoll"),-0.35f);
    glBindTexture(GL_TEXTURE_2D,desktop);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,800,500,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    for (int i=0;i<20;++i) if (!render(0.01f,0.05)) return 9;
    auto local=read();
    // 前景允许进入投影阴影；是否被捕获球遮挡由正式合成路径验证。
    double localDelta=0;
    for (int y=0;y<500;++y) for (int x=0;x<800;++x) {
        const size_t p=static_cast<size_t>(y*800+x)*4;
        const float dx=x/500.0f-0.35f*1.6f,dy=y/500.0f-0.55f;
        double delta=std::abs(local[p]-initial[p])+std::abs(local[p+1]-initial[p+1]);
        if (dx*dx+dy*dy>0.25f && delta>0.01) return 10;
        localDelta+=delta;
    }
    if (localDelta<1.0 || DesktopTransport_FormulaMix(flow)>0) return 11;
    if (!render(0.01f,0.0)) return 12;
    auto stopped=read();
    for (size_t p=0;p<stopped.size();++p) if (std::abs(stopped[p]-local[p])>0.001f) return 13;
    // 全尺寸驱动整个桌面，检查源边缘移开后露黑，随后实际耗尽。
    for (int i=0;i<100;++i) if (!render(0.126f,0.1)) return 14;
    auto moved=read();
    int uncovered=0;
    for (size_t p=3;p<moved.size();p+=4) if (moved[p]<0.001f) ++uncovered;
    if (uncovered<1000) return 15;
    for (int i=0;i<800 && DesktopTransport_FormulaMix(flow)<1.0f;++i)
        if (!render(0.126f,0.1)) return 16;
    if (DesktopTransport_FormulaMix(flow)<1.0f) return 17;
    if (!render(0.0f,0.1)) return 18;
    auto empty=read();
    for (float value:empty) if (!std::isfinite(value) || std::abs(value)>0.001f) return 19;
    std::printf("TRANSPORT_GPU_OK localDelta=%.3f uncovered=%d formula=%.2f\n",localDelta,uncovered,DesktopTransport_FormulaMix(flow));
    DesktopTransport_Destroy(flow);
    // 使用与正式 Renderer 完全相同的组装函数，避免测试替身掩盖参数错误。
    std::string source;
    bool available=false;
    if (!BuildFragmentShader(source,stderr,available) || !available) return 20;
    // 仅测试程序注入诊断，生产配置和 Shader 不增加开关。
    source.insert(source.find('\n',source.find("#version"))+1,"uniform int diagnosticSheet;\n");
    const std::string sheetMarker="    vec2 p = (uv-center)*vec2(aspect, 1.0);";
    auto sheetPos=source.find(sheetMarker);
    if (sheetPos==std::string::npos) return 65;
    source.insert(sheetPos,"    if (diagnosticSheet==1) desktopSheet=vec4(0.0);\n");
    GLuint production=CreateProgram(*bloom,source.c_str());
    if (!production) return 21;
    bloom->gl.UseProgram(production);
    auto setFloat=[&](const char* name,float value) {uniform1f(bloom->gl.GetUniformLocation(production,name),value);};
    auto setInt=[&](const char* name,int value) {bloom->gl.Uniform1i(bloom->gl.GetUniformLocation(production,name),value);};
    auto uniform3f=reinterpret_cast<PFNGLUNIFORM3FPROC>(glfwGetProcAddress("glUniform3f"));
    uniform3f(bloom->gl.GetUniformLocation(production,"iResolution"),800,500,0);
    setInt("uLightingEffect",1); setInt("uFixedSize",1); setFloat("uFixedLevel",0.5f);
    setFloat("uHomeX",0.25f); setFloat("uHomeY",0.75f); setFloat("uBornProgress",1.0f);
    auto parameters=[&](float radius,float movement) {
        Bloom_BeginScene(bloom,800,500,true);
        bloom->gl.UseProgram(production);
        setInt("uTransportPass",1); setFloat("uHoleRadius",radius); setFloat("uMovementTime",movement);
        bloom->gl.BindVertexArray(bloom->fullscreenVao); glViewport(0,0,4,1);
        glDrawArrays(GL_TRIANGLES,0,3);
        std::vector<float> result(16); glReadPixels(0,0,4,1,GL_RGBA,GL_FLOAT,result.data());
        return result;
    };
    auto small=parameters(0.01f,0);
    auto large=parameters(0.02f,0);
    auto middle=parameters(0.02f,9);
    auto arrived=parameters(0.02f,18);
    setInt("uPresetCount",1);
    setFloat("uPresetTemp[0]",8000); setFloat("uPresetInner[0]",2.2f);
    setFloat("uPresetOuter[0]",11); setFloat("uPresetOpac[0]",0.7f);
    setFloat("uPresetDopp[0]",0.4f); setFloat("uPresetBeam[0]",2);
    setFloat("uPresetGain[0]",1.8f); setFloat("uPresetContr[0]",1.1f);
    setFloat("uPresetWind[0]",6); setFloat("uPresetSpd[0]",3);
    auto styled=parameters(0.02f,18);
    const float expectedStyle[]={8000,2.2f,11,0.7f,0.4f,2,1.8f,1.1f,6,3};
    for (int i=0;i<10;++i) if (std::abs(styled[i+6]-expectedStyle[i])>0.001f) {
        std::fprintf(stderr,"FAIL preset material channel %d actual=%.4f expected=%.4f\n",i,styled[i+6],expectedStyle[i]); return 53;
    }
    setInt("uPresetCount",0);
    arrived=parameters(0.02f,18);
    for (float v:arrived) if (!std::isfinite(v)) return 22;
    std::printf("Production parameters: small=%.5f large=%.5f spawn=%.3f,%.3f middle=%.3f,%.3f end=%.3f,%.3f\n",
        small[0],large[0],large[4],large[5],middle[4],middle[5],arrived[4],arrived[5]);
    if (std::abs(large[0]/small[0]-2.0f)>0.02f) return 23;
    if (std::abs(large[4]-0.25f)>0.001 || std::abs(large[5]-0.75f)>0.001
        || std::abs(middle[4]-0.375f)>0.001 || std::abs(middle[5]-0.625f)>0.001
        || std::abs(arrived[4]-0.5f)>0.001 || std::abs(arrived[5]-0.5f)>0.001) return 24;
    // 投影阴影不等于遮挡蒙版：前景可见，后景和捕获球内材料不可见。
    GLuint sheet=0; glGenTextures(1,&sheet);
    setInt("uTransportPass",0); setInt("uTransportReady",1); setFloat("uTransportFormula",0);
    setInt("uTransportTexture",3); setInt("uFormulaTexture",2);
    bloom->gl.ActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,sheet);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    float centerRed[4]={};
    float foregroundRingGreen=0;
    float outsideRed=0;
    for (int i=0;i<4;++i) {
        const float depth=i<2 ? (i==0 ? 0.5625f : 0.4375f)
            : 0.5f+(i==2 ? 0.97f : 1.10f)*arrived[0]/2.598076211f*0.125f;
        const float material[4]={0.7f,0,0,depth};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,1,1,0,GL_RGBA,GL_FLOAT,material);
        glViewport(0,0,800,500); glDrawArrays(GL_TRIANGLES,0,3);
        float p[4]={}; glReadPixels(400,250,1,1,GL_RGBA,GL_FLOAT,p); centerRed[i]=p[0];
        if (i==0) {
            glReadPixels(static_cast<int>(400+arrived[0]*500*1.8f),250,1,1,GL_RGBA,GL_FLOAT,p);
            outsideRed=p[0];
            glReadPixels(static_cast<int>(400+arrived[0]*500*1.012f),250,1,1,GL_RGBA,GL_FLOAT,p);
            foregroundRingGreen=p[1];
        }
    }
    std::printf("Visible core front/back/near-depths=%.4f/%.4f/%.4f/%.4f exterior=%.4f ringGreen=%.4f\n",
        centerRed[0],centerRed[1],centerRed[2],centerRed[3],outsideRed,foregroundRingGreen);
    if (centerRed[0]<0.6f || centerRed[1]>0.001f || centerRed[2]>0.001f || centerRed[3]>0.001f || outsideRed<0.6f || foregroundRingGreen>0.05f) return 25;
    // 桌面材质在进入发光盘前融化，不等到捕获球边缘才结束遮挡。
    setInt("uPresetCount",1); setFloat("uPresetIncl[0]",0.0f); setFloat("uPresetRoll[0]",0.0f);
    float blendDelta[3]={}, mergedBaseline=0;
    const int probeX=static_cast<int>(400+2.8f*arrived[0]/2.598076211f*500);
    for (int stage=0;stage<3;++stage) {
        float reads[3][4]={};
        // 中间探针位于三维热化区过渡段，远探针在离盘 6 r_s 处。
        const float z=stage==0 ? 1.1f : (stage==1 ? 3.0f : 6.0f);
        for (int sample=0;sample<3;++sample) {
            const float material[4]={sample==0 ? 0.7f : 0.0f,0,sample==1 ? 0.7f : 0.0f,
                sample==2 ? 0.0f : 0.5f+z*arrived[0]/2.598076211f*0.125f};
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,1,1,0,GL_RGBA,GL_FLOAT,material);
            glDrawArrays(GL_TRIANGLES,0,3);
            glReadPixels(probeX,250,1,1,GL_RGBA,GL_FLOAT,reads[sample]);
        }
        for (int c=0;c<3;++c) {
            blendDelta[stage]+=std::abs(reads[0][c]-reads[1][c]);
            if (stage==0) mergedBaseline+=std::abs(reads[0][c]-reads[2][c]);
        }
    }
    std::printf("Disk handoff source delta merged/middle/far=%.4f/%.4f/%.4f baseline=%.4f\n",
        blendDelta[0],blendDelta[1],blendDelta[2],mergedBaseline);
    if (blendDelta[0]>0.01f || blendDelta[1]<0.1f || blendDelta[1]>1.2f || blendDelta[2]<1.3f || mergedBaseline>0.01f) return 52;
    glDeleteTextures(1,&sheet);
    flow=DesktopTransport_Create(800,500,stderr);
    if (!flow) return 26;
    GLuint formula=CreateFormulaTexture();
    if (!formula) return 29;
    bloom->gl.ActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,formula);
    setInt("uFormulaTexture",2); setInt("iChannel0",0);
    setInt("uPresetCount",1);
    setFloat("uPresetIncl[0]",1.05f); setFloat("uPresetRoll[0]",-0.35f);
    setFloat("uPresetTemp[0]",5500); setFloat("uPresetInner[0]",1.8f); setFloat("uPresetOuter[0]",8.0f);
    setFloat("uPresetOpac[0]",0.9f); setFloat("uPresetDopp[0]",0.6f); setFloat("uPresetBeam[0]",2.5f);
    setFloat("uPresetGain[0]",2.2f); setFloat("uPresetContr[0]",1.0f); setFloat("uPresetSpd[0]",5.0f);
    setFloat("uPresetExpo[0]",1.0f);
    setFloat("uPresetStar[0]",0.8f); setFloat("uPresetWind[0]",7.0f);
    if (narrowSequence) {setFloat("uPresetIncl[0]",1.5f); setFloat("uPresetRoll[0]",0.35f);}
    if (sequence) {
        std::ifstream fixture("build/transport-desktop-fixture.ppm",std::ios::binary);
        std::string magic; int w=0,h=0,max=0;
        fixture>>magic>>w>>h>>max; fixture.get();
        if (magic!="P6" || w!=800 || h!=500 || max!=255) return 30;
        std::vector<unsigned char> image(800*500*3);
        fixture.read(reinterpret_cast<char*>(image.data()),static_cast<std::streamsize>(image.size()));
        if (!fixture) return 31;
        bloom->gl.ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,desktop);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,800,500,GL_RGB,GL_UNSIGNED_BYTE,image.data());
    }
    // 真实参数通道驱动移动中心，直到耗尽；停留中心后继续推进也不恢复。
    setInt("uFixedSize",0); setFloat("uHoleRadius",0.02f);
    float drainedTime=-1;
    for (int i=0;i<1200 && (sequence ? i<1000 : DesktopTransport_FormulaMix(flow)<1.0f);++i) {
        float time=i*0.1f;
        setFloat("iTime",time); setFloat("uMovementTime",time*0.3f);
        if (angleSequence) {
            setFloat("uPresetIncl[0]",0.8f+0.7f*std::sin(time*0.09f));
            setFloat("uPresetRoll[0]",-0.35f+0.55f*std::sin(time*0.07f));
        }
        setFloat("uBornProgress",std::min(time/2.0f,1.0f));
        if (!DesktopTransport_Render(flow,production,desktop,800,500,0.1)) return 27;
        if (drainedTime<0 && DesktopTransport_FormulaMix(flow)>0) drainedTime=time;
        if (narrowSequence && i==80) {
            for (int mode=0;mode<2;++mode) {
                setInt("diagnosticSheet",mode);
                Bloom_BeginScene(bloom,800,500,true);
                bloom->gl.UseProgram(production); bloom->gl.BindVertexArray(bloom->fullscreenVao);
                glDrawArrays(GL_TRIANGLES,0,3); Bloom_EndScene(bloom,800,500,true);
                std::vector<unsigned char> rgb(800*500*3);
                glReadPixels(0,0,800,500,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
                std::ofstream file("build/sheet-diagnostic-"+std::to_string(mode)+".ppm",std::ios::binary);
                file<<"P6\n800 500\n255\n";
                for (int y=499;y>=0;--y) file.write(reinterpret_cast<char*>(rgb.data()+y*800*3),800*3);
                bloom->gl.UseProgram(production);
            }
            setInt("diagnosticSheet",0);
        }
        if (sequence && i%4==0) {
            Bloom_BeginScene(bloom,800,500,true);
            bloom->gl.UseProgram(production); bloom->gl.BindVertexArray(bloom->fullscreenVao);
            glDrawArrays(GL_TRIANGLES,0,3);
            Bloom_EndScene(bloom,800,500,true);
            std::vector<unsigned char> image(800*500*3);
            glReadPixels(0,0,800,500,GL_RGB,GL_UNSIGNED_BYTE,image.data());
            std::ofstream file(std::string(angleSequence ? "build/thermal-angle-sequence-" : "build/transport-sequence-")+std::to_string(i/4)+".ppm",std::ios::binary);
            if (!file) return 32;
            file<<"P6\n800 500\n255\n";
            for (int y=499;y>=0;--y) file.write(reinterpret_cast<char*>(image.data()+y*800*3),800*3);
            bloom->gl.UseProgram(production);
        }
    }
    if (DesktopTransport_FormulaMix(flow)<1.0f) return 28;
    std::printf("PRODUCTION_TRANSPORT_INTEGRATION_OK drainedAt=%.1f\n",drainedTime);
    auto getUniform=reinterpret_cast<PFNGLGETUNIFORMFVPROC>(glfwGetProcAddress("glGetUniformfv"));
    if (!getUniform) return 58;
    float phaseBefore=0,phaseAfter=0;
    getUniform(production,bloom->gl.GetUniformLocation(production,"uTransportPhase"),&phaseBefore);
    setFloat("iTime",10000); setFloat("uPresetSpd[0]",2.5f);
    if (!DesktopTransport_Render(flow,production,desktop,800,500,0.1)) return 59;
    getUniform(production,bloom->gl.GetUniformLocation(production,"uTransportPhase"),&phaseAfter);
    std::printf("Preset speed phase delta=%.5f required=0.05\n",phaseAfter-phaseBefore);
    if (std::abs(phaseAfter-phaseBefore-0.05f)>0.0001f) return 60;
    setFloat("uPresetSpd[0]",5.0f);
    if (presetFrames) {
        const float looks[][6]={{5500,1.5f,0.35f,1.8f,8,2.2f},{3800,0.55f,-0.3f,2.2f,6,1.6f},
                               {18000,1.05f,0.55f,3,16,1},{2500,1.35f,0.2f,1.6f,6,2.6f}};
        for (int style=0;style<4;++style) {
            setFloat("uPresetTemp[0]",looks[style][0]); setFloat("uPresetIncl[0]",looks[style][1]);
            setFloat("uPresetRoll[0]",looks[style][2]); setFloat("uPresetInner[0]",looks[style][3]);
            setFloat("uPresetOuter[0]",looks[style][4]); setFloat("uPresetGain[0]",looks[style][5]);
            setFloat("iTime",90); setFloat("uMovementTime",27);
            if (!DesktopTransport_Render(flow,production,desktop,800,500,0)) return 56;
            Bloom_BeginScene(bloom,800,500,true);
            bloom->gl.UseProgram(production); bloom->gl.BindVertexArray(bloom->fullscreenVao);
            glDrawArrays(GL_TRIANGLES,0,3); Bloom_EndScene(bloom,800,500,true);
            std::vector<unsigned char> rgb(800*500*3);
            glReadPixels(0,0,800,500,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
            std::ofstream file("build/preset-look-"+std::to_string(style)+".ppm",std::ios::binary);
            if (!file) return 57;
            file<<"P6\n800 500\n255\n";
            for (int y=499;y>=0;--y) file.write(reinterpret_cast<char*>(rgb.data()+y*800*3),800*3);
            bloom->gl.UseProgram(production);
        }
    }
    DesktopTransport_Destroy(flow);
    glDeleteTextures(1,&formula);
    bloom->gl.DeleteProgram(production);
    glDeleteTextures(1,&desktop);
    bloom->gl.DeleteProgram(program);
    Bloom_Destroy(bloom);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
