#pragma once

#include <models/utility_state.hpp>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace utilist::app::ui {

class UtilityCard : public QFrame {
    Q_OBJECT

public:
    explicit UtilityCard(const ManagedUtility& utility, QWidget* parent = nullptr);

    void updateState(const ManagedUtility& utility);
    [[nodiscard]] std::string utilityId() const noexcept { return m_utilityId; }

signals:
    void enableToggled(const std::string& utilityId, bool enabled);
    void featureToggleClicked(const std::string& utilityId);
    void startStopClicked(const std::string& utilityId);
    void configureClicked(const std::string& utilityId);

private:
    std::string m_utilityId;
    QLabel* m_iconLabel{nullptr};
    QLabel* m_nameLabel{nullptr};
    QLabel* m_descLabel{nullptr};
    QLabel* m_categoryPill{nullptr};
    QLabel* m_statusPill{nullptr};
    QLabel* m_detailLabel{nullptr};
    QLabel* m_shortcutBadge{nullptr};
    QCheckBox* m_enableCheck{nullptr};
    QPushButton* m_actionButton{nullptr};
    QPushButton* m_processButton{nullptr};
    QPushButton* m_configureButton{nullptr};

    void setupUi();
    void applyTheme();
};

} // namespace utilist::app::ui
