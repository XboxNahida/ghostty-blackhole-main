#version 330 core
uniform sampler2D stateTexture;
uniform sampler2D parameters;
uniform vec2 stepInfo; // dt, aspect
out vec4 nextState;
void main() {
    ivec2 id = ivec2(gl_FragCoord.xy);
    vec4 state = texelFetch(stateTexture,id,0);
    vec4 disk = texelFetch(parameters,ivec2(0,0),0);
    vec2 home = texelFetch(parameters,ivec2(1,0),0).xy;
    vec3 center = vec3(home.x*stepInfo.y,1.0-home.y,0.0);
    // 捕获后保存最后相对方位；网格边界仍能向同一侧收束，而非都挤到一个顶点。
    if (state.w < 0.5) { nextState=state; return; }
    float rh = disk.x;
    if (rh < 0.00001 || stepInfo.x <= 0.0) { nextState=state; return; }
    vec3 p = state.xyz-center;
    float distance = length(p);
    // 直接使用实际屏幕半径，不除以同样受尺寸旋钮缩放的 fullRh。
    float influence = 12.0*rh;
    float force = 1.0-smoothstep(influence*0.45,influence,distance);
    if (force <= 0.0) { nextState=state; return; }
    float sn=sin(disk.y), cs=cos(disk.y), sr=sin(disk.z), cr=cos(disk.z);
    vec3 axisX=vec3(cr,-sr,0.0);
    vec3 axisY=vec3(sr*cs,cr*cs,-sn);
    vec3 normal=vec3(sr*sn,cr*sn,cs);
    vec2 plane=vec2(dot(p,axisX),dot(p,axisY));
    float radius=length(plane);
    float height=dot(p,normal);
    float dt=stepInfo.x*force;
    float unit=rh/2.598076211;
    // 外圈低速、内圈快速；半径变化使用稳定的正值收缩，避免越过中心后反弹。
    float radial=0.06*sqrt(rh/max(distance,rh));
    float nextRadius=max(radius-radial*dt,0.0);
    float omega=22.627417/pow(max(distance/max(unit,0.000001),3.0),1.5);
    float angle=atan(plane.y,plane.x)+omega*dt;
    float fold=0.16*nextRadius*sin(2.0*angle+0.7*log(max(nextRadius/unit,1.0)));
    fold*=smoothstep(2.0*rh,7.0*rh,nextRadius);
    height=mix(height,fold,1.0-exp(-1.2*dt));
    p=axisX*(nextRadius*cos(angle))+axisY*(nextRadius*sin(angle))+normal*height;
    nextState=length(p)<rh*0.40 ? vec4(p,0.0) : vec4(center+p,1.0);
}
