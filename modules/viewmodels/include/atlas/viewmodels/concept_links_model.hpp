#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

QString relationshipLabel(atlas::core::RelationshipType type);

class ConceptLinksModel : public QAbstractListModel, public ProvidedSingleton<ConceptLinksModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(ConceptLinks)
    QML_SINGLETON
    Q_PROPERTY(QString conceptId READ conceptId WRITE setConceptId NOTIFY conceptIdChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QStringList typeNames READ typeNames CONSTANT)

public:
    static constexpr int kCandidateLimit = 8;

    enum Role {
        LinkIdRole = Qt::UserRole + 1,
        OtherIdRole,
        OtherTitleRole,
        TypeNameRole,
        OutgoingRole,
        SymmetricRole,
        NoteRole,
        RecallRole
    };

    ConceptLinksModel(WorkspaceController& workspace, MemoryController& memory, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString conceptId() const { return conceptId_; }
    void setConceptId(const QString& conceptId);
    int count() const { return static_cast<int>(rows_.size()); }
    QStringList typeNames() const;

    Q_INVOKABLE bool add(const QString& otherId, int typeIndex, bool outgoing, const QString& note);
    Q_INVOKABLE bool remove(const QString& linkId);
    Q_INVOKABLE QVariantList candidates(const QString& query);

signals:
    void conceptIdChanged();
    void countChanged();
    void errorOccurred(const QString& message);

private:
    struct Row {
        QString linkId;
        QString otherId;
        QString otherTitle;
        QString typeName;
        bool outgoing = true;
        bool symmetric = false;
        QString note;
        double recall = -1.0;
    };

    void refresh();
    bool fail(const QString& message);

    WorkspaceController* workspace_;
    MemoryController* memory_;
    QString conceptId_;
    std::vector<Row> rows_;
    std::unordered_map<std::string, std::string> topicNames_;
};

}  // namespace atlas::viewmodels
