// blackholecore.cpp — 黑洞配置管理 + 进程控制 实现
#include "blackholecore.h"
#include "application_catalog.h"
#include "avatar_storage.h"
#include "autostart_registry.h"
#include "foreground_window.h"
#include "game_detection.h"
#include "media_session.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QTextStream>
#include <QStandardPaths>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <tlhelp32.h>
#endif

static constexpr int kCloseHotkeyId = 0x4248;

// ========== PresetModel ==========

PresetModel::PresetModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PresetModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_presets.size();
}

QVariant PresetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_presets.size())
        return QVariant();

    const PresetData &p = m_presets.at(index.row());

    switch (role) {
    case NameRole:       return p.name;
    case DiskTempRole:   return p.diskTemp;
    case DiskInclRole:   return p.diskIncl;
    case DiskRollRole:   return p.diskRoll;
    case DiskInnerRole:  return p.diskInner;
    case DiskOuterRole:  return p.diskOuter;
    case DiskOpacRole:   return p.diskOpac;
    case DiskDoppRole:   return p.diskDopp;
    case DiskBeamRole:   return p.diskBeam;
    case DiskGainRole:   return p.diskGain;
    case DiskContrRole:  return p.diskContr;
    case DiskWindRole:   return p.diskWind;
    case DiskSpeedRole:  return p.diskSpeed;
    case DiskExpoRole:   return p.diskExpo;
    case DiskStarRole:   return p.diskStar;
    default: return QVariant();
    }
}

bool PresetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_presets.size())
        return false;

    PresetData &p = m_presets[index.row()];

    switch (role) {
    case NameRole:       p.name = value.toString(); break;
    case DiskTempRole:   p.diskTemp  = value.toFloat(); break;
    case DiskInclRole:   p.diskIncl  = value.toFloat(); break;
    case DiskRollRole:   p.diskRoll  = value.toFloat(); break;
    case DiskInnerRole:  p.diskInner = value.toFloat(); break;
    case DiskOuterRole:  p.diskOuter = value.toFloat(); break;
    case DiskOpacRole:   p.diskOpac  = value.toFloat(); break;
    case DiskDoppRole:   p.diskDopp  = value.toFloat(); break;
    case DiskBeamRole:   p.diskBeam  = value.toFloat(); break;
    case DiskGainRole:   p.diskGain  = value.toFloat(); break;
    case DiskContrRole:  p.diskContr = value.toFloat(); break;
    case DiskWindRole:   p.diskWind  = value.toFloat(); break;
    case DiskSpeedRole:  p.diskSpeed = value.toFloat(); break;
    case DiskExpoRole:   p.diskExpo  = value.toFloat(); break;
    case DiskStarRole:   p.diskStar  = value.toFloat(); break;
    default: return false;
    }

    emit dataChanged(index, index, {role});
    return true;
}

QHash<int, QByteArray> PresetModel::roleNames() const
{
    return {
        {NameRole,       "presetName"},
        {DiskTempRole,   "diskTemp"},
        {DiskInclRole,   "diskIncl"},
        {DiskRollRole,   "diskRoll"},
        {DiskInnerRole,  "diskInner"},
        {DiskOuterRole,  "diskOuter"},
        {DiskOpacRole,   "diskOpac"},
        {DiskDoppRole,   "diskDopp"},
        {DiskBeamRole,   "diskBeam"},
        {DiskGainRole,   "diskGain"},
        {DiskContrRole,  "diskContr"},
        {DiskWindRole,   "diskWind"},
        {DiskSpeedRole,  "diskSpeed"},
        {DiskExpoRole,   "diskExpo"},
        {DiskStarRole,   "diskStar"}
    };
}

void PresetModel::setPresets(const QVector<PresetData> &presets)
{
    beginResetModel();
    m_presets = presets;
    endResetModel();
}

QVector<PresetData> PresetModel::presets() const
{
    return m_presets;
}

void PresetModel::updateParam(int index, const QString &param, float value)
{
    if (index < 0 || index >= m_presets.size()) return;

    PresetData &p = m_presets[index];

    if (param == "diskTemp")       p.diskTemp  = value;
    else if (param == "diskIncl")  p.diskIncl  = value;
    else if (param == "diskRoll")  p.diskRoll  = value;
    else if (param == "diskInner") p.diskInner = value;
    else if (param == "diskOuter") p.diskOuter = value;
    else if (param == "diskOpac")  p.diskOpac  = value;
    else if (param == "diskDopp")  p.diskDopp  = value;
    else if (param == "diskBeam")  p.diskBeam  = value;
    else if (param == "diskGain")  p.diskGain  = value;
    else if (param == "diskContr") p.diskContr = value;
    else if (param == "diskWind")  p.diskWind  = value;
    else if (param == "diskSpeed") p.diskSpeed = value;
    else if (param == "diskExpo")  p.diskExpo  = value;
    else if (param == "diskStar")  p.diskStar  = value;
    else return;

    QModelIndex idx = this->index(index, 0);
    emit dataChanged(idx, idx);
}

void PresetModel::movePreset(int from, int to)
{
    if (from < 0 || from >= m_presets.size() || to < 0 || to >= m_presets.size() || from == to)
        return;
    int destRow = (to > from) ? to + 1 : to;
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), destRow);
    m_presets.move(from, to);
    endMoveRows();
}


// ========== BlackHoleCore ==========

static const char *DEFAULT_PRESET_NAMES[16] = {
    "Inferno", "Gargantua", "M87* Donut", "Face-on Ember",
    "Quasar", "Blazar", "Pure Lens", "Inferno 2",
    "Crimson Vortex", "Azure Spiral", "Ruby Ring", "Ghost Halo",
    "Top-down Galaxy", "White Dwarf Beam", "Solar Forge", "Obsidian Eye"
};

static const float DEFAULT_PRESET_VALUES[16][14] = {
    // temp,   incl,  roll, inner,outer,opac, dopp, beam, gain, contr,wind, speed,expo, star
    { 5500,   1.50f, 0.35f, 1.8f, 8.0f, 0.90f,0.60f,2.5f, 2.2f, 1.6f, 7.0f, 5.0f, 1.40f, 0.0f }, // Inferno
    { 4500,   1.52f, 0.10f, 2.2f, 7.0f, 0.85f,0.35f,2.0f, 1.4f, 0.5f, 7.0f, 5.0f, 1.20f, 0.0f }, // Gargantua
    { 3800,   0.55f,-0.30f, 2.2f, 6.0f, 0.45f,0.90f,3.5f, 1.6f, 0.4f, 3.0f, 2.5f, 1.10f, 0.0f }, // M87* Donut
    { 6500,   0.30f, 0.00f, 3.0f,10.0f, 0.50f,0.80f,2.5f, 1.0f, 1.1f, 7.0f, 5.0f, 1.00f, 0.0f }, // Face-on Ember
    { 15000,  1.30f, 0.35f, 3.0f,14.0f, 0.35f,1.00f,4.0f, 1.2f, 1.3f, 8.0f, 5.0f, 0.80f, 0.0f }, // Quasar
    { 18000,  1.05f, 0.55f, 3.0f,16.0f, 0.30f,1.00f,5.0f, 1.0f, 1.5f, 9.0f, 6.0f, 0.75f, 0.0f }, // Blazar
    { 5500,   1.50f, 0.35f, 1.8f, 8.0f, 0.00f,1.00f,2.5f, 0.0f, 1.6f, 7.0f, 5.0f, 1.00f, 0.6f }, // Pure Lens
    { 5500,   1.50f, 0.35f, 1.8f, 8.0f, 0.90f,0.60f,2.5f, 2.2f, 1.6f, 7.0f, 5.0f, 1.40f, 0.0f }, // Inferno 2
    { 3200,   1.45f, 0.60f, 2.0f, 9.0f, 0.95f,0.20f,1.5f, 3.0f, 2.0f, 5.0f, 4.0f, 1.60f, 0.0f }, // Crimson Vortex
    { 8000,   1.20f,-0.50f, 2.5f, 7.0f, 0.70f,0.75f,2.8f, 1.8f, 1.4f, 8.0f, 5.5f, 1.30f, 0.0f }, // Azure Spiral
    { 2500,   1.55f, 0.20f, 1.6f, 6.0f, 0.60f,0.10f,1.2f, 2.6f, 1.8f, 4.0f, 3.0f, 1.50f, 0.0f }, // Ruby Ring
    { 12000,  0.80f, 0.45f, 2.8f,12.0f, 0.40f,0.95f,3.5f, 1.5f, 1.2f, 8.5f, 5.0f, 0.90f, 0.0f }, // Ghost Halo
    { 5000,   0.10f, 0.00f, 2.0f, 9.0f, 0.85f,0.50f,2.0f, 1.3f, 1.5f, 6.0f, 4.5f, 1.10f, 0.0f }, // Top-down Galaxy
    { 22000,  1.40f, 0.70f, 3.5f,18.0f, 0.25f,1.00f,4.5f, 0.9f, 1.7f,10.0f, 6.5f, 0.70f, 0.0f }, // White Dwarf Beam
    { 4200,   1.48f, 0.15f, 1.9f, 7.5f, 0.80f,0.45f,2.2f, 2.0f, 0.8f, 6.5f, 4.8f, 1.25f, 0.0f }, // Solar Forge
    { 9000,   0.45f,-0.15f, 2.6f,11.0f, 0.55f,0.85f,3.0f, 1.1f, 1.3f, 7.5f, 5.2f, 1.05f, 0.0f }  // Obsidian Eye
};

BlackHoleCore::BlackHoleCore(QObject *parent)
    : QObject(parent)
    , m_presetModel(new PresetModel(this))
    , m_presetListModel(new PresetListModel(this))
    , m_rendererProcess(nullptr)
{
    initDefaultPresets();
    QCoreApplication::instance()->installNativeEventFilter(this);

    // 空闲检测定时器
    m_idleTimer = new QTimer(this);
    m_idleTimer->setInterval(1000);
    connect(m_idleTimer, &QTimer::timeout, this, &BlackHoleCore::checkIdle);

    // 默认黑名单 = 原生 blackhole.exe isWatchingVideo() 中的列表
    m_idleBlacklist = QStringList({
        "vlc", "mpv", "potplayer", "mpc", "wmplayer",
        "bilibili", "bili",
        "iqiyi", "youku", "mgtv",
        "douyin", "kuaishou",
        "哔哩哔哩", "爱奇艺", "优酷", "芒果",
        "抖音", "快手", "腾讯视频",
        "qqlive", "nvidia",
        "chrome", "msedge", "firefox", "opera", "brave",
        "steam", "epic", "ubisoft", "ubiconnect", "eaapp", "origin",
        "battlenet", "riot", "gog", "xbox", "gamebar"
    });
}

BlackHoleCore::~BlackHoleCore()
{
    unregisterCloseHotkey();
    QCoreApplication::instance()->removeNativeEventFilter(this);
    stopRenderer();
}

// ====== 默认预设 ======

void BlackHoleCore::initDefaultPresets()
{
    QVector<PresetData> presets;
    presets.reserve(16);
    for (int i = 0; i < 16; i++) {
        PresetData p;
        p.name      = QString::fromUtf8(DEFAULT_PRESET_NAMES[i]);
        p.diskTemp  = DEFAULT_PRESET_VALUES[i][0];
        p.diskIncl  = DEFAULT_PRESET_VALUES[i][1];
        p.diskRoll  = DEFAULT_PRESET_VALUES[i][2];
        p.diskInner = DEFAULT_PRESET_VALUES[i][3];
        p.diskOuter = DEFAULT_PRESET_VALUES[i][4];
        p.diskOpac  = DEFAULT_PRESET_VALUES[i][5];
        p.diskDopp  = DEFAULT_PRESET_VALUES[i][6];
        p.diskBeam  = DEFAULT_PRESET_VALUES[i][7];
        p.diskGain  = DEFAULT_PRESET_VALUES[i][8];
        p.diskContr = DEFAULT_PRESET_VALUES[i][9];
        p.diskWind  = DEFAULT_PRESET_VALUES[i][10];
        p.diskSpeed = DEFAULT_PRESET_VALUES[i][11];
        p.diskExpo  = DEFAULT_PRESET_VALUES[i][12];
        p.diskStar  = DEFAULT_PRESET_VALUES[i][13];
        presets.append(p);
    }
    m_allLists.append(presets);
    m_listNames.append(QString::fromUtf8("\xe9\xbb\x98\xe8\xae\xa4"));
    m_presetModel->setPresets(presets);
    m_presetListModel->setNames(m_listNames);
}

// ====== 配置文件路径 ======

QString BlackHoleCore::configFilePath() const
{
    // 优先使用 blackhole.exe --render 实际读取的"项目根"目录,
    // 保证 Qt UI 与渲染器子进程读写同一份 blackhole_presets.txt.
    // blackhole.exe main.cpp:670-674 把 cwd 设为 build/ 的父目录, 即项目根.
    QString projectRoot;
    findRendererExe(&projectRoot);
    if (!projectRoot.isEmpty()) {
        return projectRoot + "/blackhole_presets.txt";
    }

    // 回退: 找不到 blackhole.exe 时, 仍按原逻辑用 appDir 或其上溯目录
    QString appDir = QCoreApplication::applicationDirPath();
    QString cfgPath = appDir + "/blackhole_presets.txt";
    if (QFileInfo::exists(cfgPath))
        return cfgPath;

    QDir dir(appDir);
    dir.cdUp(); // build → project root
    cfgPath = dir.absoluteFilePath("blackhole_presets.txt");
    if (QFileInfo::exists(cfgPath))
        return cfgPath;

    // 默认在应用目录创建
    return appDir + "/blackhole_presets.txt";
}

QString BlackHoleCore::configDir() const
{
    return QFileInfo(configFilePath()).absolutePath();
}

QString BlackHoleCore::configPath(const QString &fileName) const
{
    return QDir(configDir()).absoluteFilePath(fileName);
}

QString BlackHoleCore::legacyConfigPath(const QString &fileName) const
{
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(fileName);
}

bool BlackHoleCore::openConfigForRead(QFile &file, const QString &fileName) const
{
    const QString primaryPath = configPath(fileName);
    file.setFileName(primaryPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return true;
    }

    const QString legacyPath = legacyConfigPath(fileName);
    if (QDir::cleanPath(legacyPath) == QDir::cleanPath(primaryPath)) {
        return false;
    }

    file.setFileName(legacyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "BlackHoleCore: loaded legacy config path:" << legacyPath;
        return true;
    }

    file.setFileName(primaryPath);
    return false;
}


// ====== blackhole.exe 查找 (与 main.cpp --render 子进程对应) ======

QString BlackHoleCore::findRendererExe(QString *projectRootOut) const
{
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList searchPaths;
    QDir dir(appDir);
    // 顺序: 同级 → 上溯各级 → 兄弟 build 目录
    searchPaths << dir.absoluteFilePath("blackhole.exe");
    searchPaths << dir.absoluteFilePath("../blackhole.exe");
    searchPaths << dir.absoluteFilePath("../../blackhole.exe");
    searchPaths << dir.absoluteFilePath("../../build/blackhole.exe");       // Qt UI 默认: Blakhole_UI/build_qt → 项目根/build/
    searchPaths << dir.absoluteFilePath("../../../blackhole.exe");
    searchPaths << dir.absoluteFilePath("../../../build/blackhole.exe");
    searchPaths << dir.absoluteFilePath("../../../release/blackhole.exe");

    for (const QString &p : searchPaths) {
        if (QFileInfo::exists(p)) {
            if (projectRootOut) {
                QFileInfo fi(p);
                QDir exeDir = fi.dir();
                // blackhole.exe main.cpp 假设 exe 在 <root>/build/, 项目根 = build 的父目录
                QString parent = exeDir.absolutePath();
                if (exeDir.dirName() == "build" || exeDir.dirName() == "Build") {
                    QDir root = exeDir;
                    root.cdUp();
                    parent = root.absolutePath();
                }
                *projectRootOut = parent;
            }
            return p;
        }
    }
    return QString();
}

// ====== 配置读写 (v4 格式，与原始项目完全兼容) ======

void BlackHoleCore::loadConfig()
{
    QString path = configFilePath();
    QFile file(path);
    bool parsed = file.open(QIODevice::ReadOnly | QIODevice::Text);

    if (!parsed) {
        qDebug() << "BlackHoleCore: config file not found, using defaults:" << path;
        initDefaultPresets();
    }

    if (parsed) {
        QTextStream in(&file);

        // Line 1: comment "# Blackhole Presets v4"
        QString line = in.readLine();
        bool isV5 = line.contains("v5");
        if (!line.contains("v4") && !isV5) {
            parsed = false;
        }

        if (parsed) {
            // Line 2: mode idleSec slotSec playMode videoAsIdle autoStart [fixedSize fixedLevel captureMode displayMode]
            // 后 4 字段为扩展，旧文件缺这些字段时保留默认值
            line = in.readLine();
            if (line.isNull()) {
                parsed = false;
            } else {
                QTextStream ls(&line);
                int vidAsIdle = 0, autoSt = 0;
                ls >> m_displayMode >> m_idleSeconds >> m_slotSeconds >> m_playMode >> vidAsIdle >> autoSt;
                m_videoAsIdle = (vidAsIdle != 0);
                m_autoStart   = (autoSt != 0);
                // 扩展字段: fixedSize fixedLevel captureMode displayMode(screenTarget)
                // QTextStream 读到 EOF 或非数字时返回 0 / 不修改变量
                int fixedSz = 0;  ls >> fixedSz;
                if (!ls.atEnd()) {
                    m_fixedSize = (fixedSz != 0);
                    float fl = 1.0f;  ls >> fl;  if (fl > 0.0f) m_fixedLevel = fl;
                    int cm = -1;      ls >> cm; m_captureMode = cm;
                    int dm = 0;       ls >> dm; if (!ls.atEnd()) m_screenTarget = dm;
                }
            }
        }

        if (parsed) {
            // Line 3: presetCount
            line = in.readLine();
            if (line.isNull()) {
                parsed = false;
            } else {
                int count = line.toInt();
                if (count < 1 || count > 64) count = 16;

                QVector<PresetData> presets;
                presets.reserve(count);
                for (int i = 0; i < count; i++) {
                    QString name = in.readLine();
                    QString vals = in.readLine();
                    if (name.isNull() || vals.isNull()) break;

                    PresetData p;
                    p.name = name.trimmed();

                    QTextStream vs(&vals);
                    vs >> p.diskTemp >> p.diskIncl >> p.diskRoll >> p.diskInner >> p.diskOuter
                       >> p.diskOpac >> p.diskDopp >> p.diskBeam >> p.diskGain >> p.diskContr
                       >> p.diskWind >> p.diskSpeed >> p.diskExpo >> p.diskStar;

                    presets.append(p);
                }

                if (!presets.isEmpty()) {
                    m_presetModel->setPresets(presets);
                } else {
                    parsed = false;
                }
            }
        }

        file.close();

        if (!parsed) {
            initDefaultPresets();
        }
    }

    setCurrentPresetIndex(0);
    refreshCurrentPresetProps();
#ifdef Q_OS_WIN
    std::wstring registeredCommand;
    m_autoStart = AutoStart_Query(registeredCommand);
#endif
    emit displayModeChanged();
    emit idleSecondsChanged();
    emit playModeChanged();
    emit slotSecondsChanged();
    emit videoAsIdleChanged();
    emit autoStartChanged();
    emit fixedSizeChanged();
    emit fixedLevelChanged();
    emit captureModeChanged();
    emit screenTargetChanged();
    emit configLoaded();
    qDebug() << "BlackHoleCore: loaded" << m_presetModel->rowCount() << "presets from" << path;

    // 子配置始终加载，不受主配置解析结果影响
    loadAdvancedConfig();
    loadIdleListConfig();
    loadScheduleConfig();
    loadSystemConfig();
    loadAllLists();
}

void BlackHoleCore::saveConfig()
{
    QString path = configFilePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "BlackHoleCore: cannot write config:" << path;
        return;
    }

    QTextStream out(&file);

    out << "# Blackhole Presets v4\n";
    // 第 2 行 10 字段（与 ImGui UI / main.cpp 共用）:
    //   mode idleSec slotSec playMode videoAsIdle autoStart
    //   fixedSize fixedLevel captureMode displayMode
    out << m_displayMode << " " << m_idleSeconds << " "
        << QString::number(m_slotSeconds, 'f', 3) << " "
        << m_playMode << " " << (m_videoAsIdle ? 1 : 0) << " "
        << (m_autoStart ? 1 : 0) << " "
        << (m_fixedSize ? 1 : 0) << " "
        << QString::number(m_fixedLevel, 'f', 3) << " "
        << m_captureMode << " "
        << m_screenTarget << "\n";

    QVector<PresetData> presets = m_presetModel->presets();
    out << presets.size() << "\n";

    for (const PresetData &p : presets) {
        out << p.name << "\n";
        out << QString::number(p.diskTemp,  'f', 0) << " "
            << QString::number(p.diskIncl,  'f', 2) << " "
            << QString::number(p.diskRoll,  'f', 2) << " "
            << QString::number(p.diskInner, 'f', 1) << " "
            << QString::number(p.diskOuter, 'f', 1) << " "
            << QString::number(p.diskOpac,  'f', 2) << " "
            << QString::number(p.diskDopp,  'f', 2) << " "
            << QString::number(p.diskBeam,  'f', 1) << " "
            << QString::number(p.diskGain,  'f', 2) << " "
            << QString::number(p.diskContr, 'f', 2) << " "
            << QString::number(p.diskWind,  'f', 1) << " "
            << QString::number(p.diskSpeed, 'f', 1) << " "
            << QString::number(p.diskExpo,  'f', 2) << " "
            << QString::number(p.diskStar,  'f', 3) << "\n";
    }

    file.close();

    // 开机自启与配置文件使用同一状态；写入后由共享模块回读验证。
#ifdef Q_OS_WIN
    const AutoStartResult autoStartResult = AutoStart_Set(
        m_autoStart,
        QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString());
    if (!autoStartResult.success) {
        m_autoStartStatus = tr("开机自启动更新失败，错误码 %1").arg(autoStartResult.errorCode);
        emit autoStartStatusChanged();
    }
#endif

    // also save all other configs
    saveAdvancedConfig();
    saveIdleListConfig();
    saveScheduleConfig();
    saveSystemConfig();
    saveAllLists();

    emit configSaved();
    qDebug() << "BlackHoleCore: saved" << presets.size() << "presets to" << path;
}

void BlackHoleCore::resetDefaults()
{
    // 构建硬编码的规范默认预设
    QVector<PresetData> canonical;
    canonical.reserve(16);
    for (int i = 0; i < 16; i++) {
        PresetData p;
        p.name      = QString::fromUtf8(DEFAULT_PRESET_NAMES[i]);
        p.diskTemp  = DEFAULT_PRESET_VALUES[i][0];
        p.diskIncl  = DEFAULT_PRESET_VALUES[i][1];
        p.diskRoll  = DEFAULT_PRESET_VALUES[i][2];
        p.diskInner = DEFAULT_PRESET_VALUES[i][3];
        p.diskOuter = DEFAULT_PRESET_VALUES[i][4];
        p.diskOpac  = DEFAULT_PRESET_VALUES[i][5];
        p.diskDopp  = DEFAULT_PRESET_VALUES[i][6];
        p.diskBeam  = DEFAULT_PRESET_VALUES[i][7];
        p.diskGain  = DEFAULT_PRESET_VALUES[i][8];
        p.diskContr = DEFAULT_PRESET_VALUES[i][9];
        p.diskWind  = DEFAULT_PRESET_VALUES[i][10];
        p.diskSpeed = DEFAULT_PRESET_VALUES[i][11];
        p.diskExpo  = DEFAULT_PRESET_VALUES[i][12];
        p.diskStar  = DEFAULT_PRESET_VALUES[i][13];
        canonical.append(p);
    }

    // 检查是否已有"默认"列表
    int existingIndex = -1;
    for (int i = 0; i < m_listNames.size(); i++) {
        if (m_listNames[i] == QString::fromUtf8("\xe9\xbb\x98\xe8\xae\xa4")) {
            existingIndex = i;
            break;
        }
    }

    if (existingIndex >= 0) {
        // 已存在：逐项比较，不一致则强制覆盖为规范默认
        const QVector<PresetData>& existing = m_allLists[existingIndex];
        bool needsUpdate = (existing.size() != canonical.size());
        if (!needsUpdate) {
            for (int i = 0; i < canonical.size(); i++) {
                if (existing[i].name      != canonical[i].name      ||
                    existing[i].diskTemp  != canonical[i].diskTemp  ||
                    existing[i].diskIncl  != canonical[i].diskIncl  ||
                    existing[i].diskRoll  != canonical[i].diskRoll  ||
                    existing[i].diskInner != canonical[i].diskInner ||
                    existing[i].diskOuter != canonical[i].diskOuter ||
                    existing[i].diskOpac  != canonical[i].diskOpac  ||
                    existing[i].diskDopp  != canonical[i].diskDopp  ||
                    existing[i].diskBeam  != canonical[i].diskBeam  ||
                    existing[i].diskGain  != canonical[i].diskGain  ||
                    existing[i].diskContr != canonical[i].diskContr ||
                    existing[i].diskWind  != canonical[i].diskWind  ||
                    existing[i].diskSpeed != canonical[i].diskSpeed ||
                    existing[i].diskExpo  != canonical[i].diskExpo  ||
                    existing[i].diskStar  != canonical[i].diskStar)
                {
                    needsUpdate = true;
                    break;
                }
            }
        }
        if (needsUpdate) {
            m_allLists[existingIndex] = canonical;
        }
        m_currentListIndex = -1;
        m_presetModel->setPresets(m_allLists[existingIndex]);
        m_currentListIndex = existingIndex;
        emit currentListChanged();
    } else {
        m_allLists.append(canonical);
        m_listNames.append(QString::fromUtf8("\xe9\xbb\x98\xe8\xae\xa4"));
        m_presetModel->setPresets(canonical);
        m_presetListModel->setNames(m_listNames);
        m_currentListIndex = m_allLists.size() - 1;
        emit currentListChanged();
    }

    m_displayMode = 1;
    m_idleSeconds = 300;
    m_playMode    = 1;
    m_slotSeconds = 5.25f;
    m_videoAsIdle = false;
    m_autoStart   = false;
    m_captureMode = -1; // 默认自动检测 (Win10→DXGI 无黄框, Win11 22H2+→WGC)
    m_fixedSize   = false;
    m_fixedLevel  = 1.0f;
    m_mouseInertia = 0.30f;
    m_limitMouseOvershoot = true;
    m_lightingEffect = false;

    emit displayModeChanged();
    emit idleSecondsChanged();
    emit playModeChanged();
    emit slotSecondsChanged();
    emit videoAsIdleChanged();
    emit autoStartChanged();
    emit captureModeChanged();
    emit fixedSizeChanged();
    emit fixedLevelChanged();
    emit screenTargetChanged();

    setCurrentPresetIndex(0);
    emit followMouseChanged();
    emit mouseInertiaChanged();
    emit limitMouseOvershootChanged();
    emit randomPathChanged();
    emit animationSpeedChanged();
    emit lightingEffectChanged();
    emit distortionChanged();
    emit holeSizeChanged();
    emit growEnabledChanged();
    emit initialSizeChanged();
    emit idleWhitelistChanged();
    emit idleBlacklistChanged();
    emit idleForceBlocklistChanged();
    emit scheduleEnabledChanged();
    emit startHourChanged(); emit startMinuteChanged();
    emit endHourChanged(); emit endMinuteChanged();
    emit countdownEnabledChanged();
    emit countdownMinutesChanged();

    refreshCurrentPresetProps();
}


void BlackHoleCore::startRenderer()
{
    if (m_rendererProcess && m_rendererProcess->state() != QProcess::NotRunning) {
        qDebug() << "BlackHoleCore: renderer already running";
        return;
    }

    // 先保存配置到文件（blackhole.exe --render 从文件读取配置）
    saveConfig();

    // 查找 blackhole.exe + 它的项目根目录 (保证读写同一份 presets + shaders)
    QString projectRoot;
    QString exePath = findRendererExe(&projectRoot);

    if (exePath.isEmpty()) {
        qWarning() << "BlackHoleCore: blackhole.exe not found";
        emit rendererError(QStringLiteral("找不到 blackhole.exe，请确认程序位置"));
        return;
    }

    if (!m_rendererProcess) {
        m_rendererProcess = new QProcess(this);

        connect(m_rendererProcess, &QProcess::started, this, [this]() {
            qDebug() << "BlackHoleCore: renderer started";
            emit rendererStarted();
            emit rendererRunningChanged();
            emit systemActiveChanged();
        });

        connect(m_rendererProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus) {
            qDebug() << "BlackHoleCore: renderer stopped, exit code:" << code;
            emit rendererStopped();
            emit rendererRunningChanged();
            emit systemActiveChanged();
        });

        connect(m_rendererProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
            Q_UNUSED(err);
            QString msg = m_rendererProcess->errorString();
            qWarning() << "BlackHoleCore: renderer error:" << msg;
            emit rendererError(msg);
            emit rendererRunningChanged();
            emit systemActiveChanged();
        });
    }

    // 设 working directory 到项目根, 与 blackhole.exe main.cpp 的 SetCurrentDirectory 一致,
    // 让渲染器从项目根读 shaders/ + blackhole_presets.txt (与 Qt UI 写入的同一份)
    if (!projectRoot.isEmpty()) {
        m_rendererProcess->setWorkingDirectory(projectRoot);
    }

    qDebug() << "BlackHoleCore: starting" << exePath << "--render" << "cwd:" << projectRoot;
    m_rendererProcess->start(exePath, QStringList() << "--render");
}
void BlackHoleCore::stopRenderer()
{
    if (!m_rendererProcess)
        return;

    if (m_rendererProcess->state() != QProcess::NotRunning) {
        m_rendererProcess->terminate();
        if (!m_rendererProcess->waitForFinished(3000)) {
            m_rendererProcess->kill();
            m_rendererProcess->waitForFinished(2000);
        }
    }
}

void BlackHoleCore::applyAndStart()
{
    qDebug() << "BlackHoleCore: applyAndStart() called, displayMode=" << m_displayMode;
    saveConfig();
    if (m_displayMode == 0) {
        // 始终显示模式: 直接启动渲染器
        startRenderer();
    } else {
        // 空闲检测模式: 启动定时器，等待空闲时自动启动
        stopRenderer();
        if (m_idleTimer && !m_idleTimer->isActive()) {
            m_idleTimer->start();
            emit systemActiveChanged();
            qDebug() << "BlackHoleCore: idle detection started, waiting for idle...";
        }
    }
}

void BlackHoleCore::stopAll()
{
    if (m_idleTimer && m_idleTimer->isActive()) {
        m_idleTimer->stop();
        qDebug() << "BlackHoleCore: idle detection stopped";
    }
    stopRenderer();
    emit systemActiveChanged();
}

bool BlackHoleCore::rendererRunning() const
{
    return m_rendererProcess && m_rendererProcess->state() != QProcess::NotRunning;
}

bool BlackHoleCore::isSystemActive() const
{
    return (m_idleTimer && m_idleTimer->isActive()) || (m_rendererProcess && m_rendererProcess->state() != QProcess::NotRunning);
}

// ====== 属性访问器 ======

int BlackHoleCore::displayMode() const       { return m_displayMode; }
void BlackHoleCore::setDisplayMode(int mode) {
    if (m_displayMode == mode) return;
    m_displayMode = mode;
    emit displayModeChanged();
    // 切换模式时停止一切，等待用户手动启动
    if (m_idleTimer && m_idleTimer->isActive()) m_idleTimer->stop();
    stopRenderer();
    emit systemActiveChanged();
}

int BlackHoleCore::idleSeconds() const        { return m_idleSeconds; }
void BlackHoleCore::setIdleSeconds(int sec)   { if (m_idleSeconds != sec) { m_idleSeconds = sec; emit idleSecondsChanged(); } }

int BlackHoleCore::playMode() const           { return m_playMode; }
void BlackHoleCore::setPlayMode(int m)        { if (m_playMode != m) { m_playMode = m; emit playModeChanged(); } }

float BlackHoleCore::slotSeconds() const      { return m_slotSeconds; }
void BlackHoleCore::setSlotSeconds(float s)   { if (!qFuzzyCompare(m_slotSeconds, s)) { m_slotSeconds = s; emit slotSecondsChanged(); } }

bool BlackHoleCore::videoAsIdle() const       { return m_videoAsIdle; }
void BlackHoleCore::setVideoAsIdle(bool v)    { if (m_videoAsIdle != v) { m_videoAsIdle = v; emit videoAsIdleChanged(); } }

bool BlackHoleCore::autoStart() const         { return m_autoStart; }
void BlackHoleCore::setAutoStart(bool v)
{
    if (m_autoStart == v) return;
#ifdef Q_OS_WIN
    const AutoStartResult result = AutoStart_Set(
        v, QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString());
    if (!result.success) {
        m_autoStartStatus = tr("开机自启动更新失败，错误码 %1").arg(result.errorCode);
        emit autoStartStatusChanged();
        emit autoStartChanged();
        return;
    }
#endif
    m_autoStart = v;
    m_autoStartStatus = v ? tr("开机自启动已开启") : tr("开机自启动已关闭");
    emit autoStartChanged();
    emit autoStartStatusChanged();
}
QString BlackHoleCore::autoStartStatus() const { return m_autoStartStatus; }

bool BlackHoleCore::launchMinimized() const          { return m_launchMinimized; }
void BlackHoleCore::setLaunchMinimized(bool v)     { if (m_launchMinimized == v) return; m_launchMinimized = v; emit launchMinimizedChanged(); }

int BlackHoleCore::screenTarget() const { return m_screenTarget; }
void BlackHoleCore::setScreenTarget(int v) { if (m_screenTarget == v) return; m_screenTarget = v; emit screenTargetChanged(); }

int BlackHoleCore::captureMode() const { return m_captureMode; }
void BlackHoleCore::setCaptureMode(int v) { if (m_captureMode == v) return; m_captureMode = v; emit captureModeChanged(); }

bool BlackHoleCore::fixedSize() const { return m_fixedSize; }
void BlackHoleCore::setFixedSize(bool v) { if (m_fixedSize == v) return; m_fixedSize = v; emit fixedSizeChanged(); }

float BlackHoleCore::fixedLevel() const { return m_fixedLevel; }
void BlackHoleCore::setFixedLevel(float v) {
    if (v < 0.01f) v = 0.01f;
    if (v > 1.0f) v = 1.0f;
    if (qFuzzyCompare(m_fixedLevel, v)) return;
    m_fixedLevel = v; emit fixedLevelChanged();
}

int BlackHoleCore::currentPresetIndex() const { return m_currentPresetIndex; }
void BlackHoleCore::setCurrentPresetIndex(int index)
{
    if (index < 0 || index >= m_presetModel->rowCount()) return;
    if (m_currentPresetIndex != index) {
        m_currentPresetIndex = index;
        emit currentPresetIndexChanged();
        refreshCurrentPresetProps();
    }
}

QAbstractItemModel* BlackHoleCore::presetModel()
{
    return m_presetModel;
}

// ====== 当前预设参数 ======

void BlackHoleCore::refreshCurrentPresetProps()
{
    if (m_refreshingProps) return;  // 防止信号级联
    m_refreshingProps = true;
    emit currentPresetChanged();
    m_refreshingProps = false;
}

float BlackHoleCore::diskTemp()  const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskTempRole).toFloat(); }
float BlackHoleCore::diskIncl()  const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskInclRole).toFloat(); }
float BlackHoleCore::diskRoll()  const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskRollRole).toFloat(); }
float BlackHoleCore::diskInner() const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskInnerRole).toFloat(); }
float BlackHoleCore::diskOuter() const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskOuterRole).toFloat(); }
float BlackHoleCore::diskOpac()  const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskOpacRole).toFloat(); }
float BlackHoleCore::diskDopp()  const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskDoppRole).toFloat(); }
float BlackHoleCore::diskBeam()  const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskBeamRole).toFloat(); }
float BlackHoleCore::diskGain()  const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskGainRole).toFloat(); }
float BlackHoleCore::diskContr() const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskContrRole).toFloat(); }
float BlackHoleCore::diskWind()  const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskWindRole).toFloat(); }
float BlackHoleCore::diskSpeed() const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskSpeedRole).toFloat(); }
float BlackHoleCore::diskExpo()  const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskExpoRole).toFloat(); }
float BlackHoleCore::diskStar()  const { return m_presetModel->data(m_presetModel->index(m_currentPresetIndex, 0), PresetModel::DiskStarRole).toFloat(); }

// ====== 公开槽函数 ======

void BlackHoleCore::selectPreset(int index)
{
    setCurrentPresetIndex(index);
}

void BlackHoleCore::updatePresetParam(int index, const QString &param, float value)
{
    m_presetModel->updateParam(index, param, value);
    // 如果修改的是当前预设，刷新属性
    if (index == m_currentPresetIndex) {
        refreshCurrentPresetProps();
    }
}

void BlackHoleCore::updateCurrentPresetParam(const QString &param, float value)
{
    updatePresetParam(m_currentPresetIndex, param, value);
}


void BlackHoleCore::movePreset(int from, int to)
{
    int oldCurrent = m_currentPresetIndex;
    m_presetModel->movePreset(from, to);
    if (oldCurrent == from) {
        m_currentPresetIndex = to;
    } else if (from < oldCurrent && to >= oldCurrent) {
        m_currentPresetIndex--;
    } else if (from > oldCurrent && to <= oldCurrent) {
        m_currentPresetIndex++;
    }
    emit currentPresetIndexChanged();
    refreshCurrentPresetProps();
}


void BlackHoleCore::copyPreset(int index)
{
    if (index < 0 || index >= m_presetModel->rowCount()) return;
    QModelIndex idx = m_presetModel->index(index, 0);
    m_copiedPreset.name      = m_presetModel->data(idx, PresetModel::NameRole).toString();
    m_copiedPreset.diskTemp  = m_presetModel->data(idx, PresetModel::DiskTempRole).toFloat();
    m_copiedPreset.diskIncl  = m_presetModel->data(idx, PresetModel::DiskInclRole).toFloat();
    m_copiedPreset.diskRoll  = m_presetModel->data(idx, PresetModel::DiskRollRole).toFloat();
    m_copiedPreset.diskInner = m_presetModel->data(idx, PresetModel::DiskInnerRole).toFloat();
    m_copiedPreset.diskOuter = m_presetModel->data(idx, PresetModel::DiskOuterRole).toFloat();
    m_copiedPreset.diskOpac  = m_presetModel->data(idx, PresetModel::DiskOpacRole).toFloat();
    m_copiedPreset.diskDopp  = m_presetModel->data(idx, PresetModel::DiskDoppRole).toFloat();
    m_copiedPreset.diskBeam  = m_presetModel->data(idx, PresetModel::DiskBeamRole).toFloat();
    m_copiedPreset.diskGain  = m_presetModel->data(idx, PresetModel::DiskGainRole).toFloat();
    m_copiedPreset.diskContr = m_presetModel->data(idx, PresetModel::DiskContrRole).toFloat();
    m_copiedPreset.diskWind  = m_presetModel->data(idx, PresetModel::DiskWindRole).toFloat();
    m_copiedPreset.diskSpeed = m_presetModel->data(idx, PresetModel::DiskSpeedRole).toFloat();
    m_copiedPreset.diskExpo  = m_presetModel->data(idx, PresetModel::DiskExpoRole).toFloat();
    m_copiedPreset.diskStar  = m_presetModel->data(idx, PresetModel::DiskStarRole).toFloat();
    m_hasCopiedPreset = true;
}

void BlackHoleCore::pastePreset()
{
    if (!m_hasCopiedPreset) return;
    QString baseName = m_copiedPreset.name;
    if (baseName.endsWith(')')) {
        int lastParen = baseName.lastIndexOf(" (");
        if (lastParen > 0) baseName = baseName.left(lastParen);
    }
    QVector<PresetData> all = m_presetModel->presets();
    int copyNum = 1;
    QString newName;
    do {
        newName = baseName + " (" + QString::number(copyNum) + ')';
        copyNum++;
    } while (std::any_of(all.begin(), all.end(),
              [&](const PresetData &p) { return p.name == newName; }));
    PresetData newPreset = m_copiedPreset;
    newPreset.name = newName;
    all.append(newPreset);
    m_presetModel->setPresets(all);
}

void BlackHoleCore::removePreset(int index)
{
    if (index < 0 || index >= m_presetModel->rowCount() || m_presetModel->rowCount() <= 1)
        return;
    QVector<PresetData> all = m_presetModel->presets();
    all.removeAt(index);
    m_presetModel->setPresets(all);
    if (m_currentPresetIndex >= m_presetModel->rowCount())
        m_currentPresetIndex = m_presetModel->rowCount() - 1;
    else if (m_currentPresetIndex > index)
        m_currentPresetIndex--;
    emit currentPresetIndexChanged();
    refreshCurrentPresetProps();
}

QAbstractItemModel* BlackHoleCore::presetListModel()
{
    return m_presetListModel;
}

QString BlackHoleCore::currentListName() const
{
    if (m_currentListIndex >= 0 && m_currentListIndex < m_listNames.size())
        return m_listNames.at(m_currentListIndex);
    return QString();
}

void BlackHoleCore::switchToList(int index)
{
    if (index < 0 || index >= m_allLists.size() || index == m_currentListIndex)
        return;
    m_currentListIndex = index;
    m_presetModel->setPresets(m_allLists[index]);
    m_currentPresetIndex = 0;
    emit currentPresetIndexChanged();
    emit currentListChanged();
    refreshCurrentPresetProps();
}

void BlackHoleCore::deleteCurrentList()
{
    if (m_allLists.size() <= 1)
        return;  // 至少保留一个列表
    int idx = m_currentListIndex;
    m_allLists.removeAt(idx);
    m_listNames.removeAt(idx);
    int newIdx = (idx > 0) ? idx - 1 : 0;
    m_currentListIndex = -1;
    m_presetModel->setPresets(m_allLists[newIdx]);
    m_presetListModel->setNames(m_listNames);
    m_currentListIndex = newIdx;
    m_currentPresetIndex = 0;
    emit currentPresetIndexChanged();
    emit currentListChanged();
    refreshCurrentPresetProps();
}

void BlackHoleCore::createPresetList(const QString &name)
{
    m_listNames.append(name);
    QVector<PresetData> newList;
    PresetData defaultPreset;
    defaultPreset.name = "Default";
    newList.append(defaultPreset);
    m_allLists.append(newList);
    m_presetListModel->setNames(m_listNames);
    switchToList(m_allLists.size() - 1);
}

void BlackHoleCore::renameCurrentList(const QString &name)
{
    if (m_currentListIndex < 0 || m_currentListIndex >= m_listNames.size())
        return;
    m_listNames[m_currentListIndex] = name;
    m_presetListModel->setNames(m_listNames);
    emit currentListChanged();
}

void BlackHoleCore::createPreset()
{
    if (m_currentListIndex < 0 || m_currentListIndex >= m_allLists.size())
        return;
    QVector<PresetData> &list = m_allLists[m_currentListIndex];
    QString base = "New Preset";
    int n = 1;
    QString name;
    bool exists;
    do {
        name = base + " " + QString::number(n++);
        exists = false;
        for (const auto &p : list) { if (p.name == name) { exists = true; break; } }
    } while (exists);
    PresetData newPreset;
    newPreset.name = name;
    list.append(newPreset);
    m_presetModel->setPresets(list);
    m_currentPresetIndex = list.size() - 1;
    emit currentPresetIndexChanged();
    refreshCurrentPresetProps();
}

void BlackHoleCore::renameCurrentPreset(const QString &name)
{
    if (m_currentPresetIndex < 0 || m_currentPresetIndex >= m_presetModel->rowCount())
        return;
    QModelIndex idx = m_presetModel->index(m_currentPresetIndex, 0);
    m_presetModel->setData(idx, name, PresetModel::NameRole);
    if (m_currentListIndex >= 0 && m_currentListIndex < m_allLists.size()) {
        m_allLists[m_currentListIndex][m_currentPresetIndex].name = name;
    }
}



// ====== 空闲检测 ======

// IAudioMeterInformation GUID (missing from MinGW headers)
static const GUID IID_IAudioMeterInformation2 = {0xC02216F6,0x8C67,0x4B5B,{0x9D,0x00,0xD0,0x08,0xE7,0x3E,0x00,0x64}};
struct IAudioMeterInformation2 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT, float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD*) = 0;
};

// Get process name from PID (matches native blackhole.exe)
static void GetProcName(DWORD pid, char* out, int maxLen) {
    out[0] = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W peW = { sizeof(peW) };
    if (Process32FirstW(snap, &peW)) {
        do {
            if (peW.th32ProcessID == pid) {
                int len = WideCharToMultiByte(CP_UTF8, 0, peW.szExeFile, -1, out, maxLen, NULL, NULL);
                if (len == 0) { out[0] = 0; break; }
                for (int i = 0; out[i] && i < maxLen - 1; i++) {
                    out[i] = (char)tolower((unsigned char)out[i]);
                }
                break;
            }
        } while (Process32NextW(snap, &peW));
    }
    CloseHandle(snap);
}

static bool IdleListMatchesProcess(const QStringList &list, const char *processName)
{
    const QString process = QString::fromUtf8(processName);
    for (const QString &entry : list) {
        if (process.contains(entry, Qt::CaseInsensitive)) return true;
    }
    return false;
}

void BlackHoleCore::checkIdle()
{
#ifdef Q_OS_WIN
    if (m_displayMode != 1) {
        setIdleDetectionState(tr("空闲检测未启用"), false);
        return;
    }

    LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
    if (!GetLastInputInfo(&lii)) return;
    DWORD idleMs = GetTickCount() - lii.dwTime;
    DWORD idleThresholdMs = static_cast<DWORD>(m_idleSeconds) * 1000;

    // === 完全匹配原生 isWatchingVideo() 6层检测 ===

    HWND fg = GetForegroundWindow();
    bool watchingVideo = false;
    bool forceBlocked = false;
    bool uwpDetected = false;
    QString foregroundProcess;
    QString detectionReason = tr("未发现可靠媒体或游戏信号");

    if (fg) {
        // 第1层: 排除桌面、系统外壳和黑洞自己的渲染窗口。
        const ForegroundKind foregroundKind = Foreground_Classify(fg);
        if (foregroundKind != ForegroundKind::Application) {
            detectionReason = foregroundKind == ForegroundKind::Desktop
                ? tr("Windows 桌面，允许触发")
                : tr("系统界面，允许触发");
            goto check_foreground_done;
        }

        // 第2层: D3D 独占全屏
        {
            typedef enum { QUNS_NOT_PRESENT=1, QUNS_BUSY=2,
                           QUNS_RUNNING_D3D_FULL_SCREEN=3, QUNS_PRESENTATION_MODE=4 } QUNS;
            typedef HRESULT (WINAPI *PFN_QUNS)(QUNS*);
            static PFN_QUNS pfnQuns = nullptr;
            if (!pfnQuns) pfnQuns = (PFN_QUNS)GetProcAddress(
                GetModuleHandleA("shell32.dll"), "SHQueryUserNotificationState");
            if (pfnQuns) {
                QUNS state;
                if (SUCCEEDED(pfnQuns(&state)) &&
                    (state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE)) {
                    watchingVideo = true;
                    detectionReason = tr("系统全屏或演示模式");
                }
            }
        }

        // 第3层: 当前显示器上的无边框全屏窗口。
        if (!watchingVideo && Foreground_IsBorderlessFullscreen(fg)) {
            watchingVideo = true;
            detectionReason = tr("无边框全屏应用");
        }

        // 获取进程名 (使用 CreateToolhelp32Snapshot, 匹配原生)
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        if (!pid) goto check_foreground_done;
        char pname[260];
        GetProcName(pid, pname, sizeof(pname));
        if (!pname[0]) goto check_foreground_done;
        foregroundProcess = QString::fromUtf8(pname);

        // 白名单检测 (新UI独有: 白名单进程不阻止黑洞触发)
        bool isWhitelisted = false;
        if (!m_idleWhitelist.isEmpty()) {
            isWhitelisted = IdleListMatchesProcess(m_idleWhitelist, pname);
            if (isWhitelisted) {
                watchingVideo = false;
                detectionReason = tr("命中始终允许触发名单");
                goto check_foreground_done;
            }
        }

        if (IdleListMatchesProcess(m_idleForceBlocklist, pname)) {
            forceBlocked = true;
            detectionReason = tr("命中前台强制不触发名单");
            goto check_foreground_done;
        }

        if (GameDetection_IsKnownGameProcess(pid)) {
            watchingVideo = true;
            detectionReason = tr("Windows 或 Steam 游戏记录");
            goto check_foreground_done;
        }

        // 第4层: 进程名分类检测

        // 专用视频播放器黑名单匹配
        bool isDedicatedVideoPlayer = false;
        isDedicatedVideoPlayer = IdleListMatchesProcess(m_idleBlacklist, pname);

        // 浏览器
        bool isBrowser = (strstr(pname, "chrome") || strstr(pname, "msedge") ||
                          strstr(pname, "firefox") || strstr(pname, "opera") ||
                          strstr(pname, "brave"));

        // 第5层: UWP 窗口标题检测
        bool isUWPVideo = false;
        if (strstr(pname, "applicationframehost")) {
            WCHAR wtitle[256] = {};
            GetWindowTextW(fg, wtitle, 256);
            if (wtitle[0]) {
                if (wcsstr(wtitle, L"\u7535\u5f71") || wcsstr(wtitle, L"\u7535\u89c6") ||
                    wcsstr(wtitle, L"\u5a92\u4f53") || wcsstr(wtitle, L"\u64ad\u653e") ||
                    wcsstr(wtitle, L"\u89c6\u9891") || wcsstr(wtitle, L"Movies") ||
                    wcsstr(wtitle, L"Media")  || wcsstr(wtitle, L"Player") ||
                    wcsstr(wtitle, L"Video")  || wcsstr(wtitle, L"TV") ||
                    wcsstr(wtitle, L"\u5f71\u89c6") || wcsstr(wtitle, L"Films")) {
                    isUWPVideo = true;
                    uwpDetected = true;
                }
            }
        }

        // 非视频播放器 AND 非浏览器 AND 非UWP视频 -> 不阻止
        if (!isDedicatedVideoPlayer && !isBrowser && !isUWPVideo)
            goto check_foreground_done;

        // 浏览器: 需要窗口标题含视频关键词 (匹配原生)
        if (isBrowser && !uwpDetected) {
            WCHAR wtitle[256] = {};
            GetWindowTextW(fg, wtitle, 256);
            bool hasVideoKeyword = (wcsstr(wtitle, L"YouTube") || wcsstr(wtitle, L"Youtube") ||
                                    wcsstr(wtitle, L"youtube") || wcsstr(wtitle, L"Bilibili") ||
                                    wcsstr(wtitle, L"bilibili") || wcsstr(wtitle, L"\u54d4\u54e9") ||
                                    wcsstr(wtitle, L"\u89c6\u9891") || wcsstr(wtitle, L"Video") ||
                                    wcsstr(wtitle, L"\u64ad\u653e") || wcsstr(wtitle, L"Netflix") ||
                                    wcsstr(wtitle, L"\u7231\u5947\u827a") || wcsstr(wtitle, L"\u4f18\u9177") ||
                                    wcsstr(wtitle, L"\u817e\u8baf") || wcsstr(wtitle, L"\u6296\u97f3") ||
                                    wcsstr(wtitle, L"\u7535\u5f71") || wcsstr(wtitle, L"Movie") ||
                                    wcsstr(wtitle, L"\u76f4\u64ad") || wcsstr(wtitle, L"Live"));
            if (!hasVideoKeyword) goto check_foreground_done;
        }

        const MediaSessionSnapshot mediaSession = MediaSession_QueryCurrent();
        if (mediaSession.state == MediaPlaybackState::Playing &&
            MediaSession_SourceMatchesProcess(mediaSession.sourceAppId, pname)) {
            watchingVideo = true;
            detectionReason = tr("系统媒体会话正在播放");
            goto check_foreground_done;
        }

        // 第6层: 音频检测 (匹配原生 IAudioSessionManager2 + IAudioMeterInformation2)
        {
            CoInitializeEx(NULL, COINIT_MULTITHREADED);
            IMMDeviceEnumerator* en = nullptr;
            HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                           __uuidof(IMMDeviceEnumerator), (void**)&en);
            if (FAILED(hr)) goto check_foreground_done;

            IMMDevice* dev = nullptr;
            hr = en->GetDefaultAudioEndpoint(eRender, eConsole, &dev);
            en->Release();
            if (FAILED(hr)) goto check_foreground_done;

            IAudioSessionManager2* mgr = nullptr;
            hr = dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&mgr);
            dev->Release();
            if (FAILED(hr)) goto check_foreground_done;

            IAudioSessionEnumerator* se = nullptr;
            hr = mgr->GetSessionEnumerator(&se);
            if (FAILED(hr)) { mgr->Release(); goto check_foreground_done; }

            int count = 0; se->GetCount(&count);
            bool hasAudio = false;
            for (int i = 0; i < count && !hasAudio; i++) {
                IAudioSessionControl* sc = nullptr;
                if (FAILED(se->GetSession(i, &sc))) continue;
                IAudioSessionControl2* sc2 = nullptr;
                if (SUCCEEDED(sc->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sc2))) {
                    DWORD spid; sc2->GetProcessId(&spid);
                    if (spid != GetCurrentProcessId()) {
                        bool match;
                        if (uwpDetected) {
                            match = true;
                        } else {
                            char spname[260];
                            GetProcName(spid, spname, sizeof(spname));
                            match = spname[0] && (strcmp(spname, pname) == 0);
                        }
                        if (match) {
                            IAudioMeterInformation2* meter = nullptr;
                            if (SUCCEEDED(sc2->QueryInterface(IID_IAudioMeterInformation2, (void**)&meter))) {
                                float peak = 0; meter->GetPeakValue(&peak);
                                if (peak > 0.02f) hasAudio = true;
                                meter->Release();
                            }
                        }
                    }
                    sc2->Release();
                }
                sc->Release();
            }
            se->Release(); mgr->Release();
            watchingVideo = hasAudio;
            if (hasAudio) detectionReason = tr("媒体应用音频正在播放");
        }
    } else {
        detectionReason = tr("未检测到前台窗口");
    }

check_foreground_done:

    // videoAsIdle: 视频播放时不阻止黑洞触发
    bool blocked = forceBlocked || (watchingVideo && !m_videoAsIdle);
    if (watchingVideo && m_videoAsIdle && !forceBlocked) {
        detectionReason += tr("，已按设置允许触发");
    }
    const QString detectionSummary = foregroundProcess.isEmpty()
        ? detectionReason
        : QStringLiteral("%1 · %2").arg(foregroundProcess, detectionReason);
    setIdleDetectionState(detectionSummary, blocked);

    bool running = (m_rendererProcess && m_rendererProcess->state() != QProcess::NotRunning);

    if (idleMs >= idleThresholdMs && !blocked) {
        if (!running) {
            qDebug() << "BlackHoleCore: idle detected, starting renderer";
            startRenderer();
        }
    } else {
        if (running) {
            qDebug() << "BlackHoleCore: user active or blocked, stopping renderer";
            stopRenderer();
        }
    }
#else
    Q_UNUSED(this);
#endif
}
// ====== 高级设置 getter/setter ======

bool BlackHoleCore::followMouse() const { return m_followMouse; }
void BlackHoleCore::setFollowMouse(bool v) { if (m_followMouse == v) return; m_followMouse = v; emit followMouseChanged(); }

float BlackHoleCore::mouseInertia() const { return m_mouseInertia; }
void BlackHoleCore::setMouseInertia(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    if (qFuzzyCompare(m_mouseInertia, v)) return;
    m_mouseInertia = v;
    emit mouseInertiaChanged();
}

bool BlackHoleCore::limitMouseOvershoot() const { return m_limitMouseOvershoot; }
void BlackHoleCore::setLimitMouseOvershoot(bool v)
{
    if (m_limitMouseOvershoot == v) return;
    m_limitMouseOvershoot = v;
    emit limitMouseOvershootChanged();
}

bool BlackHoleCore::randomPath() const { return m_randomPath; }
void BlackHoleCore::setRandomPath(bool v) { if (m_randomPath == v) return; m_randomPath = v; emit randomPathChanged(); }

int BlackHoleCore::animationSpeed() const { return m_animationSpeed; }
void BlackHoleCore::setAnimationSpeed(int v) { if (m_animationSpeed == v) return; m_animationSpeed = v; emit animationSpeedChanged(); }

bool BlackHoleCore::lightingEffect() const { return m_lightingEffect; }
void BlackHoleCore::setLightingEffect(bool v) { if (m_lightingEffect == v) return; m_lightingEffect = v; emit lightingEffectChanged(); }

float BlackHoleCore::distortion() const { return m_distortion; }
void BlackHoleCore::setDistortion(float v) { if (qFuzzyCompare(m_distortion, v)) return; m_distortion = v; emit distortionChanged(); }

bool BlackHoleCore::allowRecordingCapture() const { return m_allowRecordingCapture; }
void BlackHoleCore::setAllowRecordingCapture(bool v)
{
    if (m_allowRecordingCapture == v) return;
    m_allowRecordingCapture = v;
    emit allowRecordingCaptureChanged();
}

float BlackHoleCore::holeSize() const { return m_holeSize; }
void BlackHoleCore::setHoleSize(float v) { if (qFuzzyCompare(m_holeSize, v)) return; m_holeSize = v; emit holeSizeChanged(); }

bool BlackHoleCore::growEnabled() const { return m_growEnabled; }
void BlackHoleCore::setGrowEnabled(bool v) { if (m_growEnabled == v) return; m_growEnabled = v; emit growEnabledChanged(); }

float BlackHoleCore::initialSize() const { return m_initialSize; }
void BlackHoleCore::setInitialSize(float v) { if (qFuzzyCompare(m_initialSize, v)) return; m_initialSize = v; emit initialSizeChanged(); }

// ====== 空闲名单 getter/setter ======

static QStringList NormalizeIdleList(const QStringList &list)
{
    QStringList normalized;
    for (const QString &entry : list) {
        const QString value = entry.trimmed();
        if (value.isEmpty()) continue;
        bool duplicate = false;
        for (const QString &existing : normalized) {
            if (existing.compare(value, Qt::CaseInsensitive) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) normalized.append(value);
    }
    return normalized;
}

static QVariantMap ApplicationEntryToMap(const ApplicationCatalogEntry &entry)
{
    if (entry.processName.isEmpty() || entry.executablePath.isEmpty()) return {};
    return {
        {QStringLiteral("displayName"), entry.displayName},
        {QStringLiteral("processName"), entry.processName},
        {QStringLiteral("executablePath"), entry.executablePath},
        {QStringLiteral("iconDataUrl"), entry.iconDataUrl}
    };
}

QStringList BlackHoleCore::idleWhitelist() const { return m_idleWhitelist; }
void BlackHoleCore::setIdleWhitelist(const QStringList &list)
{
    const QStringList normalized = NormalizeIdleList(list);
    if (m_idleWhitelist == normalized) return;
    m_idleWhitelist = normalized;
    emit idleWhitelistChanged();
}

QStringList BlackHoleCore::idleBlacklist() const { return m_idleBlacklist; }
void BlackHoleCore::setIdleBlacklist(const QStringList &list)
{
    const QStringList normalized = NormalizeIdleList(list);
    if (m_idleBlacklist == normalized) return;
    m_idleBlacklist = normalized;
    emit idleBlacklistChanged();
}

QStringList BlackHoleCore::idleForceBlocklist() const { return m_idleForceBlocklist; }
void BlackHoleCore::setIdleForceBlocklist(const QStringList &list)
{
    const QStringList normalized = NormalizeIdleList(list);
    if (m_idleForceBlocklist == normalized) return;
    m_idleForceBlocklist = normalized;
    emit idleForceBlocklistChanged();
}

QVariantList BlackHoleCore::runningApplications() const
{
    QVariantList applications;
#ifdef Q_OS_WIN
    const QVector<ApplicationCatalogEntry> entries =
        ApplicationCatalog_EnumerateRunning(GetCurrentProcessId());
    applications.reserve(entries.size());
    for (const ApplicationCatalogEntry &entry : entries) {
        const QVariantMap application = ApplicationEntryToMap(entry);
        if (!application.isEmpty()) applications.append(application);
    }
#endif
    return applications;
}

QVariantMap BlackHoleCore::chooseExecutable()
{
    const QString path = QFileDialog::getOpenFileName(
        nullptr,
        tr("选择程序"),
        QString(),
        tr("可执行程序 (*.exe)"));
    if (path.isEmpty()) return {};
    return ApplicationEntryToMap(ApplicationCatalog_FromExecutable(path));
}

QString BlackHoleCore::idleDetectionSummary() const
{
    return m_idleDetectionSummary;
}

bool BlackHoleCore::idleDetectionBlocked() const
{
    return m_idleDetectionBlocked;
}

void BlackHoleCore::setIdleDetectionState(const QString &summary, bool blocked)
{
    if (m_idleDetectionSummary != summary) {
        m_idleDetectionSummary = summary;
        emit idleDetectionSummaryChanged();
    }
    if (m_idleDetectionBlocked != blocked) {
        m_idleDetectionBlocked = blocked;
        emit idleDetectionBlockedChanged();
    }
}

// ====== 定时显示 getter/setter ======

bool BlackHoleCore::scheduleEnabled() const { return m_scheduleEnabled; }
void BlackHoleCore::setScheduleEnabled(bool v) { if (m_scheduleEnabled == v) return; m_scheduleEnabled = v; emit scheduleEnabledChanged(); }

int BlackHoleCore::startHour() const { return m_startHour; }
void BlackHoleCore::setStartHour(int v) { if (m_startHour == v) return; m_startHour = v; emit startHourChanged(); }

int BlackHoleCore::startMinute() const { return m_startMinute; }
void BlackHoleCore::setStartMinute(int v) { if (m_startMinute == v) return; m_startMinute = v; emit startMinuteChanged(); }

int BlackHoleCore::endHour() const { return m_endHour; }
void BlackHoleCore::setEndHour(int v) { if (m_endHour == v) return; m_endHour = v; emit endHourChanged(); }

int BlackHoleCore::endMinute() const { return m_endMinute; }
void BlackHoleCore::setEndMinute(int v) { if (m_endMinute == v) return; m_endMinute = v; emit endMinuteChanged(); }

bool BlackHoleCore::countdownEnabled() const { return m_countdownEnabled; }
void BlackHoleCore::setCountdownEnabled(bool v) { if (m_countdownEnabled == v) return; m_countdownEnabled = v; emit countdownEnabledChanged(); }

int BlackHoleCore::countdownMinutes() const { return m_countdownMinutes; }
void BlackHoleCore::setCountdownMinutes(int v) { if (m_countdownMinutes == v) return; m_countdownMinutes = v; emit countdownMinutesChanged(); }

// ====== 高级设置 保存/加载 ======

void BlackHoleCore::saveAdvancedConfig()
{
    QString path = configPath("blackhole_advanced.txt");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "# Blackhole Advanced Settings v1\n";
    out << "followMouse="   << (m_followMouse ? 1 : 0) << "\n";
    out << "mouseInertia="  << QString::number(m_mouseInertia, 'f', 2) << "\n";
    out << "limitMouseOvershoot=" << (m_limitMouseOvershoot ? 1 : 0) << "\n";
    out << "videoAsIdle="   << (m_videoAsIdle ? 1 : 0) << "\n";
    out << "randomPath="    << (m_randomPath ? 1 : 0) << "\n";
    out << "animationSpeed=" << m_animationSpeed << "\n";
    out << "lightingEffect=" << (m_lightingEffect ? 1 : 0) << "\n";
    out << "distortion="    << QString::number(m_distortion, 'f', 2) << "\n";
    out << "allowRecordingCapture=" << (m_allowRecordingCapture ? 1 : 0) << "\n";
    out << "holeSize="      << QString::number(m_holeSize, 'f', 2) << "\n";
    out << "growEnabled="   << (m_growEnabled ? 1 : 0) << "\n";
    out << "initialSize="   << QString::number(m_initialSize, 'f', 2) << "\n";
    out << "holeRadius="  << (qFuzzyCompare(m_holeSize, 1.0f) ? QString("-1.0") : QString::number(0.08f * m_holeSize, 'f', 3)) << "\n";
    out << "diskGain="    << QString::number(m_overrideDiskGain, 'f', 3) << "\n";
    out << "diskTemp="    << QString::number(m_overrideDiskTemp, 'f', 1) << "\n";
    out << "exposure="    << QString::number(m_overrideExposure, 'f', 3) << "\n";
    out << "spd="         << (m_animationSpeed == 1 ? QString("-1.0") : QString::number(m_animationSpeed, 'f', 3)) << "\n";
    out << "starGain="    << QString::number(m_overrideStarGain, 'f', 3) << "\n";
    out << "diskIncl="    << QString::number(m_overrideDiskIncl, 'f', 3) << "\n";
    file.close();
}
void BlackHoleCore::loadAdvancedConfig()
{
    QFile file;
    if (!openConfigForRead(file, "blackhole_advanced.txt")) {
        qDebug() << "BlackHoleCore: advanced config not found, using defaults";
        return;
    }

    QTextStream in(&file);
    bool hasLightingEffect = false;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        int eq = line.indexOf('=');
        if (eq < 0) continue;
        QString key = line.left(eq).trimmed();
        QString val = line.mid(eq + 1).trimmed();
        if (key == "followMouse")    m_followMouse    = (val.toInt() != 0);
        else if (key == "mouseInertia") setMouseInertia(val.toFloat());
        else if (key == "limitMouseOvershoot") setLimitMouseOvershoot(val.toInt() != 0);
        else if (key == "videoAsIdle")   m_videoAsIdle    = (val.toInt() != 0);
        else if (key == "randomPath")     m_randomPath     = (val.toInt() != 0);
        else if (key == "animationSpeed") m_animationSpeed = val.toInt();
        else if (key == "lightingEffect") { m_lightingEffect = (val.toInt() != 0); hasLightingEffect = true; }
        else if (key == "screenSwallow" && !hasLightingEffect) m_lightingEffect = (val.toInt() != 0);
        else if (key == "swallowStrength") { /* 旧配置兼容：强度参数已废弃。 */ }
        else if (key == "distortion")     m_distortion     = val.toFloat();
        else if (key == "allowRecordingCapture") m_allowRecordingCapture = (val.toInt() != 0);
        else if (key == "holeSize")       m_holeSize       = val.toFloat();
        else if (key == "growEnabled")    m_growEnabled    = (val.toInt() != 0);
        else if (key == "initialSize")    m_initialSize    = val.toFloat();
        else if (key == "holeRadius") { float hv = val.toFloat(); m_holeSize = (hv <= 0.0f) ? 1.0f : (hv / 0.08f); }
        else if (key == "diskGain")   m_overrideDiskGain   = val.toFloat();
        else if (key == "diskTemp")   m_overrideDiskTemp   = val.toFloat();
        else if (key == "exposure")   m_overrideExposure   = val.toFloat();
        else if (key == "spd")        { float sv = val.toFloat(); m_animationSpeed = (sv <= 0.0f) ? 1 : (int)sv; }
        else if (key == "starGain")   m_overrideStarGain   = val.toFloat();
        else if (key == "diskIncl")   m_overrideDiskIncl   = val.toFloat();
    }
    file.close();
    emit mouseInertiaChanged();
    emit limitMouseOvershootChanged();
    emit lightingEffectChanged();
    emit allowRecordingCaptureChanged();
    qDebug() << "BlackHoleCore: loaded advanced config";
}

// ====== 空闲名单 保存/加载 ======

void BlackHoleCore::saveIdleListConfig()
{
    QString path = configPath("blackhole_idlelist.txt");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "# Blackhole Idle Lists v2\n";
    out << "[whitelist]\n";
    for (const QString &s : m_idleWhitelist) out << s << "\n";
    out << "[/whitelist]\n";
    out << "[mediaHints]\n";
    for (const QString &s : m_idleBlacklist) out << s << "\n";
    out << "[/mediaHints]\n";
    out << "[forceBlocklist]\n";
    for (const QString &s : m_idleForceBlocklist) out << s << "\n";
    out << "[/forceBlocklist]\n";
    file.close();
}

void BlackHoleCore::loadIdleListConfig()
{
    QFile file;
    if (!openConfigForRead(file, "blackhole_idlelist.txt")) {
        qDebug() << "BlackHoleCore: idle list config not found, using defaults";
        return;
    }

    QTextStream in(&file);
    m_idleWhitelist.clear();
    m_idleBlacklist.clear();
    m_idleForceBlocklist.clear();
    QStringList *currentList = nullptr;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line == "[whitelist]") { currentList = &m_idleWhitelist; continue; }
        if (line == "[/whitelist]") { currentList = nullptr; continue; }
        if (line == "[mediaHints]") { currentList = &m_idleBlacklist; continue; }
        if (line == "[/mediaHints]") { currentList = nullptr; continue; }
        if (line == "[blacklist]") { currentList = &m_idleBlacklist; continue; }
        if (line == "[/blacklist]") { currentList = nullptr; continue; }
        if (line == "[forceBlocklist]") { currentList = &m_idleForceBlocklist; continue; }
        if (line == "[/forceBlocklist]") { currentList = nullptr; continue; }
        if (currentList) currentList->append(line);
    }
    m_idleWhitelist = NormalizeIdleList(m_idleWhitelist);
    m_idleBlacklist = NormalizeIdleList(m_idleBlacklist);
    m_idleForceBlocklist = NormalizeIdleList(m_idleForceBlocklist);
    file.close();
    qDebug() << "BlackHoleCore: loaded idle lists";
}

// ====== 定时显示 保存/加载 ======

void BlackHoleCore::saveScheduleConfig()
{
    QString path = configPath("blackhole_schedule.txt");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "# Blackhole Schedule v1\n";
    out << "scheduleEnabled="  << (m_scheduleEnabled ? 1 : 0) << "\n";
    out << "startHour="        << m_startHour << "\n";
    out << "startMinute="      << m_startMinute << "\n";
    out << "endHour="          << m_endHour << "\n";
    out << "endMinute="        << m_endMinute << "\n";
    out << "countdownEnabled=" << (m_countdownEnabled ? 1 : 0) << "\n";
    out << "countdownMinutes=" << m_countdownMinutes << "\n";
    file.close();
}

void BlackHoleCore::loadScheduleConfig()
{
    QFile file;
    if (!openConfigForRead(file, "blackhole_schedule.txt")) {
        qDebug() << "BlackHoleCore: schedule config not found, using defaults";
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        int eq = line.indexOf('=');
        if (eq < 0) continue;
        QString key = line.left(eq).trimmed();
        QString val = line.mid(eq + 1).trimmed();
        if (key == "scheduleEnabled")  m_scheduleEnabled  = (val.toInt() != 0);
        else if (key == "startHour")        m_startHour        = val.toInt();
        else if (key == "startMinute")      m_startMinute      = val.toInt();
        else if (key == "endHour")          m_endHour          = val.toInt();
        else if (key == "endMinute")        m_endMinute        = val.toInt();
        else if (key == "countdownEnabled") m_countdownEnabled = (val.toInt() != 0);
        else if (key == "countdownMinutes") m_countdownMinutes = val.toInt();
    }
    file.close();
    qDebug() << "BlackHoleCore: loaded schedule config";
}

// ====== 系统设置 保存/加载 ======

void BlackHoleCore::saveSystemConfig()
{
    QString path = configPath("blackhole_system.txt");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "BlackHoleCore: cannot write system config:" << path;
        return;
    }

    QTextStream out(&file);
    out << "isDark=" << (m_isDark ? 1 : 0) << "\n";
    out << "followSystem=" << (m_followSystem ? 1 : 0) << "\n";
    out << "focusColor=" << m_focusColor.name(QColor::HexArgb) << "\n";
    out << "launchMinimized=" << (m_launchMinimized ? 1 : 0) << "\n";
    out << "skipExitDialog=" << (m_skipExitDialog ? 1 : 0) << "\n";
    out << "defaultCloseAction=" << m_defaultCloseAction << "\n";
    out << "closeHotkeyEnabled=" << (m_closeHotkeyEnabled ? 1 : 0) << "\n";
    out << "closeHotkeySequence=" << m_closeHotkeySequence << "\n";
    out << "customAvatarPath=" << QDir::toNativeSeparators(m_customAvatarPath) << "\n";
    file.close();
    qDebug() << "BlackHoleCore: saved system config";
}

void BlackHoleCore::loadSystemConfig()
{
    QFile file;
    if (!openConfigForRead(file, "blackhole_system.txt")) {
        qDebug() << "BlackHoleCore: system config not found, using defaults";
        updateCloseHotkeyRegistration();
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        int eq = line.indexOf('=');
        if (eq < 0) continue;
        QString key = line.left(eq).trimmed();
        QString val = line.mid(eq + 1).trimmed();
        if (key == "isDark")            m_isDark            = (val.toInt() != 0);
        else if (key == "followSystem")      m_followSystem       = (val.toInt() != 0);
        else if (key == "focusColor")        m_focusColor         = QColor(val);
        else if (key == "launchMinimized") m_launchMinimized = (val.toInt() != 0);
        else if (key == "skipExitDialog")    m_skipExitDialog     = (val.toInt() != 0);
        else if (key == "defaultCloseAction") m_defaultCloseAction = val.toInt();
        else if (key == "closeHotkeyEnabled") m_closeHotkeyEnabled = (val.toInt() != 0);
        else if (key == "closeHotkeySequence") m_closeHotkeySequence = normalizedHotkeySequence(val);
        else if (key == "customAvatarPath") m_customAvatarPath = QDir::fromNativeSeparators(val);
    }
    file.close();
    if (!AvatarStorage_FileUrl(m_customAvatarPath).isEmpty()) {
        m_avatarStatus.clear();
    } else if (!m_customAvatarPath.isEmpty()) {
        m_customAvatarPath.clear();
        m_avatarStatus = tr("自定义头像文件不存在，已恢复默认头像");
    }
    updateCloseHotkeyRegistration();
    qDebug() << "BlackHoleCore: loaded system config";
}

// ====== 多预设列表持久化 ======

void BlackHoleCore::saveAllLists()
{
    // 先将当前列表的修改同步回 allLists
    if (m_currentListIndex >= 0 && m_currentListIndex < m_allLists.size()) {
        m_allLists[m_currentListIndex] = m_presetModel->presets();
    }

    QString path = configPath("blackhole_lists.txt");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "BlackHoleCore: cannot write lists config:" << path;
        return;
    }

    QTextStream out(&file);
    out << "# Blackhole Preset Lists v1\n";
    out << "listCount " << m_allLists.size() << "\n";
    out << "currentList " << m_currentListIndex << "\n\n";

    for (int i = 0; i < m_allLists.size(); i++) {
        const QVector<PresetData> &list = m_allLists[i];
        out << "[list]\n";
        out << "listName " << m_listNames[i] << "\n";
        out << "presetCount " << list.size() << "\n";
        for (const PresetData &p : list) {
            out << p.name << "\n";
            out << QString::number(p.diskTemp,  'f', 0) << " "
                << QString::number(p.diskIncl,  'f', 2) << " "
                << QString::number(p.diskRoll,  'f', 2) << " "
                << QString::number(p.diskInner, 'f', 1) << " "
                << QString::number(p.diskOuter, 'f', 1) << " "
                << QString::number(p.diskOpac,  'f', 2) << " "
                << QString::number(p.diskDopp,  'f', 2) << " "
                << QString::number(p.diskBeam,  'f', 1) << " "
                << QString::number(p.diskGain,  'f', 2) << " "
                << QString::number(p.diskContr, 'f', 2) << " "
                << QString::number(p.diskWind,  'f', 1) << " "
                << QString::number(p.diskSpeed, 'f', 1) << " "
                << QString::number(p.diskExpo,  'f', 2) << " "
                << QString::number(p.diskStar,  'f', 3) << "\n";
        }
        out << "[/list]\n";
        if (i < m_allLists.size() - 1) out << "\n";
    }
    file.close();
    qDebug() << "BlackHoleCore: saved" << m_allLists.size() << "preset lists";
}

void BlackHoleCore::loadAllLists()
{
    QFile file;
    if (!openConfigForRead(file, "blackhole_lists.txt")) {
        // 文件不存在 (首次运行或旧版本): 用当前模型数据创建默认列表
        if (m_allLists.isEmpty()) {
            QVector<PresetData> current = m_presetModel->presets();
            if (!current.isEmpty()) {
                m_allLists.append(current);
                m_listNames.append(QString::fromUtf8("\xe9\xbb\x98\xe8\xae\xa4"));
            }
        }
        if (m_listNames.isEmpty()) {
            m_listNames.append(QString::fromUtf8("\xe9\xbb\x98\xe8\xae\xa4"));
        }
        m_currentListIndex = 0;
        m_presetListModel->setNames(m_listNames);
        return;
    }

    QTextStream in(&file);
    m_allLists.clear();
    m_listNames.clear();

    int savedCurrentList = 0;
    bool inList = false;
    QVector<PresetData> currentPresets;
    QString currentListName;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        if (line.startsWith("listCount ")) {
            // 跳过，仅用于验证
        } else if (line.startsWith("currentList ")) {
            savedCurrentList = line.mid(12).toInt();
        } else if (line == "[list]") {
            inList = true;
            currentPresets.clear();
            currentListName.clear();
        } else if (line == "[/list]") {
            if (inList && !currentListName.isEmpty() && !currentPresets.isEmpty()) {
                m_allLists.append(currentPresets);
                m_listNames.append(currentListName);
            }
            inList = false;
        } else if (inList && line.startsWith("listName ")) {
            currentListName = line.mid(9);
        } else if (inList && line.startsWith("presetCount ")) {
            // 跳过，按实际读取行数确定数量
        } else if (inList) {
            // 预设名行 + 参数行 成对出现
            QString presetName = line;
            QString valsLine = in.readLine();
            if (valsLine.isNull()) break;
            valsLine = valsLine.trimmed();
            if (valsLine.isEmpty()) continue;

            PresetData p;
            p.name = presetName;
            QTextStream vs(&valsLine);
            vs >> p.diskTemp >> p.diskIncl >> p.diskRoll >> p.diskInner >> p.diskOuter
               >> p.diskOpac >> p.diskDopp >> p.diskBeam >> p.diskGain >> p.diskContr
               >> p.diskWind >> p.diskSpeed >> p.diskExpo >> p.diskStar;
            currentPresets.append(p);
        }
    }
    file.close();

    // 兜底: 如果解析为空, 使用当前模型数据
    if (m_allLists.isEmpty()) {
        QVector<PresetData> fallback = m_presetModel->presets();
        if (!fallback.isEmpty()) {
            m_allLists.append(fallback);
            m_listNames.append(QString::fromUtf8("\xe9\xbb\x98\xe8\xae\xa4"));
        }
    }
    if (m_listNames.isEmpty()) {
        m_listNames.append(QString::fromUtf8("\xe9\xbb\x98\xe8\xae\xa4"));
    }

    m_currentListIndex = (savedCurrentList >= 0 && savedCurrentList < m_allLists.size())
                         ? savedCurrentList : 0;
    m_presetListModel->setNames(m_listNames);
    qDebug() << "BlackHoleCore: loaded" << m_allLists.size() << "preset lists";
}

// ====== 系统设置 getter/setter ======

bool BlackHoleCore::isDark() const { return m_isDark; }
void BlackHoleCore::setIsDark(bool v) { if (m_isDark == v) return; m_isDark = v; emit isDarkChanged(); }

bool BlackHoleCore::followSystem() const { return m_followSystem; }
void BlackHoleCore::setFollowSystem(bool v) { if (m_followSystem == v) return; m_followSystem = v; emit followSystemChanged(); }

QColor BlackHoleCore::focusColor() const { return m_focusColor; }
void BlackHoleCore::setFocusColor(const QColor &c) { if (m_focusColor == c) return; m_focusColor = c; emit focusColorChanged(); }

bool BlackHoleCore::skipExitDialog() const { return m_skipExitDialog; }
void BlackHoleCore::setSkipExitDialog(bool v) { if (m_skipExitDialog == v) return; m_skipExitDialog = v; emit skipExitDialogChanged(); }

int BlackHoleCore::defaultCloseAction() const { return m_defaultCloseAction; }
void BlackHoleCore::setDefaultCloseAction(int v) { if (m_defaultCloseAction == v) return; m_defaultCloseAction = v; emit defaultCloseActionChanged(); }

bool BlackHoleCore::closeHotkeyEnabled() const { return m_closeHotkeyEnabled; }
void BlackHoleCore::setCloseHotkeyEnabled(bool v)
{
    if (m_closeHotkeyEnabled == v) return;
    m_closeHotkeyEnabled = v;
    emit closeHotkeyEnabledChanged();
    updateCloseHotkeyRegistration();
}

QString BlackHoleCore::closeHotkeySequence() const { return m_closeHotkeySequence; }
void BlackHoleCore::setCloseHotkeySequence(const QString &v)
{
    QString normalized = normalizedHotkeySequence(v);
    if (normalized.isEmpty()) normalized = QStringLiteral("Ctrl+Alt+B");
    if (m_closeHotkeySequence == normalized) return;
    m_closeHotkeySequence = normalized;
    emit closeHotkeySequenceChanged();
    updateCloseHotkeyRegistration();
}

QString BlackHoleCore::closeHotkeyStatus() const { return m_closeHotkeyStatus; }

QString BlackHoleCore::customAvatarUrl() const
{
    const QString customUrl = AvatarStorage_FileUrl(m_customAvatarPath);
    return customUrl.isEmpty()
        ? QStringLiteral("qrc:/new/prefix1/fonts/pic/avatar.png")
        : customUrl;
}

QString BlackHoleCore::avatarStatus() const
{
    return m_avatarStatus;
}

void BlackHoleCore::chooseCustomAvatar()
{
    const QString sourcePath = QFileDialog::getOpenFileName(
        nullptr, tr("选择头像"), QString(),
        tr("图片文件 (*.png *.jpg *.jpeg *.webp)"));
    if (sourcePath.isEmpty()) return;

    const QString avatarDirectory = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/avatar");
    QString savedPath;
    QString error;
    if (!AvatarStorage_Save(sourcePath, avatarDirectory, &savedPath, &error)) {
        m_avatarStatus = error;
        emit avatarStatusChanged();
        return;
    }

    m_customAvatarPath = savedPath;
    m_avatarStatus = tr("头像已更新");
    saveSystemConfig();
    emit customAvatarUrlChanged();
    emit avatarStatusChanged();
}

bool BlackHoleCore::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    MSG *msg = static_cast<MSG*>(message);
    if (msg && msg->message == WM_HOTKEY && msg->wParam == kCloseHotkeyId) {
        stopAll();
        return true;
    }
#else
    Q_UNUSED(message);
#endif
    return false;
}

void BlackHoleCore::updateCloseHotkeyRegistration()
{
#ifdef Q_OS_WIN
    unregisterCloseHotkey();
    if (!m_closeHotkeyEnabled) {
        m_closeHotkeyStatus = QStringLiteral("已禁用");
        emit closeHotkeyStatusChanged();
        return;
    }

    quint32 modifiers = 0;
    quint32 key = 0;
    if (!parseHotkeySequence(m_closeHotkeySequence, &modifiers, &key)) {
        m_closeHotkeyStatus = QStringLiteral("快捷键无效");
        emit closeHotkeyStatusChanged();
        return;
    }

    if (RegisterHotKey(nullptr, kCloseHotkeyId, modifiers | MOD_NOREPEAT, key)) {
        m_closeHotkeyRegistered = true;
        m_closeHotkeyStatus = QStringLiteral("已绑定 ") + m_closeHotkeySequence;
    } else {
        m_closeHotkeyStatus = QStringLiteral("注册失败，可能被其他程序占用");
    }
    emit closeHotkeyStatusChanged();
#else
    m_closeHotkeyStatus = QStringLiteral("当前系统不支持全局快捷键");
    emit closeHotkeyStatusChanged();
#endif
}

void BlackHoleCore::unregisterCloseHotkey()
{
#ifdef Q_OS_WIN
    if (m_closeHotkeyRegistered) {
        UnregisterHotKey(nullptr, kCloseHotkeyId);
        m_closeHotkeyRegistered = false;
    }
#endif
}

bool BlackHoleCore::parseHotkeySequence(const QString &sequence, quint32 *modifiers, quint32 *key) const
{
    if (!modifiers || !key) return false;
    *modifiers = 0;
    *key = 0;

    const QStringList parts = sequence.split('+', Qt::SkipEmptyParts);
    for (QString part : parts) {
        part = part.trimmed();
        const QString lower = part.toLower();
        if (lower == "ctrl" || lower == "control") {
            *modifiers |= MOD_CONTROL;
        } else if (lower == "alt") {
            *modifiers |= MOD_ALT;
        } else if (lower == "shift") {
            *modifiers |= MOD_SHIFT;
        } else if (lower == "win" || lower == "meta") {
            *modifiers |= MOD_WIN;
        } else if (part.length() == 1 && part.at(0).isLetterOrNumber()) {
            const ushort ch = part.at(0).toUpper().unicode();
            *key = ch;
        } else if (lower.startsWith('f')) {
            bool ok = false;
            int f = lower.mid(1).toInt(&ok);
            if (ok && f >= 1 && f <= 24) *key = VK_F1 + static_cast<quint32>(f - 1);
        } else if (lower == "escape" || lower == "esc") {
            *key = VK_ESCAPE;
        } else if (lower == "space") {
            *key = VK_SPACE;
        } else if (lower == "delete" || lower == "del") {
            *key = VK_DELETE;
        }
    }

    return *key != 0 && *modifiers != 0;
}

QString BlackHoleCore::normalizedHotkeySequence(const QString &sequence) const
{
    QStringList out;
    QString keyPart;
    const QStringList parts = sequence.split('+', Qt::SkipEmptyParts);
    for (QString part : parts) {
        part = part.trimmed();
        const QString lower = part.toLower();
        if ((lower == "ctrl" || lower == "control") && !out.contains("Ctrl")) out << "Ctrl";
        else if (lower == "alt" && !out.contains("Alt")) out << "Alt";
        else if (lower == "shift" && !out.contains("Shift")) out << "Shift";
        else if ((lower == "win" || lower == "meta") && !out.contains("Win")) out << "Win";
        else if (part.length() == 1 && part.at(0).isLetterOrNumber()) keyPart = part.toUpper();
        else if (lower.startsWith('f')) {
            bool ok = false;
            int f = lower.mid(1).toInt(&ok);
            if (ok && f >= 1 && f <= 24) keyPart = QStringLiteral("F%1").arg(f);
        } else if (lower == "escape" || lower == "esc") keyPart = "Esc";
        else if (lower == "space") keyPart = "Space";
        else if (lower == "delete" || lower == "del") keyPart = "Delete";
    }
    if (keyPart.isEmpty()) return QString();
    out << keyPart;
    return out.join('+');
}

// ====== 渲染器覆盖参数 getter/setter ======

float BlackHoleCore::overrideHoleRadius() const { return m_overrideHoleRadius; }
void BlackHoleCore::setOverrideHoleRadius(float v) { if (qFuzzyCompare(m_overrideHoleRadius, v)) return; m_overrideHoleRadius = v; emit overrideHoleRadiusChanged(); }

float BlackHoleCore::overrideDiskGain() const { return m_overrideDiskGain; }
void BlackHoleCore::setOverrideDiskGain(float v) { if (qFuzzyCompare(m_overrideDiskGain, v)) return; m_overrideDiskGain = v; emit overrideDiskGainChanged(); }

float BlackHoleCore::overrideDiskTemp() const { return m_overrideDiskTemp; }
void BlackHoleCore::setOverrideDiskTemp(float v) { if (qFuzzyCompare(m_overrideDiskTemp, v)) return; m_overrideDiskTemp = v; emit overrideDiskTempChanged(); }

float BlackHoleCore::overrideExposure() const { return m_overrideExposure; }
void BlackHoleCore::setOverrideExposure(float v) { if (qFuzzyCompare(m_overrideExposure, v)) return; m_overrideExposure = v; emit overrideExposureChanged(); }

float BlackHoleCore::overrideSpd() const { return m_overrideSpd; }
void BlackHoleCore::setOverrideSpd(float v) { if (qFuzzyCompare(m_overrideSpd, v)) return; m_overrideSpd = v; emit overrideSpdChanged(); }

float BlackHoleCore::overrideStarGain() const { return m_overrideStarGain; }
void BlackHoleCore::setOverrideStarGain(float v) { if (qFuzzyCompare(m_overrideStarGain, v)) return; m_overrideStarGain = v; emit overrideStarGainChanged(); }

float BlackHoleCore::overrideDiskIncl() const { return m_overrideDiskIncl; }
void BlackHoleCore::setOverrideDiskIncl(float v) { if (qFuzzyCompare(m_overrideDiskIncl, v)) return; m_overrideDiskIncl = v; emit overrideDiskInclChanged(); }
