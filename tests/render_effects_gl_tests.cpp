#define GLFW_INCLUDE_NONE
#include <windows.h>
#include <GLFW/glfw3.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#include "../src/bloom_renderer.cpp"
#pragma GCC diagnostic pop
#include "../src/fragment_shader_builder.h"
#include <fstream>
#include <sstream>
#include <vector>

void* Win32GL_GetProcAddress(const char* name) { return reinterpret_cast<void*>(glfwGetProcAddress(name)); }
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static std::string read(const char* path) { std::ifstream f(path); std::ostringstream s; s<<f.rdbuf(); return s.str(); }
static double difference(const std::vector<unsigned char>& a,const std::vector<unsigned char>& b) {
    double sum=0; for (size_t i=0;i<a.size();++i) sum+=std::abs(int(a[i])-int(b[i])); return sum/a.size();
}
static void save(const char* name,int w,int h,const std::vector<unsigned char>& p) {
    CreateDirectoryA("build/v3-visual",nullptr);
    std::ofstream f(std::string("build/v3-visual/")+name+".ppm",std::ios::binary);
    f<<"P6\n"<<w<<" "<<h<<"\n255\n";
    for(int y=h-1;y>=0;--y) f.write(reinterpret_cast<const char*>(p.data()+y*w*3),w*3);
}
int main() {
    CHECK(glfwInit());
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    GLFWwindow* window=glfwCreateWindow(800,500,"V3 GPU tests",nullptr,nullptr);
    CHECK(window); glfwMakeContextCurrent(window);
    BloomRenderer* b=Bloom_Create(800,500,stderr); CHECK(b);
    const auto one=reinterpret_cast<PFNGLUNIFORM1FPROC>(glfwGetProcAddress("glUniform1f"));
    const auto three=reinterpret_cast<PFNGLUNIFORM3FPROC>(glfwGetProcAddress("glUniform3f"));
    GLuint desktop=0; glGenTextures(1,&desktop);
    glBindTexture(GL_TEXTURE_2D,desktop);
    std::vector<unsigned char> pattern(800*500*3);
    for(int y=0;y<500;++y) for(int x=0;x<800;++x) {
        const int p=(y*800+x)*3;
        pattern[p]=static_cast<unsigned char>(x*220/800);
        pattern[p+1]=((x/16+y/16)%2) ? 190 : 45;
        pattern[p+2]=static_cast<unsigned char>(y*220/500);
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,800,500,0,GL_RGB,GL_UNSIGNED_BYTE,pattern.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    const GLuint checker=CreateProgram(*b,R"GLSL(#version 330 core
in vec2 uv; out vec4 fragColor; uniform sampler2D sceneTexture;
void main() { fragColor=texture(sceneTexture,uv); }
)GLSL"); CHECK(checker);
    auto pixels=[](int w,int h) { std::vector<unsigned char> p(w*h*3); glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,p.data()); return p; };
    auto drawRipple=[&](int w,int h,const RippleState& r,double now) {
        Bloom_BeginScene(b,w,h,true); b->gl.UseProgram(checker); b->gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,desktop); b->gl.BindVertexArray(b->fullscreenVao);
        glDrawArrays(GL_TRIANGLES,0,3); Bloom_EndScene(b,w,h,true,&r,now,false); return pixels(w,h);
    };
    for(auto dimensions : {std::pair<int,int>{800,500},{360,480}}) {
        const int w=dimensions.first,h=dimensions.second;
        RippleState r; const auto empty=drawRipple(w,h,r,0);
        r.add(0.35f,0.55f,0); const auto first=drawRipple(w,h,r,0.35);
        const auto second=drawRipple(w,h,r,0.65);
        CHECK(difference(empty,first)>0.2); CHECK(difference(first,second)>0.2);
        // Refraction must move the checkerboard edges, not only add highlight.
        int changed=0;
        for(size_t i=1;i<first.size();i+=3) if (std::abs(int(first[i])-int(empty[i]))>65) ++changed;
        CHECK(changed>40);
        r.add(0.85f,0.15f,0.4); CHECK(difference(second,drawRipple(w,h,r,0.65))>0.05);
        r.expire(3.0); CHECK(difference(empty,drawRipple(w,h,r,3.0))==0.0);
        if(w==800) { save("ripple-off",w,h,empty); save("ripple-035",w,h,first); save("ripple-065",w,h,second); }
    }
    std::string source; bool available=false;
    CHECK(BuildFragmentShader(source,stderr,available,true) && available);
    const GLuint mainProgram=CreateProgram(*b,source.c_str()); CHECK(mainProgram);
    const std::string preview=read("shaders/frag_preview_header.glsl")+"\n"+read("shaders/blackhole_preview.glsl")
        +"\nvoid main(){vec4 c;mainImage(c,gl_FragCoord.xy);fragColor=c;}";
    const GLuint previewProgram=CreateProgram(*b,preview.c_str()); CHECK(previewProgram);
    auto drawDisk=[&](GLuint program,int mode,int consumption) {
        Bloom_BeginScene(b,800,500,true); b->gl.UseProgram(program);
        auto integer=[&](const char* key,int v){b->gl.Uniform1i(b->gl.GetUniformLocation(program,key),v);};
        auto scalar=[&](const char* key,float v){one(b->gl.GetUniformLocation(program,key),v);};
        three(b->gl.GetUniformLocation(program,"iResolution"),800,500,0);
        scalar("iTime",8); scalar("uMovementTime",0); scalar("uBornProgress",1);
        scalar("uPresetTemp[0]",5500); scalar("uPresetIncl[0]",1.5f); scalar("uPresetRoll[0]",0.35f);
        scalar("uPresetInner[0]",1.8f); scalar("uPresetOuter[0]",8); scalar("uPresetOpac[0]",0.9f);
        scalar("uPresetDopp[0]",0.6f); scalar("uPresetBeam[0]",2.5f); scalar("uPresetGain[0]",2.2f);
        scalar("uPresetContr[0]",1.6f); scalar("uPresetWind[0]",7); scalar("uPresetSpd[0]",5);
        scalar("uPresetExpo[0]",1.4f);
        scalar("uFixedLevel",0.55f); integer("uFixedSize",1);
        scalar("uHomeX",0.5f); scalar("uHomeY",0.5f); integer("uFollowMouse",1);
        integer("uLightingEffect",consumption); integer("uDiskRenderMode",mode);
        integer("uRippleOnly",0); integer("iChannel0",0);
        b->gl.ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,desktop);
        b->gl.BindVertexArray(b->fullscreenVao); glDrawArrays(GL_TRIANGLES,0,3);
        return pixels(800,500);
    };
    for(int scene=0;scene<3;++scene) {
        const GLuint program=scene==2 ? previewProgram : mainProgram;
        const auto soft=drawDisk(program,0,scene==1);
        const auto lines=drawDisk(program,1,scene==1);
        const auto edge=drawDisk(program,2,scene==1);
        const double d1=difference(soft,lines),d2=difference(lines,edge);
        std::printf("scene %d: soft/lines %.6f lines/edge %.6f\n",scene,d1,d2);
        CHECK(d1>0.002 && d2>0.002);
        CHECK(difference(soft,drawDisk(program,99,scene==1))==0);
        char name[64];
        std::snprintf(name,sizeof(name),"disk-%d-soft",scene); save(name,800,500,soft);
        std::snprintf(name,sizeof(name),"disk-%d-lines",scene); save(name,800,500,lines);
        std::snprintf(name,sizeof(name),"disk-%d-edge",scene); save(name,800,500,edge);
    }
    CHECK(glGetError()==GL_NO_ERROR);
    b->gl.DeleteProgram(checker); b->gl.DeleteProgram(mainProgram); b->gl.DeleteProgram(previewProgram);
    glDeleteTextures(1,&desktop); Bloom_Destroy(b); glfwDestroyWindow(window); glfwTerminate();
    std::puts("V3 RIPPLE AND DISK GPU PASS");
}
