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

Sidebar::Sidebar(QWidget *parent) : QFrame(parent), onroad(false), flag_pressed(false), settings_pressed(false) {
  connect(this, &Sidebar::valueChanged, [=] { update(); });

  setAttribute(Qt::WA_OpaquePaintEvent);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(300);

  QObject::connect(uiState(), &UIState::uiUpdate, this, &Sidebar::updateState);

  pm = std::make_unique<PubMaster, const std::initializer_list<const char *>>({"userFlag"});

  // FrogPilot variables
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &Sidebar::updateTheme);
}

void Sidebar::updateTheme() {
  update_theme(frogpilotUIState());

  FrogPilotUIState &fs = *frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;
  QJsonObject &frogpilot_toggles = fs.frogpilot_toggles;

  isCPU = frogpilot_toggles.value("cpu_metrics").toBool();
  isFahrenheit = frogpilot_toggles.value("fahrenheit").toBool();
  isGPU = frogpilot_toggles.value("gpu_metrics").toBool();
  isIP = frogpilot_toggles.value("ip_metrics").toBool();
  isMemoryUsage = frogpilot_toggles.value("memory_metrics").toBool();
  isNumericalTemp = frogpilot_toggles.value("numerical_temp").toBool();
  isSidebarMetrics = frogpilot_toggles.value("sidebar_metrics").toBool();
  isStorageLeft = frogpilot_toggles.value("storage_left_metrics").toBool();
  isStorageUsed = frogpilot_toggles.value("storage_used_metrics").toBool();
  sidebar_color1 = frogpilot_scene.use_stock_colors ? good_color : frogpilot_scene.sidebar_color1;
  sidebar_color2 = frogpilot_scene.use_stock_colors ? good_color : frogpilot_scene.sidebar_color2;
  sidebar_color3 = frogpilot_scene.use_stock_colors ? good_color : frogpilot_scene.sidebar_color3;

  if (util::random_int(0, 100) == 100 && frogpilot_toggles.value("random_events").toBool()) {
    loadImage("../../frogpilot/assets/random_events/icons/button_home", home_img, home_gif, home_btn.size(), this);
  } else {
    loadImage("../../frogpilot/assets/active_theme/icons/button_home", home_img, home_gif, home_btn.size(), this);
  }
  loadImage("../../frogpilot/assets/active_theme/icons/button_flag", flag_img, flag_gif, home_btn.size(), this);
  loadImage("../../frogpilot/assets/active_theme/icons/button_settings", settings_img, settings_gif, settings_btn.size(), this, Qt::IgnoreAspectRatio);
}

void Sidebar::showEvent(QShowEvent *event) {
  updateTheme();
}

void Sidebar::mousePressEvent(QMouseEvent *event) {
  QPoint pos = event->pos();

  QRect cpuRect = {30, 496, 240, 126};
  QRect memoryRect = {30, 654, 240, 126};
  QRect tempRect = {30, 338, 240, 126};

  static int showChip = 0;
  static int showMemory = 0;
  static int showTemp = 0;

  if (cpuRect.contains(pos) && isSidebarMetrics) {
    showChip = (showChip + 1) % 3;

    isCPU = showChip == 1;
    isGPU = showChip == 2;

    params.putBool("ShowCPU", isCPU);
    params.putBool("ShowGPU", isGPU);
  } else if (memoryRect.contains(pos) && isSidebarMetrics) {
    showMemory = (showMemory + 1) % 4;

    isMemoryUsage = showMemory == 1;
    isStorageLeft = showMemory == 2;
    isStorageUsed = showMemory == 3;

    params.putBool("ShowMemoryUsage", isMemoryUsage);
    params.putBool("ShowStorageLeft", isStorageLeft);
    params.putBool("ShowStorageUsed", isStorageUsed);
  } else if (tempRect.contains(pos) && isSidebarMetrics) {
    showTemp = (showTemp + 1) % 3;

    isFahrenheit = showTemp == 2;
    isNumericalTemp = showTemp != 0;

    params.putBool("Fahrenheit", showTemp == 2);
    params.putBool("NumericalTemp", showTemp != 0);
  } else if (onroad && home_btn.contains(pos)) {
    flag_pressed = true;
  } else if (settings_btn.contains(pos)) {
    settings_pressed = true;
  }

  update();
  updateFrogPilotToggles();
}

void Sidebar::mouseReleaseEvent(QMouseEvent *event) {
  if (flag_pressed || settings_pressed) {
    flag_pressed = settings_pressed = false;
    update();
  }
  if (onroad && home_btn.contains(event->pos())) {
    MessageBuilder msg;
    msg.initEvent().initUserFlag();
    pm->send("userFlag", msg);
  } else if (settings_btn.contains(event->pos())) {
    emit openSettings();
  }
}

void Sidebar::offroadTransition(bool offroad) {
  onroad = !offroad;

  // FrogPilot variables
  if (onroad) {
    updateTheme();
  }

  update();
}

void Sidebar::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible()) return;

  auto &sm = *(s.sm);

  auto deviceState = sm["deviceState"].getDeviceState();
  setProperty("netType", network_type[deviceState.getNetworkType()]);
  int strength = (int)deviceState.getNetworkStrength();
  setProperty("netStrength", strength > 0 ? strength + 1 : 0);

  ItemStatus connectStatus;
  auto last_ping = deviceState.getLastAthenaPingTime();
  if (last_ping == 0) {
    connectStatus = ItemStatus{{tr("CONNECT"), tr("OFFLINE")}, warning_color};
  } else {
    connectStatus = nanos_since_boot() - last_ping < 80e9
                        ? ItemStatus{{tr("CONNECT"), tr("ONLINE")}, sidebar_color3}
                        : ItemStatus{{tr("CONNECT"), tr("ERROR")}, danger_color};
  }
  setProperty("connectStatus", QVariant::fromValue(connectStatus));

  int maxTempC = deviceState.getMaxTempC();
  QString max_temp = isFahrenheit ? QString::number(maxTempC * 9 / 5 + 32) + "°F" : QString::number(maxTempC) + "°C";
  ItemStatus tempStatus = {{tr("TEMP"), isNumericalTemp ? max_temp : tr("HIGH")}, danger_color};
  auto ts = deviceState.getThermalStatus();
  if (ts == cereal::DeviceState::ThermalStatus::GREEN) {
    tempStatus = {{tr("TEMP"), isNumericalTemp ? max_temp : tr("GOOD")}, sidebar_color1};
  } else if (ts == cereal::DeviceState::ThermalStatus::YELLOW) {
    tempStatus = {{tr("TEMP"), isNumericalTemp ? max_temp : tr("OK")}, warning_color};
  }
  setProperty("tempStatus", QVariant::fromValue(tempStatus));

  ItemStatus pandaStatus = {{tr("VEHICLE"), tr("ONLINE")}, sidebar_color2};
  if (s.scene.pandaType == cereal::PandaState::PandaType::UNKNOWN) {
    pandaStatus = {{tr("NO"), tr("PANDA")}, danger_color};
  } else if (s.scene.started && !sm["liveLocationKalman"].getLiveLocationKalman().getGpsOK()) {
    pandaStatus = {{tr("GPS"), tr("SEARCH")}, warning_color};
  }
  setProperty("pandaStatus", QVariant::fromValue(pandaStatus));

  // FrogPilot variables
  const SubMaster &frog_sm = *(fs.sm);
  const cereal::FrogPilotDeviceState::Reader &frogpilotDeviceState = frog_sm["frogpilotDeviceState"].getFrogpilotDeviceState();

  if (isCPU || isGPU) {
    capnp::List<int8_t>::Reader cpu_loads = deviceState.getCpuUsagePercent();
    int cpu_usage = cpu_loads.size() != 0 ? std::accumulate(cpu_loads.begin(), cpu_loads.end(), 0) / cpu_loads.size() : 0;
    int gpu_usage = deviceState.getGpuUsagePercent();
    int usage = isCPU ? cpu_usage : gpu_usage;

    QString chip_usage = QString::number(usage) + "%";

    ItemStatus chipStatus = {{isGPU ? tr("GPU") : tr("CPU"), chip_usage}, sidebar_color2};
    if (usage >= 85) {
      chipStatus = {{isGPU ? tr("GPU") : tr("CPU"), chip_usage}, danger_color};
    } else if (usage >= 70) {
      chipStatus = {{isGPU ? tr("GPU") : tr("CPU"), chip_usage}, warning_color};
    }
    setProperty("chipStatus", QVariant::fromValue(chipStatus));
  }

  if (isMemoryUsage || isStorageLeft || isStorageUsed) {
    int free_space = deviceState.getFreeSpacePercent();
    int memory_usage = deviceState.getMemoryUsagePercent();
    int storage_left = frogpilotDeviceState.getFreeSpace();
    int storage_used = frogpilotDeviceState.getUsedSpace();

    QString memory = QString::number(memory_usage) + "%";
    QString storage = QString::number(isStorageLeft ? storage_left : storage_used) + tr(" GB");

    if (isMemoryUsage) {
      ItemStatus memoryStatus = {{tr("MEMORY"), memory}, sidebar_color3};
      if (memory_usage >= 85) {
        memoryStatus = {{tr("MEMORY"), memory}, danger_color};
      } else if (memory_usage >= 70) {
        memoryStatus = {{tr("MEMORY"), memory}, warning_color};
      }
      setProperty("memoryStatus", QVariant::fromValue(memoryStatus));
    } else {
      ItemStatus storageStatus = {{isStorageLeft ? tr("LEFT") : tr("USED"), storage}, sidebar_color3};
      if (free_space < 25 && free_space >= 10) {
        storageStatus = {{isStorageLeft ? tr("LEFT") : tr("USED"), storage}, warning_color};
      } else if (10 > free_space) {
        storageStatus = {{isStorageLeft ? tr("LEFT") : tr("USED"), storage}, danger_color};
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
  if (settings_gif) {
    p.drawPixmap(settings_btn.x(), settings_btn.y(), settings_gif->currentPixmap());
  } else {
    p.drawPixmap(settings_btn.x(), settings_btn.y(), settings_img);
  }
  p.setOpacity(onroad && flag_pressed ? 0.65 : 1.0);
  if (onroad) {
    if (flag_gif) {
      p.drawPixmap(home_btn.x(), home_btn.y(), flag_gif->currentPixmap());
    } else {
      p.drawPixmap(home_btn.x(), home_btn.y(), flag_img);
    }
  } else {
    if (home_gif) {
      p.drawPixmap(home_btn.x(), home_btn.y(), home_gif->currentPixmap());
    } else {
      p.drawPixmap(home_btn.x(), home_btn.y(), home_img);
    }
  }
  p.setOpacity(1.0);

  // network
  int x = 58;
  const QColor gray(0x54, 0x54, 0x54);
  if (isIP) {
    p.drawText(QRect(x, 196, 225, 27), Qt::AlignLeft | Qt::AlignVCenter, frogpilotUIState()->wifi->getIp4Address());
    p.setFont(InterFont(30));
  } else {
    for (int i = 0; i < 5; ++i) {
      p.setBrush(i < net_strength ? Qt::white : gray);
      p.setFont(InterFont(35));
      p.drawEllipse(x, 196, 27, 27);
      x += 37;
    }
  }

  p.setPen(QColor(0xff, 0xff, 0xff));
  const QRect r = QRect(50, 247, 100, 50);
  p.drawText(r, Qt::AlignCenter, net_type);

  // metrics
  drawMetric(p, temp_status.first, temp_status.second, 338);
  if (isCPU || isGPU) {
    drawMetric(p, chip_status.first, chip_status.second, 496);
  } else {
    drawMetric(p, panda_status.first, panda_status.second, 496);
  }
  if (isMemoryUsage) {
    drawMetric(p, memory_status.first, memory_status.second, 654);
  } else if (isStorageLeft || isStorageUsed) {
    drawMetric(p, storage_status.first, storage_status.second, 654);
  } else {
    drawMetric(p, connect_status.first, connect_status.second, 654);
  }
}
