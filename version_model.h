#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QList>
#include <atomic>
#include <functional>

struct Download {
    QString url, sha1, path;
    qint64 size = -1;
};
struct NativeArchive {
    Download file;
    QStringList excludes;
};
struct VersionPlan {
    QString id, root, natives;
    int javaMajor = 0;
    QJsonObject metadata;
    QList<Download> files;
    QList<NativeArchive> archives;
    QStringList classpath;
};
struct FileStatus {
    int missing = 0, damaged = 0, checked = 0;
    QStringList examples;
    bool ready() const { return missing == 0 && damaged == 0; }
};

bool validVersionId(const QString &id);
QString versionFile(const QString &root, const QString &id);
QString safePath(const QString &base, const QString &relative);
bool validDownload(const Download &item);
bool validFile(const Download &item, const std::atomic_bool *cancel = nullptr);
bool buildVersionPlan(const QJsonObject &version, const QString &root, const QString &id,
                      const QJsonObject &index, VersionPlan &plan, QString &error);
FileStatus evaluatePlan(const VersionPlan &plan, const std::atomic_bool *cancel = nullptr,
                        const std::function<void(int, int)> &progress = {});
bool launchArguments(const VersionPlan &plan, const QString &name, QStringList &arguments, QString &error);
QString offlineUuid(const QString &name);
