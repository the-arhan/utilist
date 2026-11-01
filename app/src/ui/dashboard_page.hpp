#pragma once

#include <core/utility_manager.hpp>
#include <ui/utility_card.hpp>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <map>

namespace utilist::app::ui {

class DashboardPage : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(core::UtilityManager* utilityManager, QWidget* parent = nullptr);

public slots:
    void refresh();

private slots:
    void onUtilityStateChanged(const std::string& utilityId);
    void applyFilter();

private:
    core::UtilityManager* m_utilityManager{nullptr};
    QLabel* m_totalCountLabel{nullptr};
    QLabel* m_enabledCountLabel{nullptr};
    QLabel* m_runningCountLabel{nullptr};
    QLineEdit* m_searchEdit{nullptr};
    QComboBox* m_categoryCombo{nullptr};
    QVBoxLayout* m_cardsLayout{nullptr};
    QWidget* m_emptyStateWidget{nullptr};
    std::map<std::string, UtilityCard*> m_cardWidgets;

    void setupUi();
    void updateMetrics();
};

} // namespace utilist::app::ui
