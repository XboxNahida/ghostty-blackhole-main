#version 330 core
uniform sampler2D desktopTexture;
uniform sampler2D parameters;
in vec2 sourceUV;
in float alive;
in float materialDepth;
in vec3 materialPosition;
in vec2 projectedPoint;
flat in vec3 visibleShadow;
out vec4 material;
void main() {
    // 活死混合面延伸到三维捕获边界，不在插值中点切断成锯齿。
    if (alive <= 0.0) discard;
    // 不能按屏幕圆形剪去前景盘带；捕获球与盘面的前后顺序由合成器判断。
    vec3 pigment=texture(desktopTexture,sourceUV).rgb;
    if (visibleShadow.z>0.00001) {
        vec4 disk=texelFetch(parameters,ivec2(0,0),0);
        vec4 thermal=texelFetch(parameters,ivec2(1,0),0);
        vec4 optical=texelFetch(parameters,ivec2(2,0),0);
        vec4 detail=texelFetch(parameters,ivec2(3,0),0);
        bool styled=thermal.z>100.0;
        float temperature=styled ? thermal.z : 5500.0;
        float inner=styled ? max(thermal.w,1.6) : 1.8;
        float outer=styled ? max(optical.x,inner+0.5) : 8.0;
        float sn=sin(disk.y),cs=cos(disk.y),sr=sin(disk.z),cr=cos(disk.z);
        // 与输运步进使用同一三维盘面基，半径和高度不能由屏幕圆形距离替代。
        vec3 axisX=vec3(cr,-sr,0.0),axisY=vec3(sr*cs,cr*cs,-sn);
        vec3 normal=vec3(sr*sn,cr*sn,cs);
        vec2 plane=vec2(dot(materialPosition,axisX),dot(materialPosition,axisY));
        // 和主盘相同的 Schwarzschild 半径单位，离盘高度使接近区跟随倾角。
        float radius=length(plane)/visibleShadow.z*2.598076211;
        float height=abs(dot(materialPosition,normal))/visibleShadow.z*2.598076211;
        float heated=consumptionHeat(radius,height,inner,outer);
        float angle=atan(plane.y,plane.x);
        float beta=sqrt(0.5/max(radius,3.0));
        float doppler=mix(1.0,sqrt(1.0-beta*beta)/(1.0-0.55*beta*sin(disk.y)*cos(angle)),
                          styled ? clamp(optical.z,0.0,1.0) : 0.6);
        // 使用 consumptionRender 主盘相同的暖金到白金色，避免独立谱色变成灰蓝盖片。
        vec3 gold=consumptionDiskColor(temperature,inner,radius,doppler);
        vec3 emission=gold*(0.45+0.30*heated)*pow(doppler,styled ? max(optical.w,0.0) : 2.5);
        emission*=styled ? max(detail.x,0.0)/2.2 : 1.0;
        pigment=mix(pigment,emission,heated);
    }
    // alpha 仍只编码网格深度；HDR 辐射由现有 Bloom 处理。
    material=vec4(pigment,clamp(0.5+materialDepth*0.125,0.01,0.99));
}
