#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <vector>

#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class TopicsModel : public QAbstractListModel, public ProvidedSingleton<TopicsModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(Topics)
    QML_SINGLETON
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY countChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, ConceptCountRole, IsUncategorizedRole };

    explicit TopicsModel(WorkspaceController& workspace, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return static_cast<int>(rows_.size()); }
    QVariantList entries() const;

    Q_INVOKABLE QString createTopic(const QString& name);
    Q_INVOKABLE bool rename(const QString& id, const QString& name);
    Q_INVOKABLE bool remove(const QString& id);
    Q_INVOKABLE QString nameOf(const QString& id) const;
    Q_INVOKABLE void refresh();

signals:
    void countChanged();
    void errorOccurred(const QString& message);

private:
    struct Row {
        QString id;
        QString name;
        int conceptCount = 0;
        bool uncategorized = false;
    };

    bool fail(const QString& message);

    WorkspaceController* workspace_;
    std::vector<Row> rows_;
};

}  // namespace atlas::viewmodels
