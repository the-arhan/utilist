#include "shortcuts_page.hpp"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>

namespace utilist::app::ui {

ShortcutsPage::ShortcutsPage(core::UtilityManager* utilityManager,
                             core::ShortcutManager* shortcutManager,
                             QWidget* parent)
    : QWidget(parent),
      m_utilityManager(utilityManager),
      m_shortcutManager(shortcutManager) {
    setupUi();
    connect(m_utilityManager, &core::UtilityManager::utilitiesRefreshed, this, &ShortcutsPage::refresh);
    connect(m_utilityManager, &core::UtilityManager::utilityStateChanged, this, &ShortcutsPage::refresh);
    refresh();
}

void ShortcutsPage::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(20);

    auto* headerLayout = new QVBoxLayout();
    headerLayout->setSpacing(4);

    auto* titleLabel = new QLabel("Global Keyboard Shortcuts", this);
    titleLabel->setStyleSheet("color: #f0f6fc; font-size: 22px; font-weight: bold;");
    headerLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(
        "Configure global hotkeys to trigger utility actions even when UTILIST is running in the background.",
        this
    );
    subtitleLabel->setStyleSheet("color: #8b949e; font-size: 13px;");
    headerLayout->addWidget(subtitleLabel);

    rootLayout->addLayout(headerLayout);

    // Scroll Area
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background: transparent;");

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("background: transparent;");
    m_tableLayout = new QVBoxLayout(scrollContent);
    m_tableLayout->setContentsMargins(0, 0, 0, 0);
    m_tableLayout->setSpacing(12);

    scrollArea->setWidget(scrollContent);
    rootLayout->addWidget(scrollArea, 1);
}

void ShortcutsPage::refresh() {
    // Clear previous items
    QLayoutItem* item;
    while ((item = m_tableLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    auto utilities = m_utilityManager->getManagedUtilities();
    if (utilities.empty()) {
        auto* emptyLabel = new QLabel("No utilities installed to bind shortcuts.", this);
        emptyLabel->setStyleSheet("color: #8b949e; font-size: 14px; padding: 20px;");
        m_tableLayout->addWidget(emptyLabel);
        m_tableLayout->addStretch();
        return;
    }

    for (const auto& u : utilities) {
        auto* row = new QFrame(this);
        row->setStyleSheet(
            "background-color: #161b22;"
            "border: 1px solid #30363d;"
            "border-radius: 8px;"
            "padding: 14px;"
        );
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(14, 12, 14, 12);
        rowLayout->setSpacing(14);

        // Name and description
        auto* infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(2);
        auto* nameLabel = new QLabel(QString::fromStdString(u.descriptor.name), row);
        nameLabel->setStyleSheet("color: #f0f6fc; font-size: 14px; font-weight: bold;");
        infoLayout->addWidget(nameLabel);

        auto* idLabel = new QLabel("Default: " + QString::fromStdString(u.descriptor.defaultShortcut.empty() ? "None" : u.descriptor.defaultShortcut), row);
        idLabel->setStyleSheet("color: #8b949e; font-size: 11px;");
        infoLayout->addWidget(idLabel);
        rowLayout->addLayout(infoLayout, 1);

        // Shortcut editor line edit
        auto* edit = new QLineEdit(QString::fromStdString(u.configuredShortcut), row);
        edit->setPlaceholderText("e.g. Ctrl+Shift+H");
        edit->setFixedWidth(180);
        edit->setFixedHeight(32);
        edit->setStyleSheet(
            "QLineEdit {"
            "  background-color: #0d1117;"
            "  color: #f0f6fc;"
            "  border: 1px solid #30363d;"
            "  border-radius: 6px;"
            "  padding: 0 10px;"
            "  font-family: Consolas, monospace;"
            "  font-size: 12px;"
            "}"
            "QLineEdit:focus { border: 1px solid #00b4d8; }"
        );
        rowLayout->addWidget(edit);

        // Status badge
        auto shortcutOpt = m_shortcutManager->getShortcut(u.descriptor.id);
        auto* statusLabel = new QLabel(row);
        if (shortcutOpt && shortcutOpt->active) {
            statusLabel->setText("ACTIVE");
            statusLabel->setStyleSheet(
                "background-color: #123321; color: #3fb950; border: 1px solid #238636; border-radius: 4px; padding: 3px 8px; font-size: 10px; font-weight: bold;"
            );
        } else if (shortcutOpt && !shortcutOpt->lastError.empty()) {
            statusLabel->setText("FAILED");
            statusLabel->setToolTip(QString::fromStdString(shortcutOpt->lastError));
            statusLabel->setStyleSheet(
                "background-color: #3d1d24; color: #f85149; border: 1px solid #da3633; border-radius: 4px; padding: 3px 8px; font-size: 10px; font-weight: bold;"
            );
        } else {
            statusLabel->setText("UNBOUND");
            statusLabel->setStyleSheet(
                "background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 3px 8px; font-size: 10px; font-weight: bold;"
            );
        }
        rowLayout->addWidget(statusLabel);

        // Save Button
        auto* saveBtn = new QPushButton("Apply", row);
        saveBtn->setFixedHeight(32);
        saveBtn->setCursor(Qt::PointingHandCursor);
        saveBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #238636;"
            "  color: white;"
            "  border: 1px solid #2ea043;"
            "  border-radius: 6px;"
            "  padding: 0 14px;"
            "  font-weight: bold;"
            "  font-size: 12px;"
            "}"
            "QPushButton:hover { background-color: #2ea043; }"
        );
        std::string utilId = u.descriptor.id;
        connect(saveBtn, &QPushButton::clicked, this, [this, utilId, edit]() {
            m_utilityManager->setUtilityShortcut(utilId, edit->text().trimmed().toStdString());
        });
        rowLayout->addWidget(saveBtn);

        m_tableLayout->addWidget(row);
    }

    m_tableLayout->addStretch();
}

} // namespace utilist::app::ui
