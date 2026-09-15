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
    CHECK(core.rippleMode()==0 && core.diskRenderMode()==0);
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
    settings.write("rippleMode=nan\ndiskRenderMode=1.5\n"); settings.close();
    core.loadConfig(); CHECK(core.rippleMode()==0 && core.diskRenderMode()==0);
    CHECK(settings.open(QIODevice::WriteOnly)); settings.write("distortion=1.0\n"); settings.close();
    core.loadConfig(); CHECK(core.rippleMode()==0 && core.diskRenderMode()==0);
    std::puts("QT EFFECTS CONFIG PASS");
}
