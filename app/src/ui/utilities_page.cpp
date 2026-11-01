#include "utilities_page.hpp"
#include <platform/windows_paths.hpp>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>

namespace utilist::app::ui {

UtilitiesPage::UtilitiesPage(core::UtilityManager* utilityManager, QWidget* parent)
    : QWidget(parent),
      m_utilityManager(utilityManager) {
    setupUi();
    connect(m_utilityManager, &core::UtilityManager::utilitiesRefreshed, this, &UtilitiesPage::refresh);
    refresh();
}

void UtilitiesPage::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(20);

    // Top action row
    auto* headerLayout = new QHBoxLayout();
    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    auto* titleLabel = new QLabel("Discovered Utilities", this);
    titleLabel->setStyleSheet("color: #f0f6fc; font-size: 22px; font-weight: bold;");
    titleLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel("View manifest schemas, capability signatures, and installation paths.", this);
    subtitleLabel->setStyleSheet("color: #8b949e; font-size: 13px;");
    titleLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(titleLayout);

    headerLayout->addStretch();

    auto* scanBtn = new QPushButton("Rescan", this);
    scanBtn->setFixedHeight(32);
    scanBtn->setCursor(Qt::PointingHandCursor);
    scanBtn->setStyleSheet(
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
    connect(scanBtn, &QPushButton::clicked, this, [this]() {
        m_utilityManager->refreshUtilities();
    });
    headerLayout->addWidget(scanBtn);

    auto* openFolderBtn = new QPushButton("Open Utilities Folder", this);
    openFolderBtn->setFixedHeight(32);
    openFolderBtn->setCursor(Qt::PointingHandCursor);
    openFolderBtn->setStyleSheet(
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
    connect(openFolderBtn, &QPushButton::clicked, this, []() {
        auto dir = platform::WindowsPaths::getUtilitiesDir();
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(dir.wstring())));
    });
    headerLayout->addWidget(openFolderBtn);

    rootLayout->addLayout(headerLayout);

    // Scroll Area
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background: transparent;");

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(scrollContent);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(12);

    scrollArea->setWidget(scrollContent);
    rootLayout->addWidget(scrollArea, 1);
}

void UtilitiesPage::refresh() {
    // Clear existing
    QLayoutItem* item;
    while ((item = m_listLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    auto utilities = m_utilityManager->getManagedUtilities();
    if (utilities.empty()) {
        auto* emptyLabel = new QLabel("No utilities registered yet.", this);
        emptyLabel->setStyleSheet("color: #8b949e; font-size: 14px; padding: 20px;");
        m_listLayout->addWidget(emptyLabel);
        m_listLayout->addStretch();
        return;
    }

    for (const auto& u : utilities) {
        auto* card = new QFrame(this);
        card->setStyleSheet(
            "background-color: #161b22;"
            "border: 1px solid #30363d;"
            "border-radius: 8px;"
            "padding: 14px;"
        );
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setSpacing(8);

        auto* topRow = new QHBoxLayout();
        auto* nameLabel = new QLabel(QString::fromStdString(u.descriptor.name), card);
        nameLabel->setStyleSheet("color: #f0f6fc; font-size: 15px; font-weight: bold;");
        topRow->addWidget(nameLabel);

        auto* verLabel = new QLabel("v" + QString::fromStdString(u.descriptor.version), card);
        verLabel->setStyleSheet("background: #21262d; color: #8b949e; border-radius: 4px; padding: 2px 6px; font-size: 11px;");
        topRow->addWidget(verLabel);

        auto* idLabel = new QLabel("ID: " + QString::fromStdString(u.descriptor.id), card);
        idLabel->setStyleSheet("color: #6e7681; font-family: Consolas; font-size: 11px;");
        topRow->addWidget(idLabel);
        topRow->addStretch();
        cardLayout->addLayout(topRow);

        // Path
        auto* pathLabel = new QLabel("Executable: " + QString::fromStdString(u.descriptor.getExecutableAbsolutePath()), card);
        pathLabel->setStyleSheet("color: #8b949e; font-size: 11px; font-family: Consolas;");
        cardLayout->addWidget(pathLabel);

        // Capabilities Row
        auto* capsRow = new QHBoxLayout();
        capsRow->setSpacing(6);
        auto* capsTitle = new QLabel("Capabilities:", card);
        capsTitle->setStyleSheet("color: #8b949e; font-size: 11px; font-weight: bold;");
        capsRow->addWidget(capsTitle);

        for (const auto& cap : u.descriptor.capabilities) {
            auto* capBadge = new QLabel(QString::fromStdString(cap), card);
            capBadge->setStyleSheet(
                "background-color: #0f3a47;"
                "color: #00b4d8;"
                "border: 1px solid #00b4d8;"
                "border-radius: 3px;"
                "padding: 1px 6px;"
                "font-size: 10px;"
                "font-weight: bold;"
            );
            capsRow->addWidget(capBadge);
        }
        capsRow->addStretch();
        cardLayout->addLayout(capsRow);

        m_listLayout->addWidget(card);
    }

    m_listLayout->addStretch();
}

} // namespace utilist::app::ui
