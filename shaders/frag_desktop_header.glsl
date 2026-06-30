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
uniform float uRandPhase = 0.0;    // random phase offset for trajectory
uniform float uPresetOffset = 0.0; // random time offset for preset cycling (seconds)

#define fragColor gl_FragColor