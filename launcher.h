#pragma once

#include "version_model.h"
#include <QMutex>
#include <QNetworkAccessManager>
#include <QThread>
#include <QStringList>
#include <atomic>

class Installer : public QThread {
    Q_OBJECT
public:
    explicit Installer(QString root, QString id, QObject *parent = nullptr);
    void cancel();
signals:
    void progress(int done, int total, QString detail);
    void completed(bool success, QString message);
protected:
    void run() override;
private:
    bool fetch(const Download &item, QNetworkAccessManager &network);
    bool fetchMetadata(QNetworkAccessManager &network, const QString &url, const QString &path, const QString &sha1 = {});
    bool install(QNetworkAccessManager &network);
    bool valid(const Download &item) const;
    bool extractNatives(const QString &archive, const QStringList &excludes, const QString &destination);
    QString root_, id_;
    std::atomic_bool cancelled_{false};
    std::atomic_bool failed_{false};
    std::atomic_int done_{0};
    int total_ = 0;
    QMutex errorMutex_;
    QString error_;
};
