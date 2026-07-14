import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Clutter from 'gi://Clutter';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

import {BlackholePrototypeEffect} from './effect.js';

const BUS_NAME = 'io.github.xboxnahida.Blackhole';
const OBJECT_PATH = '/io/github/xboxnahida/Blackhole';
const EFFECT_NAME = 'blackhole-desktop-effect';

const DBUS_XML = `
<node>
  <interface name="io.github.xboxnahida.Blackhole">
    <method name="Start"/>
    <method name="Stop"/>
    <method name="ReloadConfig"/>
    <method name="GetState">
      <arg name="running" type="b" direction="out"/>
    </method>
    <property name="Running" type="b" access="read"/>
  </interface>
</node>`;

export default class BlackholeExtension extends Extension {
    enable() {
        this._running = false;
        this._effects = [];

        // Apply the effect to Mutter's real paint actors.  A Clutter.Clone
        // overlay only receives source damage intermittently and obscures the
        // live windows with a stale snapshot.
        for (const [index, actor] of [
            Main.layoutManager._backgroundGroup,
            global.window_group,
        ].entries()) {
            const effect = new BlackholePrototypeEffect(this.path);
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
        console.log(`[blackhole] enabled with ${this._effects.length} compositor effects`);
    }

    disable() {
        this.Stop();
        for (const {actor, name} of this._effects ?? [])
            actor.remove_effect_by_name(name);
        this._effects = [];

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
        for (const {effect} of this._effects) {
            effect.reloadConfig();
            effect.setRunning(true);
        }
        this._dbus?.emit_property_changed('Running', new GLib.Variant('b', true));
        console.log('[blackhole] effect started');
    }

    ReloadConfig() {
        for (const {effect} of this._effects)
            effect.reloadConfig();
        console.log('[blackhole] configuration reloaded');
    }

    Stop() {
        this._running = false;
        for (const {effect} of this._effects ?? [])
            effect.setRunning(false);
        this._dbus?.emit_property_changed('Running', new GLib.Variant('b', false));
        console.log('[blackhole] effect stopped');
    }

    GetState() {
        return this._running;
    }

    get Running() {
        return this._running;
    }
}
