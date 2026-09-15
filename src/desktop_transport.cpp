#include "desktop_transport.h"
#include "win32_gl.h"
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_COMPILE_STATUS
#include <GL/glcorearb.h>
#endif
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
#define FLOW_GL(X) \
 X(PFNGLCREATESHADERPROC,CreateShader) X(PFNGLSHADERSOURCEPROC,ShaderSource) \
 X(PFNGLCOMPILESHADERPROC,CompileShader) X(PFNGLGETSHADERIVPROC,GetShaderiv) \
 X(PFNGLGETSHADERINFOLOGPROC,GetShaderInfoLog) X(PFNGLDELETESHADERPROC,DeleteShader) \
 X(PFNGLCREATEPROGRAMPROC,CreateProgram) X(PFNGLATTACHSHADERPROC,AttachShader) \
 X(PFNGLLINKPROGRAMPROC,LinkProgram) X(PFNGLGETPROGRAMIVPROC,GetProgramiv) \
 X(PFNGLGETPROGRAMINFOLOGPROC,GetProgramInfoLog) X(PFNGLDELETEPROGRAMPROC,DeleteProgram) \
 X(PFNGLUSEPROGRAMPROC,UseProgram) X(PFNGLGETUNIFORMLOCATIONPROC,GetUniformLocation) \
 X(PFNGLUNIFORM1IPROC,Uniform1i) X(PFNGLUNIFORM1FPROC,Uniform1f) X(PFNGLUNIFORM2FPROC,Uniform2f) \
 X(PFNGLACTIVETEXTUREPROC,ActiveTexture) X(PFNGLGENVERTEXARRAYSPROC,GenVertexArrays) \
 X(PFNGLBINDVERTEXARRAYPROC,BindVertexArray) X(PFNGLDELETEVERTEXARRAYSPROC,DeleteVertexArrays) \
 X(PFNGLGENFRAMEBUFFERSPROC,GenFramebuffers) X(PFNGLBINDFRAMEBUFFERPROC,BindFramebuffer) \
 X(PFNGLFRAMEBUFFERTEXTURE2DPROC,FramebufferTexture2D) X(PFNGLCHECKFRAMEBUFFERSTATUSPROC,CheckFramebufferStatus) \
 X(PFNGLDELETEFRAMEBUFFERSPROC,DeleteFramebuffers) X(PFNGLGENBUFFERSPROC,GenBuffers) \
 X(PFNGLBINDBUFFERPROC,BindBuffer) X(PFNGLBUFFERDATAPROC,BufferData) \
 X(PFNGLDELETEBUFFERSPROC,DeleteBuffers) X(PFNGLVERTEXATTRIBPOINTERPROC,VertexAttribPointer) \
 X(PFNGLENABLEVERTEXATTRIBARRAYPROC,EnableVertexAttribArray)
struct FlowGL {
#define DECLARE(type,name) type name=nullptr;
    FLOW_GL(DECLARE)
#undef DECLARE
    bool load() {
#define LOAD(type,name) name=reinterpret_cast<type>(Win32GL_GetProcAddress("gl" #name)); if (!name) return false;
        FLOW_GL(LOAD)
#undef LOAD
        return true;
    }
};
#undef FLOW_GL
constexpr int columns=512, rows=1080;
std::string readSource(const char* path) {
    std::ifstream file(path,std::ios::binary);
    std::ostringstream text; text<<file.rdbuf(); return text.str();
}
const char* fullscreen=R"GLSL(#version 330 core
void main() {
    vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));
    gl_Position=vec4(p*2.0-1.0,0.0,1.0);
}
)GLSL";
}

struct DesktopTransport {
    FlowGL gl;
    FILE* log=nullptr;
    GLuint step=0, mesh=0, vao=0, quadBuffer=0;
    GLuint state[2]={}, stateFbo[2]={}, parameter=0, parameterFbo=0;
    GLuint image=0, depth=0, imageFbo=0;
    int current=0,width=0,height=0;
    double checkTime=0,formulaAge=0,phase=0;
    bool drained=false;
    std::vector<float> readback;
};

namespace {
GLuint compile(DesktopTransport& f,GLenum type,const std::string& source) {
    GLuint shader=f.gl.CreateShader(type);
    const char* data=source.c_str();
    f.gl.ShaderSource(shader,1,&data,nullptr); f.gl.CompileShader(shader);
    GLint ok=0; f.gl.GetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (ok) return shader;
    char log[4096]={}; f.gl.GetShaderInfoLog(shader,sizeof(log),nullptr,log);
    if (f.log) std::fprintf(f.log,"[Transport][FAIL] Shader: %s\n",log);
    f.gl.DeleteShader(shader); return 0;
}
GLuint link(DesktopTransport& f,const std::string& vertex,const std::string& fragment) {
    GLuint vs=compile(f,GL_VERTEX_SHADER,vertex), fs=compile(f,GL_FRAGMENT_SHADER,fragment);
    GLuint program=0;
    if (vs && fs) {
        program=f.gl.CreateProgram(); f.gl.AttachShader(program,vs); f.gl.AttachShader(program,fs);
        f.gl.LinkProgram(program);
        GLint ok=0; f.gl.GetProgramiv(program,GL_LINK_STATUS,&ok);
        if (!ok) {
            char log[4096]={}; f.gl.GetProgramInfoLog(program,sizeof(log),nullptr,log);
            if (f.log) std::fprintf(f.log,"[Transport][FAIL] Link: %s\n",log);
            f.gl.DeleteProgram(program); program=0;
        }
    }
    if (vs) f.gl.DeleteShader(vs);
    if (fs) f.gl.DeleteShader(fs);
    return program;
}
bool target(DesktopTransport& f,GLuint& tex,GLuint& fbo,int w,int h,GLint format,const float* data=nullptr) {
    glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
    glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,GL_RGBA,GL_FLOAT,data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    f.gl.GenFramebuffers(1,&fbo); f.gl.BindFramebuffer(GL_FRAMEBUFFER,fbo);
    f.gl.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
    return f.gl.CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
}
void sampler(DesktopTransport& f,GLuint program,const char* name,int unit,GLuint texture) {
    f.gl.ActiveTexture(GL_TEXTURE0+unit); glBindTexture(GL_TEXTURE_2D,texture);
    f.gl.Uniform1i(f.gl.GetUniformLocation(program,name),unit);
}
}

DesktopTransport* DesktopTransport_Create(int width,int height,FILE* log) {
    if (width<=0 || height<=0) return nullptr;
    auto* f=new DesktopTransport;
    f->log=log; f->width=width; f->height=height;
    if (!f->gl.load()) { delete f; return nullptr; }
    GLint previousFbo=0,previousVao=0,previousBuffer=0,previousTexture=0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previousFbo); glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previousBuffer); glGetIntegerv(GL_TEXTURE_BINDING_2D,&previousTexture);
    f->step=link(*f,fullscreen,readSource("shaders/desktop_transport_step.glsl"));
    std::string material=readSource("shaders/desktop_transport_fragment.glsl");
    const std::string palette=readSource("shaders/consumption_palette.glsl");
    const size_t versionEnd=material.find('\n');
    if (!palette.empty() && versionEnd!=std::string::npos) {
        material.insert(versionEnd+1,palette+"\n");
        f->mesh=link(*f,readSource("shaders/desktop_transport_mesh.glsl"),material);
    }
    f->gl.GenVertexArrays(1,&f->vao); f->gl.BindVertexArray(f->vao);
    const float quad[]={-1,-1,3,-1,-1,3};
    f->gl.GenBuffers(1,&f->quadBuffer); f->gl.BindBuffer(GL_ARRAY_BUFFER,f->quadBuffer);
    f->gl.BufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
    f->gl.VertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),nullptr);
    f->gl.EnableVertexAttribArray(0);
    f->readback.resize(static_cast<size_t>(columns+1)*(rows+1)*4);
    const float aspect=static_cast<float>(width)/height;
    for (int y=0;y<=rows;++y) for (int x=0;x<=columns;++x) {
        size_t p=static_cast<size_t>(y*(columns+1)+x)*4;
        f->readback[p]=static_cast<float>(x)/columns*aspect;
        f->readback[p+1]=1.0f-static_cast<float>(y)/rows;
        f->readback[p+2]=0; f->readback[p+3]=1;
    }
    bool ok=f->step && f->mesh;
    for (int i=0;i<2;++i)
        ok=target(*f,f->state[i],f->stateFbo[i],columns+1,rows+1,GL_RGBA32F,f->readback.data()) && ok;
    ok=target(*f,f->parameter,f->parameterFbo,4,1,GL_RGBA32F) && ok;
    // alpha 编码三维深度，小黑洞薄流带的深度差低于半精度量化步长。
    ok=target(*f,f->image,f->imageFbo,width,height,GL_RGBA32F) && ok;
    glGenTextures(1,&f->depth); glBindTexture(GL_TEXTURE_2D,f->depth);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,width,height,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    f->gl.FramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,f->depth,0);
    ok=(f->gl.CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE) && ok;
    ok=(glGetError()==GL_NO_ERROR) && ok;
    f->gl.BindFramebuffer(GL_FRAMEBUFFER,static_cast<GLuint>(previousFbo));
    f->gl.BindVertexArray(static_cast<GLuint>(previousVao)); f->gl.BindBuffer(GL_ARRAY_BUFFER,static_cast<GLuint>(previousBuffer));
    glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(previousTexture));
    if (!ok) { DesktopTransport_Destroy(f); return nullptr; }
    if (log) std::fprintf(log,"[Transport][OK] Connected mesh %dx%d, persistent RGBA32F state\n",columns,rows);
    return f;
}

bool DesktopTransport_Render(DesktopTransport* f,unsigned program,unsigned desktop,int width,int height,double seconds) {
    if (!f || width!=f->width || height!=f->height || !std::isfinite(seconds) || seconds<0) return false;
    GLint previousFbo=0,viewport[4]={},previousVao=0,active=0,bindings[3]={},depthFunc=0;
    GLfloat clear[4]={}; GLboolean depthMask=GL_TRUE;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previousFbo); glGetIntegerv(GL_VIEWPORT,viewport);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previousVao); glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    glGetIntegerv(GL_DEPTH_FUNC,&depthFunc); glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask); glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
    const bool depthEnabled=glIsEnabled(GL_DEPTH_TEST),blendEnabled=glIsEnabled(GL_BLEND),cullEnabled=glIsEnabled(GL_CULL_FACE);
    for (int i=0;i<3;++i) { f->gl.ActiveTexture(GL_TEXTURE0+i); glGetIntegerv(GL_TEXTURE_BINDING_2D,&bindings[i]); }
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    f->gl.BindVertexArray(f->vao);
    f->gl.BindFramebuffer(GL_FRAMEBUFFER,f->parameterFbo); glViewport(0,0,4,1);
    f->gl.UseProgram(program);
    f->gl.Uniform1i(f->gl.GetUniformLocation(program,"uTransportPass"),1);
    glDrawArrays(GL_TRIANGLES,0,3);
    f->gl.Uniform1i(f->gl.GetUniformLocation(program,"uTransportPass"),0);
    // 一帧最多推进 0.1 秒；长时间暂停不会用巨大步长拉坏网格。
    double dt=std::min(seconds,0.1);
    // 仅累计本帧速度，避免预设交叉渐变时 iTime*speed 造成相位跳动或倒退。
    float materialParameters[12]={};
    glReadPixels(1,0,3,1,GL_RGBA,GL_FLOAT,materialParameters);
    float speed=materialParameters[2]>100.0f ? materialParameters[11] : 5.0f;
    if (!std::isfinite(speed)) speed=0;
    f->phase+=dt*static_cast<double>(speed)/5.0;
    int steps=std::max(1,static_cast<int>(std::ceil(dt*60.0)));
    if (!f->drained && dt>0) {
        f->gl.UseProgram(f->step);
        sampler(*f,f->step,"parameters",1,f->parameter);
        f->gl.Uniform2f(f->gl.GetUniformLocation(f->step,"stepInfo"),static_cast<float>(dt/steps),static_cast<float>(width)/height);
        glViewport(0,0,columns+1,rows+1);
        for (int i=0;i<steps;++i) {
            f->gl.BindFramebuffer(GL_FRAMEBUFFER,f->stateFbo[1-f->current]);
            sampler(*f,f->step,"stateTexture",0,f->state[f->current]);
            glDrawArrays(GL_TRIANGLES,0,3); f->current=1-f->current;
        }
        f->checkTime+=dt;
        if (f->checkTime>=0.5) {
            f->checkTime=0;
            glBindTexture(GL_TEXTURE_2D,f->state[f->current]);
            glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,f->readback.data());
            f->drained=true;
            for (size_t p=3;p<f->readback.size();p+=4) {
                if (!std::isfinite(f->readback[p]) || f->readback[p]>0.5f) { f->drained=false; break; }
            }
        }
    }
    if (f->drained) f->formulaAge=std::min(f->formulaAge+dt,4.0);
    f->gl.BindFramebuffer(GL_FRAMEBUFFER,f->imageFbo); glViewport(0,0,width,height);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
    glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if (!f->drained) {
        f->gl.UseProgram(f->mesh);
        sampler(*f,f->mesh,"stateTexture",0,f->state[f->current]);
        sampler(*f,f->mesh,"desktopTexture",1,desktop);
        sampler(*f,f->mesh,"parameters",2,f->parameter);
        f->gl.Uniform1f(f->gl.GetUniformLocation(f->mesh,"aspect"),static_cast<float>(width)/height);
        glDrawArrays(GL_TRIANGLES,0,columns*rows*6);
    }
    const bool ok=glGetError()==GL_NO_ERROR;
    f->gl.BindFramebuffer(GL_FRAMEBUFFER,static_cast<GLuint>(previousFbo)); glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    f->gl.BindVertexArray(static_cast<GLuint>(previousVao));
    for (int i=0;i<3;++i) { f->gl.ActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(bindings[i])); }
    f->gl.UseProgram(program);
    sampler(*f,program,"uTransportTexture",3,f->image);
    f->gl.Uniform1i(f->gl.GetUniformLocation(program,"uTransportReady"),1);
    f->gl.Uniform1f(f->gl.GetUniformLocation(program,"uTransportFormula"),DesktopTransport_FormulaMix(f));
    f->gl.Uniform1f(f->gl.GetUniformLocation(program,"uTransportPhase"),static_cast<float>(f->phase));
    f->gl.Uniform1i(f->gl.GetUniformLocation(program,"uTransportPhaseReady"),1);
    f->gl.ActiveTexture(static_cast<GLenum>(active));
    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glDepthFunc(static_cast<GLenum>(depthFunc)); glDepthMask(depthMask); glClearColor(clear[0],clear[1],clear[2],clear[3]);
    return ok;
}

unsigned DesktopTransport_Texture(const DesktopTransport* f) { return f ? f->image : 0; }
unsigned DesktopTransport_StateTexture(const DesktopTransport* f) { return f ? f->state[f->current] : 0; }
float DesktopTransport_FormulaMix(const DesktopTransport* f) {
    float x=f ? static_cast<float>(f->formulaAge/4.0) : 0;
    return x*x*(3.0f-2.0f*x);
}
void DesktopTransport_Destroy(DesktopTransport*& f) {
    if (!f) return;
    if (f->step) f->gl.DeleteProgram(f->step);
    if (f->mesh) f->gl.DeleteProgram(f->mesh);
    f->gl.DeleteFramebuffers(2,f->stateFbo);
    f->gl.DeleteFramebuffers(1,&f->parameterFbo); f->gl.DeleteFramebuffers(1,&f->imageFbo);
    glDeleteTextures(2,f->state); glDeleteTextures(1,&f->parameter);
    glDeleteTextures(1,&f->image); glDeleteTextures(1,&f->depth);
    f->gl.DeleteVertexArrays(1,&f->vao); f->gl.DeleteBuffers(1,&f->quadBuffer);
    delete f; f=nullptr;
}
