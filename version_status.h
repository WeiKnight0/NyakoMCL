#pragma once
#include "version_model.h"

struct VersionStatus {
    VersionPlan plan;
    FileStatus files;
    QString message;
    bool supported = false;
    bool installed = false;
};
VersionStatus inspectVersion(const QString &root, const QString &id, bool online,
                             const std::function<void(int, int)> &progress = {});
QStringList releaseVersions(const QString &root, bool online, QString &message);
