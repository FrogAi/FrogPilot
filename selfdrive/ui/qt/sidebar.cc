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

Sidebar::Sidebar(QWidget *parent) : QFrame(parent), onroad(false), flag_pressed(false), settings_pressed(false), scene(uiState()->scene) {
  home_img = loadPixmap("../assets/images/button_home.png", home_btn.size());
  flag_img = loadPixmap("../assets/images/button_flag.png", home_btn.size());
  settings_img = loadPixmap("../assets/images/button_settings.png", settings_btn.size(), Qt::IgnoreAspectRatio);

  connect(this, &Sidebar::valueChanged, [=] { update(); });

  setAttribute(Qt::WA_OpaquePaintEvent);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(300);

  QObject::connect(uiState(), &UIState::uiUpdate, this, &Sidebar::updateState);

  pm = std::make_unique<PubMaster, const std::initializer_list<const char *>>({"userFlag"});

  // FrogPilot variables
  isCPU = params.getBool("ShowCPU");
  isGPU = params.getBool("ShowGPU");

  isMemoryUsage = params.getBool("ShowMemoryUsage");
  isStorageLeft = params.getBool("ShowStorageLeft");
  isStorageUsed = params.getBool("ShowStorageUsed");

  holidayThemeConfiguration = {
    {1, {"april_fools", {QColor(255, 165, 0)}}},
    {2, {"christmas", {QColor(0, 72, 255)}}},
    {3, {"cinco_de_mayo", {QColor(255, 0, 0)}}},
    {4, {"easter", {QColor(200, 150, 200)}}},
    {5, {"fourth_of_july", {QColor(0, 72, 255)}}},
    {6, {"halloween", {QColor(255, 0, 0)}}},
    {7, {"new_years_day", {QColor(23, 134, 68)}}},
    {8, {"st_patricks_day", {QColor(0, 128, 0)}}},
    {9, {"thanksgiving", {QColor(255, 0, 0)}}},
    {10, {"valentines_day", {QColor(23, 134, 68)}}},
    {11, {"world_frog_day", {QColor(0, 72, 255)}}},
  };

  for (auto &[key, themeData] : holidayThemeConfiguration) {
    QString &themeName = themeData.first;
    QString base = QString("../frogpilot/assets/holiday_themes/%1/images").arg(themeName);
    std::vector<QString> paths = {base + "/button_home.png", base + "/button_flag.png", base + "/button_settings.png"};

    home_imgs[key] = loadPixmap(paths[0], home_btn.size());
    flag_imgs[key] = loadPixmap(paths[1], home_btn.size());
    settings_imgs[key] = loadPixmap(paths[2], settings_btn.size(), Qt::IgnoreAspectRatio);
  }

  themeConfiguration = {
    {0, {"stock", {QColor(255, 255, 255)}}},
    {1, {"frog_theme", {QColor(23, 134, 68)}}},
    {2, {"tesla_theme", {QColor(0, 72, 255)}}},
    {3, {"stalin_theme", {QColor(255, 0, 0)}}}
  };

  for (auto &[key, themeData] : themeConfiguration) {
    QString &themeName = themeData.first;
    QString base = themeName == "stock" ? "../assets/images" : QString("../frogpilot/assets/custom_themes/%1/images").arg(themeName);
    std::vector<QString> paths = {base + "/button_home.png", base + "/button_flag.png", base + "/button_settings.png"};

    home_imgs[key] = loadPixmap(paths[0], home_btn.size());
    flag_imgs[key] = loadPixmap(paths[1], home_btn.size());
    settings_imgs[key] = loadPixmap(paths[2], settings_btn.size(), Qt::IgnoreAspectRatio);
  }

  home_img = home_imgs[scene.custom_icons];
  flag_img = flag_imgs[scene.custom_icons];
  settings_img = settings_imgs[scene.custom_icons];

  currentColors = themeConfiguration[scene.custom_colors].second;
}

void Sidebar::mousePressEvent(QMouseEvent *event) {
  // Declare the click boxes
  QRect cpuRect = {30, 496, 240, 126};
  QRect memoryRect = {30, 654, 240, 126};
  QRect tempRect = {30, 338, 240, 126};

  static int showChip = 0;
  static int showMemory = 0;
  static int showTemp = 0;

  // Swap between the respective metrics upon tap
  if (cpuRect.contains(event->pos())) {
    showChip = (showChip + 1) % 3;
    isCPU = (showChip == 1);
    isGPU = (showChip == 2);
    params.putBoolNonBlocking("ShowCPU", isCPU);
    params.putBoolNonBlocking("ShowGPU", isGPU);
    update();
  } else if (memoryRect.contains(event->pos())) {
    showMemory = (showMemory + 1) % 4;
    isMemoryUsage = (showMemory == 1);
    isStorageLeft = (showMemory == 2);
    isStorageUsed = (showMemory == 3);
    params.putBoolNonBlocking("ShowMemoryUsage", isMemoryUsage);
    params.putBoolNonBlocking("ShowStorageLeft", isStorageLeft);
    params.putBoolNonBlocking("ShowStorageUsed", isStorageUsed);
    update();
  } else if (tempRect.contains(event->pos())) {
    showTemp = (showTemp + 1) % 3;
    scene.fahrenheit = showTemp == 2;
    scene.numerical_temp = showTemp != 0;
    params.putBoolNonBlocking("Fahrenheit", showTemp == 2);
    params.putBoolNonBlocking("NumericalTemp", showTemp != 0);
    update();
  } else if (onroad && home_btn.contains(event->pos())) {
    flag_pressed = true;
    update();
  } else if (settings_btn.contains(event->pos())) {
    settings_pressed = true;
    update();
  }
}

void Sidebar::mouseReleaseEvent(QMouseEvent *event) {
  if (flag_pressed || settings_pressed) {
    flag_pressed = settings_pressed = false;
    update();
  }
  if (home_btn.contains(event->pos())) {
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
  if (!isVisible()) return;

  auto &sm = *(s.sm);

  auto deviceState = sm["deviceState"].getDeviceState();
  setProperty("netType", network_type[deviceState.getNetworkType()]);
  int strength = (int)deviceState.getNetworkStrength();
  setProperty("netStrength", strength > 0 ? strength + 1 : 0);

  // FrogPilot properties
  if (scene.current_holiday_theme != 0) {
    home_img = home_imgs[scene.current_holiday_theme];
    flag_img = flag_imgs[scene.current_holiday_theme];
    settings_img = settings_imgs[scene.current_holiday_theme];
    currentColors = holidayThemeConfiguration[scene.current_holiday_theme].second;
  } else {
    home_img = home_imgs[scene.custom_icons];
    flag_img = flag_imgs[scene.custom_icons];
    settings_img = settings_imgs[scene.custom_icons];
    currentColors = themeConfiguration[scene.custom_colors].second;
  }

  auto frogpilotDeviceState = sm["frogpilotDeviceState"].getFrogpilotDeviceState();

  bool isNumericalTemp = scene.numerical_temp;

  int maxTempC = deviceState.getMaxTempC();
  QString max_temp = scene.fahrenheit ? QString::number(maxTempC * 9 / 5 + 32) + "°F" : QString::number(maxTempC) + "°C";
  QColor theme_color = currentColors[0];

  // FrogPilot metrics
  if (isCPU || isGPU) {
    auto cpu_loads = deviceState.getCpuUsagePercent();
    int cpu_usage = std::accumulate(cpu_loads.begin(), cpu_loads.end(), 0) / cpu_loads.size();
    int gpu_usage = deviceState.getGpuUsagePercent();

    QString cpu = QString::number(cpu_usage) + "%";
    QString gpu = QString::number(gpu_usage) + "%";

    QString metric = isGPU ? gpu : cpu;
    int usage = isGPU ? gpu_usage : cpu_usage;

    ItemStatus cpuStatus = {{tr(isGPU ? "GPU" : "CPU"), metric}, theme_color};
    if (usage >= 85) {
      cpuStatus = {{tr(isGPU ? "GPU" : "CPU"), metric}, danger_color};
    } else if (usage >= 70) {
      cpuStatus = {{tr(isGPU ? "GPU" : "CPU"), metric}, warning_color};
    }
    setProperty("cpuStatus", QVariant::fromValue(cpuStatus));
  }

  if (isMemoryUsage || isStorageLeft || isStorageUsed) {
    int memory_usage = deviceState.getMemoryUsagePercent();
    int storage_left = frogpilotDeviceState.getFreeSpace();
    int storage_used = frogpilotDeviceState.getUsedSpace();

    QString memory = QString::number(memory_usage) + "%";
    QString storage = QString::number(isStorageLeft ? storage_left : storage_used) + " GB";

    if (isMemoryUsage) {
      ItemStatus memoryStatus = {{tr("MEMORY"), memory}, theme_color};
      if (memory_usage >= 85) {
        memoryStatus = {{tr("MEMORY"), memory}, danger_color};
      } else if (memory_usage >= 70) {
        memoryStatus = {{tr("MEMORY"), memory}, warning_color};
      }
      setProperty("memoryStatus", QVariant::fromValue(memoryStatus));
    } else {
      ItemStatus storageStatus = {{tr(isStorageLeft ? "LEFT" : "USED"), storage}, theme_color};
      if (10 <= storage_left && storage_left < 25) {
        storageStatus = {{tr(isStorageLeft ? "LEFT" : "USED"), storage}, warning_color};
      } else if (storage_left < 10) {
        storageStatus = {{tr(isStorageLeft ? "LEFT" : "USED"), storage}, danger_color};
      }
      setProperty("storageStatus", QVariant::fromValue(storageStatus));
    }
  }

  ItemStatus connectStatus;
  auto last_ping = deviceState.getLastAthenaPingTime();
  if (last_ping == 0) {
    connectStatus = ItemStatus{{tr("CONNECT"), tr("OFFLINE")}, warning_color};
  } else {
    connectStatus = nanos_since_boot() - last_ping < 80e9
                        ? ItemStatus{{tr("CONNECT"), tr("ONLINE")}, theme_color}
                        : ItemStatus{{tr("CONNECT"), tr("ERROR")}, danger_color};
  }
  setProperty("connectStatus", QVariant::fromValue(connectStatus));

  ItemStatus tempStatus = {{tr("TEMP"), isNumericalTemp ? max_temp : tr("HIGH")}, danger_color};
  auto ts = deviceState.getThermalStatus();
  if (ts == cereal::DeviceState::ThermalStatus::GREEN) {
    tempStatus = {{tr("TEMP"), isNumericalTemp ? max_temp : tr("GOOD")}, theme_color};
  } else if (ts == cereal::DeviceState::ThermalStatus::YELLOW) {
    tempStatus = {{tr("TEMP"), isNumericalTemp ? max_temp : tr("OK")}, warning_color};
  }
  setProperty("tempStatus", QVariant::fromValue(tempStatus));

  ItemStatus pandaStatus = {{tr("VEHICLE"), tr("ONLINE")}, theme_color};
  if (s.scene.pandaType == cereal::PandaState::PandaType::UNKNOWN) {
    pandaStatus = {{tr("NO"), tr("PANDA")}, danger_color};
  } else if (s.scene.started && !sm["liveLocationKalman"].getLiveLocationKalman().getGpsOK()) {
    pandaStatus = {{tr("GPS"), tr("SEARCH")}, warning_color};
  }
  setProperty("pandaStatus", QVariant::fromValue(pandaStatus));
}

void Sidebar::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setPen(Qt::NoPen);
  p.setRenderHint(QPainter::Antialiasing);

  p.fillRect(rect(), QColor(57, 57, 57));

  // buttons
  p.setOpacity(settings_pressed ? 0.65 : 1.0);
  p.drawPixmap(settings_btn.x(), settings_btn.y(), settings_img);
  p.setOpacity(onroad && flag_pressed ? 0.65 : 1.0);
  p.drawPixmap(home_btn.x(), home_btn.y(), onroad ? flag_img : home_img);
  p.setOpacity(1.0);

  // network
  int x = 58;
  const QColor gray(0x54, 0x54, 0x54);
  for (int i = 0; i < 5; ++i) {
    p.setBrush(i < net_strength ? Qt::white : gray);
    p.drawEllipse(x, 196, 27, 27);
    x += 37;
  }

  p.setFont(InterFont(35));
  p.setPen(QColor(0xff, 0xff, 0xff));
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
