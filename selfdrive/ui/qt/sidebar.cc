#include "selfdrive/ui/qt/sidebar.h"

#include <QMouseEvent>

#include "selfdrive/ui/qt/util.h"

void Sidebar::drawMetric(QPainter &p, const QPair<QString, QString> &label, QColor c, int y) {
  const QRect rect = {30, y, 240, 126};

  p.setPen(Qt::NoPen);
  p.setBrush(QBrush(c));
  p.setClipRect(rect.x() + 4, rect.y(), 18, rect.height(), Qt::ClipOperation::ReplaceClip);
  p.drawRoundedRect(QRect(rect.x() + 4, rect.y() + 4, 100, 118), 18, 18);
  p.setClipping(false);

  QPen pen = QPen(QColor(0xff, 0xff, 0xff, 0x55));
  pen.setWidth(2);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(rect, 20, 20);

  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setFont(InterFont(35, QFont::DemiBold));
  p.drawText(rect.adjusted(22, 0, 0, 0), Qt::AlignCenter, label.first + "\n" + label.second);
}

Sidebar::Sidebar(QWidget *parent) : QFrame(parent), onroad(false), flag_pressed(false), settings_pressed(false), mic_indicator_pressed(false) {
  home_img = loadPixmap("../assets/images/button_home.png", home_btn.size());
  flag_img = loadPixmap("../assets/images/button_flag.png", home_btn.size());
  settings_img = loadPixmap("../assets/images/button_settings.png", settings_btn.size(), Qt::IgnoreAspectRatio);
  mic_img = loadPixmap("../assets/icons/microphone.png", QSize(30, 30));
  link_img = loadPixmap("../assets/icons/link.png", QSize(60, 60));

  connect(this, &Sidebar::valueChanged, [=] { update(); });

  setAttribute(Qt::WA_OpaquePaintEvent);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(300);

  QObject::connect(uiState(), &UIState::uiUpdate, this, &Sidebar::updateState);

  pm = std::make_unique<PubMaster>(std::vector<const char*>{"bookmarkButton"});

  // FrogPilot variables
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &Sidebar::updateTheme);
}

void Sidebar::mousePressEvent(QMouseEvent *event) {
  // FrogPilot variables
  QPoint pos = event->pos();

  static constexpr QRect cpuRect = {30, 496, 240, 126};
  static constexpr QRect memoryRect = {30, 654, 240, 126};
  static constexpr QRect tempRect = {30, 338, 240, 126};

  static int showChip = 0;
  static int showMemory = 0;
  static int showTemp = 0;

  FrogPilotUIState *fs = frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs->frogpilot_scene;
  QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  if (cpuRect.contains(pos) && frogpilot_toggles.value("developer_ui").toBool()) {
    showChip = (showChip + 1) % 3;

    params.putBool("ShowCPU", showChip == 1);
    params.putBool("ShowGPU", showChip == 2);
  } else if (memoryRect.contains(pos) && frogpilot_toggles.value("developer_ui").toBool()) {
    showMemory = (showMemory + 1) % 4;

    params.putBool("ShowMemoryUsage", showMemory == 1);
    params.putBool("ShowStorageLeft", showMemory == 2);
    params.putBool("ShowStorageUsed", showMemory == 3);
  } else if (tempRect.contains(pos) && frogpilot_toggles.value("developer_ui").toBool()) {
    showTemp = (showTemp + 1) % 3;

    params.putBool("Fahrenheit", showTemp == 2);
    params.putBool("NumericalTemp", showTemp != 0);
  } else if (onroad && home_btn.contains(pos)) {
    flag_pressed = true;
  } else if (settings_btn.contains(pos)) {
    settings_pressed = true;
  } else if (recording_audio && mic_indicator_btn.contains(event->pos())) {
    mic_indicator_pressed = true;
  }

  if (!(flag_pressed || mic_indicator_pressed || settings_pressed)) {
    update();
    updateFrogPilotToggles();
  }
}

void Sidebar::mouseReleaseEvent(QMouseEvent *event) {
  if (flag_pressed || settings_pressed || mic_indicator_pressed) {
    flag_pressed = settings_pressed = mic_indicator_pressed = false;
    update();
  }
  if (onroad && home_btn.contains(event->pos())) {
    MessageBuilder msg;
    msg.initEvent().initBookmarkButton();
    pm->send("bookmarkButton", msg);
  } else if (settings_btn.contains(event->pos())) {
    emit openSettings();
  } else if (recording_audio && mic_indicator_btn.contains(event->pos())) {
    emit openSettings(2, "RecordAudio");
  }
}

void Sidebar::offroadTransition(bool offroad) {
  onroad = !offroad;
  updateHomeButton();
  update();
}

void Sidebar::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible()) return;

  // FrogPilot variables
  const FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;
  const QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  const SubMaster &fpsm = *(fs.sm);

  const cereal::FrogPilotDeviceState::Reader &frogpilotDeviceState = fpsm["frogpilotDeviceState"].getFrogpilotDeviceState();

  auto &sm = *(s.sm);

  networking = networking ? networking : window()->findChild<Networking *>("");
  bool tethering_on = networking && networking->wifi->tethering_on;
  auto deviceState = sm["deviceState"].getDeviceState();
  setProperty("netType", tethering_on ? "Hotspot": network_type[deviceState.getNetworkType()]);
  int strength = tethering_on ? 4 : (int)deviceState.getNetworkStrength();
  setProperty("netStrength", strength > 0 ? strength + 1 : 0);

  ItemStatus connectStatus;
  auto last_ping = deviceState.getLastAthenaPingTime();
  if (last_ping == 0) {
    connectStatus = ItemStatus{{tr("CONNECT"), tr("OFFLINE")}, warning_color};
  } else {
    connectStatus = nanos_since_boot() - last_ping < 80e9
                        ? ItemStatus{{tr("CONNECT"), tr("ONLINE")}, QColor(frogpilot_toggles.value("sidebar_color3").toString())}
                        : ItemStatus{{tr("CONNECT"), tr("ERROR")}, danger_color};
  }
  setProperty("connectStatus", QVariant::fromValue(connectStatus));

  int maxTempC = deviceState.getMaxTempC();
  QString max_temp = frogpilot_toggles.value("fahrenheit").toBool() ? QString::number(maxTempC * 9 / 5 + 32) + "°F" : QString::number(maxTempC) + "°C";
  ItemStatus tempStatus = {{tr("TEMP"), frogpilot_toggles.value("numerical_temp").toBool() ? max_temp : tr("HIGH")}, danger_color};
  auto ts = deviceState.getThermalStatus();
  if (ts == cereal::DeviceState::ThermalStatus::GREEN) {
    tempStatus = {{tr("TEMP"), frogpilot_toggles.value("numerical_temp").toBool() ? max_temp : tr("GOOD")}, QColor(frogpilot_toggles.value("sidebar_color1").toString())};
  } else if (ts == cereal::DeviceState::ThermalStatus::YELLOW) {
    tempStatus = {{tr("TEMP"), frogpilot_toggles.value("numerical_temp").toBool() ? max_temp : tr("OK")}, warning_color};
  }
  setProperty("tempStatus", QVariant::fromValue(tempStatus));

  ItemStatus pandaStatus = {{tr("VEHICLE"), tr("ONLINE")}, QColor(frogpilot_toggles.value("sidebar_color2").toString())};
  if (s.scene.pandaType == cereal::PandaState::PandaType::UNKNOWN) {
    pandaStatus = {{tr("NO"), tr("PANDA")}, danger_color};
  }
  setProperty("pandaStatus", QVariant::fromValue(pandaStatus));

  setProperty("recordingAudio", s.scene.recording_audio);

  // FrogPilot variables
  if (frogpilot_toggles.value("cpu_metrics").toBool() || frogpilot_toggles.value("gpu_metrics").toBool()) {
    capnp::List<int8_t>::Reader cpu_loads = deviceState.getCpuUsagePercent();
    int cpu_usage = cpu_loads.size() != 0 ? std::accumulate(cpu_loads.begin(), cpu_loads.end(), 0) / cpu_loads.size() : 0;
    int gpu_usage = deviceState.getGpuUsagePercent();
    int usage = frogpilot_toggles.value("cpu_metrics").toBool() ? cpu_usage : gpu_usage;

    QString chip_usage = QString::number(usage) + "%";

    ItemStatus chipStatus = {{frogpilot_toggles.value("gpu_metrics").toBool() ? tr("GPU") : tr("CPU"), chip_usage}, QColor(frogpilot_toggles.value("sidebar_color2").toString())};
    if (usage >= 85) {
      chipStatus = {{frogpilot_toggles.value("gpu_metrics").toBool() ? tr("GPU") : tr("CPU"), chip_usage}, danger_color};
    } else if (usage >= 70) {
      chipStatus = {{frogpilot_toggles.value("gpu_metrics").toBool() ? tr("GPU") : tr("CPU"), chip_usage}, warning_color};
    }
    setProperty("chipStatus", QVariant::fromValue(chipStatus));
  }

  if (frogpilot_toggles.value("memory_metrics").toBool() || frogpilot_toggles.value("storage_left_metrics").toBool() || frogpilot_toggles.value("storage_used_metrics").toBool()) {
    int free_space = deviceState.getFreeSpacePercent();
    int memory_usage = deviceState.getMemoryUsagePercent();
    int storage_left = frogpilotDeviceState.getFreeSpace();
    int storage_used = frogpilotDeviceState.getUsedSpace();

    QString memory = QString::number(memory_usage) + "%";
    QString storage = QString::number(frogpilot_toggles.value("storage_left_metrics").toBool() ? storage_left : storage_used) + tr(" GB");

    if (frogpilot_toggles.value("memory_metrics").toBool()) {
      ItemStatus memoryStatus = {{tr("MEMORY"), memory}, QColor(frogpilot_toggles.value("sidebar_color3").toString())};
      if (memory_usage >= 85) {
        memoryStatus = {{tr("MEMORY"), memory}, danger_color};
      } else if (memory_usage >= 70) {
        memoryStatus = {{tr("MEMORY"), memory}, warning_color};
      }
      setProperty("memoryStatus", QVariant::fromValue(memoryStatus));
    } else {
      ItemStatus storageStatus = {{frogpilot_toggles.value("storage_left_metrics").toBool() ? tr("LEFT") : tr("USED"), storage}, QColor(frogpilot_toggles.value("sidebar_color3").toString())};
      if (free_space < 25 && free_space >= 10) {
        storageStatus = {{frogpilot_toggles.value("storage_left_metrics").toBool() ? tr("LEFT") : tr("USED"), storage}, warning_color};
      } else if (10 > free_space) {
        storageStatus = {{frogpilot_toggles.value("storage_left_metrics").toBool() ? tr("LEFT") : tr("USED"), storage}, danger_color};
      }
      setProperty("storageStatus", QVariant::fromValue(storageStatus));
    }
  }
}

void Sidebar::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setPen(Qt::NoPen);
  p.setRenderHint(QPainter::Antialiasing);

  p.fillRect(rect(), QColor(57, 57, 57));

  // buttons
  p.setOpacity(settings_pressed ? 0.65 : 1.0);
  p.drawPixmap(settings_btn.x(), settings_btn.y(), settings_gif ? settings_gif->currentPixmap() : settings_img);
  p.setOpacity(onroad && flag_pressed ? 0.65 : 1.0);
  p.drawPixmap(home_btn.x(), home_btn.y(), onroad ? flag_gif ? flag_gif->currentPixmap() : flag_img : home_gif ? home_gif->currentPixmap() : home_img);
  if (recording_audio) {
    p.setBrush(danger_color);
    p.setOpacity(mic_indicator_pressed ? 0.65 : 1.0);
    p.drawRoundedRect(mic_indicator_btn, mic_indicator_btn.height() / 2, mic_indicator_btn.height() / 2);
    int icon_x = mic_indicator_btn.x() + (mic_indicator_btn.width() - mic_img.width()) / 2;
    int icon_y = mic_indicator_btn.y() + (mic_indicator_btn.height() - mic_img.height()) / 2;
    p.drawPixmap(icon_x, icon_y, mic_img);
  }
  p.setOpacity(1.0);

  // FrogPilot variables
  FrogPilotUIState *fs = frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs->frogpilot_scene;
  QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  // network
  if (frogpilot_toggles.value("ip_metrics").toBool()) {
    p.setPen(QColor(0xff, 0xff, 0xff));
    p.save();
    p.setFont(InterFont(30));
    p.drawText(QRect(50, 196, 225, 27), Qt::AlignLeft | Qt::AlignVCenter, frogpilotUIState()->wifi->getIp4Address());
    p.restore();
  } else {
    int x = 58;
    const QColor gray(0x54, 0x54, 0x54);
    for (int i = 0; i < 5; ++i) {
      p.setBrush(i < net_strength ? Qt::white : gray);
      p.drawEllipse(x, 196, 27, 27);
      x += 37;
    }
  }

  p.setFont(InterFont(35));
  p.setPen(QColor(0xff, 0xff, 0xff));
  const QRect r = QRect(58, 247, width() - 100, 50);

  if (net_type == "Hotspot") {
    p.drawPixmap(r.x(), r.y() + (r.height() - link_img.height()) / 2, link_img);
  } else {
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, net_type);
  }

  // metrics
  drawMetric(p, temp_status.first, temp_status.second, 338);
  if (frogpilot_toggles.value("cpu_metrics").toBool() || frogpilot_toggles.value("gpu_metrics").toBool()) {
    drawMetric(p, chip_status.first, chip_status.second, 496);
  } else {
    drawMetric(p, panda_status.first, panda_status.second, 496);
  }
  if (frogpilot_toggles.value("memory_metrics").toBool()) {
    drawMetric(p, memory_status.first, memory_status.second, 654);
  } else if (frogpilot_toggles.value("storage_left_metrics").toBool() || frogpilot_toggles.value("storage_used_metrics").toBool()) {
    drawMetric(p, storage_status.first, storage_status.second, 654);
  } else {
    drawMetric(p, connect_status.first, connect_status.second, 654);
  }
}

// FrogPilot variables
void Sidebar::showEvent(QShowEvent *event) {
  onroad = uiState()->scene.started;

  updateTheme();
}

void Sidebar::updateHomeButton() {
  if (onroad) {
    loadGif(QString(), home_gif, home_btn.size(), this);
    loadImage("../../frogpilot/assets/active_theme/icons/button_flag", flag_img, flag_gif, home_btn.size(), this);
  } else {
    loadGif(QString(), flag_gif, home_btn.size(), this);
    loadImage("../../frogpilot/assets/active_theme/icons/button_home", home_img, home_gif, home_btn.size(), this);
  }
}

void Sidebar::updateTheme() {
  loadGif(QString(), home_gif, home_btn.size(), this);
  loadGif(QString(), flag_gif, home_btn.size(), this);
  loadGif(QString(), settings_gif, settings_btn.size(), this);

  updateHomeButton();

  loadImage("../../frogpilot/assets/active_theme/icons/button_settings", settings_img, settings_gif, settings_btn.size(), this);
}
