#pragma once

#include <core/utility_manager.hpp>
#include <core/shortcut_manager.hpp>
#include <QWidget>
#include <QVBoxLayout>

namespace utilist::app::ui {

class ShortcutsPage : public QWidget {
    Q_OBJECT

public:
    explicit ShortcutsPage(core::UtilityManager* utilityManager,
                           core::ShortcutManager* shortcutManager,
                           QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    core::UtilityManager* m_utilityManager{nullptr};
    core::ShortcutManager* m_shortcutManager{nullptr};
    QVBoxLayout* m_tableLayout{nullptr};

    void setupUi();
};

} // namespace utilist::app::ui
