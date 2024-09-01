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
  home_label = new QLabel(this);
  settings_label = new QLabel(this);

  flagPngPath = "../frogpilot/assets/active_theme/icons/button_flag.png";
  homeGifPath = "../frogpilot/assets/active_theme/icons/button_home.gif";
  homePngPath = "../frogpilot/assets/active_theme/icons/button_home.png";
  settingsGifPath = "../frogpilot/assets/active_theme/icons/button_settings.gif";
  settingsPngPath = "../frogpilot/assets/active_theme/icons/button_settings.png";

  randomEventGifPath = "../frogpilot/assets/random_events/icons/button_home.gif";
}

void Sidebar::showEvent(QShowEvent *event) {
  updateIcon(home_label, home_gif, homeGifPath, home_btn, homePngPath, isHomeGif);
  updateIcon(settings_label, settings_gif, settingsGifPath, settings_btn, settingsPngPath, isSettingsGif);
}

void Sidebar::updateIcon(QLabel *&label, QMovie *&gif, const QString &gifPath, const QRect &btnRect, const QString &pngPath, bool &isGif) {
  QString selectedGifPath = gifPath;
  if (qrand() % 100 == 0 && btnRect == home_btn && isRandomEvents) {
    selectedGifPath = randomEventGifPath;
  }

  if (gif != nullptr) {
    if (!gif->fileName().isEmpty()) {
      gif->stop();
    }
    delete gif;
    gif = nullptr;
    label->hide();
  }

  if (QFile::exists(selectedGifPath)) {
    gif = new QMovie(selectedGifPath);

    if (!gif->fileName().isEmpty()) {
      gif->setScaledSize(btnRect.size());

      label->setGeometry(btnRect);
      label->setMovie(gif);
      label->show();

      gif->start();

      isGif = true;
    } else {
      delete gif;
      gif = nullptr;
      isGif = false;
    }
  } else {
    if (btnRect == home_btn) {
      home_img = loadPixmap(homePngPath, btnRect.size());
      flag_img = loadPixmap(flagPngPath, btnRect.size());
    } else {
      settings_img = loadPixmap(settingsPngPath, btnRect.size(), Qt::IgnoreAspectRatio);
    }

    isGif = false;
  }
}

void Sidebar::mousePressEvent(QMouseEvent *event) {
  UIState *s = uiState();
  UIScene &scene = s->scene;

  QPoint pos = event->pos();

  QRect cpuRect = {30, 496, 240, 126};
  QRect memoryRect = {30, 654, 240, 126};
  QRect tempRect = {30, 338, 240, 126};

  static int showChip = 0;
  static int showMemory = 0;
  static int showTemp = 0;

  if (cpuRect.contains(pos) && isSidebarMetrics) {
    showChip = (showChip + 1) % 3;

    isCPU = (showChip == 1);
    isGPU = (showChip == 2);

    scene.is_CPU = isCPU;
    scene.is_GPU = isGPU;

    params.putBoolNonBlocking("ShowCPU", isCPU);
    params.putBoolNonBlocking("ShowGPU", isGPU);

    update();
    return;
  } else if (memoryRect.contains(pos) && isSidebarMetrics) {
    showMemory = (showMemory + 1) % 4;

    isMemoryUsage = (showMemory == 1);
    isStorageLeft = (showMemory == 2);
    isStorageUsed = (showMemory == 3);

    scene.is_memory = isMemoryUsage;
    scene.is_storage_left = isStorageLeft;
    scene.is_storage_used = isStorageUsed;

    params.putBoolNonBlocking("ShowMemoryUsage", isMemoryUsage);
    params.putBoolNonBlocking("ShowStorageLeft", isStorageLeft);
    params.putBoolNonBlocking("ShowStorageUsed", isStorageUsed);

    update();
    return;
  } else if (tempRect.contains(pos) && isSidebarMetrics) {
    showTemp = (showTemp + 1) % 3;

    isFahrenheit = showTemp == 2;

    scene.fahrenheit = isFahrenheit;
    scene.numerical_temp = showTemp != 0;

    params.putBoolNonBlocking("Fahrenheit", showTemp == 2);
    params.putBoolNonBlocking("NumericalTemp", showTemp != 0);

    update();
    return;
  } else if (onroad && home_btn.contains(pos)) {
    flag_pressed = true;
    update();
    return;
  } else if (settings_btn.contains(pos)) {
    settings_pressed = true;
    update();
    return;
  }
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
  update();
}

void Sidebar::updateState(const UIState &s) {
  if (!isVisible()) {
    if (home_gif != nullptr) {
      home_gif->stop();
      delete home_gif;
      home_gif = nullptr;
      home_label->hide();
    }

    if (settings_gif != nullptr) {
      settings_gif->stop();
      delete settings_gif;
      settings_gif = nullptr;
      settings_label->hide();
    }

    return;
  }

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
                        ? ItemStatus{{tr("CONNECT"), tr("ONLINE")}, sidebar_color1}
                        : ItemStatus{{tr("CONNECT"), tr("ERROR")}, danger_color};
  }
  setProperty("connectStatus", QVariant::fromValue(connectStatus));

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
  const UIScene &scene = s.scene;

  isCPU = scene.is_CPU;
  isFahrenheit = scene.fahrenheit;
  isGPU = scene.is_GPU;
  isIP = scene.is_IP;
  isMemoryUsage = scene.is_memory;
  isNumericalTemp = scene.numerical_temp;
  isRandomEvents = scene.random_events;
  isStorageLeft = scene.is_storage_left;
  isStorageUsed = scene.is_storage_used;
  isSidebarMetrics = scene.sidebar_metrics;

  bool useStockColors = scene.use_stock_colors;
  sidebar_color1 = useStockColors ? good_color : scene.sidebar_color1;
  sidebar_color2 = useStockColors ? good_color : scene.sidebar_color2;
  sidebar_color3 = useStockColors ? good_color : scene.sidebar_color3;

  const cereal::FrogPilotDeviceState::Reader &frogpilotDeviceState = sm["frogpilotDeviceState"].getFrogpilotDeviceState();

  int maxTempC = deviceState.getMaxTempC();
  max_temp = isFahrenheit ? QString::number(maxTempC * 9 / 5 + 32) + "°F" : QString::number(maxTempC) + "°C";

  if (isCPU || isGPU) {
    auto cpu_loads = deviceState.getCpuUsagePercent();
    int cpu_usage = cpu_loads.size() != 0 ? std::accumulate(cpu_loads.begin(), cpu_loads.end(), 0) / cpu_loads.size() : 0;
    int gpu_usage = deviceState.getGpuUsagePercent();

    QString cpu = QString::number(cpu_usage) + "%";
    QString gpu = QString::number(gpu_usage) + "%";

    QString metric = isGPU ? gpu : cpu;
    int usage = isGPU ? gpu_usage : cpu_usage;

    ItemStatus cpuStatus = {{isGPU ? tr("GPU") : tr("CPU"), metric}, sidebar_color2};
    if (usage >= 85) {
      cpuStatus = {{isGPU ? tr("GPU") : tr("CPU"), metric}, danger_color};
    } else if (usage >= 70) {
      cpuStatus = {{isGPU ? tr("GPU") : tr("CPU"), metric}, warning_color};
    }
    setProperty("cpuStatus", QVariant::fromValue(cpuStatus));
  }

  if (isMemoryUsage || isStorageLeft || isStorageUsed) {
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
      if (25 > storage_left && storage_left >= 10) {
        storageStatus = {{isStorageLeft ? tr("LEFT") : tr("USED"), storage}, warning_color};
      } else if (10 > storage_left) {
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
  if (!isSettingsGif) {
    p.setOpacity(settings_pressed ? 0.65 : 1.0);
    p.drawPixmap(settings_btn.x(), settings_btn.y(), settings_img);
  }
  if (!isHomeGif) {
    p.setOpacity(onroad && flag_pressed ? 0.65 : 1.0);
    p.drawPixmap(home_btn.x(), home_btn.y(), onroad ? flag_img : home_img);
  }
  p.setOpacity(1.0);

  // network
  int x = 58;
  const QColor gray(0x54, 0x54, 0x54);
  p.setFont(InterFont(35));

  if (isIP) {
    p.setPen(QColor(0xff, 0xff, 0xff));
    p.save();
    p.setFont(InterFont(30));
    QRect ipBox = QRect(50, 196, 225, 27);
    p.drawText(ipBox, Qt::AlignLeft | Qt::AlignVCenter, uiState()->wifi->getIp4Address());
    p.restore();
  } else {
    for (int i = 0; i < 5; ++i) {
      p.setBrush(i < net_strength ? Qt::white : gray);
      p.drawEllipse(x, 196, 27, 27);
      x += 37;
    }
    p.setPen(QColor(0xff, 0xff, 0xff));
  }

  const QRect r = QRect(50, 247, 100, 50);
  p.drawText(r, Qt::AlignCenter, net_type);

  // metrics
  drawMetric(p, temp_status.first, temp_status.second, 338);

  if (isCPU || isGPU) {
    drawMetric(p, cpu_status.first, cpu_status.second, 496);
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
