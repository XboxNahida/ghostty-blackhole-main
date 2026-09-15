// 桌面暖化和发光盘共享颜色、融合范围，避免两条渲染路径各自定义色相。
vec3 consumptionBlackbody(float temperature) {
    float t=clamp(temperature,1500.0,40000.0)/100.0;
    float r=t<=66.0 ? 1.0 : clamp(1.292936*pow(t-60.0,-0.1332047),0.0,1.0);
    float g=t<=66.0 ? clamp(0.3900816*log(t)-0.6318414,0.0,1.0)
                   : clamp(1.129891*pow(t-60.0,-0.0755148),0.0,1.0);
    float b=t>=66.0 ? 1.0 : (t<=19.0 ? 0.0 : clamp(0.543207*log(t-10.0)-1.196254,0.0,1.0));
    return vec3(r,g,b);
}
vec3 consumptionDiskColor(float temperature,float inner,float radius,float shift) {
    float innerLight=exp(-max(radius/inner-1.0,0.0)*1.62);
    vec3 gold=mix(vec3(0.72,0.49,0.23),vec3(1.0,0.97,0.86),sqrt(innerLight));
    // 5500 K 保留参考暖金色，其他预设通过统一色温比值改变盘和物质的色相。
    vec3 tint=consumptionBlackbody(temperature*shift)/consumptionBlackbody(5500.0);
    // 色温只改变色相；预设 gain/exposure 负责能量，避免高温蓝色被再次压暗。
    tint/=max(dot(tint,vec3(0.2126,0.7152,0.0722)),0.1);
    return gold*tint;
}
float consumptionHeat(float radius,float height,float inner,float outer) {
    float nearEdge=max(5.0,inner*2.7);
    float heightWeight=mix(1.0,4.0,smoothstep(3.5,5.0,length(vec2(radius,height))));
    return 1.0-smoothstep(nearEdge,max(outer*1.75,nearEdge+0.5),length(vec2(radius,height*heightWeight)));
}
float consumptionOpacity(float radius,float height,float inner,float outer) {
    float nearEdge=max(3.5,inner*1.9);
    // 近洞热化区使用真实三维距离；高度乘四会使近洞竖向入流保持不透明，切断光环。
    float heightWeight=mix(1.0,4.0,smoothstep(3.5,5.0,length(vec2(radius,height))));
    return smoothstep(nearEdge,max(outer,nearEdge+0.5),length(vec2(radius,height*heightWeight)));
}
