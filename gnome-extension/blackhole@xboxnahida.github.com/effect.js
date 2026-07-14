import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Shell from 'gi://Shell';

function loadShader(extensionPath) {
    const shaderDir = GLib.build_filenamev([extensionPath, 'shaders']);
    const header = Shell.get_file_contents_utf8_sync(
        GLib.build_filenamev([shaderDir, 'compositor_header.glsl']));
    const body = Shell.get_file_contents_utf8_sync(
        GLib.build_filenamev([shaderDir, 'blackhole.glsl']))
        // A compositor has no Ghostty cursor token signal.  Use the token
        // branch with a stable synthetic level: unlike MODE_DEMO it keeps
        // roaming continuously and never snaps back to its seed every 42 s.
        .replace('#define TOKEN_LEVEL -1 // token-level',
            '#define TOKEN_LEVEL 0.55 // compositor-level')
        // Tour the demo's disk looks without enabling MODE_DEMO itself.  The
        // token branch therefore keeps a stable size and continuous roaming,
        // while the accretion disk still morphs smoothly between presets.
        // Add an independent roll after selecting the look so it is never
        // overwritten by demoLook().
        .replace('if (SIZE_MODE == MODE_DEMO) L = demoLook();',
            'L = demoLook();\n    L.roll += iTime * 0.08;')
        // Token mode normally grows its roam box out of the top-right seed,
        // and its disk-safe margins can collapse vertical travel at this
        // synthetic mid-level.  The desktop compositor is a persistent
        // ambient effect, so give it an independent, centered full-screen
        // orbit while leaving the 0.55 level in charge of size/intensity.
        .replace(
            '+ wobAmp * vec2(cos(t * 0.8), sin(t * 1.0));',
            `+ wobAmp * vec2(cos(t * 0.8), sin(t * 1.0));
        center = vec2(0.5) + vec2(0.38, 0.34) * lissa(t * 0.16 + 1.7);`);
    const wrapper = `
void main() {
    vec2 uv = cogl_tex_coord_in[0].st;
    vec4 color;
    mainImage(color, uv * vec2(uResolutionX, uResolutionY));
    cogl_color_out = color;
}
`;
    return `${header}\n${body}\n${wrapper}`;
}

function number(value, fallback) {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : fallback;
}

function readConfig() {
    const dir = GLib.build_filenamev([
        GLib.get_user_config_dir(), 'XboxNahida', 'Blakhole UI']);
    const config = {
        slotSec: 5.25,
        playMode: 1,
        fixedLevel: 0.55,
        fixedSize: true,
        holeRadius: 0.02,
        movementSpeed: 1.0,
        rotationSpeed: 0.08,
        presets: [],
    };

    try {
        const text = Shell.get_file_contents_utf8_sync(
            GLib.build_filenamev([dir, 'blackhole_presets.txt']));
        const lines = text.split(/\r?\n/);
        const header = (lines[1] ?? '').trim().split(/\s+/);
        config.slotSec = Math.max(0.5, number(header[2], config.slotSec));
        config.playMode = Math.max(0, Math.min(2, Math.trunc(number(header[3], 1))));
        config.fixedSize = number(header[6], 0) !== 0;
        if (config.fixedSize)
            config.fixedLevel = Math.max(0.01, Math.min(1.0, number(header[7], 0.55)));
        const count = Math.max(0, Math.min(64, Math.trunc(number(lines[2], 0))));
        for (let i = 0; i < count; i++) {
            const values = (lines[4 + i * 2] ?? '').trim().split(/\s+/).map(Number);
            if (values.length === 14 && values.every(Number.isFinite))
                config.presets.push(values);
        }
    } catch (e) {
        console.warn(`[blackhole] using built-in presets: ${e.message}`);
    }

    try {
        const text = Shell.get_file_contents_utf8_sync(
            GLib.build_filenamev([dir, 'blackhole_advanced.txt']));
        for (const line of text.split(/\r?\n/)) {
            const match = line.match(/^\s*([^#=]+)=(.*)$/);
            if (!match)
                continue;
            const key = match[1].trim();
            const value = number(match[2].trim(), NaN);
            if (key === 'holeRadius' && value > 0)
                config.holeRadius = value;
            else if (key === 'holeSize' && value > 0)
                config.holeRadius = 0.02 * value;
            else if (key === 'movementSpeed' && value > 0)
                config.movementSpeed = Math.max(0.1, Math.min(3.0, value));
            else if ((key === 'animationSpeed' || key === 'spd') && value > 0)
                config.rotationSpeed = 0.08 * value;
        }
    } catch (e) {
        console.warn(`[blackhole] using built-in advanced settings: ${e.message}`);
    }
    return config;
}

function configuredLookSource(config) {
    if (!config.presets.length)
        return null;
    const literal = values => `DiskLook(${values.map(v => Number(v).toFixed(4)).join(', ')})`;
    const branches = config.presets.map((preset, i) =>
        `    if (index == ${i}) return ${literal(preset)};`).join('\n');
    const last = config.presets.length - 1;
    return `DiskLook configuredLook(int index) {
${branches}
    return ${literal(config.presets[last])};
}

DiskLook demoLook() {
    float slot = max(${config.slotSec.toFixed(4)}, 0.5);
    float raw = iTime / slot;
    float slotIndex = floor(raw);
    int baseIndex;
    if (${config.playMode} == 0)
        baseIndex = int(min(slotIndex, ${last}.0));
    else if (${config.playMode} == 2)
        baseIndex = int(floor(fract(sin(slotIndex * 91.713) * 43758.5453) * ${config.presets.length}.0));
    else
        baseIndex = int(mod(slotIndex, ${config.presets.length}.0));
    int nextIndex;
    if (${config.playMode} == 0)
        nextIndex = min(baseIndex + 1, ${last});
    else if (${config.playMode} == 2)
        nextIndex = int(floor(fract(sin((slotIndex + 1.0) * 91.713) * 43758.5453) * ${config.presets.length}.0));
    else
        nextIndex = int(mod(float(baseIndex + 1), ${config.presets.length}.0));
    float blend = smoothstep(0.82, 1.0, fract(raw));
    return mixLook(configuredLook(baseIndex), configuredLook(nextIndex), blend);
}`;
}

function loadConfiguredShader(extensionPath) {
    const config = readConfig();
    let shader = loadShader(extensionPath)
        .replace(/const float HOLE_RADIUS\s*=\s*[-+0-9.]+;/,
            `const float HOLE_RADIUS = ${config.holeRadius.toFixed(4)};`)
        .replace(/const float DRIFT_SPEED\s*=\s*[-+0-9.]+;/,
            `const float DRIFT_SPEED = ${config.movementSpeed.toFixed(4)};`)
        .replace('#define TOKEN_LEVEL 0.55 // compositor-level',
            `#define TOKEN_LEVEL ${config.fixedLevel.toFixed(4)} // compositor-level`)
        .replace('L.roll += iTime * 0.08;',
            `L.roll += iTime * ${config.rotationSpeed.toFixed(4)};`);
    const lookSource = configuredLookSource(config);
    if (lookSource)
        shader = shader.replace(/DiskLook demoLook\(\) \{[\s\S]*?\n\}/, lookSource);
    return shader;
}

export const BlackholePrototypeEffect = GObject.registerClass(
class BlackholePrototypeEffect extends Clutter.ShaderEffect {
    _init(extensionPath) {
        super._init();
        this._extensionPath = extensionPath;
        this.reloadConfig();
        this._startUs = GLib.get_monotonic_time();
        this._redrawId = 0;
        this.set_enabled(false);
    }

    reloadConfig() {
        this.set_shader_source(loadConfiguredShader(this._extensionPath));
        this.get_actor()?.queue_redraw();
    }

    setRunning(running) {
        if (running) {
            this._startUs = GLib.get_monotonic_time();
            if (!this._redrawId) {
                this._redrawId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 16, () => {
                    if (!this.get_enabled()) {
                        this._redrawId = 0;
                        return GLib.SOURCE_REMOVE;
                    }
                    this.get_actor()?.queue_redraw();
                    return GLib.SOURCE_CONTINUE;
                });
            }
        } else {
            if (this._redrawId) {
                GLib.source_remove(this._redrawId);
                this._redrawId = 0;
            }
        }
        this.set_enabled(running);
    }

    vfunc_paint_target(paintNode = null, paintContext = null) {
        const time = (GLib.get_monotonic_time() - this._startUs) / 1000000.0;
        const actor = this.get_actor();
        const [width, height] = actor?.get_size() ?? [1920, 1080];
        this.set_uniform_value('iTime', time);
        this.set_uniform_value('uResolutionX', Math.max(width, 1));
        this.set_uniform_value('uResolutionY', Math.max(height, 1));
        this.set_uniform_value('iChannel0', 0);

        // Negative values select the shader's built-in visual defaults.
        this.set_uniform_value('uHoleRadius', -1.0);
        this.set_uniform_value('uDiskGain', -1.0);
        this.set_uniform_value('uDiskTemp', -1.0);
        this.set_uniform_value('uExposure', -1.0);
        this.set_uniform_value('uSpeed', -1.0);
        this.set_uniform_value('uStarGain', -1.0);
        this.set_uniform_value('uDiskIncl', -1.0);
        this.set_uniform_value('uBornProgress', 1.0);
        this.set_uniform_value('uDistortion', 1.0);
        this.set_uniform_value('uSlotSec', 5.25);
        this.set_uniform_value('uHomeX', 0.5);
        this.set_uniform_value('uHomeY', 0.5);
        this.set_uniform_value('uRandPhase', 0.0);
        this.set_uniform_value('uPresetOffset', 0.0);

        if (paintNode && paintContext)
            super.vfunc_paint_target(paintNode, paintContext);
        else if (paintNode)
            super.vfunc_paint_target(paintNode);
        else
            super.vfunc_paint_target();
    }
});
