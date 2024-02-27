[![openpilot on the comma 3X](https://i.imgur.com/6l2qbf5.png)](https://comma.ai/shop/comma-3x)

What is openpilot?
------

[openpilot](http://github.com/commaai/openpilot) is an open source driver assistance system. Currently, openpilot performs the functions of Adaptive Cruise Control (ACC), Automated Lane Centering (ALC), Forward Collision Warning (FCW), and Lane Departure Warning (LDW) for a growing variety of [supported car makes, models, and model years](docs/CARS.md). In addition, while openpilot is engaged, a camera-based Driver Monitoring (DM) feature alerts distracted and asleep drivers. See more about [the vehicle integration](docs/INTEGRATION.md) and [limitations](docs/LIMITATIONS.md).

<table>
  <tr>
    <td><a href="https://youtu.be/NmBfgOanCyk" title="Video By Greer Viau"><img src="https://github.com/commaai/openpilot/assets/8762862/2f7112ae-f748-4f39-b617-fabd689c3772"></a></td>
    <td><a href="https://youtu.be/VHKyqZ7t8Gw" title="Video By Logan LeGrand"><img src="https://github.com/commaai/openpilot/assets/8762862/92351544-2833-40d7-9e0b-7ef7ae37ec4c"></a></td>
    <td><a href="https://youtu.be/SUIZYzxtMQs" title="A drive to Taco Bell"><img src="https://github.com/commaai/openpilot/assets/8762862/05ceefc5-2628-439c-a9b2-89ce77dc6f63"></a></td>
  </tr>
</table>

What is FrogPilot? 🐸
------

FrogPilot is a fully open-sourced fork of openpilot, featuring clear and focused commits aimed at benefiting the openpilot community. It thrives on contributions from both users and developers, focusing on a collaborative, community-led approach to deliver an advanced openpilot experience for all!

------
FrogPilot was last updated on:

**March 15th, 2024**

Features
------

FrogPilot offers a wide range of customizable features that can be easily toggled on or off to suit your preferences. Whether you want a completely stock openpilot experience or want to add some fun and personal touches, FrogPilot has you covered! Some of the features include:

------
🎨 **Custom Themes:**

  - Themes included:
    - 🐸 Frog theme (with a bonus 🐐 sound effect)
    - <img src="https://images.emojiterra.com/google/noto-emoji/unicode-15/color/512px/1f1f7-1f1fa.png" width="20" height="18"> Russia / Joseph Stalin theme
    - 🔌 Tesla theme

  - 📢 Want to add a theme? Request one in the "feature-request" channel in the FrogPilot Discord!
------
🚀 **Conditional Experimental Mode:**

  - Auto-activates "Experimental Mode" under several conditions, including:
    - Approaching slower vehicles
    - Curve and stop light/stop sign detection
    - Driving below a set speed
    - Turn signal activation below 55mph for turn assistance
    - While using navigation to prepare for upcoming intersections and turns
------
🎮 **Custom UI:**

  - Road UI Customizations:
    - Blind spot path to indicate when and where a vehicle is in your blind spot
    - Compass that rotates according to the direction you're driving
    - Increase or decrease the lane line, path, and road edge widths
    - Path edge colors based on specific driving statuses
      - 🔵 Blue - Navigation active
      - 🟦 Light Blue - "Always On Lateral" active
      - 🟩 Light Green - Default
      - 🟠 Orange - Experimental Mode active
      - 🟡 Yellow - Conditional Experimental overridden
    - Steering wheel icons
      📢 Request your own steering wheel icon in the "feature-request" channel!
    - Steering wheel in the onroad UI rotates alongside your physical steering wheel
    - "Unlimited" road UI that extends out as far as the model can see
------
📊 **Developer UI:**

  - Display various driving logics and device state metrics
  - Lane detection measuring the width of the adjacent lanes
  - Tap the "CPU"/"GPU" gauge to toggle between CPU and GPU monitoring
  - Tap the "MEMORY" gauge to toggle between RAM and storage monitoring
------
🛠 **Device Behaviors:**

  - Adjustable screen brightness
  - Device can operate offline indefinitely
  - Disable uploads while onroad to help reduce data usage
  - Easy Panda firmware flashing via a "Flash Panda" button in the "Device" menu
  - Faster boot with prebuilt functionality
  - Set when the device will auto-shutdown to prevent battery drain
------
🚘 **Driving Behaviors:**

  - Adjust the max set speed in increments of 5 or more on short presses
  - Lateral Adjustments:
    - Activate lateral control by simply pressing the "Cruise Control" button
    - Lateral control won't disengage on gas or brake
    - Nudgeless lane changes with lane detection to prevent driving into curbs or going offroad
    - [Pfeiferj's curvature adjustment](https://github.com/commaai/openpilot/pull/28118) for smoother curve handling
    - Precise turns by using turn desires when below the minimum lane change speed
    - [Twilsonco's NNFF](https://github.com/twilsonco/openpilot) for improved steering controls
  - Longitudinal Adjustments:
    - Aggressive acceleration following a lead vehicle from a stop
    - Enhanced stopping distances and braking behaviors
    - "Map Turn Speed Controller" to slow down for curves based on stored map data
    - "Speed Limit Controller" to adjust your speed to the posted speed limit
      - With additional toggles to set offsets for the ranges of "0-34 mph", "35-54 mph", "55-64 mph", and "65-99 mph"
    - Sport and Eco acceleration modes
    - "Vision Turn Speed Controller" for smoother handling of curves
      - With additional toggles to fine tune it with your desired speed and curve detection sensitivity
  - Toggle Experimental Mode via the "Lane Departure Alert" button or by double tapping the screen
------
🚗 **Driving Personality Profiles:**

  - Ability to completely customize the following distance and jerk values for each profile
  - Adjustable via the "Distance" button on the steering wheel
    - Unsupported vehicle makes can use an Onroad UI button to switch between profiles with ease
------
⚡ **Model Switching:**

  - Select between various openpilot models to use your favorite model
------
🗺️ **Navigation:**

  - 3D buildings in the map panel
  - iOS shortcuts to use with setting navigation directions
  - Navigate on openpilot without a comma prime subscription
  - Offline maps
  - Open Street Maps integration for speed limit control and road name view
------
🚙 **Vehicle Specific Additions:**

  - GM Volt support
  - Honda Clarity support
  - Lock doors automatically when in the drive gear for Toyota/Lexus
  - openpilot longitudinal control for GM vehicles without ACC
  - Pedal interceptor support for GM vehicles
  - SNG hack for Toyota's without stop and go functionality
  - Toyota/TSS2 tuning for smooth driving
  - ZSS support for the Toyota Prius
------
🚦 **Other Quality of Life Features:**

  - Camera view selection to view your preferred camera (cosmetic only)
  - Driving statistics to show how many hours and miles you've driven on the home screen
  - Fleet Manager to easily access your driving logs and screen recordings
  - Green light alert
  - Hide the speed indicator by simply tapping on it
  - Numerical temperature gauge with the ability to use Fahrenheit by simply tapping on the gauge
  - Pause lateral control when turn signals are active
  - Retain tethering status between reboots
  - Screenrecorder
  - Sidebar displays by default for easy device monitoring
  - Silent Mode for a completely silent driving experience

How to Install
------

Easiest way to install FrogPilot is via this URL at the installation screen:

```
frogpilot.download
```

DO NOT install the "FrogPilot-Development" branch. I'm constantly breaking things on there so unless you don't want to use openpilot, NEVER install it!

![](https://i.imgur.com/wxKp3JI.png)

Bug reports / Feature Requests
------

If you encounter any issues or bugs while using FrogPilot, or if you have any suggestions for new features or improvements, please don't hesitate to reach out to me. I'm always looking for ways to improve the fork and provide a better experience for everyone!

To report a bug or request a new feature, feel free to make a post in the respective channel on the FrogPilot Discord. Provide as much detail as possible about the issue you're experiencing or the feature you'd like to see added. Photos, videos, log files, or other relevant information are very helpful!

I will do my best to respond to bug reports and feature requests in a timely manner, but please understand that I may not be able to address every request immediately. Your feedback and suggestions are valuable, and I appreciate your help in making FrogPilot the best it can be!

As for feature requests, these are my guidelines:

- Can I test it on my 2019 Lexus ES or are you up for testing it?
- How maintainable is it? Or will it frequently break with future openpilot updates?
- Is it not currently being developed by comma themselves? (i.e. Navigation)

Discord
------

[Join the FrogPilot Community Discord for easy access to updates, bug reporting, feature requests, future planned updates, and other FrogPilot related discussions!](https://discord.gg/frogpilot)

Credits
------

* [AlexandreSato](https://github.com/AlexandreSato)
* [Crwusiz](https://github.com/crwusiz)
* [DragonPilot](https://github.com/dragonpilot-community)
* [ErichMoraga](https://github.com/ErichMoraga)
* [Garrettpall](https://github.com/garrettpall)
* [Henrycc](https://github.com/henryccy)
* [KRKeegan](https://github.com/krkeegan)
* [Mike8643](https://github.com/mike8643)
* [Move-Fast](https://github.com/move-fast)
* [Neokii](https://github.com/Neokii)
* [OPGM](https://github.com/opgm)
* [OPKR](https://github.com/openpilotkr)
* [Pfeiferj](https://github.com/pfeiferj)
* [ServerDummy](https://github.com/ServerDummy)
* [Sunnyhaibin](https://github.com/sunnyhaibin)
* [Thinkpad4by3](https://github.com/Thinkpad4by3)
* [Twilsonco](https://github.com/twilsonco)

Licensing
------

openpilot is released under the MIT license. Some parts of the software are released under other licenses as specified.

Any user of this software shall indemnify and hold harmless Comma.ai, Inc. and its directors, officers, employees, agents, stockholders, affiliates, subcontractors and customers from and against all allegations, claims, actions, suits, demands, damages, liabilities, obligations, losses, settlements, judgments, costs and expenses (including without limitation attorneys’ fees and costs) which arise out of, relate to or result from any use of this software by user.

**THIS IS ALPHA QUALITY SOFTWARE FOR RESEARCH PURPOSES ONLY. THIS IS NOT A PRODUCT.
YOU ARE RESPONSIBLE FOR COMPLYING WITH LOCAL LAWS AND REGULATIONS.
NO WARRANTY EXPRESSED OR IMPLIED.**

---

<img src="https://d1qb2nb5cznatu.cloudfront.net/startups/i/1061157-bc7e9bf3b246ece7322e6ffe653f6af8-medium_jpg.jpg?buster=1458363130" width="75"></img> <img src="https://cdn-images-1.medium.com/max/1600/1*C87EjxGeMPrkTuVRVWVg4w.png" width="225"></img>

![openpilot tests](https://github.com/commaai/openpilot/actions/workflows/selfdrive_tests.yaml/badge.svg)
[![codecov](https://codecov.io/gh/commaai/openpilot/branch/master/graph/badge.svg)](https://codecov.io/gh/commaai/openpilot)
