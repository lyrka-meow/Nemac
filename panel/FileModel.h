#pragma once
#include <QAbstractListModel>
#include <QFileInfoList>
#include <QString>

class FileModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(int dirs  READ dirs  NOTIFY contentsChanged)
    Q_PROPERTY(int files READ files NOTIFY contentsChanged)
public:
    enum Roles { NameRole = Qt::UserRole + 1, FilePathRole, IsDirRole, IconRole, SizeRole };

    explicit FileModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& p = {}) const override;
    QVariant data(const QModelIndex& idx, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString path() const { return _path; }
    int dirs()  const { return _dirs; }
    int files() const { return _files; }

    Q_INVOKABLE void cd(const QString& path);
    Q_INVOKABLE void cdUp();
    Q_INVOKABLE void goHome();
    Q_INVOKABLE void open(int index);

signals:
    void pathChanged();
    void contentsChanged();

private:
    void reload();
    QString iconFor(const QString& name, bool isDir) const;

    QString _path;
    QFileInfoList _entries;
    int _dirs  = 0;
    int _files = 0;
};
