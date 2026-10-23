#include "instance_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUuid>
#include <utility>

namespace {
QString resolvedPath(const QString &path) {
    QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    QStringList suffix;
    QFileInfo info(absolute);
    while (!info.exists() && !info.isSymLink()) {
        suffix.prepend(info.fileName());
        const QString parent = info.dir().absolutePath();
        if (parent == absolute) break;
        absolute = parent;
        info.setFile(absolute);
    }
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty()) absolute = canonical;
    for (const QString &part : suffix) absolute = QDir(absolute).filePath(part);
    return QDir::cleanPath(absolute);
}

bool overlaps(const QString &left, const QString &right) {
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    const auto a = QDir::fromNativeSeparators(left);
    const auto b = QDir::fromNativeSeparators(right);
    return a.compare(b, sensitivity) == 0 ||
           a.startsWith(b.endsWith('/') ? b : b + '/', sensitivity) ||
           b.startsWith(a.endsWith('/') ? a : a + '/', sensitivity);
}

bool validId(const QString &id) {
    return !QUuid(id).isNull() && QUuid(id).toString(QUuid::WithoutBraces) == id;
}

bool validVersion(const QString &id) {
    static const QRegularExpression pattern("^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$");
    return id != ".." && pattern.match(id).hasMatch();
}

bool writableDirectory(const QString &path, QString &error) {
    QFileInfo info(path);
    if (info.exists() && !info.isDir()) {
        error = "目标路径不是目录：" + path;
        return false;
    }
    QString probe = path;
    while (!QFileInfo(probe).exists()) {
        const QString parent = QFileInfo(probe).dir().absolutePath();
        if (parent == probe) {
            error = "找不到可写的目标目录上级：" + path;
            return false;
        }
        probe = parent;
    }
    QTemporaryFile file(QDir(probe).filePath(".pclqt-write-XXXXXX"));
    if (!file.open()) {
        error = "目标目录或其上级不可写：" + probe + "（" + file.errorString() + "）";
        return false;
    }
    return true;
}

QJsonObject asJson(const InstanceRecord &record) {
    return {{"id", record.id}, {"name", record.name}, {"versionId", record.versionId},
            {"directory", record.directory}, {"javaPath", record.javaPath}, {"pending", record.pending}};
}

bool decode(const QJsonObject &object, InstanceRecord &record) {
    for (const char *key : {"id", "name", "versionId", "directory", "javaPath"})
        if (!object.value(QLatin1String(key)).isString()) return false;
    if (!object.value("pending").isBool()) return false;
    record = {object.value("id").toString(), object.value("name").toString(),
              object.value("versionId").toString(), object.value("directory").toString(),
              object.value("javaPath").toString(), object.value("pending").toBool()};
    return true;
}
}

InstanceStore::InstanceStore(QString filePath)
    : filePath_(filePath.isEmpty()
                    ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("instances.json")
                    : QDir::cleanPath(QFileInfo(filePath).absoluteFilePath())) {}

const InstanceRecord *InstanceStore::find(const QString &id) const {
    for (const auto &record : instances_) if (record.id == id) return &record;
    return nullptr;
}

bool InstanceStore::load(QString &error) {
    error.clear();
    loaded_ = false;
    QFile file(filePath_);
    if (!file.exists() && !QFileInfo(filePath_).isSymLink()) {
        instances_.clear();
        selectedId_.clear();
        loaded_ = true;
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        error = "无法读取实例登记文件：" + file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = "实例登记 JSON 无效：" + parseError.errorString();
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value("schemaVersion").toInt(-1) != 1 || !root.value("instances").isArray() ||
        !root.value("selectedId").isString()) {
        error = "实例登记文件格式或版本不支持";
        return false;
    }
    QList<InstanceRecord> records;
    for (const auto &value : root.value("instances").toArray()) {
        InstanceRecord record;
        if (!value.isObject() || !decode(value.toObject(), record) ||
            !validate(record, records, false, error)) {
            if (error.isEmpty()) error = "实例登记文件包含无效记录";
            return false;
        }
        records.append(record);
    }
    const QString selected = root.value("selectedId").toString();
    bool found = selected.isEmpty();
    for (const auto &record : records) if (record.id == selected) found = true;
    if (!found) {
        error = "选中的实例 ID 不存在：" + selected;
        return false;
    }
    instances_ = std::move(records);
    selectedId_ = selected;
    loaded_ = true;
    return true;
}

bool InstanceStore::save(QString &error) const {
    error.clear();
    if (!loaded_) {
        error = "请先加载实例登记文件，避免覆盖已有数据";
        return false;
    }
    if (QFileInfo(filePath_).isSymLink()) {
        error = "实例登记文件是符号链接，拒绝替换：" + filePath_;
        return false;
    }
    const QString parent = QFileInfo(filePath_).dir().absolutePath();
    if (!QDir().mkpath(parent)) {
        error = "无法创建实例登记目录：" + parent;
        return false;
    }
    QJsonArray array;
    for (const auto &record : instances_) array.append(asJson(record));
    const QJsonDocument document(QJsonObject{{"schemaVersion", 1}, {"selectedId", selectedId_}, {"instances", array}});
    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        error = "无法写入实例登记文件：" + file.errorString();
        return false;
    }
    const QByteArray bytes = document.toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        error = "无法完整写入实例登记文件：" + file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = "无法原子保存实例登记文件：" + file.errorString();
        return false;
    }
    return true;
}

bool InstanceStore::validate(const InstanceRecord &record, const QList<InstanceRecord> &records,
                             bool newDirectory, QString &error) const {
    if (!validId(record.id) || record.name.trimmed().isEmpty() || !validVersion(record.versionId) ||
        record.directory.isEmpty() || !QDir::isAbsolutePath(record.directory) ||
        QDir::cleanPath(record.directory) != record.directory) {
        error = "实例 ID、显示名、官方版本 ID 或绝对目标目录无效";
        return false;
    }
    const QString target = resolvedPath(record.directory);
    for (const auto &other : records) {
        if (other.id == record.id) {
            error = "实例 ID 已存在：" + record.id;
            return false;
        }
        if (overlaps(target, resolvedPath(other.directory))) {
            error = "目标目录与已有实例路径重叠：" + other.directory;
            return false;
        }
    }
    if (!newDirectory) return true;
    const QFileInfo info(record.directory);
    if (info.isSymLink() && !info.exists()) {
        error = "目标目录是失效的符号链接：" + record.directory;
        return false;
    }
    if (info.exists()) {
        if (!info.isDir()) {
            error = "目标路径不是目录：" + record.directory;
            return false;
        }
        const QDir dir(record.directory);
        if (!info.isReadable()) {
            error = "无法读取目标目录，不能确认它为空：" + record.directory;
            return false;
        }
        if (!dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System).isEmpty()) {
            error = "目标目录非空，拒绝覆盖未知数据：" + record.directory;
            return false;
        }
    }
    return writableDirectory(record.directory, error);
}

bool InstanceStore::commit(const QList<InstanceRecord> &records, const QString &selectedId, QString &error) {
    if (!loaded_) {
        error = "请先加载实例登记文件，避免覆盖已有数据";
        return false;
    }
    InstanceStore staged(*this);
    staged.instances_ = records;
    staged.selectedId_ = selectedId;
    if (!staged.save(error)) return false;
    instances_ = records;
    selectedId_ = selectedId;
    return true;
}

bool InstanceStore::add(QString name, QString versionId, QString directory, QString javaPath,
                        QString &newId, QString &error) {
    error.clear();
    newId.clear();
    if (!loaded_) { error = "请先加载实例登记文件"; return false; }
    InstanceRecord record{QUuid::createUuid().toString(QUuid::WithoutBraces), std::move(name),
                          std::move(versionId), std::move(directory), std::move(javaPath), true};
    if (!validate(record, instances_, true, error)) return false;
    auto records = instances_;
    records.append(record);
    if (!commit(records, selectedId_, error)) return false;
    newId = record.id;
    return true;
}

bool InstanceStore::update(const InstanceRecord &record, QString &error) {
    error.clear();
    if (!loaded_) { error = "请先加载实例登记文件"; return false; }
    auto records = instances_;
    for (auto &current : records) {
        if (current.id != record.id) continue;
        const bool moved = current.directory != record.directory;
        current = record;
        QList<InstanceRecord> others;
        for (const auto &entry : records) if (entry.id != record.id) others.append(entry);
        if (!validate(record, others, moved, error)) return false;
        return commit(records, selectedId_, error);
    }
    error = "实例 ID 不存在：" + record.id;
    return false;
}

bool InstanceStore::select(const QString &id, QString &error) {
    error.clear();
    if (!id.isEmpty() && !find(id)) { error = "实例 ID 不存在：" + id; return false; }
    return commit(instances_, id, error);
}

bool InstanceStore::rename(const QString &id, const QString &name, QString &error) {
    const auto *record = find(id);
    if (!record) { error = "实例 ID 不存在：" + id; return false; }
    auto changed = *record;
    changed.name = name;
    return update(changed, error);
}

bool InstanceStore::setJavaPath(const QString &id, const QString &javaPath, QString &error) {
    const auto *record = find(id);
    if (!record) { error = "实例 ID 不存在：" + id; return false; }
    auto changed = *record;
    changed.javaPath = javaPath;
    return update(changed, error);
}

bool InstanceStore::setPending(const QString &id, bool pending, QString &error) {
    const auto *record = find(id);
    if (!record) { error = "实例 ID 不存在：" + id; return false; }
    auto changed = *record;
    changed.pending = pending;
    return update(changed, error);
}
