#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "atlas/render/graph_canvas_item.hpp"
#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/palette.hpp"
#include "atlas/viewmodels/placement_controller.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

struct SessionMarks {
    QString focusId;
    std::unordered_set<QString> hiddenLinks;
    std::unordered_map<QString, atlas::render::EdgeMark> outcomes;
    std::vector<QString> confused;
    std::unordered_set<QString> hinted;
};

class MapViewModel : public QObject, public ProvidedSingleton<MapViewModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(MapView)
    QML_SINGLETON
    Q_PROPERTY(QString topicId READ topicId WRITE setTopicId NOTIFY topicIdChanged)
    Q_PROPERTY(QString selectedId READ selectedId WRITE setSelectedId NOTIFY selectedIdChanged)
    Q_PROPERTY(int conceptCount READ conceptCount NOTIFY sceneChanged)
    Q_PROPERTY(QVariantList regions READ regions NOTIFY sceneChanged)

public:
    static constexpr int kSearchLimit = 8;
    static constexpr double kRegionPadding = 48.0;

    MapViewModel(WorkspaceController& workspace, MemoryController& memory, PlacementController& placements,
                 Palette& palette, QObject* parent = nullptr);

    QString topicId() const { return topicId_; }
    void setTopicId(const QString& topicId);
    QString selectedId() const { return selectedId_; }
    void setSelectedId(const QString& conceptId);
    int conceptCount() const { return conceptCount_; }
    QVariantList regions() const { return regions_; }

    Q_INVOKABLE void attach(QQuickItem* canvas);
    Q_INVOKABLE QVariantList search(const QString& query) const;
    Q_INVOKABLE QVariantMap conceptInfo(const QString& id) const;
    Q_INVOKABLE QVariantMap linkInfo(const QString& linkId) const;
    Q_INVOKABLE QString createConcept(const QString& title);
    Q_INVOKABLE void tidy();
    Q_INVOKABLE bool moveTopic(const QString& topicId, double dx, double dy);
    Q_INVOKABLE void refresh();

    const std::vector<atlas::render::RenderNode>& nodes() const { return nodes_; }
    const std::vector<atlas::render::RenderEdge>& edges() const { return edges_; }

    void setSessionMarks(SessionMarks marks);
    void clearSessionMarks();
    bool inSession() const { return !marks_.focusId.isEmpty(); }

signals:
    void topicIdChanged();
    void selectedIdChanged();
    void sceneChanged();
    void errorOccurred(const QString& message);

private:
    SessionMarks marks_;
    std::optional<atlas::core::TopicId> scope() const;
    void scheduleRefresh();
    void openGroup(const QString& topicId);
    void dropMissingScope();
    bool isShown(const QString& conceptId) const;
    void pushToCanvas();
    void applySelection();
    void buildRegions(const std::vector<atlas::core::KnowledgeObject>& objects,
                      const std::unordered_map<QString, size_t>& nodeIndex);

    WorkspaceController* workspace_;
    MemoryController* memory_;
    PlacementController* placements_;
    Palette* palette_;
    QPointer<atlas::render::GraphCanvasItem> canvas_;
    QString topicId_;
    QString selectedId_;
    std::unordered_map<std::string, std::string> topicNames_;
    std::vector<atlas::render::RenderNode> nodes_;
    std::vector<atlas::render::RenderEdge> edges_;
    QVariantList regions_;
    std::vector<atlas::render::RenderRegion> renderRegions_;
    int conceptCount_ = 0;
    bool refreshPending_ = false;
};

}
