// 实验模式：固定桌面快照的有限输运、弯曲流面和白金色盘面。
uniform float uConsumeTime = 0.0;
uniform vec2 uConsumeOrigin = vec2(0.5);
uniform vec2 uConsumeCenter = vec2(0.5);
uniform sampler2D uFormulaTexture;
uniform vec4 uFlowMouse = vec4(-10.0, -10.0, 0.0, 0.0);
uniform int uTransportPass = 0;
uniform int uTransportReady = 0;
uniform sampler2D uTransportTexture;
uniform float uTransportFormula = 0.0;
uniform int uConsumptionFormulaEnabled = 1;
uniform float uTransportPhase = 0.0;
uniform int uTransportPhaseReady = 0;
// 吞噬后露出的背景独立于黑洞预设的 STAR_GAIN，不修改用户预设。
const float CONSUMPTION_SKY_GAIN = 0.8;

// 漂移幅度独立于黑洞生长，移动时钟为零时始终停留在出生位置。
vec2 consumptionCenter(vec2 home, float movementTime) {
    return mix(home,vec2(0.5),smoothstep(0.0,18.0,movementTime));
}

float consumptionFormulaMix() {
    return uConsumptionFormulaEnabled>0 ? uTransportFormula : 0.0;
}

vec3 consumptionDesktop(vec2 uv, float aspect) {
    // 已移动的单份网格决定哪里有纹理；没有径向黑化遮罩，也不再复制原桌面垫底。
    if (uTransportReady == 0) return texture(iChannel0,uv).rgb;
    return texture(uTransportTexture,vec2(uv.x,1.0-uv.y)).rgb;
}

vec3 consumptionSkyScreenDirection(vec2 screenPoint) {
    return normalize(vec3(screenPoint.x*8.0,-screenPoint.y*8.0,-1.0));
}
vec3 consumptionSkyRayDirection(vec3 ray,float roll,float worldScale) {
    vec3 v=normalize(ray);
    vec2 plane=rot(v.xy,-roll)/max(abs(v.z),0.05);
    return normalize(vec3(plane*(55.0/worldScale)*8.0,-1.0));
}
vec3 consumptionSky(vec3 direction,float gain) {
    vec3 d=normalize(direction);
    // 多密度恒星层，位置固定于远方球面；捕获和透射仍由主光线决定。
    vec3 sky=vec3(0.0);
    vec2 sphere=vec2(atan(d.x,-d.z),asin(clamp(d.y,-1.0,1.0)));
    for (int layer=0;layer<3;++layer) {
        float grid=90.0*pow(2.1,float(layer));
        vec2 cell=sphere*grid,id=floor(cell);
        float seed=hash21(id+float(layer)*51.7);
        vec2 offset=vec2(hash21(id+17.3),hash21(id+31.7))*0.7+0.15;
        float distance=length(fract(cell)-offset);
        float width=max(0.07,grid/iResolution.y*0.07);
        float spark=exp(-distance*distance/(width*width));
        float selected=smoothstep(0.94,0.98,seed);
        vec3 tint=mix(vec3(1.0,0.87,0.70),vec3(0.73,0.85,1.0),hash21(id+2.9));
        sky+=tint*spark*selected*(0.8/float(layer+1));
    }
    return sky*max(gain,0.0);
}

float consumptionHeight(vec2 q, float phase, float layer) {
    float r = length(q);
    float phi = atan(q.y, q.x);
    float envelope = smoothstep(3.0, 15.0, r);
    float spiral = phi-0.15*r+phase*0.055+layer*2.1;
    // 三张独立曲面在内侧汇入同一薄盘，外侧有真实高度差和不同的褶皱。
    return envelope*((layer-1.0)*4.2 + (2.1+layer*0.5)*sin(spiral)
                     + 1.1*sin(phi*2.0+r*0.19-phase*0.04+layer*1.7));
}

float consumptionNoise(vec2 p) {
    return vnoiseWrapY(p, 4096.0)*0.57 + vnoiseWrapY(p*2.07+19.3,4096.0)*0.29
         + vnoiseWrapY(p*4.13+7.1,4096.0)*0.14;
}

// 外盘运动学近似，r 的单位沿用 Schwarzschild 半径；时间是观感标定秒。
const float CONSUMPTION_SQRT_GM = 22.627417;
const float CONSUMPTION_INFLOW = 2.4;
const float CONSUMPTION_FEED_RADIUS = 38.0;
float consumptionAngularSpeed(float radius) {
    return CONSUMPTION_SQRT_GM / pow(max(radius,3.0),1.5);
}
float consumptionRadialSpeed(float radius) {
    return -CONSUMPTION_INFLOW / sqrt(max(radius,3.0));
}
vec2 consumptionBacktrace(vec2 q, float age) {
    float r = max(length(q),3.0);
    float r0 = pow(pow(r,1.5)+1.5*CONSUMPTION_INFLOW*max(age,0.0),2.0/3.0);
    float theta0 = atan(q.y,q.x)-CONSUMPTION_SQRT_GM/CONSUMPTION_INFLOW*log(r0/r);
    return r0*vec2(cos(theta0),sin(theta0));
}
vec2 consumptionFlowLabels(vec2 q, float time) {
    float r = max(length(q),3.0);
    float travel = (pow(CONSUMPTION_FEED_RADIUS,1.5)-pow(r,1.5))/(1.5*CONSUMPTION_INFLOW);
    float theta = atan(q.y,q.x)-CONSUMPTION_SQRT_GM/CONSUMPTION_INFLOW*log(CONSUMPTION_FEED_RADIUS/r);
    return vec2(theta,time-travel);
}

#ifdef CONSUMPTION_DIAGNOSTICS
uniform int testOmitLayer = -1;
#endif

float consumptionLaneOrbit(float id, float phase, float layer, float angle) {
    float injection=(layer*0.31-id-0.5)*(2.5+layer*1.25);
    float radius=pow(max(pow(CONSUMPTION_FEED_RADIUS,1.5)
                    -1.5*CONSUMPTION_INFLOW*(phase-injection),pow(3.0,1.5)),2.0/3.0);
    return angle-CONSUMPTION_SQRT_GM/CONSUMPTION_INFLOW*log(CONSUMPTION_FEED_RADIUS/radius)
                -consumptionAngularSpeed(CONSUMPTION_FEED_RADIUS)*injection;
}

// 返回亮度纹理及连续边缘遮罩。桌面采样超出快照矩形即透明，禁止镜像回卷。
vec4 consumptionMaterialScale(vec2 q, float aspect, float phase, float layer, float footprint,float formulaScale) {
    float radius = max(length(q),3.0);
    // 桌面由持久网格一次绘制；这三张光学曲面只承载耗尽后补给的公式。
    // 外缘连续补给的物质标签沿轨迹守恒；内快外慢而不累计无限剪切。
    vec2 labels = consumptionFlowLabels(q,phase);
    float boundaryOmega = consumptionAngularSpeed(CONSUMPTION_FEED_RADIUS);
    float exactOrbit = labels.x-boundaryOmega*labels.y;
    float feedDerivative = sqrt(radius)/CONSUMPTION_INFLOW;
    float feedInterval = 2.5+layer*1.25;
    float across = -labels.y/feedInterval+layer*0.31;
    // 窄束中心沿解析轨道推进，束内保留部分剪切；避免一整行字被连续拉成细丝。
    float laneOrbit = consumptionLaneOrbit(floor(across),phase,layer,atan(q.y,q.x));
    float orbit = mix(laneOrbit,exactOrbit,0.15)+layer*2.1;
    // 相邻轨道交界只混合纹理相位，不降低材质覆盖率，不再人为挖出黑缝。
    float adjacent = floor(across)+(fract(across)<0.5 ? -1.0 : 1.0);
    float otherOrbit = mix(consumptionLaneOrbit(adjacent,phase,layer,atan(q.y,q.x)),exactOrbit,0.15)+layer*2.1;
    float blend = 0.5*(1.0-smoothstep(0.0,0.18,min(fract(across),1.0-fract(across))));
    // 整周整数次数重复避免角度接缝；层间字号和径向位置不同。
    float repeats = (6.0-layer*2.0)*formulaScale;
    // 一条流束对应图集的一行，保持文字主体在流束内部，裂隙不会随机抹掉整行。
    float lane = across;
    vec2 formulaUV = vec2(orbit/6.2831853*repeats,((lane-0.06)/20.0+layer*0.2)*formulaScale);
    // 求交循环的分支不保证相邻像素导数有效，显式按足迹选 mip，避免远景公式闪成碎点。
    float angularRate = repeats/(6.2831853*radius);
    float orbitDerivative = 0.15*(CONSUMPTION_SQRT_GM/(CONSUMPTION_INFLOW*radius)-boundaryOmega*feedDerivative);
    vec2 radialUV = vec2(orbitDerivative*repeats/6.2831853,
                        -feedDerivative/feedInterval/20.0*formulaScale);
    vec2 tangentUV = vec2(angularRate,0.0);
    vec2 majorUV = length(radialUV)>length(tangentUV) ? radialUV : tangentUV;
    vec2 minorUV = length(radialUV)>length(tangentUV) ? tangentUV : radialUV;
    float texels = footprint*1024.0*max(length(minorUV),length(majorUV)/8.0);
    float lod = max(log2(max(texels,1.0)),0.0);
    float ink = 0.0;
    // 沿拉伸方向八点采样，不能用最大轴的各向同性 mip 把文字整个抹平。
    for (int tap=0; tap<8; ++tap) {
        vec2 sampleUV=formulaUV+majorUV*footprint*((float(tap)+0.5)/8.0-0.5);
        float value=textureLod(uFormulaTexture,sampleUV,lod).r;
        if (blend>0.0) value=mix(value,textureLod(uFormulaTexture,
            sampleUV+vec2((otherOrbit-orbit)/6.2831853*repeats,0.0),lod).r,blend);
        ink+=value/8.0;
    }
    float formula = consumptionFormulaMix();
    float alpha = formula*(0.20+ink*0.64);
    vec3 pigment = vec3(0.96,0.91,0.79)*formula*(0.012+ink*0.64);
    return vec4(pigment/max(alpha,0.001), alpha);
}

vec4 consumptionMaterial(vec2 q,float aspect,float phase,float layer,float footprint) {
    float r=length(q);
    // 内、中、外三档都保持整数周重复，相位连续混合，不用非整数角向缩放制造接缝。
    float level=r<20.0 ? smoothstep(10.0,18.0,r) : 1.0+smoothstep(20.0,30.0,r);
    float first=level<1.0 ? 2.0 : 1.0;
    float blend=level<1.0 ? level : level-1.0;
    vec4 a=consumptionMaterialScale(q,aspect,phase,layer,footprint,first);
    if (blend<=0.001) return a;
    vec4 b=consumptionMaterialScale(q,aspect,phase,layer,footprint,first*0.5);
    float alpha=mix(a.a,b.a,blend);
    return vec4(mix(a.rgb*a.a,b.rgb*b.a,blend)/max(alpha,0.001),alpha);
}

vec3 consumptionRender(vec2 uv, vec2 center, float rh, DiskLook look) {
    float aspect = iResolution.x/iResolution.y;
    vec3 background = consumptionDesktop(uv, aspect);
    vec4 desktopSheet = uTransportReady > 0 ? texture(uTransportTexture,vec2(uv.x,1.0-uv.y)) : vec4(0.0);
    vec2 p = (uv-center)*vec2(aspect, 1.0);
    vec3 straightSky=consumptionSky(consumptionSkyScreenDirection(p),CONSUMPTION_SKY_GAIN);
    vec3 uncoveredBackground=(uTransportReady>0 && desktopSheet.a<=0.0) ? straightSky : background;
    if (rh < 0.0002) return uncoveredBackground;
    float worldScale = B_CRIT / max(rh, 0.0002);
    float desktopZ = (desktopSheet.a-0.5)*8.0*worldScale;
    bool desktopPending = desktopSheet.a > 0.0;
    vec2 pr = rot(vec2(p.x, -p.y), look.roll)*worldScale;
    float b = length(pr);
    // 投影阴影内也要执行求交：前侧盘带先显现，捕获球只阻断其后的光。
    float farFade = 1.0-smoothstep(37.0, 43.0, b);
    if (farFade <= 0.0) return uncoveredBackground;
    // 消费进度可封顶；盘面动画使用连续时钟，不在每小时跳回第一帧。
    float phase = uTransportPhaseReady>0 ? uTransportPhase : iTime;
    // 透视射线让前景褶皱和远景层拥有不同尺度，而不是平行投影的单条带。
    vec3 x = vec3(0.0,0.0,55.0), v = normalize(vec3(pr,-55.0));
    vec3 normal = vec3(0.0, sin(look.incl), cos(look.incl));
    vec3 axis = vec3(0.0, cos(look.incl), -sin(look.incl));
    // 使用原网格的三维位置计算融合，单位与发光盘一致，不能按屏幕黑圆消失。
    vec3 desktopPoint=vec3(pr,desktopZ);
    float desktopRadius=length(vec2(desktopPoint.x,dot(desktopPoint,axis)));
    float desktopHeight=abs(dot(desktopPoint,normal));
    float diskInner=max(look.inner,1.6),diskOuter=max(look.outer,diskInner+0.5);
    // 先暖化（材质阶段 14..5），再解除遮挡（8..3.5）；输运捕获仍独立推进。
    float desktopOpacity=consumptionOpacity(desktopRadius,desktopHeight,diskInner,diskOuter);
    float exposure=max(look.expo,0.0)/1.4;
    float h2 = dot(cross(x,v),cross(x,v));
    vec3 color = vec3(0.0);
    float transmission = 1.0;
    bool captured = false;
    float mouseDistance = length((uv-uFlowMouse.xy)*vec2(aspect,1.0));
    float mouseSpot = exp(-mouseDistance*mouseDistance/0.009) * clamp(uFlowMouse.z,0.0,1.0);
    // 固定迭代预算；中心近处细分，远处粗步进。
    for (int step = 0; step < 144; ++step) {
        float r2 = dot(x,x);
        if (r2 < 1.0) { captured = true; break; }
        if ((x.z < -48.0 && v.z < 0.0) || r2 > 10000.0 || transmission < 0.008) break;
        float r = sqrt(r2);
        float dt = clamp(0.12*r, 0.025, 1.8);
        vec3 previous = x;
        vec3 accel = -1.5*h2*x / max(r2*r2*r, 0.01);
        v += accel*(0.5*dt);
        x += v*dt;
        r2 = max(dot(x,x),0.01);
        v += (-1.5*h2*x/(r2*r2*sqrt(r2)))*(0.5*dt);
        float disk0 = dot(previous,normal), disk1 = dot(x,normal);
        vec2 q0 = vec2(previous.x,dot(previous,axis));
        vec2 q1 = vec2(x.x,dot(x,axis));
        float crossings[6];
        crossings[0] = disk0*disk1 < 0.0 ? disk0/(disk0-disk1) : 2.0;
        // 桌面前侧褶皱和发光盘共享深度顺序，不再把整张网格当成最远背景。
        float desktopCross = (previous.z-desktopZ)/(previous.z-x.z);
        crossings[4] = desktopPending && desktopCross>=0.0 && desktopCross<=1.0 ? desktopCross : 2.0;
        // 本步进入捕获球的位置也参与排序，球内网格片元不能抢在捕获之前显现。
        vec3 segment=x-previous;
        float qa=dot(segment,segment), qb=dot(previous,segment), qc=dot(previous,previous)-1.0;
        float discriminant=qb*qb-qa*qc;
        float captureCross=discriminant>=0.0 ? (-qb-sqrt(discriminant))/max(qa,1e-8) : 2.0;
        crossings[5]=captureCross>=0.0 && captureCross<=1.0 ? captureCross : 2.0;
        for (int layer = 0; layer < 3; ++layer) {
            float s0 = disk0-consumptionHeight(q0,phase,float(layer))-mouseSpot*0.55;
            float s1 = disk1-consumptionHeight(q1,phase,float(layer))-mouseSpot*0.55;
            crossings[layer+1] = s0*s1 < 0.0 ? s0/(s0-s1) : 2.0;
            if (crossings[layer+1] <= 1.0) {
                // 褶皱不是平面，细化实际根，避免粗步长把文字切成斜线。
                float lo = 0.0, hi = 1.0;
                for (int refine = 0; refine < 4; ++refine) {
                    float mid = (lo+hi)*0.5;
                    float sm = mix(disk0,disk1,mid)-consumptionHeight(mix(q0,q1,mid),phase,float(layer))-mouseSpot*0.55;
                    if (s0*sm > 0.0) lo=mid; else hi=mid;
                }
                float a = mix(disk0,disk1,lo)-consumptionHeight(mix(q0,q1,lo),phase,float(layer))-mouseSpot*0.55;
                float bEnd = mix(disk0,disk1,hi)-consumptionHeight(mix(q0,q1,hi),phase,float(layer))-mouseSpot*0.55;
                crossings[layer+1] = mix(lo,hi,clamp(a/(a-bEnd),0.0,1.0));
            }
#ifdef CONSUMPTION_DIAGNOSTICS
            if (testOmitLayer == layer) crossings[layer+1] = 2.0;
#endif
        }
        // 四张光学曲面、单份桌面及捕获球入口按距离依次合成。
        for (int hit = 0; hit < 6; ++hit) {
            int surface = 0;
            for (int candidate = 1; candidate < 6; ++candidate)
                if (crossings[candidate] < crossings[surface]) surface = candidate;
            float fraction = crossings[surface];
            if (fraction > 1.0) break;
            crossings[surface] = 2.0;
            if (surface == 5) {
                captured=true;
                transmission=0.0;
                break;
            }
            if (surface == 4) {
                color += transmission*desktopSheet.rgb*desktopOpacity;
                transmission*=1.0-desktopOpacity;
                desktopPending=false;
                continue;
            }
            vec3 point = mix(previous,x,fraction);
            vec2 q = vec2(point.x,dot(point,axis));
            float rc = length(q);
            if (surface == 0) {
                float inner = diskInner;
                float band = smoothstep(inner,inner*1.34,rc)*(1.0-smoothstep(diskOuter*0.75,diskOuter,rc));
                float angle = atan(q.y,q.x);
                float flow = angle-0.65*phase/pow(max(rc/inner,1.0),1.5);
                vec2 turbulent = vec2(cos(flow),sin(flow))*rc*max(look.wind,0.0)/7.0;
                float detail = consumptionNoise(turbulent*2.8);
                float density = band*max(0.05,0.75+0.25*detail*look.contr/1.6);
                float innerLight = exp(-max(rc/inner-1.0,0.0)*1.62);
                float beta=sqrt(0.5/max(rc,3.0));
                float shift=mix(1.0,sqrt(1.0-beta*beta)/(1.0-0.55*beta*sin(look.incl)*q.x/max(rc,0.01)),clamp(look.dopp,0.0,1.0));
                vec3 gold=consumptionDiskColor(look.temp,inner,rc,shift);
                color += transmission*gold*density*(0.10+innerLight*4.0)*pow(shift,max(look.beam,0.0))*max(look.gain,0.0)/2.2*exposure;
                transmission *= 1.0-clamp(density*0.50*max(look.opac,0.0)/0.9,0.0,0.95);
            } else {
                float layer = float(surface-1);
                float extent = smoothstep(3.2,6.0,rc)*(1.0-smoothstep(29.0,38.0,rc));
                if (extent <= 0.0) continue;
                float height = consumptionHeight(q,phase,layer);
                vec2 gradient = (vec2(consumptionHeight(q+vec2(0.12,0),phase,layer),
                                      consumptionHeight(q+vec2(0,0.12),phase,layer))-height)/0.12;
                vec3 n = normalize(normal-gradient.x*vec3(1,0,0)-gradient.y*axis);
                float footprint = worldScale/iResolution.y*length(point-vec3(0,0,55))/55.0
                                /max(abs(dot(n,normalize(v))),0.15);
                vec4 material = consumptionMaterial(q,aspect,phase,layer,footprint);
                float grazing = clamp(1.0-abs(dot(n,normalize(v))),0.0,1.0);
                grazing = grazing*grazing*grazing;
                float detail = consumptionNoise(rot(q,phase*0.035+layer)*1.7);
                float light = 0.65+0.48*exp(-max(rc-4.0,0.0)*0.075);
                float alpha = clamp(extent*material.a*max(look.opac,0.0)/0.9,0.0,0.92);
                // 公式墨迹保持亮色可读性；外圈不能再次乘低温暗金衰减而变成褐色细丝。
                vec3 tint=consumptionDiskColor(look.temp,diskInner,diskInner,1.0);
                vec3 radiance = material.rgb*light*tint*1.4;
                radiance += tint*(grazing*(0.16+0.22*detail)+mouseSpot*1.8);
                radiance*=max(look.gain,0.0)/2.2*exposure;
                color += transmission*alpha*radiance;
                transmission *= 1.0-alpha;
            }
        }
    }
    if (!captured && dot(x,x)<4.0) captured=true;
    // 桌面已在交点贡献一次，不能再作为远处背景加回，否则融合后会重新污染盘面。
    if (!captured) {
        if (uTransportReady==0) color+=transmission*background;
        else {
            if (desktopPending) {
                color+=transmission*desktopSheet.rgb*desktopOpacity;
                transmission*=1.0-desktopOpacity;
            }
            vec3 bentSky=consumptionSky(consumptionSkyRayDirection(v,look.roll,worldScale),CONSUMPTION_SKY_GAIN);
            color+=transmission*mix(straightSky,bentSky,farFade);
        }
    }
    // 薄光子环，宽光晕交给 HDR Bloom；不覆盖已累积的前景盘带。
    float ring = exp(-pow((b-B_CRIT*1.012)/0.065,2.0));
    if (desktopSheet.a>0.0 && desktopZ>0.0) ring*=1.0-desktopOpacity;
    color += consumptionDiskColor(look.temp,diskInner,diskInner,1.0)*ring*1.3*max(look.gain,0.0)/2.2*exposure;
    return mix(uncoveredBackground,color,farFade);
}
