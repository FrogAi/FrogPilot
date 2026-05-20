<div align="center" style="text-align: center;">

<h1>openpilot</h1>

<p>
  <b>openpilot is an operating system for robotics.</b>
  <br>
  Currently, it upgrades the driver assistance system in 300+ supported cars.
</p>

<h3>
  <a href="https://docs.comma.ai">Docs</a>
  <span> · </span>
  <a href="https://docs.comma.ai/contributing/roadmap/">Roadmap</a>
  <span> · </span>
  <a href="https://github.com/commaai/openpilot/blob/master/docs/CONTRIBUTING.md">Contribute</a>
  <span> · </span>
  <a href="https://discord.comma.ai">Community</a>
  <span> · </span>
  <a href="https://comma.ai/shop">Try it on a comma four</a>
</h3>

Quick start: `bash <(curl -fsSL openpilot.comma.ai)`

[![openpilot tests](https://github.com/commaai/openpilot/actions/workflows/tests.yaml/badge.svg)](https://github.com/commaai/openpilot/actions/workflows/tests.yaml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![X Follow](https://img.shields.io/twitter/follow/comma_ai)](https://x.com/comma_ai)
[![Discord](https://img.shields.io/discord/469524606043160576)](https://discord.comma.ai)

</div>

<table>
  <tr>
    <td><a href="https://youtu.be/NmBfgOanCyk" title="Video By Greer Viau"><img src="https://github.com/commaai/openpilot/assets/8762862/2f7112ae-f748-4f39-b617-fabd689c3772"></a></td>
    <td><a href="https://youtu.be/VHKyqZ7t8Gw" title="Video By Logan LeGrand"><img src="https://github.com/commaai/openpilot/assets/8762862/92351544-2833-40d7-9e0b-7ef7ae37ec4c"></a></td>
    <td><a href="https://youtu.be/SUIZYzxtMQs" title="A drive to Taco Bell"><img src="https://github.com/commaai/openpilot/assets/8762862/05ceefc5-2628-439c-a9b2-89ce77dc6f63"></a></td>
  </tr>
</table>


Using openpilot in a car
------

To use openpilot in a car, you need four things:
1. **Supported Device:** a comma four, available at [comma.ai/shop/comma-four](https://www.comma.ai/shop/comma-four).
2. **Software:** The setup procedure for the comma four allows users to enter a URL for custom software. Use the URL `openpilot.comma.ai` to install the release version.
3. **Supported Car:** Ensure that you have one of [the 300+ supported cars](docs/CARS.md).
4. **Car Harness:** You will also need a [car harness](https://comma.ai/shop/car-harness) to connect your comma four to your car.

We have detailed instructions for [how to install the harness and device in a car](https://comma.ai/setup). Note that it's possible to run openpilot on [other hardware](https://blog.comma.ai/self-driving-car-for-free/), although it's not plug-and-play.

------

<div align="center" style="text-align: center;">

<h1>FrogPilot 🐸</h1>

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/FrogAi/FrogPilot)
[![Discord](https://img.shields.io/discord/1137853399715549214?label=Discord)](https://discord.frogpilot.com)
[![Last Updated](https://img.shields.io/badge/Last%20Updated-October%2018th%2C%202025-brightgreen)](https://github.com/FrogAi/FrogPilot/releases/latest)
[![Wiki](https://img.shields.io/badge/Wiki-FrogPilot-blue?logo=wiki)](https://frogpilot.wiki.gg/)

</div>

------

**FrogPilot** is a custom, community-driven, frog-themed fork of openpilot that grows and improves through the ideas and contributions of its users. It offers exciting new features and cutting-edge experiments that often arrive long before official releases. As an unofficial and highly experimental version of openpilot, **FrogPilot** should *always* be used with caution!

openpilot vs **FrogPilot**
------

#### Community
| Feature | openpilot | **FrogPilot** |
|---------|:---------:|:---------:|
| A Welcoming Community | ❌ | ✅ |
| Erich / Primary Moderators / 🦇 | ✅ | ❌ |

#### Core Features
| Feature | openpilot | **FrogPilot** |
|---------|:---------:|:---------:|
| Always On Lateral (Steering) | ❌ | ✅ |
| Blind Spot Integration | ✅ | ✅ |
| Conditional Experimental Mode | ❌ | ✅ |
| Driving Model Selector | ❌ | ✅ |
| Speed Limit Support | ❌ | ✅ |

#### Device & Hardware
| Feature | openpilot | **FrogPilot** |
|---------|:---------:|:---------:|
| comma Pedal Support | ❌ | ✅ |
| High Quality Recordings | ❌ | ✅ |
| SDSU Support | ❌ | ✅ |
| Volume Controller | ❌ | ✅ |
| ZSS Support | ❌ | ✅ |

#### Gas/Brake
| Feature | openpilot | **FrogPilot** |
|---------|:---------:|:---------:|
| Adaptive Cruise Control (ACC) | ✅ | ✅ |
| Advanced Live Tuning | ❌ | ✅ |
| Custom Following Distances | ❌ | ✅ |
| Faster Human-Like Acceleration | ❌ | ✅ |
| Smoother Human-Like Braking | ❌ | ✅ |

#### Steering
| Feature | openpilot | **FrogPilot** |
|---------|:---------:|:---------:|
| Advanced Live Tuning | ❌ | ✅ |
| Automatic Lane Changes | ❌ | ✅ |
| Increased Steering Torque* | ❌ | ✅ |
| Lane Centering (LKAS) | ✅ | ✅ |
| Lane Change Assist | ✅ | ✅ |

#### User Interface
| Feature | openpilot | **FrogPilot** |
|---------|:---------:|:---------:|
| Custom Themes | ❌ | ✅ |
| Holiday Themes | ❌ | ✅ |
| Turn Signal Animations | ❌ | ✅ |

*Select vehicles only

And much much more!

🌟 Highlight Features
------

### 🚗 Always On Lateral (AOL)

With **"Always On Lateral"**, FrogPilot can keep steering assistance active even if you briefly press the accelerator or brake while cruise control is on. In practice, that means the car can keep helping you stay centered through curves, traffic, or rolling hills instead of dropping steering support the moment you adjust speed yourself.

---

### 🧠 Conditional Experimental Mode (CEM)

openpilot has different driving styles. **"Chill Mode"** is smoother and more predictable for normal cruising, while **["Experimental Mode"](https://blog.comma.ai/090release/#experimental-mode)** is more willing to slow for curves, react to stoplights and stop signs, and adapt to changing traffic. **"Conditional Experimental Mode"** automatically switches between the two, so you can stay in the calmer mode most of the time and only use the more advanced behavior when it is likely to help.

**"Conditional Experimental Mode"** switches into **"Experimental Mode"** when conditions like these are met:
- Approaching curves and turns
- Detecting slower or stopped lead vehicles
- Driving below a set speed
- Predicting an upcoming stop (e.g. stoplight or stop sign)

When the road is simple again, it switches back to **"Chill Mode"** for steadier and more predictable behavior.

**Note: Stay attentive. "Experimental Mode" is still an alpha feature, and mistakes are expected.**

---

### 🎭 Driving Personalities

With **"Driving Personalities"**, you can choose how closely and how quickly FrogPilot follows traffic with four adjustable profiles:

- **Traffic:** Built for stop-and-go traffic with smaller gaps and quicker reactions
- **Aggressive:** Tighter following and stronger responses
- **Standard:** Balanced everyday driving
- **Relaxed:** Smoother driving with more space to the car ahead

Each profile can also be fine-tuned for following distance, acceleration, and braking, so you can match FrogPilot's behavior to your own preferences. You can switch profiles using the following-distance button on the steering wheel, and enable **"Traffic Mode"** by holding that button.

---

### 📏 Speed Limit Controller (SLC)

With **"Speed Limit Controller"**, **FrogPilot** can adjust to the posted speed limit for you. It can use downloaded **["OpenStreetMap"](https://www.openstreetmap.org)** data, online **["Mapbox"](https://www.mapbox.com)** data, and your vehicle's own speed limit information when supported.

Offsets let you fine-tune how closely **FrogPilot** follows posted limits across different speed ranges, so you can cruise a little above or below the sign for a more natural feel. If no speed limit is available, you can choose whether **FrogPilot** stays at your set speed, falls back to the last known limit, or uses **"Experimental Mode"** to estimate one with the driving model.

Maps can be downloaded in settings and updated automatically on a schedule, helping keep your speed limit data up to date.

**Note: Speed limit data is not perfect. Always watch the road and confirm the correct speed yourself!**

---

### 🎨 Themes

With **"Themes"**, you can change how **FrogPilot** looks and sounds on the driving screen. You can mix and match:

- **Color Schemes**
- **Icon Packs**
- **Sound Packs**
- **Turn Signal Animations**
- **Steering Wheel Icons**

You can use built-in **FrogPilot** themes, seasonal holiday themes, or create your own with the **"Theme Maker"** and share them with the community! If you want something more playful, features like the Mario Kart-style **"Rainbow Path"** and **"Random Events"** add extra visual flair while you drive.

---

And lots more! From safety enhancements to personalization options, **FrogPilot** continues to evolve with features that put you in control. Check it out today for yourself!

---

🔧 Branches
------
| Branch                     | Install&nbsp;URL          | Description                                            | Recommended&nbsp;For     |
|----------------------------|---------------------------|--------------------------------------------------------|--------------------------|
| FrogPilot                  | frogpilot.download        | The main release branch.                               | Everyone                 |
| FrogPilot&#8209;Staging    | staging.frogpilot.download| Beta branch with upcoming features. Expect bugs!       | Early&nbsp;Adopters      |
| FrogPilot&#8209;Testing    | testing.frogpilot.download| Alpha branch with bleeding-edge features. Breaks often!| Advanced&nbsp;Testers    |
| FrogPilot&#8209;Development| No :)                     | Active development branch. Do not use!                 | **FrogPilot**&nbsp;Developers|
| MAKE&#8209;PRS&#8209;HERE  | No :)                     | Workspace for pull requests. Do not use!               | Contributors             |

🧰 How to Install
------

The easiest way to install **FrogPilot** is by entering this URL on the installation screen:

```
frogpilot.download
```

**DO NOT** install the **FrogPilot-Development** branch. I'm constantly breaking things on there, so unless you don't want to use **FrogPilot**, **NEVER** install it!

![](https://i.imgur.com/FsufQtO.png)

🐞 Bug Reports / Feature Requests
------

If you run into bugs, issues, or have ideas for new features, please post about it on the **[FrogPilot Discord](https://discord.gg/frogpilot)**! Feedback helps improve **FrogPilot** and create a better experience for everyone!

To report a bug, please post it in [**#bug-reports**](https://discord.com/channels/1137853399715549214/1162100167110053888).
To request a feature, please post it in [**#feature-requests**](https://discord.com/channels/1137853399715549214/1160318669839147259).

Please include as much detail as possible! Photos, videos, log files, or anything that can help explain the issue or idea are very helpful!

I'll do my best to respond promptly, but not every request can be addressed right away. Your feedback is always appreciated and helps make **FrogPilot** the best it can be!

📋 Credits
------

* [Aidenir](https://github.com/Aidenir)
* [AlexandreSato](https://github.com/AlexandreSato)
* [cfranyota](https://github.com/cfranyota)
* [cydia2020](https://github.com/cydia2020)
* [dragonpilot-community](https://github.com/dragonpilot-community)
* [ErichMoraga](https://github.com/ErichMoraga)
* [garrettpall](https://github.com/garrettpall)
* [jakethesnake420](https://github.com/jakethesnake420)
* [jyoung8607](https://github.com/jyoung8607)
* [mike8643](https://github.com/mike8643)
* [neokii](https://github.com/neokii)
* [OPGM](https://github.com/opgm)
* [OPKR](https://github.com/openpilotkr)
* [pfeiferj](https://github.com/pfeiferj)
* [realfast](https://github.com/realfast)
* [syncword](https://github.com/syncword)
* [twilsonco](https://github.com/twilsonco)

Star History
------

[![Star History Chart](https://api.star-history.com/svg?repos=FrogAi/FrogPilot&type=Date)](https://www.star-history.com/#FrogAi/FrogPilot&Date)
