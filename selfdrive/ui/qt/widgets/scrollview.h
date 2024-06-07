#pragma once

#include <QScrollArea>

class ScrollView : public QScrollArea {
  Q_OBJECT

public:
  explicit ScrollView(QWidget *w = nullptr, QWidget *parent = nullptr);

  // FrogPilot functions
  void restorePosition(int previousScrollPosition);

protected:
  void hideEvent(QHideEvent *e) override;
};
