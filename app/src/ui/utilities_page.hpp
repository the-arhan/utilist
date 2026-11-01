#pragma once

#include <core/utility_manager.hpp>
#include <QWidget>
#include <QVBoxLayout>

namespace utilist::app::ui {

class UtilitiesPage : public QWidget {
    Q_OBJECT

public:
    explicit UtilitiesPage(core::UtilityManager* utilityManager, QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    core::UtilityManager* m_utilityManager{nullptr};
    QVBoxLayout* m_listLayout{nullptr};

    void setupUi();
};

} // namespace utilist::app::ui
