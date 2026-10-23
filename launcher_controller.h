#pragma once

#include "instance_store.h"
#include "java_discovery.h"
#include "version_status.h"

#include <QFutureWatcher>
#include <QObject>
#include <QProcess>
#include <QVariantList>

class Installer;

class LauncherController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool storeReady READ storeReady NOTIFY stateChanged)
    Q_PROPERTY(QVariantList instances READ instances NOTIFY instancesChanged)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY stateChanged)
    Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
    Q_PROPERTY(QVariantList javaRuntimes READ javaRuntimes NOTIFY javaChanged)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY stateChanged)
    Q_PROPERTY(QString selectedVersion READ selectedVersion NOTIFY stateChanged)
    Q_PROPERTY(QString selectedDirectory READ selectedDirectory NOTIFY stateChanged)
    Q_PROPERTY(QString javaPath READ javaPath NOTIFY stateChanged)
    Q_PROPERTY(int requiredJava READ requiredJava NOTIFY stateChanged)
    Q_PROPERTY(bool javaCompatible READ javaCompatible NOTIFY stateChanged)
    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString log READ log NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY stateChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
    Q_PROPERTY(bool loadingVersions READ loadingVersions NOTIFY stateChanged)
    Q_PROPERTY(bool installed READ installed NOTIFY stateChanged)
    Q_PROPERTY(int progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(int progressTotal READ progressTotal NOTIFY stateChanged)
public:
    explicit LauncherController(QObject *parent = nullptr);
    ~LauncherController() override;

    bool storeReady() const { return storeReady_; }
    QVariantList instances() const;
    QString selectedId() const { return store_.selectedId(); }
    QVariantList versions() const { return versions_; }
    QVariantList javaRuntimes() const { return javaRuntimes_; }
    QString selectedName() const;
    QString selectedVersion() const;
    QString selectedDirectory() const;
    QString javaPath() const;
    int requiredJava() const { return current_.plan.javaMajor; }
    bool javaCompatible() const;
    QString playerName() const { return playerName_; }
    void setPlayerName(const QString &name);
    QString status() const { return status_; }
    QString error() const { return error_; }
    QString log() const { return log_; }
    bool busy() const { return worker_ || game_.state() != QProcess::NotRunning; }
    bool checking() const { return checking_; }
    bool scanning() const { return scanning_; }
    bool loadingVersions() const { return loadingVersions_; }
    bool installed() const { return !checking_ && current_.installed; }
    int progress() const { return progress_; }
    int progressTotal() const { return progressTotal_; }

    Q_INVOKABLE void refreshVersions();
    Q_INVOKABLE void selectInstance(const QString &id);
    Q_INVOKABLE void createInstance(const QString &name, const QString &version, const QString &directory);
    Q_INVOKABLE void renameSelected(const QString &name);
    Q_INVOKABLE void checkSelected();
    Q_INVOKABLE void installSelected();
    Q_INVOKABLE void scanJava();
    Q_INVOKABLE void chooseJava(const QString &path);
    Q_INVOKABLE void launchSelected();
    Q_INVOKABLE void cancel();
signals:
    void instancesChanged();
    void versionsChanged();
    void javaChanged();
    void stateChanged();
private:
    void fail(const QString &message);
    void appendLog(const QString &message);
    void refreshInstances();
    void refreshJava();
    void recommendJava();
    void loadVersions(bool online);
    const InstanceRecord *selected() const { return store_.find(store_.selectedId()); }

    InstanceStore store_;
    QProcess game_;
    Installer *worker_ = nullptr;
    QList<QFutureWatcher<VersionStatus> *> checks_;
    QFutureWatcher<QPair<QStringList, QString>> versionsWatcher_;
    QFutureWatcher<QList<JavaRuntime>> javaWatcher_;
    QList<JavaRuntime> runtimes_;
    QVariantList versions_, javaRuntimes_;
    VersionStatus current_;
    QString playerName_, status_, error_, log_;
    int progress_ = 0, progressTotal_ = 1;
    quint64 generation_ = 0;
    bool storeReady_ = false, checking_ = false, scanning_ = false, loadingVersions_ = false;
};
