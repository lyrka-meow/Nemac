#pragma once
#include <QAbstractListModel>
#include <QStringList>
#include <QString>

class WallpaperModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY listChanged)
    Q_PROPERTY(QString selectedPath READ selectedPath NOTIFY selectionChanged)
public:
    enum Roles { NameRole = Qt::UserRole + 1, FullPathRole };

    explicit WallpaperModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& p = {}) const override;
    QVariant data(const QModelIndex& idx, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return _paths.size(); }
    QString selectedPath() const { return _selected; }

    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void apply();
    Q_INVOKABLE void addDialog();
    Q_INVOKABLE void refresh();

signals:
    void listChanged();
    void selectionChanged();
    void statusText(const QString& text);

private:
    void scanDir(const QString& dir);

    QStringList _paths;
    QStringList _names;
    QString _selected;
    QString _userDir;
    QString _defaultDir;
};
