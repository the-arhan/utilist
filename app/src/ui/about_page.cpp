#include "about_page.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>

namespace utilist::app::ui {

AboutPage::AboutPage(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void AboutPage::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(20);

    auto* titleLabel = new QLabel("About UTILIST", this);
    titleLabel->setStyleSheet("color: #f0f6fc; font-size: 22px; font-weight: bold;");
    rootLayout->addWidget(titleLabel);

    auto* card = new QFrame(this);
    card->setStyleSheet(
        "background-color: #161b22;"
        "border: 1px solid #30363d;"
        "border-radius: 8px;"
        "padding: 24px;"
    );
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(14);

    auto* appTitle = new QLabel("UTILIST Platform v1.0.0 (MVP)", card);
    appTitle->setStyleSheet("color: #00b4d8; font-size: 18px; font-weight: bold;");
    cardLayout->addWidget(appTitle);

    auto* desc = new QLabel(
        "UTILIST is a central Windows desktop utility manager designed for independently developed, "
        "native C++ utilities. Built on modern C++20 and Qt 6 Widgets with native Win32 integrations.",
        card
    );
    desc->setStyleSheet("color: #c9d1d9; font-size: 13px; line-height: 1.4;");
    desc->setWordWrap(true);
    cardLayout->addWidget(desc);

    auto* archHeader = new QLabel("Architecture Highlights:", card);
    archHeader->setStyleSheet("color: #f0f6fc; font-size: 14px; font-weight: bold; margin-top: 8px;");
    cardLayout->addWidget(archHeader);

    auto* points = new QLabel(
        "• Process Isolation: Child executables encapsulated in Windows Job Objects (JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE)\n"
        "• Local IPC: High-speed Windows Named Pipes with 4-byte length-prefixed framing and UTF-8 JSON payloads\n"
        "• Framework Freedom: Utilities run independently without linking against Qt or any heavy GUI runtime\n"
        "• Safe by Default: Principle of least privilege, strict path verification, and automatic crash diagnostics",
        card
    );
    points->setStyleSheet("color: #8b949e; font-size: 12px; font-family: Consolas; line-height: 1.5;");
    cardLayout->addWidget(points);

    auto* plannedHeader = new QLabel("Planned Ecosystem Utilities:", card);
    plannedHeader->setStyleSheet("color: #f0f6fc; font-size: 14px; font-weight: bold; margin-top: 8px;");
    cardLayout->addWidget(plannedHeader);

    auto* plannedPoints = new QLabel(
        "• MTrail: Smooth, customizable cursor trail effect\n"
        "• InputShield: Sensitive mouse/keyboard input protection with fail-safe recovery\n"
        "• Desktop Drift: Mini desktop car drifting across icons with pizza collectibles",
        card
    );
    plannedPoints->setStyleSheet("color: #8b949e; font-size: 12px; line-height: 1.4;");
    cardLayout->addWidget(plannedPoints);

    rootLayout->addWidget(card);
    rootLayout->addStretch();
}

} // namespace utilist::app::ui
