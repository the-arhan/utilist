#include "dashboard_page.hpp"
#include <QScrollArea>
#include <QFrame>

namespace utilist::app::ui {

DashboardPage::DashboardPage(core::UtilityManager* utilityManager, QWidget* parent)
    : QWidget(parent),
      m_utilityManager(utilityManager) {
    setupUi();
    connect(m_utilityManager, &core::UtilityManager::utilitiesRefreshed, this, &DashboardPage::refresh);
    connect(m_utilityManager, &core::UtilityManager::utilityStateChanged, this, &DashboardPage::onUtilityStateChanged);
    refresh();
}

void DashboardPage::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(18);

    // Header Row with Title and Actions
    auto* headerLayout = new QHBoxLayout();
    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    auto* titleLabel = new QLabel("Dashboard", this);
    titleLabel->setStyleSheet("color: #f0f6fc; font-size: 22px; font-weight: bold;");
    titleLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel("Manage, configure, and monitor your native desktop utilities.", this);
    subtitleLabel->setStyleSheet("color: #8b949e; font-size: 13px;");
    titleLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(titleLayout);

    headerLayout->addStretch();

    auto* refreshBtn = new QPushButton("Refresh Status", this);
    refreshBtn->setFixedHeight(32);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #21262d;"
        "  color: #f0f6fc;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 0 14px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #30363d; }"
    );
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        m_utilityManager->refreshUtilities();
    });
    headerLayout->addWidget(refreshBtn);

    rootLayout->addLayout(headerLayout);

    // Metrics Row
    auto* metricsLayout = new QHBoxLayout();
    metricsLayout->setSpacing(14);

    auto createMetricCard = [this](const QString& title, QLabel*& outValueLabel, const QString& accentColor) -> QFrame* {
        auto* card = new QFrame(this);
        card->setStyleSheet(
            "background-color: #161b22;"
            "border: 1px solid #30363d;"
            "border-radius: 8px;"
            "padding: 12px 16px;"
        );
        auto* l = new QVBoxLayout(card);
        l->setSpacing(4);
        l->setContentsMargins(12, 10, 12, 10);

        auto* t = new QLabel(title, card);
        t->setStyleSheet("color: #8b949e; font-size: 11px; font-weight: bold; text-transform: uppercase;");
        l->addWidget(t);

        outValueLabel = new QLabel("0", card);
        outValueLabel->setStyleSheet(QString("color: %1; font-size: 24px; font-weight: bold;").arg(accentColor));
        l->addWidget(outValueLabel);

        return card;
    };

    metricsLayout->addWidget(createMetricCard("Discovered Utilities", m_totalCountLabel, "#f0f6fc"));
    metricsLayout->addWidget(createMetricCard("Enabled", m_enabledCountLabel, "#00b4d8"));
    metricsLayout->addWidget(createMetricCard("Active / Running", m_runningCountLabel, "#3fb950"));

    rootLayout->addLayout(metricsLayout);

    // Search and Category Filter Toolbar
    auto* filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(12);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search utilities by name, description, or id...");
    m_searchEdit->setFixedHeight(34);
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #161b22;"
        "  color: #f0f6fc;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 0 12px;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #00b4d8;"
        "}"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &DashboardPage::applyFilter);
    filterLayout->addWidget(m_searchEdit, 1);

    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->setFixedHeight(34);
    m_categoryCombo->setFixedWidth(180);
    m_categoryCombo->setStyleSheet(
        "QComboBox {"
        "  background-color: #161b22;"
        "  color: #f0f6fc;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 0 10px;"
        "  font-size: 12px;"
        "}"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #161b22;"
        "  color: #f0f6fc;"
        "  selection-background-color: #1f3554;"
        "  border: 1px solid #30363d;"
        "}"
    );
    m_categoryCombo->addItem("All Categories", "");
    m_categoryCombo->addItem("Visuals & Cursor", "Visuals");
    m_categoryCombo->addItem("Power & Display", "Power");
    m_categoryCombo->addItem("Input & Security", "Security");
    m_categoryCombo->addItem("Window Management", "Windows");
    m_categoryCombo->addItem("System & Diagnostics", "Diagnostics");
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DashboardPage::applyFilter);
    filterLayout->addWidget(m_categoryCombo);

    rootLayout->addLayout(filterLayout);

    // Section title
    auto* sectionTitle = new QLabel("Installed Utilities", this);
    sectionTitle->setStyleSheet("color: #c9d1d9; font-size: 15px; font-weight: bold; margin-top: 4px;");
    rootLayout->addWidget(sectionTitle);

    // Scroll area for cards
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background: transparent;");

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("background: transparent;");
    m_cardsLayout = new QVBoxLayout(scrollContent);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(10);

    // Empty state widget
    m_emptyStateWidget = new QFrame(scrollContent);
    m_emptyStateWidget->setStyleSheet(
        "background-color: #161b22;"
        "border: 1px dashed #30363d;"
        "border-radius: 8px;"
        "padding: 30px;"
    );
    auto* emptyLayout = new QVBoxLayout(m_emptyStateWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(8);

    auto* emptyTitle = new QLabel("No Utilities Discovered", m_emptyStateWidget);
    emptyTitle->setStyleSheet("color: #f0f6fc; font-size: 16px; font-weight: bold;");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);

    auto* emptyDesc = new QLabel(
        "UTILIST looks for utilities with a valid manifest.json in your AppData utilities folder.\n"
        "Bundled utilities: MTrail, Awake, InputShield, WindowPin, and HeartbeatUtility.",
        m_emptyStateWidget
    );
    emptyDesc->setStyleSheet("color: #8b949e; font-size: 12px;");
    emptyDesc->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyDesc);

    m_cardsLayout->addWidget(m_emptyStateWidget);
    m_cardsLayout->addStretch();

    scrollArea->setWidget(scrollContent);
    rootLayout->addWidget(scrollArea, 1);
}

void DashboardPage::updateMetrics() {
    auto utilities = m_utilityManager->getManagedUtilities();
    int total = static_cast<int>(utilities.size());
    int enabled = 0;
    int running = 0;

    for (const auto& u : utilities) {
        if (u.enabled) enabled++;
        if (u.processState == ProcessState::Running) running++;
    }

    m_totalCountLabel->setText(QString::number(total));
    m_enabledCountLabel->setText(QString::number(enabled));
    m_runningCountLabel->setText(QString::number(running));
}

void DashboardPage::refresh() {
    auto utilities = m_utilityManager->getManagedUtilities();
    updateMetrics();

    if (utilities.empty()) {
        m_emptyStateWidget->show();
        for (auto& [id, widget] : m_cardWidgets) {
            widget->hide();
        }
        return;
    }

    m_emptyStateWidget->hide();

    for (const auto& u : utilities) {
        auto it = m_cardWidgets.find(u.descriptor.id);
        if (it != m_cardWidgets.end()) {
            it->second->updateState(u);
            it->second->show();
        } else {
            auto* card = new UtilityCard(u, this);
            connect(card, &UtilityCard::enableToggled, this, [this](const std::string& id, bool en) {
                m_utilityManager->setUtilityEnabled(id, en);
            });
            connect(card, &UtilityCard::featureToggleClicked, this, [this](const std::string& id) {
                m_utilityManager->toggleUtilityFeature(id);
            });
            connect(card, &UtilityCard::startStopClicked, this, [this](const std::string& id) {
                auto utilOpt = m_utilityManager->getManagedUtility(id);
                if (utilOpt && utilOpt->processState == ProcessState::Running) {
                    m_utilityManager->stopUtility(id);
                } else {
                    m_utilityManager->startUtility(id);
                }
            });
            connect(card, &UtilityCard::configureClicked, this, [this](const std::string& id) {
                m_utilityManager->sendUtilityCommand(id, "update_settings");
            });

            m_cardWidgets[u.descriptor.id] = card;
            // Insert before the trailing stretch
            m_cardsLayout->insertWidget(m_cardsLayout->count() - 1, card);
        }
    }

    applyFilter();
}

void DashboardPage::applyFilter() {
    QString search = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : "";
    QString categoryFilter = m_categoryCombo ? m_categoryCombo->currentData().toString() : "";

    auto utilities = m_utilityManager->getManagedUtilities();
    int visibleCount = 0;

    for (const auto& u : utilities) {
        auto it = m_cardWidgets.find(u.descriptor.id);
        if (it == m_cardWidgets.end()) continue;

        bool matchesSearch = true;
        if (!search.isEmpty()) {
            QString name = QString::fromStdString(u.descriptor.name).toLower();
            QString desc = QString::fromStdString(u.descriptor.description).toLower();
            QString id = QString::fromStdString(u.descriptor.id).toLower();
            matchesSearch = name.contains(search) || desc.contains(search) || id.contains(search);
        }

        bool matchesCategory = true;
        if (!categoryFilter.isEmpty()) {
            QString cat = "General";
            if (u.descriptor.id.find("mtrail") != std::string::npos) cat = "Visuals";
            else if (u.descriptor.id.find("awake") != std::string::npos) cat = "Power";
            else if (u.descriptor.id.find("inputshield") != std::string::npos) cat = "Security";
            else if (u.descriptor.id.find("windowpin") != std::string::npos) cat = "Windows";
            else if (u.descriptor.id.find("heartbeat") != std::string::npos) cat = "Diagnostics";
            matchesCategory = (cat == categoryFilter);
        }

        bool visible = matchesSearch && matchesCategory;
        it->second->setVisible(visible);
        if (visible) visibleCount++;
    }

    if (visibleCount == 0 && !utilities.empty()) {
        m_emptyStateWidget->show();
    } else if (!utilities.empty()) {
        m_emptyStateWidget->hide();
    }
}

void DashboardPage::onUtilityStateChanged(const std::string& utilityId) {
    auto utilOpt = m_utilityManager->getManagedUtility(utilityId);
    if (utilOpt) {
        auto it = m_cardWidgets.find(utilityId);
        if (it != m_cardWidgets.end()) {
            it->second->updateState(*utilOpt);
        }
        updateMetrics();
        applyFilter();
    }
}

} // namespace utilist::app::ui
