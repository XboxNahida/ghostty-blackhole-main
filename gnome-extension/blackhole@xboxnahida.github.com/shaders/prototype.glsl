uniform sampler2D tex;
uniform float time;

void main() {
    vec2 uv = cogl_tex_coord_in[0].st;
    vec2 center = vec2(0.5, 0.5);
    vec2 p = uv - center;
    p.x *= 1.7777778;

    float r = length(p);
    float pulse = 0.006 * sin(time * 1.8);
    float lens = 0.055 * exp(-22.0 * r) + pulse * exp(-10.0 * r);
    vec2 dir = r > 0.0001 ? p / r : vec2(0.0);
    vec2 warped = p + dir * lens;
    warped.x /= 1.7777778;

    vec4 desktop = texture2D(tex, center + warped);
    float horizon = 1.0 - smoothstep(0.105, 0.125, r);
    float ring = smoothstep(0.115, 0.135, r) * (1.0 - smoothstep(0.145, 0.175, r));
    vec3 glow = vec3(1.0, 0.42, 0.08) * ring * 1.8;
    vec3 color = mix(desktop.rgb, vec3(0.0), horizon) + glow;

    cogl_color_out = vec4(color, desktop.a);
}
