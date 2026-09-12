#version 330 core
uniform sampler2D stateTexture;
uniform sampler2D parameters;
uniform float aspect;
out vec2 sourceUV;
out float alive;
out float materialDepth;
out vec3 materialPosition;
out vec2 projectedPoint;
flat out vec3 visibleShadow;
void main() {
    ivec2 size=textureSize(stateTexture,0);
    int cell=gl_VertexID/6;
    int corner=gl_VertexID%6;
    // 两个三角形与相邻单元读取完全相同的边界顶点，不裁窄条带、不留缝。
    ivec2 offsets[6]=ivec2[](ivec2(0,0),ivec2(1,0),ivec2(0,1),
                             ivec2(0,1),ivec2(1,0),ivec2(1,1));
    ivec2 id=ivec2(cell%(size.x-1),cell/(size.x-1))+offsets[corner];
    vec4 state=texelFetch(stateTexture,id,0);
    sourceUV=vec2(id)/vec2(size-1);
    alive=state.w;
    materialDepth=state.z;
    vec4 disk=texelFetch(parameters,ivec2(0,0),0);
    vec2 home=texelFetch(parameters,ivec2(1,0),0).xy;
    vec2 center=vec2(home.x*aspect,1.0-home.y);
    vec2 p=state.w<0.5 ? state.xy : state.xy-center;
    if (state.w<0.5) {
        // 捕获边界由当前存活邻域决定，禁止连接已经冻结多圈的旧方位。
        vec3 neighborSum=vec3(0.0);
        float neighborCount=0.0;
        for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
            ivec2 neighbor=id+ivec2(dx,dy);
            if (any(lessThan(neighbor,ivec2(0))) || any(greaterThanEqual(neighbor,size))) continue;
            vec4 value=texelFetch(stateTexture,neighbor,0);
            if (value.w>0.5) { neighborSum+=value.xyz; neighborCount+=1.0; }
        }
        if (neighborCount>0.0) {
            vec3 edge=neighborSum/neighborCount;
            p=edge.xy-center;
            materialDepth=edge.z;
        } else { p=vec2(0.0); materialDepth=0.0; }
    }
    float rh=disk.x;
    materialPosition=vec3(p,materialDepth);
    if (rh>0.00001) {
        vec3 normal=vec3(sin(disk.z)*sin(disk.y),cos(disk.z)*sin(disk.y),cos(disk.y));
        float height=dot(materialPosition,normal);
        // 小洞阶段保持完整收拢，避免半径刚增长就提前失去左右收窄。
        float small=1.0-smoothstep(0.04,0.08,rh);
        // 以吸积盘局部坐标作为入口边界：收窄由盘面距离触发，而不是黑洞中心距离。
        vec3 along=vec3(cos(disk.z)*cos(disk.y),-sin(disk.z)*cos(disk.y),sin(disk.y));
        float diskLong=dot(materialPosition,along);
        float distance=length(materialPosition);
        float local=mix(1.0-smoothstep(1.5*rh,4.0*rh,distance),
                        1.0-smoothstep(6.0*rh,12.0*rh,distance),small);
        float thickness=mix(0.30,1.0,smoothstep(0.015,0.09,rh));
        materialPosition-=normal*height*local*(1.0-thickness);
        // 左右两侧同时收窄盘面内短轴跨度，长轴保持延伸；跟随倾角和滚转。
        vec3 across=vec3(sin(disk.z)*cos(disk.y),cos(disk.z)*cos(disk.y),-sin(disk.y));
        float diskAcross=dot(materialPosition,across);
        float narrow=small*local*0.65;
        float diskEntry=1.0-smoothstep(0.35*rh,2.2*rh,abs(diskLong));
        float entryNarrow=narrow*max(diskEntry,0.35*local);
        materialPosition-=across*diskAcross*entryNarrow;
        // 左右两侧沿吸积盘长轴向内切向汇入，保持蓝图中的盘带走向。
        materialPosition-=along*diskLong*(0.42*entryNarrow);
        // 两个正交横向分量同步收拢，近侧视时也能改变屏幕轮廓，而非只压 Z。
        // 只补足法线方向到同一收拢比例；旧厚度已更薄时不反向撑开。
        float normalScale=1.0-local*(1.0-thickness);
        float extra=max(0.0,normalScale-(1.0-narrow));
        materialPosition-=normal*height*extra;
        materialDepth=materialPosition.z;
    }
    if (state.w<0.5 && rh>0.00001) {
        // 混合面的末端沿当前邻域收束到三维捕获半径，不撑成屏幕圆环。
        materialPosition*=min(1.0,0.40*rh/max(length(materialPosition),1e-7));
        materialDepth=materialPosition.z;
    }
    projectedPoint=center+materialPosition.xy;
    visibleShadow=vec3(center,rh);
    gl_Position=vec4(projectedPoint.x/aspect*2.0-1.0,projectedPoint.y*2.0-1.0,
                     clamp(-materialDepth*0.25,-0.99,0.99),1.0);
}
