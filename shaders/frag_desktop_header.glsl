// frag_desktop_header.glsl  standalone header for full blackhole.glsl
#version 330

uniform vec3 iResolution;
uniform float iTime;
uniform vec4 iDate;
uniform sampler2D iChannel0;

uniform vec4 iCurrentCursorColor = vec4(0.0);
uniform vec4 iPreviousCursorColor = vec4(0.0);
uniform float iTimeCursorChange = 0.0;
uniform vec4 iMouse = vec4(0.0);

// GUI-adjustable uniforms (negative = use shader default)
uniform float uHoleRadius = -1.0;
uniform float uDiskGain   = -1.0;
uniform float uDiskTemp   = -1.0;
uniform float uExposure   = -1.0;
uniform float uSpeed      = -1.0;
uniform float uStarGain   = -1.0;
uniform float uDiskIncl   = -1.0;

uniform float uBornProgress = 1.0;

uniform int   uFixedSize   = 0;     // 0=正常生长, 1=固定大小
uniform float uFixedLevel   = 1.0;   // 固定大小值 (0.1~1.0)
uniform int   uGrowEnabled = 0;      // 0=完整大小, 1=从 uInitialSize 增长
uniform float uInitialSize = 0.3;    // 逐渐增长的初始大小
uniform int   uLightingEffect = 0;   // 0=关闭, 1=吸积盘分层光影与 Bloom
uniform float uDistortion = 1.0;     // 引力透镜扭曲强度

// Demo preset overrides (negative = use hardcoded default)
#define MAX_PRESETS 64
uniform int   uPresetCount = 0;
uniform float uPresetTemp [MAX_PRESETS];
uniform float uPresetIncl [MAX_PRESETS];
uniform float uPresetRoll [MAX_PRESETS];
uniform float uPresetInner[MAX_PRESETS];
uniform float uPresetOuter[MAX_PRESETS];
uniform float uPresetOpac [MAX_PRESETS];
uniform float uPresetDopp [MAX_PRESETS];
uniform float uPresetBeam [MAX_PRESETS];
uniform float uPresetGain [MAX_PRESETS];
uniform float uPresetContr[MAX_PRESETS];
uniform float uPresetWind [MAX_PRESETS];
uniform float uPresetSpd  [MAX_PRESETS];
uniform float uPresetExpo [MAX_PRESETS];
uniform float uPresetStar [MAX_PRESETS];

uniform int uPlayMode = 0;   // 0=顺序 1=循环 2=随机
uniform float uSlotSec = 5.25;   // 每个预设播放秒数

// Random spawn parameters (set once per session)
uniform float uHomeX = 0.96;       // initial hole home X (0=left, 1=right)
uniform float uHomeY = 0.04;       // initial hole home Y (0=top, 1=bottom)
uniform int uFollowMouse = 0;      // 1=lock hole center to uHomeX/uHomeY
uniform float uRandPhase = 0.0;    // random phase offset for trajectory
uniform float uPresetOffset = 0.0; // random time offset for preset cycling (seconds)

#define fragColor gl_FragColor
