// blackholecore.h — 黑洞配置管理 + 进程控制
#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QAbstractListModel>
#include <QVector>
#include <QString>
#include <QColor>

// 单个预设数据
struct PresetData {
    QString name;
    float diskTemp  = 5500.0f;
    float diskIncl  = 1.50f;
    float diskRoll  = 0.35f;
    float diskInner = 1.8f;
    float diskOuter = 8.0f;
    float diskOpac  = 0.90f;
    float diskDopp  = 0.60f;
    float diskBeam  = 2.5f;
    float diskGain  = 2.2f;
    float diskContr = 1.6f;
    float diskWind  = 7.0f;
    float diskSpeed = 5.0f;
    float diskExpo  = 1.40f;
    float diskStar  = 0.0f;
};

// 预设列表模型 — 暴露给 QML ListView / Repeater
class PresetModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        DiskTempRole, DiskInclRole, DiskRollRole,
        DiskInnerRole, DiskOuterRole, DiskOpacRole,
        DiskDoppRole, DiskBeamRole, DiskGainRole,
        DiskContrRole, DiskWindRole, DiskSpeedRole,
        DiskExpoRole, DiskStarRole
    };

    explicit PresetModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    void setPresets(const QVector<PresetData> &presets);
    QVector<PresetData> presets() const;
    void updateParam(int index, const QString &param, float value);
    void movePreset(int from, int to);

private:
    QVector<PresetData> m_presets;
};

// 预设列表名模型 — 供下拉框
class PresetListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { NameRole = Qt::UserRole + 1 };
    explicit PresetListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return parent.isValid() ? 0 : m_names.size(); }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_names.size()) return QVariant();
        return m_names.at(index.row());
    }
    QHash<int, QByteArray> roleNames() const override { return {{NameRole, "listName"}}; }
    void setNames(const QStringList &names) { beginResetModel(); m_names = names; endResetModel(); }
    QStringList names() const { return m_names; }
private:
    QStringList m_names;
};

// 黑洞核心 — 配置管理 + 进程控制
class BlackHoleCore : public QObject {
    Q_OBJECT

    // 全局设置
    Q_PROPERTY(int displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(int idleSeconds READ idleSeconds WRITE setIdleSeconds NOTIFY idleSecondsChanged)
    Q_PROPERTY(int playMode READ playMode WRITE setPlayMode NOTIFY playModeChanged)
    Q_PROPERTY(float slotSeconds READ slotSeconds WRITE setSlotSeconds NOTIFY slotSecondsChanged)
    Q_PROPERTY(bool videoAsIdle READ videoAsIdle WRITE setVideoAsIdle NOTIFY videoAsIdleChanged)
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
    Q_PROPERTY(bool launchMinimized READ launchMinimized WRITE setLaunchMinimized NOTIFY launchMinimizedChanged)
    // 多显示器
    Q_PROPERTY(int screenTarget READ screenTarget WRITE setScreenTarget NOTIFY screenTargetChanged)
    // 捕获方式 (-1=自动检测, 0=WGC, 1=DXGI) — 与 ImGui UI / main.cpp 共用 presets.txt 第 2 行字段
    Q_PROPERTY(int captureMode READ captureMode WRITE setCaptureMode NOTIFY captureModeChanged)
    // 固定大小 (黑洞不再随时间增长，保持固定比例)
    Q_PROPERTY(bool fixedSize READ fixedSize WRITE setFixedSize NOTIFY fixedSizeChanged)
    Q_PROPERTY(float fixedLevel READ fixedLevel WRITE setFixedLevel NOTIFY fixedLevelChanged)

    // 进程状态
    Q_PROPERTY(bool rendererRunning READ rendererRunning NOTIFY rendererRunningChanged)
    Q_PROPERTY(bool systemActive READ isSystemActive NOTIFY systemActiveChanged)

    // 当前预设
    Q_PROPERTY(int currentPresetIndex READ currentPresetIndex WRITE setCurrentPresetIndex NOTIFY currentPresetIndexChanged)

    // 预设模型
    Q_PROPERTY(QAbstractItemModel* presetModel READ presetModel CONSTANT)

    // 预设列表集合
    Q_PROPERTY(QAbstractItemModel* presetListModel READ presetListModel CONSTANT)
    Q_PROPERTY(QString currentListName READ currentListName NOTIFY currentListChanged)

    // 当前预设的 14 个参数 (直接属性, 供 FBO 绑定)
    Q_PROPERTY(float diskTemp  READ diskTemp  NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskIncl  READ diskIncl  NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskRoll  READ diskRoll  NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskInner READ diskInner NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskOuter READ diskOuter NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskOpac  READ diskOpac  NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskDopp  READ diskDopp  NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskBeam  READ diskBeam  NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskGain  READ diskGain  NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskContr READ diskContr NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskWind  READ diskWind  NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskSpeed READ diskSpeed NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskExpo  READ diskExpo  NOTIFY currentPresetChanged)
    Q_PROPERTY(float diskStar  READ diskStar  NOTIFY currentPresetChanged)

    // 高级设置
    Q_PROPERTY(bool followMouse READ followMouse WRITE setFollowMouse NOTIFY followMouseChanged)
    Q_PROPERTY(bool randomPath READ randomPath WRITE setRandomPath NOTIFY randomPathChanged)
    Q_PROPERTY(int animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY animationSpeedChanged)
    Q_PROPERTY(bool screenSwallow READ screenSwallow WRITE setScreenSwallow NOTIFY screenSwallowChanged)
    Q_PROPERTY(float distortion READ distortion WRITE setDistortion NOTIFY distortionChanged)
    Q_PROPERTY(float holeSize READ holeSize WRITE setHoleSize NOTIFY holeSizeChanged)
    Q_PROPERTY(bool growEnabled READ growEnabled WRITE setGrowEnabled NOTIFY growEnabledChanged)
    Q_PROPERTY(float initialSize READ initialSize WRITE setInitialSize NOTIFY initialSizeChanged)

    // 空闲名单
    Q_PROPERTY(QStringList idleWhitelist READ idleWhitelist WRITE setIdleWhitelist NOTIFY idleWhitelistChanged)
    Q_PROPERTY(QStringList idleBlacklist READ idleBlacklist WRITE setIdleBlacklist NOTIFY idleBlacklistChanged)

    // 定时显示
    Q_PROPERTY(bool scheduleEnabled READ scheduleEnabled WRITE setScheduleEnabled NOTIFY scheduleEnabledChanged)
    Q_PROPERTY(int startHour READ startHour WRITE setStartHour NOTIFY startHourChanged)
    Q_PROPERTY(int startMinute READ startMinute WRITE setStartMinute NOTIFY startMinuteChanged)
    Q_PROPERTY(int endHour READ endHour WRITE setEndHour NOTIFY endHourChanged)
    Q_PROPERTY(int endMinute READ endMinute WRITE setEndMinute NOTIFY endMinuteChanged)
    Q_PROPERTY(bool countdownEnabled READ countdownEnabled WRITE setCountdownEnabled NOTIFY countdownEnabledChanged)
    Q_PROPERTY(int countdownMinutes READ countdownMinutes WRITE setCountdownMinutes NOTIFY countdownMinutesChanged)

    // 系统设置
    Q_PROPERTY(bool isDark READ isDark WRITE setIsDark NOTIFY isDarkChanged)
    Q_PROPERTY(bool followSystem READ followSystem WRITE setFollowSystem NOTIFY followSystemChanged)
    Q_PROPERTY(QColor focusColor READ focusColor WRITE setFocusColor NOTIFY focusColorChanged)
    Q_PROPERTY(bool skipExitDialog READ skipExitDialog WRITE setSkipExitDialog NOTIFY skipExitDialogChanged)
    Q_PROPERTY(int defaultCloseAction READ defaultCloseAction WRITE setDefaultCloseAction NOTIFY defaultCloseActionChanged)


    // 渲染器覆盖参数 (默认 -1.0 = 不覆盖，使用预设值)
    Q_PROPERTY(float overrideHoleRadius READ overrideHoleRadius WRITE setOverrideHoleRadius NOTIFY overrideHoleRadiusChanged)
    Q_PROPERTY(float overrideDiskGain   READ overrideDiskGain   WRITE setOverrideDiskGain   NOTIFY overrideDiskGainChanged)
    Q_PROPERTY(float overrideDiskTemp   READ overrideDiskTemp   WRITE setOverrideDiskTemp   NOTIFY overrideDiskTempChanged)
    Q_PROPERTY(float overrideExposure   READ overrideExposure   WRITE setOverrideExposure   NOTIFY overrideExposureChanged)
    Q_PROPERTY(float overrideSpd        READ overrideSpd        WRITE setOverrideSpd        NOTIFY overrideSpdChanged)
    Q_PROPERTY(float overrideStarGain   READ overrideStarGain   WRITE setOverrideStarGain   NOTIFY overrideStarGainChanged)
    Q_PROPERTY(float overrideDiskIncl   READ overrideDiskIncl   WRITE setOverrideDiskIncl   NOTIFY overrideDiskInclChanged)


public:
    explicit BlackHoleCore(QObject *parent = nullptr);
    ~BlackHoleCore() override;

    // 全局设置
    int displayMode() const;
    void setDisplayMode(int mode);
    int idleSeconds() const;
    void setIdleSeconds(int sec);
    int playMode() const;
    void setPlayMode(int mode);
    float slotSeconds() const;
    void setSlotSeconds(float sec);
    bool videoAsIdle() const;
    void setVideoAsIdle(bool v);
    bool autoStart() const;
    void setAutoStart(bool v);
    bool launchMinimized() const;
    void setLaunchMinimized(bool v);
    int screenTarget() const;
    void setScreenTarget(int v);
    int captureMode() const;
    void setCaptureMode(int v);
    bool fixedSize() const;
    void setFixedSize(bool v);
    float fixedLevel() const;
    void setFixedLevel(float v);

    // 进程状态
    bool rendererRunning() const;
    bool isSystemActive() const;

    // 当前预设索引
    int currentPresetIndex() const;
    void setCurrentPresetIndex(int index);

    // 预设模型
    QAbstractItemModel* presetModel();
    QAbstractItemModel* presetListModel();
    QString currentListName() const;

    // 当前预设参数 (只读, 由内部更新)
    float diskTemp()  const;
    float diskIncl()  const;
    float diskRoll()  const;
    float diskInner() const;
    float diskOuter() const;
    float diskOpac()  const;
    float diskDopp()  const;
    float diskBeam()  const;
    float diskGain()  const;
    float diskContr() const;
    float diskWind()  const;
    float diskSpeed() const;
    float diskExpo()  const;
    float diskStar()  const;

    // 高级设置
    bool followMouse() const;
    void setFollowMouse(bool v);
    bool randomPath() const;
    void setRandomPath(bool v);
    int animationSpeed() const;
    void setAnimationSpeed(int v);
    bool screenSwallow() const;
    void setScreenSwallow(bool v);
    float distortion() const;
    void setDistortion(float v);
    float holeSize() const;
    void setHoleSize(float v);
    bool growEnabled() const;
    void setGrowEnabled(bool v);
    float initialSize() const;
    void setInitialSize(float v);

    // 空闲名单
    QStringList idleWhitelist() const;
    void setIdleWhitelist(const QStringList &list);
    QStringList idleBlacklist() const;
    void setIdleBlacklist(const QStringList &list);

    // 定时显示
    bool scheduleEnabled() const;
    void setScheduleEnabled(bool v);
    int startHour() const; void setStartHour(int v);
    int startMinute() const; void setStartMinute(int v);
    int endHour() const; void setEndHour(int v);
    int endMinute() const; void setEndMinute(int v);
    bool countdownEnabled() const;
    void setCountdownEnabled(bool v);
    int countdownMinutes() const;
    void setCountdownMinutes(int v);

    // 系统设置
    bool isDark() const;
    void setIsDark(bool v);
    bool followSystem() const;
    void setFollowSystem(bool v);
    QColor focusColor() const;
    void setFocusColor(const QColor &c);
    bool skipExitDialog() const;
    void setSkipExitDialog(bool v);
    int defaultCloseAction() const;
    void setDefaultCloseAction(int v);

    // 渲染器覆盖参数
    float overrideHoleRadius() const;
    void setOverrideHoleRadius(float v);
    float overrideDiskGain() const;
    void setOverrideDiskGain(float v);
    float overrideDiskTemp() const;
    void setOverrideDiskTemp(float v);
    float overrideExposure() const;
    void setOverrideExposure(float v);
    float overrideSpd() const;
    void setOverrideSpd(float v);
    float overrideStarGain() const;
    void setOverrideStarGain(float v);
    float overrideDiskIncl() const;
    void setOverrideDiskIncl(float v);
public slots:
    void loadConfig();
    void saveConfig();
    void resetDefaults();
    void startRenderer();
    void stopRenderer();
    void applyAndStart();
    void stopAll();
    void selectPreset(int index);
    void updatePresetParam(int index, const QString &param, float value);
    void updateCurrentPresetParam(const QString &param, float value);
    void movePreset(int from, int to);
    void copyPreset(int index);
    void pastePreset();
    void removePreset(int index);
    void createPresetList(const QString &name);
    void renameCurrentList(const QString &name);
    void switchToList(int index);
    void deleteCurrentList();
    void createPreset();
    void renameCurrentPreset(const QString &name);

signals:
    void displayModeChanged();
    void idleSecondsChanged();
    void playModeChanged();
    void slotSecondsChanged();
    void videoAsIdleChanged();
    void autoStartChanged();
    void launchMinimizedChanged();

    void screenTargetChanged();
    void captureModeChanged();
    void fixedSizeChanged();
    void fixedLevelChanged();
    void rendererRunningChanged();
    void systemActiveChanged();
    void currentPresetIndexChanged();
    void currentPresetChanged();  // 当前预设参数变化
    void configLoaded();
    void configSaved();
    void rendererStarted();
    void rendererStopped();
    void rendererError(const QString &msg);
    void currentListChanged();

    // 高级设置信号
    void followMouseChanged();
    void randomPathChanged();
    void animationSpeedChanged();
    void screenSwallowChanged();
    void distortionChanged();
    void holeSizeChanged();
    void growEnabledChanged();
    void initialSizeChanged();

    // 空闲名单信号
    void idleWhitelistChanged();
    void idleBlacklistChanged();

    // 定时显示信号
    void scheduleEnabledChanged();
    void startHourChanged(); void startMinuteChanged();
    void endHourChanged(); void endMinuteChanged();
    void countdownEnabledChanged();
    void countdownMinutesChanged();

    // 系统设置信号
    void isDarkChanged();
    void followSystemChanged();
    void focusColorChanged();
    void skipExitDialogChanged();
    void defaultCloseActionChanged();

    // 渲染器覆盖信号
    void overrideHoleRadiusChanged();
    void overrideDiskGainChanged();
    void overrideDiskTempChanged();
    void overrideExposureChanged();
    void overrideSpdChanged();
    void overrideStarGainChanged();
    void overrideDiskInclChanged();

private:
    void initDefaultPresets();
    QString configFilePath() const;
    QString configDir() const;
    // 查找 blackhole.exe (与 main.cpp --render 子进程对应). 若 projectRootOut 非空, 输出
    // 该 exe 的"项目根目录" (exe 在 <root>/build/ 下时取其父目录, 否则取 exe 同级目录).
    // Qt UI 与 blackhole.exe --render 必须读写同一份 blackhole_presets.txt + shaders/, 这要求
    // 两者工作目录一致 — Qt UI 用此函数定位项目根, 让 configFilePath() 与 startRenderer() 都对齐.
    QString findRendererExe(QString *projectRootOut = nullptr) const;
    void refreshCurrentPresetProps();
    void saveAdvancedConfig();
    void loadAdvancedConfig();
    void saveIdleListConfig();
    void loadIdleListConfig();
    void saveScheduleConfig();
    void loadScheduleConfig();
    void saveSystemConfig();
    void loadSystemConfig();
    void saveAllLists();
    void loadAllLists();

    PresetModel *m_presetModel;

    // 全局设置
    int     m_displayMode  = 0;  // 0=始终显示 1=空闲检测 (对齐 blackhole.exe)
    int     m_idleSeconds  = 300;
    int     m_playMode     = 1;     // 0=顺序 1=循环 2=随机
    float   m_slotSeconds  = 5.25f;
    bool    m_videoAsIdle  = false;
    bool    m_autoStart    = false;
    bool    m_launchMinimized  = false;

    int     m_screenTarget = 0;  // 0=主屏, 1=副屏, 2=跨屏, 3=一屏一黑洞
    int     m_captureMode  = -1; // -1=自动检测, 0=WGC, 1=DXGI (对齐 main.cpp)
    bool    m_fixedSize    = false;  // 固定大小（不随时间增长）
    float   m_fixedLevel   = 1.0f;   // 固定大小级别 (0.01~1.0 = 1%~100%)

    // 当前预设
    int     m_currentPresetIndex = 0;
    PresetData m_copiedPreset;
    bool      m_hasCopiedPreset = false;

    // 多预设列表
    PresetListModel *m_presetListModel;
    QVector<QVector<PresetData>> m_allLists;
    QStringList m_listNames;
    int m_currentListIndex = 0;

    // 进程
    bool m_refreshingProps = false;  // 防止currentPresetChanged信号级联
    QProcess *m_rendererProcess = nullptr;

    // 空闲检测
    QTimer *m_idleTimer = nullptr;

    // 高级设置
    bool    m_followMouse    = false;
    bool    m_randomPath     = true;
    int     m_animationSpeed = 1;
    bool    m_screenSwallow  = false;
    float   m_distortion     = 1.0f;
    float   m_holeSize       = 1.0f;
    bool    m_growEnabled    = false;
    float   m_initialSize    = 0.3f;

    // 空闲名单
    QStringList m_idleWhitelist;
    QStringList m_idleBlacklist;

    // 定时显示
    bool m_scheduleEnabled   = false;
    int  m_startHour         = 8;
    int  m_startMinute       = 0;
    int  m_endHour           = 22;
    int  m_endMinute         = 0;
    bool m_countdownEnabled  = false;
    int  m_countdownMinutes  = 30;

    // 系统设置
    bool    m_isDark             = true;
    bool    m_followSystem       = true;
    QColor  m_focusColor         = QColor("#00C4B3");
    bool    m_skipExitDialog     = false;
    int     m_defaultCloseAction = 0;

    // 渲染器覆盖参数
    float   m_overrideHoleRadius = -1.0f;
    float   m_overrideDiskGain   = -1.0f;
    float   m_overrideDiskTemp   = -1.0f;
    float   m_overrideExposure   = -1.0f;
    float   m_overrideSpd        = -1.0f;
    float   m_overrideStarGain   = -1.0f;
    float   m_overrideDiskIncl   = -1.0f;

private slots:
    void checkIdle();
};
