#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <vector>

#include "atlas/persistence/note_repository.hpp"
#include "atlas/viewmodels/clock.hpp"
#include "atlas/viewmodels/controller_error.hpp"
#include "atlas/viewmodels/provided_singleton.hpp"
#include "atlas/viewmodels/workspace_controller.hpp"

namespace atlas::viewmodels {

class NotesModel : public QAbstractListModel, public ProvidedSingleton<NotesModel> {
    Q_OBJECT
    QML_NAMED_ELEMENT(Notes)
    QML_SINGLETON
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(double minSize READ minSize CONSTANT)

public:
    enum Role { IdRole = Qt::UserRole + 1, BodyRole, ColorRole, XRole, YRole, WidthRole, HeightRole, LinksRole };
    static constexpr double kDefaultWidth = 150.0;
    static constexpr double kDefaultHeight = 100.0;
    static constexpr double kMinSize = 64.0;

    NotesModel(atlas::persistence::Database& database, WorkspaceController& workspace, Clock clock,
               QObject* parent = nullptr);

    Result<void, ControllerFailure> load();
    int count() const { return static_cast<int>(notes_.size()); }
    double minSize() const { return kMinSize; }
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString createNote(double x, double y, const QString& body = QString());
    Q_INVOKABLE bool setBody(const QString& id, const QString& body);
    Q_INVOKABLE bool move(const QString& id, double x, double y);
    Q_INVOKABLE bool resize(const QString& id, double width, double height);
    Q_INVOKABLE bool recolor(const QString& id, const QString& color);
    Q_INVOKABLE bool link(const QString& id, const QString& kind, const QString& targetId);
    Q_INVOKABLE bool unlink(const QString& id, const QString& kind, const QString& targetId);
    Q_INVOKABLE bool remove(const QString& id);
    Q_INVOKABLE QVariantMap note(const QString& id) const;

signals:
    void countChanged();
    void errorOccurred(const QString& message);

private:
    int indexOf(const QString& id) const;
    bool targetExists(const QString& kind, const QString& targetId) const;
    bool fail(const QString& message);
    bool update(const QString& id, const std::function<void(atlas::persistence::BoardNote&)>& change);
    QVariantMap toMap(const atlas::persistence::BoardNote& note) const;
    void reload();

    atlas::persistence::NoteRepository repository_;
    WorkspaceController* workspace_;
    Clock clock_;
    std::vector<atlas::persistence::BoardNote> notes_;
};

}
