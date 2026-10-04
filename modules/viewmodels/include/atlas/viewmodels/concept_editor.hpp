#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/placement_controller.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class ConceptEditor : public QObject, public ProvidedSingleton<ConceptEditor> {
    Q_OBJECT
    QML_NAMED_ELEMENT(Concept)
    QML_SINGLETON
    Q_PROPERTY(QString conceptId READ conceptId WRITE setConceptId NOTIFY loaded)
    Q_PROPERTY(bool exists READ exists NOTIFY loaded)
    Q_PROPERTY(bool dirty READ dirty NOTIFY edited)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY edited)
    Q_PROPERTY(QString definition READ definition WRITE setDefinition NOTIFY edited)
    Q_PROPERTY(QString problemSolved READ problemSolved WRITE setProblemSolved NOTIFY edited)
    Q_PROPERTY(QString whyItExists READ whyItExists WRITE setWhyItExists NOTIFY edited)
    Q_PROPERTY(QString notes READ notes WRITE setNotes NOTIFY edited)
    Q_PROPERTY(int difficulty READ difficulty WRITE setDifficulty NOTIFY edited)
    Q_PROPERTY(QString topicId READ topicId WRITE setTopicId NOTIFY edited)
    Q_PROPERTY(QVariantList examples READ examples WRITE setExamples NOTIFY edited)
    Q_PROPERTY(QVariantList miniProjects READ miniProjects WRITE setMiniProjects NOTIFY edited)
    Q_PROPERTY(QVariantList references READ references WRITE setReferences NOTIFY edited)
    Q_PROPERTY(double recall READ recall NOTIFY loaded)
    Q_PROPERTY(bool pinned READ pinned WRITE setPinned NOTIFY loaded)
    Q_PROPERTY(QStringList difficultyNames READ difficultyNames CONSTANT)

public:
    ConceptEditor(WorkspaceController& workspace, MemoryController& memory, PlacementController& placements,
                  QObject* parent = nullptr);

    QString conceptId() const { return conceptId_; }
    void setConceptId(const QString& conceptId);
    bool exists() const { return exists_; }
    bool dirty() const { return dirty_; }

    QString title() const { return draft_.title; }
    void setTitle(const QString& value) { assign(draft_.title, value); }
    QString definition() const { return draft_.definition; }
    void setDefinition(const QString& value) { assign(draft_.definition, value); }
    QString problemSolved() const { return draft_.problemSolved; }
    void setProblemSolved(const QString& value) { assign(draft_.problemSolved, value); }
    QString whyItExists() const { return draft_.whyItExists; }
    void setWhyItExists(const QString& value) { assign(draft_.whyItExists, value); }
    QString notes() const { return draft_.notes; }
    void setNotes(const QString& value) { assign(draft_.notes, value); }
    int difficulty() const { return draft_.difficulty; }
    void setDifficulty(int value);
    QString topicId() const { return draft_.topicId; }
    void setTopicId(const QString& value) { assign(draft_.topicId, value); }
    QVariantList examples() const { return draft_.examples; }
    void setExamples(const QVariantList& value) { assign(draft_.examples, value); }
    QVariantList miniProjects() const { return draft_.miniProjects; }
    void setMiniProjects(const QVariantList& value) { assign(draft_.miniProjects, value); }
    QVariantList references() const { return draft_.references; }
    void setReferences(const QVariantList& value) { assign(draft_.references, value); }

    double recall() const;
    bool pinned() const;
    void setPinned(bool pinned);
    QStringList difficultyNames() const;

    Q_INVOKABLE bool save();
    Q_INVOKABLE bool remove();
    Q_INVOKABLE void revert();

signals:
    void loaded();
    void edited();
    void errorOccurred(const QString& message);

private:
    struct Draft {
        QString title;
        QString definition;
        QString problemSolved;
        QString whyItExists;
        QString notes;
        int difficulty = 0;
        QString topicId;
        QVariantList examples;
        QVariantList miniProjects;
        QVariantList references;
    };

    template <typename T>
    void assign(T& field, const T& value) {
        if (field == value) return;
        field = value;
        dirty_ = true;
        emit edited();
    }

    void load();
    void onGraphChanged();
    bool fail(const QString& message);

    WorkspaceController* workspace_;
    MemoryController* memory_;
    PlacementController* placements_;
    QString conceptId_;
    bool exists_ = false;
    bool dirty_ = false;
    Draft draft_;
};

}  // namespace atlas::viewmodels
