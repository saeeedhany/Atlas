#include "atlas/viewmodels/notes_model.hpp"

#include <algorithm>

#include "atlas/viewmodels/ids.hpp"

namespace atlas::viewmodels {

using atlas::core::Uuid;
using atlas::persistence::BoardNote;
using atlas::persistence::NoteLink;

namespace {

bool validColor(const QString& color) {
    return color == QLatin1String("clay") || color == QLatin1String("olive") || color == QLatin1String("rose");
}

bool validKind(const QString& kind) { return kind == QLatin1String("concept") || kind == QLatin1String("topic"); }

}  // namespace

NotesModel::NotesModel(atlas::persistence::Database& database, WorkspaceController& workspace, Clock clock,
                       QObject* parent)
    : QAbstractListModel(parent), repository_(database), workspace_(&workspace), clock_(std::move(clock)) {
    connect(workspace_, &WorkspaceController::graphChanged, this, &NotesModel::reload);
    connect(workspace_, &WorkspaceController::topicsChanged, this, &NotesModel::reload);
}

Result<void, ControllerFailure> NotesModel::load() {
    auto all = repository_.findAll();
    if (!all.hasValue()) {
        return Result<void, ControllerFailure>::err({ControllerErrorCode::PersistenceFailed, all.error().detail});
    }
    beginResetModel();
    notes_ = std::move(all).value();
    endResetModel();
    emit countChanged();
    return Result<void, ControllerFailure>::ok();
}

void NotesModel::reload() {
    if (auto loaded = load(); !loaded.hasValue()) emit errorOccurred(toQString(loaded.error().detail));
}

int NotesModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : count(); }

QVariant NotesModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const auto& note = notes_[static_cast<size_t>(index.row())];
    switch (role) {
        case IdRole: return toQString(note.id.toString());
        case BodyRole: return toQString(note.body);
        case ColorRole: return toQString(note.color);
        case XRole: return note.x;
        case YRole: return note.y;
        case WidthRole: return note.width;
        case HeightRole: return note.height;
        case LinksRole: return toMap(note).value("links");
        default: return {};
    }
}

QHash<int, QByteArray> NotesModel::roleNames() const {
    return {{IdRole, "noteId"},      {BodyRole, "body"},         {ColorRole, "colorName"},     {XRole, "worldX"},
            {YRole, "worldY"},       {WidthRole, "worldWidth"},  {HeightRole, "worldHeight"},  {LinksRole, "links"}};
}

QVariantMap NotesModel::toMap(const BoardNote& note) const {
    QVariantList links;
    for (const auto& link : note.links) {
        links.append(QVariantMap{{"kind", toQString(link.kind)}, {"targetId", toQString(link.targetId.toString())}});
    }
    return {{"noteId", toQString(note.id.toString())}, {"body", toQString(note.body)}, {"color", toQString(note.color)},
            {"x", note.x}, {"y", note.y}, {"width", note.width}, {"height", note.height}, {"links", links}};
}

int NotesModel::indexOf(const QString& id) const {
    auto it = std::find_if(notes_.begin(), notes_.end(), [&](const BoardNote& n) { return toQString(n.id.toString()) == id; });
    return it == notes_.end() ? -1 : static_cast<int>(it - notes_.begin());
}

bool NotesModel::fail(const QString& message) {
    emit errorOccurred(message);
    return false;
}

QString NotesModel::create(double x, double y, const QString& body) {
    BoardNote note;
    note.id = Uuid::generate();
    note.body = toStdString(body);
    note.color = "clay";
    note.x = x;
    note.y = y;
    note.width = kDefaultWidth;
    note.height = kDefaultHeight;
    note.createdAt = clock_();
    note.updatedAt = note.createdAt;
    if (auto saved = repository_.save(note); !saved.hasValue()) {
        fail(toQString(saved.error().detail));
        return {};
    }
    beginInsertRows(QModelIndex(), count(), count());
    notes_.push_back(note);
    endInsertRows();
    emit countChanged();
    return toQString(note.id.toString());
}

bool NotesModel::update(const QString& id, const std::function<void(BoardNote&)>& change) {
    int row = indexOf(id);
    if (row < 0) return fail(tr("That note no longer exists"));
    BoardNote next = notes_[static_cast<size_t>(row)];
    change(next);
    next.updatedAt = clock_();
    if (auto saved = repository_.save(next); !saved.hasValue()) return fail(toQString(saved.error().detail));
    notes_[static_cast<size_t>(row)] = next;
    emit dataChanged(index(row), index(row));
    return true;
}

bool NotesModel::setBody(const QString& id, const QString& body) {
    return update(id, [&](BoardNote& note) { note.body = toStdString(body); });
}

bool NotesModel::move(const QString& id, double x, double y) {
    return update(id, [&](BoardNote& note) {
        note.x = x;
        note.y = y;
    });
}

bool NotesModel::resize(const QString& id, double width, double height) {
    return update(id, [&](BoardNote& note) {
        note.width = std::max(width, kMinSize);
        note.height = std::max(height, kMinSize);
    });
}

bool NotesModel::recolor(const QString& id, const QString& color) {
    if (!validColor(color)) return false;
    return update(id, [&](BoardNote& note) { note.color = toStdString(color); });
}

bool NotesModel::targetExists(const QString& kind, const QString& targetId) const {
    if (kind == QLatin1String("concept")) {
        auto conceptId = parseId<KnowledgeObjectId>(targetId);
        return conceptId && workspace_->graph().findNode(*conceptId) != nullptr;
    }
    auto topicId = parseId<TopicId>(targetId);
    return topicId && workspace_->findTopic(*topicId).has_value();
}

bool NotesModel::link(const QString& id, const QString& kind, const QString& targetId) {
    int row = indexOf(id);
    auto target = Uuid::parse(toStdString(targetId));
    if (row < 0 || !validKind(kind) || !target || !targetExists(kind, targetId)) return false;
    NoteLink link{toStdString(kind), *target};
    if (auto saved = repository_.addLink(notes_[static_cast<size_t>(row)].id, link); !saved.hasValue()) {
        return fail(toQString(saved.error().detail));
    }
    auto& links = notes_[static_cast<size_t>(row)].links;
    if (std::find(links.begin(), links.end(), link) == links.end()) links.push_back(link);
    emit dataChanged(index(row), index(row));
    return true;
}

bool NotesModel::unlink(const QString& id, const QString& kind, const QString& targetId) {
    int row = indexOf(id);
    auto target = Uuid::parse(toStdString(targetId));
    if (row < 0 || !validKind(kind) || !target) return false;
    NoteLink link{toStdString(kind), *target};
    if (auto saved = repository_.removeLink(notes_[static_cast<size_t>(row)].id, link); !saved.hasValue()) {
        return fail(toQString(saved.error().detail));
    }
    auto& links = notes_[static_cast<size_t>(row)].links;
    links.erase(std::remove(links.begin(), links.end(), link), links.end());
    emit dataChanged(index(row), index(row));
    return true;
}

bool NotesModel::remove(const QString& id) {
    int row = indexOf(id);
    if (row < 0) return false;
    if (auto removed = repository_.remove(notes_[static_cast<size_t>(row)].id); !removed.hasValue()) {
        return fail(toQString(removed.error().detail));
    }
    beginRemoveRows(QModelIndex(), row, row);
    notes_.erase(notes_.begin() + row);
    endRemoveRows();
    emit countChanged();
    return true;
}

QVariantMap NotesModel::note(const QString& id) const {
    int row = indexOf(id);
    return row < 0 ? QVariantMap() : toMap(notes_[static_cast<size_t>(row)]);
}

}  // namespace atlas::viewmodels
