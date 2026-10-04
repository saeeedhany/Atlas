#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atlas/render/graph_canvas_item.hpp"
#include "atlas/viewmodels/memory_controller.hpp"
#include "atlas/viewmodels/palette.hpp"
#include "atlas/viewmodels/placement_controller.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class MapViewModel : public QObject, public ProvidedSingleton<MapViewModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(MapView)
    QML_SINGLETON
    Q_PROPERTY(QString topicId READ topicId WRITE setTopicId NOTIFY topicIdChanged)
    Q_PROPERTY(QString selectedId READ selectedId WRITE setSelectedId NOTIFY selectedIdChanged)
    Q_PROPERTY(int conceptCount READ conceptCount NOTIFY sceneChanged)

public:
    static constexpr int kSearchLimit = 8;

    MapViewModel(WorkspaceController& workspace, MemoryController& memory, PlacementController& placements,
                 Palette& palette, QObject* parent = nullptr);

    QString topicId() const { return topicId_; }
    void setTopicId(const QString& topicId);
    QString selectedId() const { return selectedId_; }
    void setSelectedId(const QString& conceptId);
    int conceptCount() const { return conceptCount_; }

    Q_INVOKABLE void attach(QQuickItem* canvas);
    Q_INVOKABLE QVariantList search(const QString& query) const;
    Q_INVOKABLE QString createConcept(const QString& title);
    Q_INVOKABLE void tidy();
    Q_INVOKABLE void refresh();

    const std::vector<atlas::render::RenderNode>& nodes() const { return nodes_; }
    const std::vector<atlas::render::RenderEdge>& edges() const { return edges_; }

signals:
    void topicIdChanged();
    void selectedIdChanged();
    void sceneChanged();
    void errorOccurred(const QString& message);

private:
    std::optional<atlas::core::TopicId> scope() const;
    void scheduleRefresh();
    void dropMissingScope();
    bool isShown(const QString& conceptId) const;
    void pushToCanvas();
    void applySelection();

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
    int conceptCount_ = 0;
    bool refreshPending_ = false;
};

}  // namespace atlas::viewmodels
