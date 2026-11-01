#pragma once

#include <QWidget>

namespace utilist::app::ui {

class AboutPage : public QWidget {
    Q_OBJECT

public:
    explicit AboutPage(QWidget* parent = nullptr);

private:
    void setupUi();
};

} // namespace utilist::app::ui
