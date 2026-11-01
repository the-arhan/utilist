#include "utility_card.hpp"
#include <QPainter>
#include <QPixmap>

namespace utilist::app::ui {

UtilityCard::UtilityCard(const ManagedUtility& utility, QWidget* parent)
    : QFrame(parent),
      m_utilityId(utility.descriptor.id) {
    setupUi();
    updateState(utility);
}

void UtilityCard::setupUi() {
    setObjectName("UtilityCard");
    setStyleSheet(
        "#UtilityCard {"
        "  background-color: #1e222b;"
        "  border: 1px solid #2d3340;"
        "  border-radius: 8px;"
        "  padding: 12px;"
        "}"
        "#UtilityCard:hover {"
        "  border: 1px solid #384252;"
        "}"
    );

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(14, 12, 14, 12);
    mainLayout->setSpacing(14);

    // Left: Icon Badge
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(44, 44);
    m_iconLabel->setStyleSheet(
        "background-color: #272c38;"
        "border: 1px solid #3d4554;"
        "border-radius: 6px;"
        "color: #00b4d8;"
        "font-size: 18px;"
        "font-weight: bold;"
    );
    m_iconLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_iconLabel);

    // Center: Information
    auto* centerLayout = new QVBoxLayout();
    centerLayout->setSpacing(4);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setStyleSheet("color: #f0f6fc; font-size: 15px; font-weight: bold;");
    titleRow->addWidget(m_nameLabel);

    m_categoryPill = new QLabel(this);
    m_categoryPill->setStyleSheet(
        "background-color: #1c2129;"
        "color: #58a6ff;"
        "border: 1px solid #1f3554;"
        "border-radius: 10px;"
        "padding: 2px 8px;"
        "font-size: 11px;"
        "font-weight: 500;"
    );
    titleRow->addWidget(m_categoryPill);

    m_statusPill = new QLabel(this);
    m_statusPill->setStyleSheet(
        "background-color: #21262d;"
        "color: #8b949e;"
        "border: 1px solid #30363d;"
        "border-radius: 10px;"
        "padding: 2px 8px;"
        "font-size: 11px;"
        "font-weight: bold;"
    );
    titleRow->addWidget(m_statusPill);
    titleRow->addStretch();
    centerLayout->addLayout(titleRow);

    m_descLabel = new QLabel(this);
    m_descLabel->setStyleSheet("color: #8b949e; font-size: 12px;");
    m_descLabel->setWordWrap(true);
    centerLayout->addWidget(m_descLabel);

    m_detailLabel = new QLabel(this);
    m_detailLabel->setStyleSheet("color: #6e7681; font-size: 11px;");
    centerLayout->addWidget(m_detailLabel);

    mainLayout->addLayout(centerLayout, 1);

    // Right: Controls
    auto* rightLayout = new QVBoxLayout();
    rightLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rightLayout->setSpacing(8);

    auto* topControls = new QHBoxLayout();
    topControls->setSpacing(10);

    m_shortcutBadge = new QLabel(this);
    m_shortcutBadge->setStyleSheet(
        "background-color: #161b22;"
        "color: #c9d1d9;"
        "border: 1px solid #30363d;"
        "border-radius: 4px;"
        "padding: 3px 8px;"
        "font-family: Consolas, monospace;"
        "font-size: 11px;"
    );
    topControls->addWidget(m_shortcutBadge);

    m_enableCheck = new QCheckBox("Enabled", this);
    m_enableCheck->setStyleSheet(
        "QCheckBox { color: #f0f6fc; font-size: 12px; font-weight: bold; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 1px solid #30363d; background: #161b22; }"
        "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }"
    );
    connect(m_enableCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit enableToggled(m_utilityId, checked);
    });
    topControls->addWidget(m_enableCheck);
    rightLayout->addLayout(topControls);

    auto* bottomControls = new QHBoxLayout();
    bottomControls->setSpacing(8);

    m_actionButton = new QPushButton("Activate", this);
    m_actionButton->setFixedHeight(28);
    m_actionButton->setCursor(Qt::PointingHandCursor);
    m_actionButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #238636;"
        "  color: white;"
        "  border: 1px solid #2ea043;"
        "  border-radius: 4px;"
        "  padding: 0 12px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #2ea043; }"
        "QPushButton:disabled { background-color: #21262d; border-color: #30363d; color: #484f58; }"
    );
    connect(m_actionButton, &QPushButton::clicked, this, [this]() {
        emit featureToggleClicked(m_utilityId);
    });
    bottomControls->addWidget(m_actionButton);

    m_processButton = new QPushButton("Start", this);
    m_processButton->setFixedHeight(28);
    m_processButton->setCursor(Qt::PointingHandCursor);
    m_processButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #21262d;"
        "  color: #c9d1d9;"
        "  border: 1px solid #30363d;"
        "  border-radius: 4px;"
        "  padding: 0 10px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #30363d; color: #f0f6fc; }"
    );
    connect(m_processButton, &QPushButton::clicked, this, [this]() {
        emit startStopClicked(m_utilityId);
    });
    bottomControls->addWidget(m_processButton);

    m_configureButton = new QPushButton("Settings", this);
    m_configureButton->setFixedHeight(28);
    m_configureButton->setCursor(Qt::PointingHandCursor);
    m_configureButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #21262d;"
        "  color: #c9d1d9;"
        "  border: 1px solid #30363d;"
        "  border-radius: 4px;"
        "  padding: 0 10px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #30363d; color: #f0f6fc; }"
    );
    connect(m_configureButton, &QPushButton::clicked, this, [this]() {
        emit configureClicked(m_utilityId);
    });
    bottomControls->addWidget(m_configureButton);

    rightLayout->addLayout(bottomControls);
    mainLayout->addLayout(rightLayout);
}

void UtilityCard::updateState(const ManagedUtility& utility) {
    m_nameLabel->setText(QString::fromStdString(utility.descriptor.name));
    m_descLabel->setText(QString::fromStdString(utility.descriptor.description));

    // Category
    QString cat = "General";
    if (utility.descriptor.id.find("mtrail") != std::string::npos) cat = "Visuals";
    else if (utility.descriptor.id.find("awake") != std::string::npos) cat = "Power";
    else if (utility.descriptor.id.find("inputshield") != std::string::npos) cat = "Security";
    else if (utility.descriptor.id.find("windowpin") != std::string::npos) cat = "Windows";
    else if (utility.descriptor.id.find("heartbeat") != std::string::npos) cat = "Diagnostics";
    m_categoryPill->setText(cat);

    // Glyph or Initial
    QString initial = utility.descriptor.name.empty() ? "U" : QString(utility.descriptor.name[0]).toUpper();
    m_iconLabel->setText(initial);

    // Shortcut
    if (!utility.configuredShortcut.empty()) {
        m_shortcutBadge->setText(QString::fromStdString(utility.configuredShortcut));
        m_shortcutBadge->show();
    } else {
        m_shortcutBadge->hide();
    }

    // Enable checkbox
    m_enableCheck->blockSignals(true);
    m_enableCheck->setChecked(utility.enabled);
    m_enableCheck->blockSignals(false);

    // Configure button visibility based on settings capability
    m_configureButton->setEnabled(utility.descriptor.hasCapability("settings") && utility.processState == ProcessState::Running);

    // Status Pill and Controls
    if (!utility.enabled) {
        m_statusPill->setText("DISABLED");
        m_statusPill->setStyleSheet(
            "background-color: #21262d; color: #6e7681; border: 1px solid #30363d; border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;"
        );
        m_actionButton->setEnabled(false);
        m_actionButton->setText("Disabled");
        m_processButton->setEnabled(false);
        m_processButton->setText("Disabled");
        m_processButton->setStyleSheet("QPushButton { background-color: #21262d; border-color: #30363d; color: #484f58; }");
        m_detailLabel->setText("Utility is disabled");
    } else {
        if (utility.processState == ProcessState::Running) {
            m_processButton->setEnabled(true);
            m_processButton->setText("Stop");
            m_processButton->setStyleSheet(
                "QPushButton { background-color: #3d1d24; color: #f85149; border: 1px solid #da3633; border-radius: 4px; padding: 0 10px; font-size: 12px; font-weight: bold; }"
                "QPushButton:hover { background-color: #da3633; color: white; }"
            );

            m_actionButton->setEnabled(true);
            if (utility.featureActive) {
                m_statusPill->setText("ACTIVE");
                m_statusPill->setStyleSheet(
                    "background-color: #0f3a47; color: #00b4d8; border: 1px solid #00b4d8; border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;"
                );
                m_actionButton->setText("Deactivate");
                m_actionButton->setStyleSheet(
                    "QPushButton { background-color: #9e6a03; color: white; border: 1px solid #bb8009; border-radius: 4px; padding: 0 12px; font-weight: bold; font-size: 12px; }"
                    "QPushButton:hover { background-color: #bb8009; }"
                );
            } else {
                m_statusPill->setText("RUNNING");
                m_statusPill->setStyleSheet(
                    "background-color: #123321; color: #3fb950; border: 1px solid #238636; border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;"
                );
                m_actionButton->setText("Activate");
                m_actionButton->setStyleSheet(
                    "QPushButton { background-color: #238636; color: white; border: 1px solid #2ea043; border-radius: 4px; padding: 0 12px; font-weight: bold; font-size: 12px; }"
                    "QPushButton:hover { background-color: #2ea043; }"
                );
            }
            m_detailLabel->setText(QString("PID: %1 • %2").arg(utility.processId).arg(QString::fromStdString(utility.stateDescription)));
        } else if (utility.processState == ProcessState::Starting) {
            m_statusPill->setText("STARTING");
            m_statusPill->setStyleSheet(
                "background-color: #3b2e10; color: #d29922; border: 1px solid #9e6a03; border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;"
            );
            m_actionButton->setEnabled(false);
            m_actionButton->setText("Starting...");
            m_processButton->setEnabled(false);
            m_processButton->setText("Starting...");
            m_detailLabel->setText("Initializing process and pipe connection...");
        } else if (utility.processState == ProcessState::Stopping) {
            m_statusPill->setText("STOPPING");
            m_statusPill->setStyleSheet(
                "background-color: #3b2e10; color: #d29922; border: 1px solid #9e6a03; border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;"
            );
            m_actionButton->setEnabled(false);
            m_actionButton->setText("Stopping...");
            m_processButton->setEnabled(false);
            m_processButton->setText("Stopping...");
            m_detailLabel->setText("Terminating process cleanly...");
        } else if (utility.processState == ProcessState::Error) {
            m_statusPill->setText("ERROR");
            m_statusPill->setStyleSheet(
                "background-color: #3d1d24; color: #f85149; border: 1px solid #da3633; border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;"
            );
            m_actionButton->setEnabled(false);
            m_actionButton->setText("Unavailable");
            m_processButton->setEnabled(true);
            m_processButton->setText("Retry Launch");
            m_processButton->setStyleSheet(
                "QPushButton { background-color: #1f3554; color: #58a6ff; border: 1px solid #388bfd; border-radius: 4px; padding: 0 10px; font-size: 12px; }"
                "QPushButton:hover { background-color: #388bfd; color: white; }"
            );
            m_detailLabel->setText(QString::fromStdString(utility.lastError.empty() ? "Process launch or runtime failure" : utility.lastError));
        } else {
            m_statusPill->setText("STOPPED");
            m_statusPill->setStyleSheet(
                "background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;"
            );
            m_actionButton->setEnabled(false);
            m_actionButton->setText("Dormant");
            m_actionButton->setStyleSheet(
                "QPushButton { background-color: #21262d; border-color: #30363d; color: #484f58; }"
            );
            m_processButton->setEnabled(true);
            m_processButton->setText("Launch");
            m_processButton->setStyleSheet(
                "QPushButton { background-color: #1f3554; color: #58a6ff; border: 1px solid #388bfd; border-radius: 4px; padding: 0 10px; font-size: 12px; }"
                "QPushButton:hover { background-color: #388bfd; color: white; }"
            );
            m_detailLabel->setText("Process is not running");
        }
    }
}

} // namespace utilist::app::ui
