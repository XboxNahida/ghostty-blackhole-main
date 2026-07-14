// Cogl/Clutter adapter for the shared Shadertoy-style blackhole shader.
uniform float uResolutionX;
uniform float uResolutionY;
uniform float iTime;
uniform sampler2D iChannel0;

#define iResolution vec3(uResolutionX, uResolutionY, 1.0)
#define iDate vec4(0.0, 0.0, 0.0, iTime)
#define iCurrentCursorColor vec4(0.0)
#define iPreviousCursorColor vec4(0.0)
#define iTimeCursorChange 0.0
#define iMouse vec4(0.0)

uniform float uHoleRadius;
uniform float uDiskGain;
uniform float uDiskTemp;
uniform float uExposure;
uniform float uSpeed;
uniform float uStarGain;
uniform float uDiskIncl;
uniform float uBornProgress;

uniform int uFixedSize;
uniform float uFixedLevel;
uniform int uGrowEnabled;
uniform float uInitialSize;
uniform int uLightingEffect;
uniform float uDistortion;

#define MAX_PRESETS 64
uniform int uPresetCount;
uniform float uPresetTemp[MAX_PRESETS];
uniform float uPresetIncl[MAX_PRESETS];
uniform float uPresetRoll[MAX_PRESETS];
uniform float uPresetInner[MAX_PRESETS];
uniform float uPresetOuter[MAX_PRESETS];
uniform float uPresetOpac[MAX_PRESETS];
uniform float uPresetDopp[MAX_PRESETS];
uniform float uPresetBeam[MAX_PRESETS];
uniform float uPresetGain[MAX_PRESETS];
uniform float uPresetContr[MAX_PRESETS];
uniform float uPresetWind[MAX_PRESETS];
uniform float uPresetSpd[MAX_PRESETS];
uniform float uPresetExpo[MAX_PRESETS];
uniform float uPresetStar[MAX_PRESETS];

uniform int uPlayMode;
uniform float uSlotSec;
uniform float uHomeX;
uniform float uHomeY;
uniform int uFollowMouse;
uniform float uRandPhase;
uniform float uPresetOffset;

// Cogl's snippet compiler supplies the output and texture coordinates.
#define texture texture2D
