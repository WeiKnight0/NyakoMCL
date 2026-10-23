#pragma once

#include <QString>
#include <QList>

struct JavaRuntime {
    QString path;
    QString version;
    QString architecture;
    int major = 0;

    bool compatible(int requiredMajor) const { return major == requiredMajor; }
    QString label(int requiredMajor = 0) const;
};

JavaRuntime inspectJava(const QString &path);
QList<JavaRuntime> discoverJava(const QString &preferredPath);
