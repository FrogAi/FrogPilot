#include "frogpilot/ui/qt/widgets/account_popup.h"

#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <QrCode.hpp>

#include "selfdrive/ui/qt/util.h"

using qrcodegen::QrCode;

FrogPilotAccountQRWidget::FrogPilotAccountQRWidget(const QString &purpose, QWidget* parent) : QWidget(parent), purpose(purpose) {
  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &FrogPilotAccountQRWidget::refresh);
  connect(uiState(), &UIState::uiUpdate, this, &FrogPilotAccountQRWidget::updateState);
}

void FrogPilotAccountQRWidget::showEvent(QShowEvent *event) {
  refresh();
  timer->start(4 * 60 * 1000);
  device()->setOffroadBrightness(100);
}

void FrogPilotAccountQRWidget::hideEvent(QHideEvent *event) {
  timer->stop();
  device()->setOffroadBrightness(BACKLIGHT_OFFROAD);
}

void FrogPilotAccountQRWidget::refresh() {
  frogpilotUIState()->requestAccountUrl(purpose);
  waiting = true;
}

void FrogPilotAccountQRWidget::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!waiting) {
    return;
  }

  const cereal::FrogPilotAccountState::Reader &frogpilotAccountState = (*fs.sm)["frogpilotAccountState"].getFrogpilotAccountState();
  if (frogpilotAccountState.getUrlRequestTime() < fs.account_url_request_time) {
    return;
  }

  waiting = false;

  this->updateQrCode(QString::fromStdString(frogpilotAccountState.getUrl()));
  update();
}

void FrogPilotAccountQRWidget::updateQrCode(const QString &text) {
  QrCode qr = QrCode::encodeText(text.toUtf8().data(), QrCode::Ecc::LOW);
  qint32 sz = qr.getSize();
  QImage im(sz, sz, QImage::Format_RGB32);

  QRgb black = qRgb(0, 0, 0);
  QRgb white = qRgb(255, 255, 255);
  for (int y = 0; y < sz; y++) {
    for (int x = 0; x < sz; x++) {
      im.setPixel(x, y, qr.getModule(x, y) ? black : white);
    }
  }

  // Integer division to prevent anti-aliasing
  int final_sz = ((width() / sz) - 1) * sz;
  img = QPixmap::fromImage(im.scaled(final_sz, final_sz, Qt::KeepAspectRatio), Qt::MonoOnly);
}

void FrogPilotAccountQRWidget::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  p.fillRect(rect(), Qt::white);

  QSize s = (size() - img.size()) / 2;
  p.drawPixmap(s.width(), s.height(), img);
}


FrogPilotAccountPopup::FrogPilotAccountPopup(const QString &purpose, QWidget *parent) : DialogBase(parent) {
  QHBoxLayout *hlayout = new QHBoxLayout(this);
  hlayout->setContentsMargins(0, 0, 0, 0);
  hlayout->setSpacing(0);

  setStyleSheet("FrogPilotAccountPopup { background-color: #E0E0E0; }");

  QString titleText;
  QString confirmStep;
  QString siteStep;
  if (purpose == "delete") {
    titleText = tr("Delete your FrogPilot data");
    siteStep = tr("Check what will be deleted on FrogPilot.com");
    confirmStep = tr("Confirm to delete it");
  } else {
    titleText = tr("Link this device to your FrogPilot.com account");
    siteStep = tr("Sign in to FrogPilot.com if asked");
    confirmStep = tr("Confirm to link this device");
  }

  // text
  QVBoxLayout *vlayout = new QVBoxLayout();
  vlayout->setContentsMargins(85, 70, 50, 70);
  vlayout->setSpacing(50);
  hlayout->addLayout(vlayout, 1);
  {
    QPushButton *close = new QPushButton(QIcon(":/icons/close.svg"), "", this);
    close->setIconSize(QSize(80, 80));
    close->setStyleSheet("border: none;");
    vlayout->addWidget(close, 0, Qt::AlignLeft);
    QObject::connect(close, &QPushButton::clicked, this, &QDialog::reject);

    vlayout->addSpacing(30);

    QLabel *title = new QLabel(titleText, this);
    title->setStyleSheet("font-size: 75px; color: black;");
    title->setWordWrap(true);
    vlayout->addWidget(title);

    QLabel *instructions = new QLabel(QString(R"(
      <ol type='1' style='margin-left: 15px;'>
        <li style='margin-bottom: 50px;'>%1</li>
        <li style='margin-bottom: 50px;'>%2</li>
        <li style='margin-bottom: 50px;'>%3</li>
      </ol>
    )").arg(tr("Scan the QR code on the right with your phone"))
    .arg(siteStep)
    .arg(confirmStep), this);

    instructions->setStyleSheet("font-size: 47px; font-weight: bold; color: black;");
    instructions->setWordWrap(true);
    vlayout->addWidget(instructions);

    vlayout->addStretch();
  }

  // QR code
  FrogPilotAccountQRWidget *qr = new FrogPilotAccountQRWidget(purpose, this);
  hlayout->addWidget(qr, 1);
}

int FrogPilotAccountPopup::exec() {
  if (!util::system_time_valid()) {
    ConfirmationDialog::alert(tr("Connect to the internet to set the clock first"), parentWidget());
    return QDialog::Rejected;
  }
  return DialogBase::exec();
}
