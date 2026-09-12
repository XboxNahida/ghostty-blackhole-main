// 实验模式：固定桌面快照的有限输运、弯曲流面和白金色盘面。
uniform float uConsumeTime = 0.0;
uniform vec2 uConsumeOrigin = vec2(0.5);
uniform vec2 uConsumeCenter = vec2(0.5);
uniform sampler2D uFormulaTexture;
uniform vec4 uFlowMouse = vec4(-10.0, -10.0, 0.0, 0.0);

// 漂移幅度独立于黑洞生长，移动时钟为零时始终停留在出生位置。
vec2 consumptionCenter(vec2 home, float movementTime) {
    vec2 room = min(vec2(0.15), max(min(home, 1.0-home), vec2(0.0)));
    return home + room * vec2(sin(movementTime*0.17), sin(movementTime*0.13));
}

float consumptionFarRadius(float aspect) {
    return length(max(uConsumeOrigin, 1.0-uConsumeOrigin) * vec2(aspect, 1.0)) + 0.025;
}

float consumptionFormulaMix() {
    return smoothstep(72.0, 80.0, uConsumeTime);
}

float consumptionPathMask(vec2 uv, float aspect) {
    float farRadius = consumptionFarRadius(aspect);
    vec2 scale = vec2(aspect, 1.0);
    float originRadius = length((uv-uConsumeOrigin) * scale);
    float centerRadius = length((uv-uConsumeCenter) * scale);
    float front = farRadius * clamp(uConsumeTime / 60.0, 0.0, 1.0);
    float originMask = smoothstep(front-0.018, front+0.018, originRadius);
    float centerMask = smoothstep(front-0.028, front+0.028, centerRadius);
    vec2 path = uConsumeCenter-uConsumeOrigin;
    float pathLen = max(dot(path,path), 1e-5);
    float along = clamp(dot(uv-uConsumeOrigin,path)/pathLen, 0.0, 1.0);
    vec2 nearest = uConsumeOrigin + path*along;
    float pathSoft = smoothstep(0.055, 0.0, length((uv-nearest)*scale));
    return max(originMask, mix(centerMask, 0.0, pathSoft*0.72));
}

vec3 consumptionFormulaBackground(vec2 uv, float aspect) {
    float flow = iTime*0.006;
    vec2 p = uv*vec2(aspect, 1.0);
    float warp = 0.035*sin(p.y*11.0+iTime*0.05) + 0.02*sin(p.x*17.0-iTime*0.035);
    vec2 fuv = fract(vec2(uv.x*1.35 + flow + warp, uv.y*2.8 - flow*0.42 + warp*0.35));
    float ink0 = texture(uFormulaTexture, fuv).r;
    float ink1 = texture(uFormulaTexture, fract(fuv*1.73+vec2(0.17,0.31))).r;
    float ink = max(ink0, ink1*0.55);
    float vignette = 0.30 + 0.70*smoothstep(0.0, 0.28, min(min(uv.x,1.0-uv.x),min(uv.y,1.0-uv.y)));
    return vec3(0.98,0.92,0.78)*ink*vignette*0.72;
}

vec3 consumptionDesktop(vec2 uv, float aspect) {
    float remaining = consumptionPathMask(uv, aspect);
    vec3 desktop = texture(iChannel0, uv).rgb * remaining;
    float formula = consumptionFormulaMix();
    return mix(desktop, consumptionFormulaBackground(uv, aspect), formula);
}

float consumptionHeight(vec2 q, float phase) {
    float r = length(q);
    float phi = atan(q.y, q.x);
    float envelope = smoothstep(4.5, 16.0, r);
    return envelope * (1.6*sin(phi*2.0-r*0.24+phase*0.24)
                     + 0.65*sin(phi*3.0+r*0.35-phase*0.17));
}

// 返回亮度纹理及连续边缘遮罩。桌面采样超出快照矩形即透明，禁止镜像回卷。
vec4 consumptionMaterial(vec2 q, float aspect, float phase) {
    float radius = length(q);
    float phi = atan(q.y, q.x);
    float farRadius = consumptionFarRadius(aspect);
    float outward = farRadius * max(uConsumeTime-12.0, 0.0) / 60.0;
    float transport = outward + radius * farRadius / 28.0;
    float twist = 2.8*log(max(radius, 1.0)) - phase*0.12;
    vec2 source = uConsumeOrigin + transport * vec2(cos(phi+twist)/aspect, sin(phi+twist));
    vec2 edge = min(source, 1.0-source);
    float inside = smoothstep(0.0, 0.012, min(edge.x, edge.y));
    float desktopWeight = inside * (1.0-smoothstep(71.5, 72.0, uConsumeTime));
    vec3 desktop = texture(iChannel0, clamp(source, vec2(0.0), vec2(1.0))).rgb;
    float ink = texture(uFormulaTexture, fract(vec2(phi/6.2831853*2.0 + 0.025*radius-phase*0.014,
                                                  radius*0.030-phase*0.003))).r;
    float formula = consumptionFormulaMix();
    vec3 letters = vec3(0.98, 0.94, 0.82) * ink;
    return vec4(desktop * desktopWeight + letters * formula,
                max(desktopWeight * 0.82, formula * (0.025 + ink*0.86)));
}

vec3 consumptionRender(vec2 uv, vec2 center, float rh, DiskLook look) {
    float aspect = iResolution.x/iResolution.y;
    vec3 background = consumptionDesktop(uv, aspect);
    if (rh < 0.0002) return background;
    vec2 p = (uv-center)*vec2(aspect, 1.0);
    float worldScale = B_CRIT / max(rh, 0.0002);
    vec2 pr = rot(vec2(p.x, -p.y), look.roll)*worldScale;
    float b = length(pr);
    float farFade = 1.0-smoothstep(22.0, 29.0, b);
    if (farFade <= 0.0) return background;
    // 消费进度可封顶；盘面动画使用连续时钟，不在每小时跳回第一帧。
    float phase = iTime;
    vec3 x = vec3(pr, 36.0), v = vec3(0.0, 0.0, -1.0);
    vec3 normal = vec3(0.0, sin(look.incl), cos(look.incl));
    vec3 axis = vec3(0.0, cos(look.incl), -sin(look.incl));
    float h2 = dot(pr, pr);
    vec3 color = vec3(0.0);
    float transmission = 1.0;
    bool captured = false;
    float mouseDistance = length((uv-uFlowMouse.xy)*vec2(aspect,1.0));
    float mouseSpot = exp(-mouseDistance*mouseDistance/0.009) * clamp(uFlowMouse.z,0.0,1.0);
    // 固定迭代预算；中心近处细分，远处粗步进。
    for (int step = 0; step < 96; ++step) {
        float r2 = dot(x,x);
        if (r2 < 1.0) { captured = true; break; }
        if ((x.z < -36.0 && v.z < 0.0) || r2 > 6400.0 || transmission < 0.008) break;
        float r = sqrt(r2);
        float dt = clamp(0.14*r, 0.025, 2.3);
        vec3 previous = x;
        vec3 accel = -1.5*h2*x / max(r2*r2*r, 0.01);
        v += accel*(0.5*dt);
        x += v*dt;
        r2 = max(dot(x,x),0.01);
        v += (-1.5*h2*x/(r2*r2*sqrt(r2)))*(0.5*dt);
        float disk0 = dot(previous,normal), disk1 = dot(x,normal);
        vec2 q0 = vec2(previous.x,dot(previous,axis));
        vec2 q1 = vec2(x.x,dot(x,axis));
        float sheet0 = disk0-consumptionHeight(q0,phase)-mouseSpot*0.55;
        float sheet1 = disk1-consumptionHeight(q1,phase)-mouseSpot*0.55;
        float td = disk0*disk1 < 0.0 ? disk0/(disk0-disk1) : 2.0;
        float ts = sheet0*sheet1 < 0.0 ? sheet0/(sheet0-sheet1) : 2.0;
        // 同一步内两个交点按深度排序，确保前景流面能遮住后景。
        for (int hit = 0; hit < 2; ++hit) {
            bool diskFirst = td <= ts;
            bool disk = hit == 0 ? diskFirst : !diskFirst;
            float fraction = disk ? td : ts;
            if (fraction > 1.0) continue;
            vec3 point = mix(previous,x,fraction);
            vec2 q = vec2(point.x,dot(point,axis));
            float rc = length(q);
            if (disk) {
                float inner = 1.75;
                float band = smoothstep(inner,2.2,rc)*(1.0-smoothstep(4.0,8.5,rc));
                float angle = atan(q.y,q.x);
                float flow = angle-0.65*phase/pow(max(rc/inner,1.0),1.5);
                float detail = vnoiseWrapY(vec2(rc*5.5,flow*3.024),19.0);
                float filaments = 0.75+0.25*sin(rc*31.0+detail*5.0+sin(flow*3.0));
                float density = band*(0.38+0.62*detail)*filaments;
                float innerLight = exp(-(rc-inner)*0.80);
                float side = 0.82+0.26*q.x/max(rc,0.01);
                vec3 gold = mix(vec3(0.72,0.49,0.23),vec3(1.0,0.97,0.86),sqrt(innerLight));
                color += transmission*gold*density*(0.55+innerLight*5.5)*side;
                transmission *= 1.0-clamp(density*0.68,0.0,0.85);
            } else {
                float extent = smoothstep(3.0,7.0,rc)*(1.0-smoothstep(24.0,31.0,rc));
                if (extent <= 0.0) continue;
                vec4 material = consumptionMaterial(q,aspect,phase);
                float grooves = 0.5+0.5*sin(rc*17.0+2.0*sin(atan(q.y,q.x)*3.0-phase*0.2));
                float light = 0.65+0.35*exp(-max(rc-4.0,0.0)*0.09);
                float transition = smoothstep(2.0,8.0,rc) * (1.0-smoothstep(26.0,34.0,rc));
                vec3 reflected = material.rgb * vec3(1.0,0.96,0.88) * light;
                vec3 haze = material.rgb * vec3(0.52,0.46,0.35) * (0.18 + 0.22*grooves);
                vec3 sheen = vec3(1.0,0.89,0.63)*(0.035*grooves+mouseSpot*1.8)*material.a;
                color += transmission*(extent*(reflected+sheen) + transition*haze*0.62);
                transmission *= 1.0-clamp((extent*material.a + transition*0.12),0.0,0.97);
            }
        }
    }
    if (!captured && dot(x,x)<4.0) captured=true;
    if (!captured) color += transmission*background;
    // 薄光子环，宽光晕交给 HDR Bloom；不覆盖已累积的前景盘带。
    float ring = exp(-pow((b-B_CRIT*1.012)/0.065,2.0));
    color += vec3(1.0,0.91,0.70)*ring*1.9;
    return mix(background,color,farFade);
}
