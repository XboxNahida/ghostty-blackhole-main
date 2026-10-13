#include "../Blakhole_UI/core/blackholecore.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <cstdio>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const QString dir=QCoreApplication::applicationDirPath();
    // The executable lives in its own test directory; a non-executable marker
    // makes the production path resolver choose this directory exclusively.
    QFile marker(dir+"/blackhole.exe"); CHECK(marker.open(QIODevice::WriteOnly)); marker.close();
    BlackHoleCore core;
#ifdef BLACKHOLE_VERIFY_PAYMENT_RESOURCES
    CHECK(core.paymentQrAvailable());
    CHECK(core.paymentQrPrimaryUrl().startsWith("qrc:/"));
    CHECK(core.paymentQrSecondaryUrl().startsWith("qrc:/"));
#endif
    CHECK(core.rippleMode()==0 && core.diskRenderMode()==0);
    CHECK(core.effectMode()==0 && core.rippleAmplitude()==1 && core.rippleOscillations()==1 && core.rippleSpread()==1);
    CHECK(core.presetModel()->rowCount()==14);
    CHECK(core.idleSeconds()==60 && core.slotSeconds()==5.3f);
    CHECK(core.movementSpeed()==0.3f && core.growEnabled() && core.initialSize()==0.05f);
    CHECK(core.frameRateLimit()==0 && core.skipExitDialog());
    for (const char* name : {"blackhole_presets.txt","blackhole_lists.txt","blackhole_advanced.txt",
                             "blackhole_system.txt","blackhole_schedule.txt","blackhole_idlelist.txt"}) {
        QFile::remove(dir+"/"+name);
    }
    {
        BlackHoleCore missing; missing.loadConfig();
        CHECK(missing.presetModel()->rowCount()==14 && missing.presetListModel()->rowCount()==1);
        CHECK(missing.rippleOscillations()==1 && missing.rippleSpread()==1);
    }
    core.setEffectMode(2); core.setEmergenceWaitMin(12); core.setEmergenceWaitMax(30);
    core.setRippleAmplitude(1.5f); core.setRippleOscillations(1); core.setRippleSpread(2);
    core.saveConfig();
    {
        BlackHoleCore loaded; loaded.loadConfig();
        CHECK(loaded.effectMode()==2 && !loaded.lightingEffect());
        CHECK(loaded.emergenceWaitMin()==12 && loaded.emergenceWaitMax()==30);
        CHECK(loaded.rippleAmplitude()==1.5f && loaded.rippleOscillations()==1 && loaded.rippleSpread()==2);
        loaded.setLightingEffect(true); CHECK(loaded.effectMode()==1);
        loaded.setEffectMode(2); loaded.setEmergenceWaitMin(40);
        CHECK(loaded.emergenceWaitMax()==40);
        loaded.setEmergenceWaitMax(5); CHECK(loaded.emergenceWaitMin()==5);
        loaded.setDisplayMode(1);
        loaded.applyAndStart();
        CHECK(loaded.isSystemActive());
        loaded.setScheduleEnabled(true); loaded.setStartHour(4); loaded.setEndHour(12);
        loaded.setCountdownEnabled(true); loaded.setCountdownMinutes(3);
        loaded.setIdleWhitelist({"temporary"}); loaded.setIdleBlacklist({"temporary"});
        loaded.setIdleForceBlocklist({"temporary"});
        loaded.setLaunchMinimized(true); loaded.setIsDark(false); loaded.setFollowSystem(false);
        loaded.setFocusColor(QColor("#ff0000")); loaded.setDefaultCloseAction(1);
        loaded.resetDefaults();
        CHECK(!loaded.isSystemActive());
        CHECK(loaded.effectMode()==0 && loaded.rippleAmplitude()==1 && loaded.rippleOscillations()==1 && loaded.rippleSpread()==1);
        CHECK(loaded.displayMode()==0 && loaded.idleSeconds()==60 && loaded.slotSeconds()==5.3f);
        CHECK(loaded.movementSpeed()==0.3f && loaded.growEnabled() && loaded.initialSize()==0.05f);
        CHECK(loaded.frameRateLimit()==0 && loaded.presetModel()->rowCount()==14);
        CHECK(!loaded.scheduleEnabled() && loaded.startHour()==8 && loaded.endHour()==22);
        CHECK(!loaded.countdownEnabled() && loaded.countdownMinutes()==30);
        CHECK(loaded.idleWhitelist().isEmpty() && loaded.idleForceBlocklist().isEmpty());
        CHECK(loaded.idleBlacklist().contains("哔哩哔哩") && !loaded.idleBlacklist().contains("temporary"));
        CHECK(!loaded.launchMinimized() && loaded.isDark() && loaded.followSystem());
        CHECK(loaded.focusColor()==QColor("#00c4b3") && loaded.defaultCloseAction()==0);
        for (const auto& preset : static_cast<PresetModel*>(loaded.presetModel())->presets()) {
            CHECK(preset.name!="Crimson Vortex" && preset.name!="Ruby Ring");
        }
        loaded.saveConfig();
        BlackHoleCore restored; restored.loadConfig();
        CHECK(restored.presetModel()->rowCount()==14 && restored.rippleSpread()==1);
        CHECK(restored.growEnabled() && restored.initialSize()==0.05f && restored.frameRateLimit()==0);
    }
    int rippleSignals=0,diskSignals=0;
    QObject::connect(&core,&BlackHoleCore::rippleModeChanged,[&]{++rippleSignals;});
    QObject::connect(&core,&BlackHoleCore::diskRenderModeChanged,[&]{++diskSignals;});
    for(int mode=0;mode<3;++mode) {
        core.setRippleMode(mode); core.setDiskRenderMode(mode); core.saveConfig();
        BlackHoleCore loaded; loaded.loadConfig();
        CHECK(loaded.rippleMode()==mode && loaded.diskRenderMode()==mode);
        loaded.setCurrentPresetIndex(1);
        CHECK(loaded.rippleMode()==mode && loaded.diskRenderMode()==mode);
    }
    core.setRippleMode(9); core.setDiskRenderMode(-3);
    CHECK(core.rippleMode()==0 && core.diskRenderMode()==0);
    core.setRippleMode(2); core.setDiskRenderMode(2); core.resetDefaults();
    CHECK(core.rippleMode()==0 && core.diskRenderMode()==0);
    CHECK(rippleSignals>0 && diskSignals>0);
    QFile settings(dir+"/blackhole_advanced.txt"); CHECK(settings.open(QIODevice::WriteOnly));
    settings.write("rippleMode=nan\ndiskRenderMode=1.5\nrippleAmplitude=nan\nrippleOscillations=1.5\nrippleSpread=8\nemergenceWaitMin=nan\nemergenceWaitMax=-100\n"); settings.close();
    core.loadConfig(); CHECK(core.rippleMode()==0 && core.diskRenderMode()==0);
    CHECK(core.rippleAmplitude()==1 && core.rippleOscillations()==1 && core.rippleSpread()==0);
    CHECK(core.emergenceWaitMin()>=1 && core.emergenceWaitMin()<=core.emergenceWaitMax());
    CHECK(settings.open(QIODevice::WriteOnly)); settings.write("distortion=1.0\n"); settings.close();
    core.loadConfig(); CHECK(core.rippleMode()==0 && core.diskRenderMode()==0);
    CHECK(core.effectMode()==0 && core.emergenceWaitMin()==10 && core.emergenceWaitMax()==25);
    CHECK(core.rippleOscillations()==1 && core.rippleSpread()==1);
    std::puts("QT EFFECTS CONFIG PASS");
}
