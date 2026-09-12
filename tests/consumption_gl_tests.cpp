// 直接运行生产 Bloom 的 GLSL，避免仅匹配源码字符串。
#define GLFW_INCLUDE_NONE
#include <windows.h>
#include <GLFW/glfw3.h>
// 单翻译单元测试复用私有 shader 构建函数；该类型不会跨模块传递。
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#include "../src/bloom_renderer.cpp"
#pragma GCC diagnostic pop
#include <cmath>
#include <vector>
#include <fstream>
#include <sstream>
#include "../src/formula_texture.h"
#include "../src/desktop_transport.h"

static std::string ReadText(const char* path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

static int TestConsumption(BloomRenderer& renderer, bool saveSequence) {
    const std::string effect = ReadText("shaders/consumption_palette.glsl")+"\n"+ReadText("shaders/consumption.glsl");
    if (effect.empty()) { std::fprintf(stderr, "FAIL missing consumption shader\n"); return 10; }
    std::string body = ReadText("blackhole.glsl");
    const size_t entry = body.find("void mainImage(");
    if (entry == std::string::npos) return 11;
    body.insert(entry, effect + "\n");
    const std::string source = ReadText("shaders/frag_desktop_header.glsl") + "\n#define CONSUMPTION_DIAGNOSTICS\n" + body + R"GLSL(
uniform float testTime;
uniform float testInclination;
uniform vec2 testCenter;
uniform int testBackgroundOnly;
uniform float testRoll;
uniform int testFlowProbe;
uniform vec4 testFlowPoint;
uniform float testRadius = 0.11;
uniform int testStyle = 0;
void main() {
    if (uTransportPass > 0) {
        fragColor=gl_FragCoord.x<1.0 ? vec4(testRadius,testInclination,testRoll,0.12615663) : vec4(testCenter,0,1);
        return;
    }
    if (testFlowProbe != 0) {
        if (testFlowProbe==9) {
            vec2 p=(gl_FragCoord.xy/iResolution.xy-0.5)*vec2(1.6,1.0);
            float scale=2.5980762/0.11;
            vec3 ray=normalize(vec3(rot(vec2(p.x,-p.y),testRoll)*scale,-55.0));
            fragColor=vec4(abs(consumptionSkyScreenDirection(p)-consumptionSkyRayDirection(ray,testRoll,scale)),1.0);
            return;
        }
        vec2 q = testFlowPoint.xy;
        if (testFlowProbe >= 3) {
            vec2 uv = gl_FragCoord.xy/iResolution.xy;
            vec2 samplePoint = (uv-0.5)*60.0;
            if (testFlowProbe == 6 || testFlowProbe == 7)
                samplePoint = testFlowPoint.x*vec2(cos(uv.x*6.2831853),sin(uv.x*6.2831853));
            if (testFlowProbe == 5) samplePoint = mix(6.0,10.0,uv.y)*vec2(cos(uv.x*6.2831853),sin(uv.x*6.2831853));
            vec4 m = consumptionMaterial(samplePoint,iResolution.x/iResolution.y,
                                        testFlowProbe >= 6 ? testFlowPoint.z : (testFlowProbe == 5 ? 0.0 : 90.0),
                                        testFlowProbe >= 5 ? 0.0 : 2.0,0.004);
            fragColor = testFlowProbe == 6 ? vec4(m.rgb*m.a,1.0)
                      : (testFlowProbe == 3 ? vec4(m.rgb,1.0) : vec4(vec3(m.a),1.0));
            return;
        }
        if (testFlowProbe == 1)
            fragColor = vec4(consumptionAngularSpeed(length(q)),consumptionRadialSpeed(length(q)),
                             consumptionBacktrace(q,testFlowPoint.z));
        else fragColor = vec4(consumptionFlowLabels(q,testFlowPoint.z),0.0,1.0);
        return;
    }
    if (testBackgroundOnly != 0) {
        fragColor = vec4(consumptionDesktop(gl_FragCoord.xy/iResolution.xy, iResolution.x/iResolution.y), 1.0);
        return;
    }
    DiskLook look = LOOK_DEFAULT;
    look.incl = testInclination;
    look.roll = testRoll;
    if (testStyle==1) look.temp=2500.0;
    if (testStyle==2) look.temp=18000.0;
    if (testStyle==3) look.outer=4.0;
    if (testStyle==4) look.outer=14.0;
    if (testStyle==5) { look.gain=0.0; look.star=1.0; }
    if (testStyle==6) { look.gain=0.0; look.star=0.0; }
    if (testStyle==7) look.expo=0.0;
    fragColor = vec4(consumptionRender(vec2(gl_FragCoord.x, iResolution.y-gl_FragCoord.y)/iResolution.xy,
                                    testCenter, testRadius, look), 1.0);
}
)GLSL";
    GLuint program = CreateProgram(renderer, source.c_str());
    if (!program) return 12;
    GLuint formula = CreateFormulaTexture();
    if (!formula) return 13;
    std::vector<unsigned char> atlasPixels(1024*1024*4);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,atlasPixels.data());
    unsigned firstRowInk=0;
    for (int y=0;y<51;++y) for (int x=0;x<1024;++x)
        firstRowInk += atlasPixels[static_cast<size_t>(y*1024+x)*4];
    if (firstRowInk<1000) { std::fprintf(stderr,"FAIL formula glyph baseline misses its atlas row\n"); return 29; }
    constexpr int width = 1280, height = 800;
    GLuint desktop = 0;
    glGenTextures(1, &desktop);
    glBindTexture(GL_TEXTURE_2D, desktop);
    std::vector<unsigned char> pixels(width * height * 4, 255);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const size_t p = static_cast<size_t>(y * width + x) * 4;
        const bool line = x % 80 < 2 || y % 40 < 2;
        pixels[p] = line ? 180 : 18;
        pixels[p+1] = line ? 200 : 24;
        pixels[p+2] = line ? 210 : 30;
    }
    if (saveSequence) {
        std::ifstream fixture("build/flow-desktop-fixture.ppm",std::ios::binary);
        if (fixture) {
            std::string magic;
            int fixtureW=0,fixtureH=0,maximum=0;
            fixture >> magic >> fixtureW >> fixtureH >> maximum;
            fixture.get();
            if (magic!="P6" || fixtureW!=width || fixtureH!=height || maximum!=255) return 30;
            std::vector<unsigned char> rgb(width*height*3);
            fixture.read(reinterpret_cast<char*>(rgb.data()),static_cast<std::streamsize>(rgb.size()));
            if (!fixture) return 31;
            for (int p=0;p<width*height;++p) for (int c=0;c<3;++c) pixels[p*4+c]=rgb[p*3+c];
        }
    }
    const auto capturedDesktop = pixels;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    auto uniform1f = reinterpret_cast<PFNGLUNIFORM1FPROC>(glfwGetProcAddress("glUniform1f"));
    auto uniform3f = reinterpret_cast<PFNGLUNIFORM3FPROC>(glfwGetProcAddress("glUniform3f"));
    auto uniform4f = reinterpret_cast<PFNGLUNIFORM4FPROC>(glfwGetProcAddress("glUniform4f"));
    if (!uniform1f || !uniform3f || !uniform4f) return 14;
    GLuint emptyTransport=0;
    glGenTextures(1,&emptyTransport);
    renderer.gl.ActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D,emptyTransport);
    const float black[4]={};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,1,1,0,GL_RGBA,GL_FLOAT,black);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    std::vector<float> frame(width * height * 4);
    std::vector<float> exhausted;
    std::vector<float> mouseBaseline;
    std::vector<float> formulaBaseline;
    double fullMouseDifference = 0.0;
    for (int index = 0; index < 18; ++index) {
        const float seconds[] = {0, 30, 72, 90, 90, 90, 72, 90, 90, 90, 90, 90, 90, 90, 91, 92, 3600, 86400};
        const float inclinations[] = {1.05f, 1.05f, 1.05f, 1.05f, 0.05f, 1.55f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f, 1.05f};
        Bloom_BeginScene(&renderer, width, height, true);
        renderer.gl.UseProgram(program);
        renderer.gl.ActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D,emptyTransport);
        renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"uTransportTexture"),3);
        renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"uTransportReady"),index>0 ? 1 : 0);
        uniform1f(renderer.gl.GetUniformLocation(program,"uTransportFormula"),seconds[index]>=72 ? 1.0f : 0.0f);
        renderer.gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, desktop);
        renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program, "iChannel0"), 0);
        renderer.gl.ActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, formula);
        renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program, "uFormulaTexture"), 2);
        uniform3f(renderer.gl.GetUniformLocation(program, "iResolution"), width, height, 0);
        uniform1f(renderer.gl.GetUniformLocation(program, "uConsumeTime"), seconds[index]);
        uniform1f(renderer.gl.GetUniformLocation(program, "iTime"), seconds[index]);
        uniform1f(renderer.gl.GetUniformLocation(program, "testInclination"), inclinations[index]);
        uniform1f(renderer.gl.GetUniformLocation(program, "testRoll"), -0.35f+(index == 13 ? 3.14159265f : 0.0f));
        renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program, "testOmitLayer"),
                             index >= 10 && index <= 12 ? index-10 : -1);
        const float mouseEnergy = index == 8 ? 1.0f : (index == 9 ? 0.01f : 0.0f);
        uniform4f(renderer.gl.GetUniformLocation(program, "uFlowMouse"), 0.70f, 0.50f, mouseEnergy, 0.0f);
        renderer.gl.Uniform2f(renderer.gl.GetUniformLocation(program, "uConsumeOrigin"), 0.5f, 0.5f);
        renderer.gl.Uniform2f(renderer.gl.GetUniformLocation(program, "testCenter"), 0.5f, 0.5f);
        renderer.gl.BindVertexArray(renderer.fullscreenVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, frame.data());
        float peak = 0.0f;
        for (float v : frame) { if (!std::isfinite(v)) return 15; peak = std::max(v, peak); }
        if (peak <= 1.0f) { std::fprintf(stderr, "FAIL no HDR light\n"); return 16; }
        if (index == 2) exhausted = frame;
        if (index == 3) formulaBaseline = frame;
        if (index >= 10) {
            double difference = 0.0, total = 0.0;
            for (size_t p = 0; p < frame.size(); p += 4) {
                const size_t reference = index == 13 ? frame.size()-4-p : p;
                for (int c = 0; c < 3; ++c) {
                    difference += std::abs(frame[p+c]-formulaBaseline[reference+c]);
                    total += formulaBaseline[p+c];
                }
            }
            double ratio = difference/std::max(total,0.001);
            std::printf("Layer/rotation probe %d: relative difference=%.5f\n",index,ratio);
            if (index <= 12 && ratio < 0.005) { std::fprintf(stderr,"FAIL layer has no visible contribution\n"); return 22; }
            if (index == 13 && ratio > 0.025) { std::fprintf(stderr,"FAIL layers do not rotate with disk\n"); return 23; }
            if (index >= 14 && ratio < 0.01) { std::fprintf(stderr,"FAIL formula disk does not animate\n"); return 24; }
        }
        if (index == 7) mouseBaseline = frame;
        if (index == 8 || index == 9) {
            double difference = 0.0;
            double distantDifference = 0.0;
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
                const size_t p = static_cast<size_t>(y * width + x) * 4;
                const float dx = (static_cast<float>(x) / width - 0.70f) * width / height;
                const float dy = static_cast<float>(y) / height - 0.50f;
                for (int c = 0; c < 3; ++c) {
                    const double delta = std::abs(frame[p+c] - mouseBaseline[p+c]);
                    difference += delta;
                    if (dx*dx + dy*dy > 0.16f) distantDifference += delta;
                }
            }
            std::printf("Mouse energy %.2f: difference=%.3f distant=%.3f\n", mouseEnergy, difference, distantDifference);
            if (index == 8) {
                fullMouseDifference = difference;
                if (difference < 1.0 || distantDifference > difference * 0.02) return 19;
            } else if (difference >= fullMouseDifference * 0.2) return 20;
        }
        if (index == 6) {
            for (size_t p = 0; p < frame.size(); ++p)
                if (std::abs(frame[p] - exhausted[p]) > 0.005f) {
                    std::fprintf(stderr, "FAIL exhausted desktop returned\n"); return 17;
                }
        }
        Bloom_EndScene(&renderer, width, height, true);
        std::vector<unsigned char> rgb(width * height * 3);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
        std::ofstream file("build/consumption-frame-" + std::to_string(index) + ".ppm", std::ios::binary);
        file << "P6\n" << width << " " << height << "\n255\n";
        for (int y = height-1; y >= 0; --y)
            file.write(reinterpret_cast<char*>(rgb.data()+y*width*3), width*3);
        if (index == 5) {
            renderer.gl.ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, desktop);
            std::fill(pixels.begin(), pixels.end(), 255);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        }
    }
    // 色温/外径影响主盘；吞噬星空独立于黑洞星光预设和盘面曝光，中心仍被捕获。
    std::vector<float> styleFrames[7];
    for (int style=1;style<=7;++style) {
        Bloom_BeginScene(&renderer,width,height,true);
        renderer.gl.UseProgram(program);
        renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"testStyle"),style);
        renderer.gl.BindVertexArray(renderer.fullscreenVao);
        uniform1f(renderer.gl.GetUniformLocation(program,"uTransportFormula"),0);
        uniform1f(renderer.gl.GetUniformLocation(program,"testInclination"),0.5f);
        glDrawArrays(GL_TRIANGLES,0,3);
        styleFrames[style-1].resize(frame.size());
        glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,styleFrames[style-1].data());
    }
    double temperatureDelta=0,extentDelta=0,skyEnergy=0,blackEnergy=0,zeroExposure=0,skyPresetDelta=0;
    double warmLuminance=0,blueLuminance=0;
    for (size_t p=0;p<frame.size();p+=4) for (int c=0;c<3;++c) {
        temperatureDelta+=std::abs(styleFrames[0][p+c]-styleFrames[1][p+c]);
        extentDelta+=std::abs(styleFrames[2][p+c]-styleFrames[3][p+c]);
        skyEnergy+=styleFrames[4][p+c]; blackEnergy+=styleFrames[5][p+c];
        zeroExposure+=styleFrames[6][p+c];
        skyPresetDelta+=std::abs(styleFrames[4][p+c]-styleFrames[5][p+c]);
    }
    for (size_t p=0;p<frame.size();p+=4) {
        const double weights[]={0.2126,0.7152,0.0722};
        for (int c=0;c<3;++c) {
            warmLuminance+=styleFrames[0][p+c]*weights[c];
            blueLuminance+=styleFrames[1][p+c]*weights[c];
        }
    }
    std::printf("Equal-control blue/warm luminance ratio=%.4f\n",blueLuminance/std::max(warmLuminance,0.001));
    if (blueLuminance<warmLuminance*0.65) return 41;
    size_t centerPixel=static_cast<size_t>((height/2)*width+width/2)*4;
    std::printf("Preset pixels temperature=%.2f extent=%.2f sky=%.2f disabled=%.2f core=%.4f\n",
        temperatureDelta,extentDelta,skyEnergy,blackEnergy,styleFrames[4][centerPixel]);
    if (temperatureDelta<100 || extentDelta<100 || skyEnergy<10 || skyPresetDelta>0.01 || std::abs(zeroExposure-blackEnergy)>0.01 || styleFrames[4][centerPixel]>0.001f) return 37;
    renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"testStyle"),0);
    renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"testFlowProbe"),9);
    glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,frame.data());
    for (size_t p=0;p<frame.size();p+=4) for (int c=0;c<3;++c)
        if (frame[p+c]>0.0001f) {std::fprintf(stderr,"FAIL direct and unbent sky mismatch\n");return 38;}
    renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"testFlowProbe"),0);
    Bloom_BeginScene(&renderer, width, height, true);
    renderer.gl.UseProgram(program);
    renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program, "testBackgroundOnly"), 1);
    renderer.gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,desktop);
    renderer.gl.BindVertexArray(renderer.fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, frame.data());
    for (size_t p = 0; p < frame.size(); p += 4) {
        if (!std::isfinite(frame[p]+frame[p+1]+frame[p+2]) || frame[p] + frame[p+1] + frame[p+2] > 0.001f) {
            std::fprintf(stderr, "FAIL formulas drawn in background without a disk surface\n"); return 21;
        }
    }
    if (saveSequence) {
        DesktopTransport* sequenceFlow=DesktopTransport_Create(width,height,stderr);
        if (!sequenceFlow) return 35;
        double simulated=0;
        // 生产网格连续推进历史；不是按目标秒数重新生成一张扭曲桌面。
        for (int sample = 0; sample < 80; ++sample) {
            bool desktopStage = sample < 40;
            float seconds = (desktopStage ? 20.0f : 90.0f)+(sample%40)*0.1f;
            Bloom_BeginScene(&renderer,width,height,true);
            renderer.gl.UseProgram(program);
            renderer.gl.ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,desktop);
            if (sample==0)
                glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,capturedDesktop.data());
            renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"testBackgroundOnly"),0);
            uniform1f(renderer.gl.GetUniformLocation(program,"iTime"),seconds);
            uniform1f(renderer.gl.GetUniformLocation(program,"uConsumeTime"),seconds);
            uniform1f(renderer.gl.GetUniformLocation(program,"testInclination"),1.05f);
            uniform1f(renderer.gl.GetUniformLocation(program,"testRoll"),-0.35f);
            renderer.gl.Uniform2f(renderer.gl.GetUniformLocation(program,"testCenter"),0.5f,0.5f);
            while (simulated+0.0001<seconds) {
                double dt=std::min(0.1,seconds-simulated);
                simulated+=dt;
                uniform1f(renderer.gl.GetUniformLocation(program,"testRadius"),0.017841f+(0.12615663f-0.017841f)*static_cast<float>(std::min(simulated/40.0,1.0)));
                if (!DesktopTransport_Render(sequenceFlow,program,desktop,width,height,dt)) return 36;
            }
            uniform1f(renderer.gl.GetUniformLocation(program,"testRadius"),0.017841f+(0.12615663f-0.017841f)*static_cast<float>(std::min(simulated/40.0,1.0)));
            if (!DesktopTransport_Render(sequenceFlow,program,desktop,width,height,0.0)) return 36;
            renderer.gl.BindVertexArray(renderer.fullscreenVao);
            glDrawArrays(GL_TRIANGLES,0,3);
            Bloom_EndScene(&renderer,width,height,true);
            std::vector<unsigned char> rgb(width*height*3);
            glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
            std::ofstream file(std::string("build/shear-")+(desktopStage ? "desktop-" : "formula-")+std::to_string(sample%40)+".ppm",std::ios::binary);
            if (!file) return 25;
            file << "P6\n" << width << " " << height << "\n255\n";
            for (int y=height-1;y>=0;--y)
                file.write(reinterpret_cast<char*>(rgb.data()+y*width*3),width*3);
        }
        DesktopTransport_Destroy(sequenceFlow);
        renderer.gl.UseProgram(program);
        uniform1f(renderer.gl.GetUniformLocation(program,"testRadius"),0.11f);
        renderer.gl.ActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,emptyTransport);
    }
    // 在真实 GLSL 上测运动规律，并核对独立前向轨迹与反向物质坐标。
    bool probesFinite = true;
    auto probe = [&](int mode, float x, float y, float time) {
        Bloom_BeginScene(&renderer,width,height,true);
        renderer.gl.UseProgram(program);
        renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"testFlowProbe"),mode);
        uniform4f(renderer.gl.GetUniformLocation(program,"testFlowPoint"),x,y,time,0.0f);
        renderer.gl.BindVertexArray(renderer.fullscreenVao);
        glDrawArrays(GL_TRIANGLES,0,3);
        glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,frame.data());
        for (int c=0;c<4;++c) probesFinite = probesFinite && std::isfinite(frame[c]);
        return std::vector<float>(frame.begin(),frame.begin()+4);
    };
    uniform1f(renderer.gl.GetUniformLocation(program,"uTransportFormula"),1.0f);
    renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"uConsumptionFormulaEnabled"),0);
    auto disabledFormula=probe(5,0,0,90);
    if (disabledFormula[0]>0.001f) {std::fprintf(stderr,"FAIL formula option ignored\n");return 39;}
    renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"uConsumptionFormulaEnabled"),1);
    auto enabledFormula=probe(5,0,0,90);
    if (enabledFormula[0]<0.1f) return 40;
    const auto nearFlow=probe(1,6.0f,0.0f,0.0f);
    const auto middleFlow=probe(1,12.0f,0.0f,0.0f);
    const auto farFlow=probe(1,24.0f,0.0f,0.0f);
    std::printf("Flow omega near/mid/far: %.5f %.5f %.5f\n",nearFlow[0],middleFlow[0],farFlow[0]);
    if (std::abs(nearFlow[0]/farFlow[0]-8.0f)>0.025f
        || std::abs(nearFlow[0]/middleFlow[0]-std::sqrt(8.0f))>0.015f
        || nearFlow[1]>=0.0f || std::abs(nearFlow[1]/farFlow[1]-2.0f)>0.015f) return 26;
    const double r0=20.0, phi0=0.35, elapsed=3.0, inflow=2.4, sqrtGM=22.627417;
    const double r=std::pow(std::pow(r0,1.5)-1.5*inflow*elapsed,2.0/3.0);
    const double phi=phi0+sqrtGM/inflow*std::log(r0/r);
    const float x=static_cast<float>(r*std::cos(phi)), y=static_cast<float>(r*std::sin(phi));
    const auto traced=probe(1,x,y,static_cast<float>(elapsed));
    if (std::abs(traced[2]-r0*std::cos(phi0))>0.04 || std::abs(traced[3]-r0*std::sin(phi0))>0.04) return 27;
    const auto startLabels=probe(2,static_cast<float>(r0*std::cos(phi0)),static_cast<float>(r0*std::sin(phi0)),0.0f);
    const auto endLabels=probe(2,x,y,static_cast<float>(elapsed));
    if (std::abs(startLabels[0]-endLabels[0])>0.015f || std::abs(startLabels[1]-endLabels[1])>0.08f) return 28;
    std::puts("KEPLER_ADVECTION_GPU_OK");
    renderer.gl.UseProgram(program);
    uniform1f(renderer.gl.GetUniformLocation(program,"uConsumeTime"),0.0f);
    uniform1f(renderer.gl.GetUniformLocation(program,"uTransportFormula"),0.0f);
    renderer.gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,desktop);
    probe(5,0,0,0);
    glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,frame.data());
    int gaps=0,filled=0;
    for (size_t p=0;p<frame.size();p+=4) {
        if (!std::isfinite(frame[p])) return 32;
        if (frame[p]<0.01f) ++gaps;
        if (frame[p]>0.1f) ++filled;
    }
    const double gapRatio=static_cast<double>(gaps)/(width*height);
    const double filledRatio=static_cast<double>(filled)/(width*height);
    std::printf("Desktop not duplicated on formula surfaces: empty=%.3f filled=%.3f\n",gapRatio,filledRatio);
    if (gapRatio<0.999 || filledRatio>0.001) return 33;
    renderer.gl.UseProgram(program);
    uniform1f(renderer.gl.GetUniformLocation(program,"uConsumeTime"),90.0f);
    uniform1f(renderer.gl.GetUniformLocation(program,"uTransportFormula"),1.0f);
    if (saveSequence) for (int mode=3;mode<=4;++mode) {
        renderer.gl.UseProgram(program);
        renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"testOmitLayer"),-2);
        renderer.gl.ActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D,formula);
        if (mode==3) {
            std::vector<unsigned char> atlas(1024*1024*3);
            glGetTexImage(GL_TEXTURE_2D,0,GL_RGB,GL_UNSIGNED_BYTE,atlas.data());
            std::ofstream atlasFile("build/flow-atlas.ppm",std::ios::binary);
            atlasFile << "P6\n1024 1024\n255\n";
            atlasFile.write(reinterpret_cast<char*>(atlas.data()),static_cast<std::streamsize>(atlas.size()));
        }
        probe(mode,0,0,0);
        std::vector<unsigned char> rgb(width*height*3);
        glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
        std::ofstream file("build/flow-material-"+std::to_string(mode)+".ppm",std::ios::binary);
        file << "P6\n" << width << " " << height << "\n255\n";
        file.write(reinterpret_cast<char*>(rgb.data()),static_cast<std::streamsize>(rgb.size()));
    }
    // 直接测最终材质，不让曲面高度和内盘动画代替纹理运动验收。
    renderer.gl.UseProgram(program);
    renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program,"testOmitLayer"),-1);
    bool materialChecks = true;
    for (float seconds : {64.0f,66.0f,68.0f,70.0f,72.0f}) {
        uniform1f(renderer.gl.GetUniformLocation(program,"uTransportFormula"),0.0f);
        uniform1f(renderer.gl.GetUniformLocation(program,"uConsumeTime"),seconds);
        probe(8,0,0,seconds);
        glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,frame.data());
        double energy=0;
        for (size_t p=0;p<frame.size();p+=4) {
            probesFinite = probesFinite && std::isfinite(frame[p]);
            energy += frame[p];
        }
        if (!(energy<0.001)) { std::fprintf(stderr,"FAIL formula starts by wall clock before transport drains at %.1f seconds\n",seconds); materialChecks=false; }
    }
    uniform1f(renderer.gl.GetUniformLocation(program,"uTransportFormula"),1.0f);
    uniform1f(renderer.gl.GetUniformLocation(program,"uConsumeTime"),90.0f);
    std::vector<float> seamSide;
    for (int lane=-33;lane<=-12;lane+=3) for (double offset : {-0.00001,0.00001}) {
        // 编号切换仍连续覆盖，不以透明裂隙隐藏相位差。
        const double travel=90.0+(lane+offset)*2.5;
        const float radius=static_cast<float>(std::pow(std::pow(38.0,1.5)-3.6*travel,2.0/3.0));
        probe(7,radius,0,90.0f);
        glReadPixels(0,0,width,1,GL_RGBA,GL_FLOAT,frame.data());
        for (int p=0;p<width*4;p+=4) if (!std::isfinite(frame[p]) || frame[p]<0.199f) {
            std::fprintf(stderr,"FAIL artificial gap at formula lane boundary %d\n",lane); materialChecks=false; break;
        }
        if (offset<0) seamSide.assign(frame.begin(),frame.begin()+width*4);
        else for (int p=0;p<width*4;p+=4) if (std::abs(frame[p]-seamSide[p])>0.02f) {
            std::fprintf(stderr,"FAIL formula phase jumps at lane boundary %d\n",lane); materialChecks=false; break;
        }
    }
    // 单频标记图集：比较同一束中心沿径向内落前后的角向位移。
    renderer.gl.ActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,formula);
    std::vector<unsigned char> marker(1024*1024*4,255);
    for (int y=0;y<1024;++y) for (int x=0;x<1024;++x)
        for (int c=0;c<3;++c) marker[static_cast<size_t>(y*1024+x)*4+c]=
            static_cast<unsigned char>(127.5*(1.0+std::cos(6.28318530718*x/1024.0)));
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1024,1024,GL_RGBA,GL_UNSIGNED_BYTE,marker.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    double measured[2]={};
    int ringIndex=0;
    for (int lane : {-13,-32}) {
        const double injection=(-lane-0.5)*2.5;
        const double radius0=std::pow(std::pow(38.0,1.5)-3.6*(90.0-injection),2.0/3.0);
        constexpr double dt=0.1;
        const double radius1=std::pow(std::pow(radius0,1.5)-3.6*dt,2.0/3.0);
        std::vector<float> rings[2];
        for (int sample=0;sample<2;++sample) {
            probe(6,static_cast<float>(sample ? radius1 : radius0),0,static_cast<float>(90.0+sample*dt));
            glReadPixels(0,0,width,1,GL_RGBA,GL_FLOAT,frame.data());
            rings[sample].assign(frame.begin(),frame.begin()+width*4);
        }
        float minimum=1e9f,maximum=0;
        for (int p=0;p<width;++p) {minimum=std::min(minimum,rings[0][p*4]);maximum=std::max(maximum,rings[0][p*4]);}
        const float threshold=(minimum+maximum)*0.5f;
        int repeats=0;
        for (int p=0;p<width;++p) if (rings[0][p*4]<=threshold && rings[0][((p+1)%width)*4]>threshold) ++repeats;
        const int requiredRepeats=ringIndex==0 ? 12 : 3;
        std::printf("Formula marker angular repeats: %d (required %d, inner small / outer large)\n",repeats,requiredRepeats);
        if (repeats!=requiredRepeats) materialChecks=false;
        double bestError=1e30;
        int bestShift=0;
        for (int shift=0;shift<30;++shift) {
            double error=0;
            for (int p=0;p<width;++p) {
                const double delta=rings[1][((p+shift)%width)*4]-rings[0][p*4];
                error+=delta*delta;
            }
            if (error<bestError) {bestError=error;bestShift=shift;}
        }
        measured[ringIndex++]=bestShift*6.28318530718/width/dt;
        const double expected=sqrtGM/inflow*std::log(radius0/radius1)/dt;
        std::printf("Actual material r=%.3f: omega=%.4f expected=%.4f\n",radius0,measured[ringIndex-1],expected);
        if (!std::isfinite(bestError) || std::abs(measured[ringIndex-1]-expected)>0.035) materialChecks=false;
    }
    if (!(measured[0]>measured[1]*4.0) || !probesFinite || !materialChecks) return 34;
    std::puts("MATERIAL_SHEAR_AND_HANDOFF_GPU_OK");
    glDeleteTextures(1, &desktop);
    glDeleteTextures(1, &emptyTransport);
    glDeleteTextures(1, &formula);
    renderer.gl.DeleteProgram(program);
    return glGetError() == GL_NO_ERROR ? 0 : 18;
}

void* Win32GL_GetProcAddress(const char* name) {
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

int main(int argc, char** argv) {
    if (!glfwInit()) return 2;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow* window = glfwCreateWindow(1280, 800, "GPU regression", nullptr, nullptr);
    if (!window) return 3;
    glfwMakeContextCurrent(window);
    std::printf("GPU: %s\n", glGetString(GL_RENDERER));
    BloomRenderer* bloom = Bloom_Create(128, 128, stderr);
    if (!bloom) return 4;
    for (float level : {0.0f, 0.25f, 0.5f, 1.0f}) {
        if (!Bloom_BeginScene(bloom, 128, 128, true)) return 5;
        glClearColor(level, level, level, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Bloom_EndScene(bloom, 128, 128, true);
        float pixel[4] = {};
        glReadPixels(64, 64, 1, 1, GL_RGBA, GL_FLOAT, pixel);
        if (!std::isfinite(pixel[0]) || std::abs(pixel[0] - level) > 0.008f) {
            std::fprintf(stderr, "FAIL ordinary desktop glows: input=%.3f output=%.3f\n", level, pixel[0]);
            return 6;
        }
    }
    Bloom_BeginScene(bloom, 128, 128, true);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(60, 60, 8, 8);
    glClearColor(5, 4, 2, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    Bloom_EndScene(bloom, 128, 128, true);
    float halo[4] = {};
    glReadPixels(70, 64, 1, 1, GL_RGBA, GL_FLOAT, halo);
    if (!(halo[0] > 0.01f && halo[0] < 0.9f) || glGetError() != GL_NO_ERROR) return 7;
    const bool saveSequence = argc > 1 && std::string(argv[1]) == "--sequence";
    const int consumptionResult = TestConsumption(*bloom,saveSequence);
    Bloom_Destroy(bloom);
    glfwDestroyWindow(window);
    glfwTerminate();
    if (consumptionResult) return consumptionResult;
    std::puts("BLOOM_AND_CONSUMPTION_GPU_OK");
    return 0;
}
