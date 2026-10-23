#pragma once

#include <QList>
#include <QString>

struct InstanceRecord {
    QString id;
    QString name;
    QString versionId;
    QString directory;
    QString javaPath;
    bool pending = true;
};

class InstanceStore {
public:
    explicit InstanceStore(QString filePath = {});

    const QList<InstanceRecord> &instances() const { return instances_; }
    const QString &selectedId() const { return selectedId_; }
    const QString &filePath() const { return filePath_; }
    const InstanceRecord *find(const QString &id) const;

    bool load(QString &error);
    bool save(QString &error) const;
    bool add(QString name, QString versionId, QString directory, QString javaPath,
             QString &newId, QString &error);
    bool update(const InstanceRecord &record, QString &error);
    bool select(const QString &id, QString &error);
    bool rename(const QString &id, const QString &name, QString &error);
    bool setJavaPath(const QString &id, const QString &javaPath, QString &error);
    bool setPending(const QString &id, bool pending, QString &error);

private:
    bool commit(const QList<InstanceRecord> &records, const QString &selectedId, QString &error);
    bool validate(const InstanceRecord &record, const QList<InstanceRecord> &records,
                  bool newDirectory, QString &error) const;
    bool loaded_ = false;
    QString filePath_;
    QList<InstanceRecord> instances_;
    QString selectedId_;
};
