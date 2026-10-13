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
    GLuint outputFbo=0,outputTexture=0;
    auto drawRipple=[&](int w,int h,const RippleState& r,double now) {
        Bloom_BeginScene(b,w,h,true); b->gl.UseProgram(checker); b->gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,desktop); b->gl.BindVertexArray(b->fullscreenVao);
        glDrawArrays(GL_TRIANGLES,0,3); Bloom_EndScene(b,w,h,true,&r,now,false);
        if (outputFbo) {
            // Reuse the production composite program and its uploaded uniforms,
            // but read from an FBO so Windows cannot clamp 4K to monitor size.
            b->gl.BindFramebuffer(GL_FRAMEBUFFER,outputFbo);
            b->gl.UseProgram(b->compositeProgram); b->gl.ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,b->sceneTexture);
            b->gl.BindVertexArray(b->fullscreenVao); glDrawArrays(GL_TRIANGLES,0,3);
        }
        return pixels(w,h);
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
    RippleSettings clickSettings;
    RippleState adjustable;
    const auto untouched=drawRipple(800,500,adjustable,0);
    clickSettings.amplitude=0; adjustable.addClick(0.25f,0.5f,0,clickSettings);
    CHECK(difference(untouched,drawRipple(800,500,adjustable,0.25))==0);
    clickSettings.amplitude=0.3f; adjustable.addClick(0.25f,0.5f,0,clickSettings);
    const double lowAmplitude=difference(untouched,drawRipple(800,500,adjustable,0.25));
    adjustable.clear(); clickSettings.amplitude=1.5f;
    adjustable.addClick(0.25f,0.5f,0,clickSettings);
    CHECK(difference(untouched,drawRipple(800,500,adjustable,0.25))>lowAmplitude*1.2);
    const auto threeCycles=drawRipple(800,500,adjustable,0.55);
    adjustable.clear(); clickSettings.oscillations=1;
    adjustable.addClick(0.25f,0.5f,0,clickSettings);
    CHECK(difference(threeCycles,drawRipple(800,500,adjustable,0.55))>0.2);

    // Recover displaced coordinates from a complementary colour ramp. Subtract
    // blue to remove common specular light, then divide out diffuse lighting.
    std::vector<unsigned char> ramp(800*500*3);
    for (int y=0;y<500;++y) for (int x=0;x<800;++x) {
        const int p=(y*800+x)*3; ramp[p]=40+x*175/799;
        ramp[p+1]=255-ramp[p]; ramp[p+2]=0;
    }
    glBindTexture(GL_TEXTURE_2D,desktop);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,800,500,GL_RGB,GL_UNSIGNED_BYTE,ramp.data());
    auto position=[](const std::vector<unsigned char>& p,int x) {
        const int i=(250*800+x)*3;
        return (float(p[i])-p[i+2])/(float(p[i])+p[i+1]-2.0f*p[i+2]);
    };
    RippleState impulse; const auto rest=drawRipple(800,500,impulse,0);
    impulse.add(0.25f,0.5f,0);
    const auto stretch=drawRipple(800,500,impulse,0.16);
    const auto rebound=drawRipple(800,500,impulse,0.56);
    const float left=position(stretch,104)-position(rest,104);
    const float right=position(stretch,296)-position(rest,296);
    const float returnLeft=position(rebound,104)-position(rest,104);
    const float returnRight=position(rebound,296)-position(rest,296);
    std::printf("elastic stretch %.5f %.5f rebound %.5f %.5f\n",left,right,returnLeft,returnRight);
    CHECK(left < -0.003f && right > 0.003f);
    CHECK(returnLeft > 0.003f && returnRight < -0.003f);
    for (int direction : {1,-1}) {
        RippleState travelling;
        travelling.add(0.25f,0.5f,0,0.55f,direction);
        int previousPeak=-1;
        for (double age : {0.24,0.6,0.96}) {
            const auto frame=drawRipple(800,500,travelling,age);
            float strongest=0; int peak=-1;
            for (int x=230;x<770;++x) {
                const float displacement=position(frame,x)-position(rest,x);
                const float movement=-direction*displacement;
                if (movement>strongest) { strongest=movement; peak=x; }
            }
            std::printf("travelling direction %d age %.2f peak %d strength %.5f\n",direction,age,peak,strongest);
            CHECK(strongest>0.005f);
            if (previousPeak>=0) CHECK(direction*(peak-previousPeak)>100);
            previousPeak=peak;
        }
        CHECK(difference(rest,drawRipple(800,500,travelling,1.2))==0);
    }
    for (int spread : {1,2}) {
        RippleState travelling;
        clickSettings.amplitude=1; clickSettings.oscillations=1; clickSettings.spread=spread;
        travelling.addClick(0.25f,0.5f,0,clickSettings);
        const int direction=spread==1 ? 1 : -1;
        int previousPeak=-1;
        for (double age : {0.48,1.2,1.92}) {
            const auto frame=drawRipple(800,500,travelling,age);
            float strongest=0; int peak=-1;
            for (int x=230;x<770;++x) {
                const float displacement=-direction*(position(frame,x)-position(rest,x));
                if (displacement>strongest) { strongest=displacement; peak=x; }
            }
            CHECK(strongest>0.003f);
            if (previousPeak>=0) CHECK(direction*(peak-previousPeak)>100);
            previousPeak=peak;
        }
        CHECK(difference(rest,drawRipple(800,500,travelling,2.4))==0);
    }
    glBindTexture(GL_TEXTURE_2D,desktop);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,800,500,GL_RGB,GL_UNSIGNED_BYTE,pattern.data());
    // Broad pressure waves must affect distant content without fine rings.
    b->gl.GenFramebuffers(1,&outputFbo); glGenTextures(1,&outputTexture);
    for (auto dimensions : {std::pair<int,int>{800,500},{1920,1080},{1080,1920},{3840,2160}}) {
        const int w=dimensions.first,h=dimensions.second;
        b->gl.ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,outputTexture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        b->gl.BindFramebuffer(GL_FRAMEBUFFER,outputFbo);
        b->gl.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,outputTexture,0);
        CHECK(b->gl.CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
        RippleState r;
        const auto base=drawRipple(w,h,r,0);
        r.add(0.1f,0.5f,0);
        const auto shaken=drawRipple(w,h,r,0.12);
        int refracted=0;
        for (int y=h*4/10;y<h*6/10;++y) for (int x=w*85/100;x<w*98/100;++x) {
            const int p=(y*w+x)*3+1;
            if (std::abs(int(shaken[p])-int(base[p]))>65) ++refracted;
        }
        std::printf("fullscreen %dx%d: far-edge refraction=%d\n",w,h,refracted);
        CHECK(refracted>40);
        std::vector<unsigned char> flat(800*500*3,128);
        glBindTexture(GL_TEXTURE_2D,desktop);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,800,500,GL_RGB,GL_UNSIGNED_BYTE,flat.data());
        for (double age : {0.12,0.18}) {
            const auto waves=drawRipple(w,h,r,age);
            int crests=0,start=-1,minWidth=w,firstCrest=-1;
            for (int x=w/10;x<w;++x) {
                const int value=waves[(h/2*w+x)*3];
                if (start<0 && value>130) { start=x; ++crests; }
                if (start>=0 && value<=129) {
                    minWidth=std::min(minWidth,x-start);
                    if (firstCrest<0) firstCrest=(start+x)/2;
                    start=-1;
                }
            }
            std::printf("fullscreen %dx%d age %.1f: crests=%d width=%d\n",w,h,age,crests,minWidth);
            CHECK(crests>=1 && crests<=2);
            CHECK(minWidth>w/20);
            if (w==1920 && age==0.12) save("ripple-fullscreen-flat",w,h,waves);
        }
        glBindTexture(GL_TEXTURE_2D,desktop);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,800,500,GL_RGB,GL_UNSIGNED_BYTE,pattern.data());
        const auto settled=drawRipple(w,h,r,2.4);
        CHECK(difference(base,settled)==0.0);
        if (w==1920) save("ripple-fullscreen-shake",w,h,shaken);
    }
    b->gl.BindFramebuffer(GL_FRAMEBUFFER,0);
    b->gl.DeleteFramebuffers(1,&outputFbo); glDeleteTextures(1,&outputTexture); outputFbo=0;
    std::string source; bool available=false;
    CHECK(BuildFragmentShader(source,stderr,available,true) && available);
    const GLuint mainProgram=CreateProgram(*b,source.c_str()); CHECK(mainProgram);
    const std::string preview=read("shaders/frag_preview_header.glsl")+"\n"+read("shaders/blackhole_preview.glsl")
        +"\nvoid main(){vec4 c;mainImage(c,gl_FragCoord.xy);fragColor=c;}";
    const GLuint previewProgram=CreateProgram(*b,preview.c_str()); CHECK(previewProgram);
    struct DiskSettings {
        float birthScale=1, time=8, movementTime=0, level=0.55f;
        float radiusDial=-1;
        int width=800, height=500;
        bool fixed=true, grow=false, follow=true, probe=false, limited=false, preset=false;
    };
    auto drawDisk=[&](GLuint program,int mode,int consumption,const DiskSettings& settings=DiskSettings{}) {
        Bloom_BeginScene(b,800,500,true); b->gl.UseProgram(program);
        auto integer=[&](const char* key,int v){b->gl.Uniform1i(b->gl.GetUniformLocation(program,key),v);};
        auto scalar=[&](const char* key,float v){one(b->gl.GetUniformLocation(program,key),v);};
        three(b->gl.GetUniformLocation(program,"iResolution"),static_cast<float>(settings.width),static_cast<float>(settings.height),0);
        scalar("iTime",settings.time); scalar("uMovementTime",settings.movementTime);
        scalar("uBornProgress",settings.birthScale);
        integer("uCenterProbe",settings.probe ? 1 : 0);
        integer("uEmergenceSizeLimit",settings.limited ? 1 : 0);
        integer("uPresetCount",settings.preset ? 1 : 0);
        scalar("uHoleRadius",settings.radiusDial);
        scalar("uPresetTemp[0]",5500); scalar("uPresetIncl[0]",1.5f); scalar("uPresetRoll[0]",0.35f);
        scalar("uPresetInner[0]",1.8f); scalar("uPresetOuter[0]",8); scalar("uPresetOpac[0]",0.9f);
        scalar("uPresetDopp[0]",0.6f); scalar("uPresetBeam[0]",2.5f); scalar("uPresetGain[0]",2.2f);
        scalar("uPresetContr[0]",1.6f); scalar("uPresetWind[0]",7); scalar("uPresetSpd[0]",5);
        scalar("uPresetExpo[0]",1.4f);
        scalar("uFixedLevel",settings.level); integer("uFixedSize",settings.fixed ? 1 : 0);
        integer("uGrowEnabled",settings.grow ? 1 : 0); scalar("uInitialSize",0.2f);
        scalar("uHomeX",0.5f); scalar("uHomeY",0.5f); integer("uFollowMouse",settings.follow ? 1 : 0);
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
    auto hdrPixels=[]() {
        std::vector<float> values(800*500*3);
        glReadPixels(0,0,800,500,GL_RGB,GL_FLOAT,values.data());
        return values;
    };
    DiskSettings settings;
    settings.birthScale=0; const auto invisible=drawDisk(mainProgram,0,0,settings);
    settings.birthScale=1; const auto visible=drawDisk(mainProgram,0,0,settings);
    settings.birthScale=0.5f; const auto halfway=drawDisk(mainProgram,0,0,settings);
    CHECK(difference(invisible,visible)>0.01);
    auto shadowArea=[](const std::vector<unsigned char>& pixels) {
        int count=0;
        for (int y=150;y<350;++y) for (int x=250;x<550;++x) {
            const int p=(y*800+x)*3;
            if (pixels[p]<12 && pixels[p+1]<12 && pixels[p+2]<12) ++count;
        }
        return count;
    };
    std::printf("shrink shadow full=%d half=%d absent=%d\n",shadowArea(visible),shadowArea(halfway),shadowArea(invisible));
    CHECK(shadowArea(visible)>100);
    CHECK(shadowArea(halfway)>20 && shadowArea(halfway)<shadowArea(visible)*0.6);
    CHECK(shadowArea(invisible)==0);
    save("emergence-shrink-half",800,500,halfway);
    auto probe=[&](const DiskSettings& frame) {
        drawDisk(mainProgram,0,0,frame); const auto values=hdrPixels();
        return std::array<float,3>{values[0],values[1],values[2]};
    };
    settings=DiskSettings{}; settings.probe=true; settings.fixed=false;
    settings.time=2; const auto constantStart=probe(settings);
    settings.time=8; const auto constantEnd=probe(settings);
    CHECK(std::abs(constantStart[2]-constantEnd[2])<0.0001f);
    settings.grow=true; settings.time=2; const auto growingStart=probe(settings);
    settings.time=8; const auto growingEnd=probe(settings);
    CHECK(growingEnd[2]>growingStart[2]*1.1f);
    settings.grow=false; settings.follow=false; settings.movementTime=0;
    const auto motionStart=probe(settings);
    settings.movementTime=8; const auto motionEnd=probe(settings);
    CHECK(std::hypot(motionEnd[0]-motionStart[0],motionEnd[1]-motionStart[1])>0.01f);
    for (auto dimensions : {std::pair<int,int>{800,500},{500,800},{1800,500}}) {
        settings=DiskSettings{}; settings.probe=true; settings.fixed=false;
        settings.preset=true;
        settings.width=dimensions.first; settings.height=dimensions.second;
        settings.radiusDial=0.16f;
        const float unrestricted=probe(settings)[2];
        settings.limited=true;
        const float limited=probe(settings)[2];
        const float diameter=2*limited*(8.0f/2.5980762f)*settings.height;
        std::printf("emergence limit %dx%d: unrestricted=%.5f limited=%.5f disk diameter=%.1f\n",
            settings.width,settings.height,unrestricted,limited,diameter);
        CHECK(limited>0 && limited<unrestricted*0.5f);
        CHECK(diameter<=0.401f*std::min(settings.width,settings.height));
        settings.grow=true; settings.time=2; const float growingSmall=probe(settings)[2];
        settings.time=8; CHECK(probe(settings)[2]>growingSmall*1.1f);
        settings.grow=false; settings.radiusDial=0.002f;
        const float smallLimited=probe(settings)[2];
        settings.limited=false;
        CHECK(std::abs(probe(settings)[2]-smallLimited)<0.0001f);
    }
    CHECK(glGetError()==GL_NO_ERROR);
    b->gl.DeleteProgram(checker); b->gl.DeleteProgram(mainProgram); b->gl.DeleteProgram(previewProgram);
    glDeleteTextures(1,&desktop); Bloom_Destroy(b); glfwDestroyWindow(window); glfwTerminate();
    std::puts("V3 RIPPLE AND DISK GPU PASS");
}
