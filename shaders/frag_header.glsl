// ===== Standalone header (no Ghostty-specific uniforms) =====
#version 330

uniform vec3 iResolution;
uniform float iTime;
uniform vec4 iDate;

// Dummy uniforms for Ghostty variables referenced in shader body
uniform vec4 iCurrentCursorColor;
uniform vec4 iPreviousCursorColor;

// Use built-in gl_FragColor (compatibility profile)
#define fragColor gl_FragColor

// ===== Procedural starfield sky =====
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 sky(vec2 uv) {
     vec3 col = vec3(0.01, 0.01, 0.04);
     vec2 np = uv * 3.0 + vec2(sin(iTime * 0.02), cos(iTime * 0.015)) * 0.3;
     float neb = 0.0;
     neb += 0.3 * sin(np.x * 1.2 + np.y * 0.8 + iTime * 0.005);
     neb += 0.2 * sin(np.x * 0.7 - np.y * 1.5 + iTime * 0.008);
     neb = max(0.0, neb);
     col += vec3(0.02, 0.005, 0.04) * neb;
     for (int layer = 0; layer < 3; layer++) {
         float scale = 50.0 + float(layer) * 80.0;
         vec2 p = uv * scale;
         vec2 ip = floor(p);
         vec2 fp = fract(p) - 0.5;
         float r = rand(ip + float(layer) * 3.7);
         float star = 1.0 - smoothstep(0.02, 0.15 * (1.0 - r * 0.5), length(fp));
         star *= step(0.92, r);
         float twinkle = 0.7 + 0.3 * sin(iTime * (0.5 + 2.0 * r) + 40.0 * r);
         col += vec3(0.8 + r * 0.2) * star * 0.6 * twinkle;
     }
     float mw = 0.03 * max(0.0, sin((uv.x + uv.y * 0.3) * 1.5 + 0.5) * 0.5 + 0.5);
     col += vec3(0.01, 0.005, 0.015) * mw;
     return clamp(col, 0.0, 1.0);
 }
