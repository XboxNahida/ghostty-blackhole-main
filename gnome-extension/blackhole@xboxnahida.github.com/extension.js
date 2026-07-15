import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Clutter from 'gi://Clutter';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

import {BlackholePrototypeEffect, ConfiguredShaderCache} from './effect.js';

const BUS_NAME = 'io.github.xboxnahida.Blackhole';
const OBJECT_PATH = '/io/github/xboxnahida/Blackhole';
const EFFECT_NAME = 'blackhole-desktop-effect';
const STOP_SHORTCUT = 'stop-shortcut';

const DBUS_XML = `
<node>
  <interface name="io.github.xboxnahida.Blackhole">
    <method name="Start"/>
    <method name="Stop"/>
    <method name="ReloadConfig"/>
    <method name="GetState">
      <arg name="running" type="b" direction="out"/>
    </method>
    <method name="ConfigureShortcut">
      <arg name="enabled" type="b" direction="in"/>
      <arg name="accelerator" type="s" direction="in"/>
      <arg name="success" type="b" direction="out"/>
    </method>
    <signal name="StopShortcutActivated"/>
    <property name="Running" type="b" access="read"/>
  </interface>
</node>`;

export default class BlackholeExtension extends Extension {
    enable() {
        this._running = false;
        this._effects = [];
        this._unredirectDisabled = false;
        this._restoreUnredirectId = 0;
        this._settings = this.getSettings();
        this._shortcutRegistered = false;
        this._shaderCache = new ConfiguredShaderCache(this.path);
        this._shortcutSettingsChangedId = this._settings.connect(
            'changed', (_settings, key) => {
                if (key === 'shortcut-enabled' || key === STOP_SHORTCUT)
                    this._registerStopShortcut();
            });

        // Apply the effect to Mutter's real paint actors.  A Clutter.Clone
        // overlay only receives source damage intermittently and obscures the
        // live windows with a stale snapshot. Fullscreen direct scanout is
        // disabled separately while running so these actors remain in the
        // compositor path.
        const configured = this._shaderCache.get();
        for (const [index, actor] of [
            Main.layoutManager._backgroundGroup,
            global.window_group,
        ].entries()) {
            const effect = new BlackholePrototypeEffect(configured);
            actor.add_effect_with_name(`${EFFECT_NAME}-${index}`, effect);
            this._effects.push({actor, effect, name: `${EFFECT_NAME}-${index}`});
        }

        this._dbus = Gio.DBusExportedObject.wrapJSObject(DBUS_XML, this);
        this._dbus.export(Gio.DBus.session, OBJECT_PATH);
        this._busOwner = Gio.bus_own_name_on_connection(
            Gio.DBus.session,
            BUS_NAME,
            Gio.BusNameOwnerFlags.NONE,
            null,
            null);
        this._registerStopShortcut();
        console.log(`[blackhole] enabled with ${this._effects.length} compositor effects`);
    }

    disable() {
        this._unregisterStopShortcut();
        if (this._settings && this._shortcutSettingsChangedId) {
            this._settings.disconnect(this._shortcutSettingsChangedId);
            this._shortcutSettingsChangedId = 0;
        }
        this._settings = null;
        this.Stop();
        for (const {effect} of this._effects ?? [])
            effect.stopImmediately();
        if (this._restoreUnredirectId) {
            GLib.source_remove(this._restoreUnredirectId);
            this._restoreUnredirectId = 0;
        }
        this._restoreUnredirect();
        for (const {actor, name} of this._effects ?? [])
            actor.remove_effect_by_name(name);
        this._effects = [];
        this._shaderCache = null;

        if (this._dbus) {
            this._dbus.unexport();
            this._dbus = null;
        }
        if (this._busOwner) {
            Gio.bus_unown_name(this._busOwner);
            this._busOwner = 0;
        }
        console.log('[blackhole] disabled');
    }

    Start() {
        this._running = true;
        if (this._restoreUnredirectId) {
            GLib.source_remove(this._restoreUnredirectId);
            this._restoreUnredirectId = 0;
        }
        this._disableUnredirect();
        // Both compositor actors must share one spawn or their distortion
        // fields would not line up. Keep enough edge margin for the entrance.
        const homeX = 0.18 + Math.random() * 0.64;
        const homeY = 0.18 + Math.random() * 0.64;
        const phase = Math.random() * Math.PI * 2.0;
        const configured = this._shaderCache.get();
        for (const {effect} of this._effects) {
            effect.applyConfiguredShader(configured);
            effect.prepareStart(homeX, homeY, phase);
            effect.setRunning(true);
        }
        this._dbus?.emit_property_changed('Running', new GLib.Variant('b', true));
        console.log('[blackhole] effect started');
    }

    ReloadConfig() {
        const configured = this._shaderCache.get(true);
        for (const {effect} of this._effects)
            effect.applyConfiguredShader(configured);
        console.log('[blackhole] configuration reloaded');
    }

    Stop() {
        this._running = false;
        for (const {effect} of this._effects ?? [])
            effect.setRunning(false);
        if (this._unredirectDisabled && !this._restoreUnredirectId) {
            // Keep fullscreen surfaces composited until the 650 ms exit
            // transition has finished, then restore Mutter's direct-scanout
            // optimization while the effect is idle.
            this._restoreUnredirectId = GLib.timeout_add(
                GLib.PRIORITY_DEFAULT, 700, () => {
                    this._restoreUnredirectId = 0;
                    this._restoreUnredirect();
                    return GLib.SOURCE_REMOVE;
                });
        }
        this._dbus?.emit_property_changed('Running', new GLib.Variant('b', false));
        console.log('[blackhole] effect stopped');
    }

    _disableUnredirect() {
        if (this._unredirectDisabled)
            return;
        if (Meta.disable_unredirect_for_display !== undefined)
            Meta.disable_unredirect_for_display(global.display);
        else if (global.compositor.disable_unredirect !== undefined)
            global.compositor.disable_unredirect();
        else {
            console.warn('[blackhole] Mutter does not expose an unredirect control API');
            return;
        }
        this._unredirectDisabled = true;
        console.log('[blackhole] fullscreen direct scanout disabled');
    }

    _restoreUnredirect() {
        if (!this._unredirectDisabled)
            return;
        if (Meta.enable_unredirect_for_display !== undefined)
            Meta.enable_unredirect_for_display(global.display);
        else if (global.compositor.enable_unredirect !== undefined)
            global.compositor.enable_unredirect();
        this._unredirectDisabled = false;
        console.log('[blackhole] fullscreen direct scanout restored');
    }

    GetState() {
        return this._running;
    }

    ConfigureShortcut(enabled, accelerator) {
        if (!this._settings)
            return false;

        const value = accelerator.trim();
        if (enabled && !value)
            return false;

        // Keep the selected accelerator while disabled so re-enabling does not
        // silently reset the user's choice.
        if (value)
            this._settings.set_strv(STOP_SHORTCUT, [value]);
        this._settings.set_boolean('shortcut-enabled', enabled);
        return this._registerStopShortcut();
    }

    _registerStopShortcut() {
        this._unregisterStopShortcut();
        if (!this._settings?.get_boolean('shortcut-enabled'))
            return true;

        try {
            const action = Main.wm.addKeybinding(
                STOP_SHORTCUT,
                this._settings,
                Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW,
                () => {
                    this.Stop();
                    this._dbus?.emit_signal('StopShortcutActivated', null);
                });
            if (action === Meta.KeyBindingAction.NONE)
                throw new Error('GNOME rejected the accelerator or it conflicts with another binding');
            this._shortcutRegistered = true;
            console.log(`[blackhole] registered stop shortcut: ${this._settings.get_strv(STOP_SHORTCUT).join(', ')}`);
            return true;
        } catch (error) {
            console.error(`[blackhole] failed to register stop shortcut: ${error.message}`);
            return false;
        }
    }

    _unregisterStopShortcut() {
        if (!this._shortcutRegistered)
            return;
        Main.wm.removeKeybinding(STOP_SHORTCUT);
        this._shortcutRegistered = false;
    }

    get Running() {
        return this._running;
    }
}
