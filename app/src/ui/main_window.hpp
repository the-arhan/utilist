#pragma once

#include <core/utility_manager.hpp>
#include <core/shortcut_manager.hpp>
#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QCloseEvent>
#include <vector>

namespace utilist::app::ui {

class DashboardPage;
class UtilitiesPage;
class ShortcutsPage;
class SettingsPage;
class AboutPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(core::UtilityManager* utilityManager,
                        core::ShortcutManager* shortcutManager,
                        QWidget* parent = nullptr);
    ~MainWindow() override = default;

public slots:
    void navigateToPage(int pageIndex);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    core::UtilityManager* m_utilityManager{nullptr};
    core::ShortcutManager* m_shortcutManager{nullptr};

    QStackedWidget* m_pageStack{nullptr};
    std::vector<QPushButton*> m_navButtons;

    DashboardPage* m_dashboardPage{nullptr};
    UtilitiesPage* m_utilitiesPage{nullptr};
    ShortcutsPage* m_shortcutsPage{nullptr};
    SettingsPage* m_settingsPage{nullptr};
    AboutPage* m_aboutPage{nullptr};

    void setupUi();
    void setupSidebar(QWidget* container);
    void updateNavStyles(int activeIndex);
};

} // namespace utilist::app::ui
