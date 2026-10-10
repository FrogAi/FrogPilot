<div align="center" style="text-align: center;">

<h1>FrogPilot 🐸</h1>

[![Ask FrogBot](https://img.shields.io/badge/Ask-FrogBot-green)](https://frogpilot.com/frogbot/)
[![Blog](https://img.shields.io/badge/Blog-FrogPilot-green)](https://frogpilot.com/blog/)
[![Discord](https://img.shields.io/discord/1137853399715549214?label=Discord)](https://discord.frogpilot.com)
[![Last Updated](https://img.shields.io/badge/Last%20Updated-July%204th%2C%202026-brightgreen)](https://github.com/FrogAi/FrogPilot/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Wiki](https://img.shields.io/badge/Wiki-FrogPilot-blue?logo=wiki)](https://frogpilot.com/wiki/)

<a href="https://frogpilot.com"><img src="https://frogpilot.com/images/frogpilot-share-card.png" alt="FrogPilot, with the line “We may have gotten carried away with the frogs.” Its frog mascot stands beside a car with a FrogPilot license plate" width="800"></a>

</div>

**FrogPilot** is a bleeding-edge, highly customizable fork of [openpilot](#new-to-openpilot) for comma devices. It packs advanced features and cutting-edge experiments that often arrive long before official releases, and lets you tune how your car brakes, looks, sounds, speeds up and steers. As a highly experimental and unofficial version of openpilot, **FrogPilot** should *always* be used with caution!

<a id="new-to-openpilot"></a>

What is openpilot?
------

**openpilot** is free, open-source driver assistance software made by [comma](https://comma.ai) ([docs](https://docs.comma.ai)). It runs on a **comma device**, which mounts on your windshield and connects to your car through a **car harness**.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-how-it-works-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-how-it-works-medium.svg"><img src="https://frogpilot.com/images/frogpilot-how-it-works.svg" alt="A side view of a car with a small gadget, the comma device, mounted on the inside of the windshield near the rearview mirror. Its road cameras look forward and see lanes, cars, curves, stop signs and stop lights, and its driver camera looks back at the driver and checks you're paying attention. A short cable runs up from the device to the car harness, which plugs in behind the rearview mirror at the top of the windshield and brings in your car's data, such as its speed, and radar on some cars. A dotted line leads to a box showing what is inside the comma device, where it runs openpilot, comma's driving software, with FrogPilot's changes, its features and settings, on top. Through the car harness, your car steers and, on many cars, speeds up, brakes and stops for red lights and stop signs. If you stop paying attention, it issues an urgent takeover warning." width="800"></picture></p>

On [335+ supported cars](https://github.com/commaai/openpilot/blob/master/docs/CARS.md), it steers to keep you centered in your lane and, on many of them, manages your following distance and speed, while you stay alert and ready to take over.

**FrogPilot** is a *fork* of openpilot, meaning it's based on comma's openpilot and adds its own features and settings on top. It installs the same way and runs on the **comma 3**, **comma 3X** and **comma four**.

See openpilot in action:

<table>
  <tr>
    <td><a href="https://youtu.be/NmBfgOanCyk" title="Video By Greer Viau"><img src="https://github.com/commaai/openpilot/assets/8762862/2f7112ae-f748-4f39-b617-fabd689c3772"></a></td>
    <td><a href="https://youtu.be/VHKyqZ7t8Gw" title="Video By Logan LeGrand"><img src="https://github.com/commaai/openpilot/assets/8762862/92351544-2833-40d7-9e0b-7ef7ae37ec4c"></a></td>
    <td><a href="https://youtu.be/SUIZYzxtMQs" title="A drive to Taco Bell"><img src="https://github.com/commaai/openpilot/assets/8762862/05ceefc5-2628-439c-a9b2-89ce77dc6f63"></a></td>
  </tr>
</table>

✨ What FrogPilot adds
------

**comma four** uses a different interface without the **FrogPilot** menu or the **Share FrogPilot Data** control. The FrogPilot feature and privacy menu paths below do not apply to that interface.

Your comma device's **Settings → FrogPilot** menu holds these features and their options. **Tuning Level**, in that menu, controls how many settings you see, from **Minimal** through **Standard** and **Advanced** to **Developer**. The descriptions below note when a setting needs a higher tuning level or a supported car.

In the menu paths, **Manage** is the button beside the preceding setting. If the button is disabled, turn that setting on first. **Gas / Brake** only appears when openpilot controls your car's gas and brake.

### <picture><img src="https://frogpilot.com/images/frogpilot-icon-human-like-driving.svg" width="36" height="36" align="absmiddle" alt=""></picture> Acceleration and Deceleration Profiles

In **"Chill Mode"**, acceleration and deceleration profiles let you choose how briskly your car speeds up and how firmly it slows down.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-acceleration-profiles-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-acceleration-profiles-medium.svg"><img src="https://frogpilot.com/images/frogpilot-acceleration-profiles.svg" alt="Two panels compare FrogPilot's Acceleration Profile and Deceleration Profile choices. The left panel is a line graph of speed in mph over the first 15 seconds of speeding up from a stop. Sport+ is as quick as your car allows and reaches about 67 mph, Sport (the default) reaches about 51 mph, Standard about 41 mph and Eco about 38 mph. These are the quickest each profile allows, and your car may be slower. The right panel shows the hardest FrogPilot brakes with nobody ahead. Standard uses full braking, Eco (the default) brakes half as hard and Eco+ just coasts. Full braking returns for a car ahead or a curve to slow for." width="800"></picture></p>

**"Acceleration Profile"** sets how briskly it speeds up, with **"Eco"**, **"Standard"**, **"Sport"** (the default) and **"Sport+"** to choose from. Each profile sets an upper limit on acceleration, but your car may accelerate more slowly.

**"Deceleration Profile"** starts out on **"Eco"**, so with no car ahead it brakes at most half as hard as stock openpilot and slows gently for a lower set speed or speed limit. Switch it to **"Eco+"** and it just lets off the gas and coasts. Full braking returns for a car ahead or a curve to slow for. These profile limits do not apply in **"Experimental Mode"**, including when **"Conditional Experimental Mode"** switches it on.

Adjust these settings under **FrogPilot → Driving Controls → Gas / Brake → Acceleration and Braking → Manage**. **"Acceleration Profile"** is available at every tuning level, while **"Deceleration Profile"** requires the **Advanced** or **Developer** tuning level.

---

### <picture><img src="https://frogpilot.com/images/frogpilot-icon-always-on-lateral.svg" width="36" height="36" align="absmiddle" alt=""></picture> Always On Lateral (AOL)

On stock openpilot, canceling cruise or tapping the brake turns everything off, steering included, often right when you're slowing for a tight bend or traffic. With **"Always On Lateral"**, **FrogPilot** keeps steering for you the whole time your car's cruise control is switched on\*.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-always-on-lateral-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-always-on-lateral-medium.svg"><img src="https://frogpilot.com/images/frogpilot-always-on-lateral.svg" alt="On stock openpilot, tapping the brake turns off both steering and speed control until you resume cruise. With FrogPilot's Always On Lateral, tapping the brake turns off only speed control. Steering stays on, and resuming cruise brings speed control back." width="800"></picture></p>

Press cancel, tap the brake or work the pedals yourself, and it keeps holding your lane. Resuming cruise brings speed control back. Whether you're easing off for a curve, tapping the brake to let someone merge or crawling through stop-and-go traffic, the steering stays on, so you can stay relaxed instead of grabbing the wheel.

Turn it on under **FrogPilot → Driving Controls → Steering → Always On Lateral**. To customize its behavior, choose the **Standard** tuning level or higher and tap **Manage** beside the toggle.

\*On newer Genesis, Hyundai and Kia cars with a lane-keeping (LKAS) button on the steering wheel, that button turns it on instead of cruise control. Press it once after you start the car.

---

### <picture><img src="https://frogpilot.com/images/frogpilot-icon-conditional-experimental-mode.svg" width="36" height="36" align="absmiddle" alt=""></picture> Conditional Experimental Mode (CEM)

**"Conditional Experimental Mode"** switches between openpilot's two driving modes for you. It cruises in **"Chill Mode"** and hands over to **"Experimental Mode"** only while one of its triggers applies.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-conditional-experimental-mode-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-conditional-experimental-mode-medium.svg"><img src="https://frogpilot.com/images/frogpilot-conditional-experimental-mode.svg" alt="How Conditional Experimental Mode works. FrogPilot watches six things and checks each one. For curves ahead, it checks whether a curve is detected. For low speeds, whether you're driving below a speed you choose. For slower or stopped cars ahead, whether a slower or stopped car is detected. For stop signs and stop lights, whether a stop sign or stop light is detected. For turns, whether your turn signal is on below a speed you choose. For unknown speed limits, whether no speed limit is found. If any one is true, it uses Experimental Mode, where the driving model picks the speed, slows for curves and turns, stops at red lights and stop signs, and never goes above your set speed. If none are true, it uses Chill Mode, which holds your set speed and follows the car ahead." width="800"></picture></p>

**"Chill Mode"** is predictable and smooth for steady cruising. **["Experimental Mode"](https://blog.comma.ai/090release/#experimental-mode)** lets the driving model pick the speed itself, so it slows for curves and stops for red lights and stop signs, but it's less predictable on the open road. On stock openpilot, you switch between them by hand.

Out of the box, it hands over for stop signs, stop lights and turns. To choose its triggers, open **FrogPilot → Driving Controls → Gas / Brake → Conditional Experimental Mode → Manage** with your tuning level set to **Standard** or higher. The **Advanced** and **Developer** tuning levels show additional trigger settings. The unknown-speed-limit trigger is set separately under **FrogPilot → Driving Controls → Gas / Brake → Speed Limit Controller → Manage → Fallback Speed → Experimental Mode**.

Once no trigger applies, it drops back to **"Chill Mode"**. You get the calm of **"Chill Mode"** on the highway and the stop-for-lights smarts of **"Experimental Mode"** around town, with no button presses.

**Stay attentive. "Experimental Mode" is an alpha feature, and mistakes are expected!**

---

### <picture><img src="https://frogpilot.com/images/frogpilot-icon-curve-speed-controller.svg" width="36" height="36" align="absmiddle" alt=""></picture> Curve Speed Controller

In **"Chill Mode"**, stock openpilot doesn't slow down for curves ahead. **FrogPilot**'s **"Curve Speed Controller"** sees curves coming and slows down on its own, early and smoothly, so you're not braking hard into an off-ramp or a twisty back road.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-curve-speed-profiles-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-curve-speed-profiles-medium.svg"><img src="https://frogpilot.com/images/frogpilot-curve-speed-profiles.svg" alt="Line graph of speed through a curve in mph (0 to 80) against curve tightness, from tighter curves on the left to wider curves on the right, for FrogPilot's four Curve Speed Profiles. Sport is a red dashed line along the top that goes up to what your car's steering allows. Standard is a solid green line, a little brisker than Gentle. Gentle is a dotted blue line just below Standard, at a calm, unhurried pace. Adaptive, the default, is a light purple band with a purple edge, from below Gentle up to the Sport line. It starts like Standard and learns your pace from curves you drive yourself. For example, on a tight back-road bend, Gentle is 13 mph, Standard is 14 and Sport is up to 17. On a highway ramp, Gentle is 30, Standard is 32 and Sport is up to 39. On a sweeping highway bend, Gentle is 59, Standard is 63 and Sport is up to 77. It never goes above your set speed or past what your car's steering allows." width="800"></picture></p>

**"Adaptive"**, the default, starts like **"Standard"** and learns your pace from the curves you take yourself with cruise control off. **"Gentle"** takes curves at a calm, unhurried pace, **"Standard"** is a little brisker and **"Sport"** goes up to what your car's steering allows. None goes above your set speed or past what your car's steering allows.

Turn it on under **FrogPilot → Driving Controls → Gas / Brake → Curve Speed Controller** with your tuning level set to **Standard** or higher. To choose a different **"Curve Speed Profile"**, set your tuning level to **Advanced** or **Developer**, then tap **Manage** beside **"Curve Speed Controller"**.

---

### <picture><img src="https://frogpilot.com/images/frogpilot-icon-driving-personalities.svg" width="36" height="36" align="absmiddle" alt=""></picture> Driving Personalities

Stock openpilot's **"Aggressive"**, **"Standard"** and **"Relaxed"** profiles are fixed. Switch on **"Driving Personalities"** to tune how much space each one leaves behind the car ahead, so **"Aggressive"** keeps up with traffic the way you like and **"Relaxed"** really feels relaxed.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-following-distance-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-following-distance-medium.svg"><img src="https://frogpilot.com/images/frogpilot-following-distance.svg" alt="Three road lanes, one for each driving personality, show the default gap behind a car going 65 mph, drawn to scale. Aggressive leaves 1.25 s, about 139 ft. Standard leaves 1.45 s, about 158 ft. Relaxed leaves 1.75 s, about 187 ft. The gap is seconds × your speed + about 20 ft. For Standard at 65 mph (95 ft/s), 1.45 s × 95 ft/s + 20 ft ≈ 158 ft." width="800"></picture></p>

With the default settings at 65 mph, **"Aggressive"** leaves the least room, **"Standard"** a little more and **"Relaxed"** the most. Following distance is measured in seconds, so the gap grows as you go faster.

You can also tune how smoothly each one brakes and speeds up. With the default button assignment, you switch profiles with a tap of the following distance button on your steering wheel. On GM cars with adaptive cruise, the first tap opens the menu. Tap again within 3.5 seconds to change the profile.

To adjust a profile, set your tuning level to **Advanced** or **Developer**, then open **FrogPilot → Driving Controls → Gas / Brake → Driving Personalities → Manage** and tap **Manage** beside **Aggressive**, **Standard** or **Relaxed**. **Following Distance** is available at both the **Advanced** and **Developer** tuning levels. The other five settings below require **Developer**. Each profile has its own values. The percentages use stock openpilot's **"Standard"** profile as the 100% reference. For the smoothness and response ones, a higher value feels gentler and a lower one quicker but more abrupt.

| Setting | Range | What it changes |
|---------|-------|-----------------|
| **Acceleration Smoothness** | 25–200% | How quickly it adjusts its acceleration while speeding up or holding speed |
| **Braking Smoothness** | 25–200% | How quickly it adjusts its braking while slowing down |
| **Following Distance** | 1.00–3.00 seconds | How much room it leaves behind the car ahead. It's measured in seconds, so the gap grows as you go faster |
| **Safety Gap Bias** | 25–200% | How strongly it avoids getting too close to the car ahead. Higher means it brakes earlier and more firmly to keep its space |
| **Slowdown Response** | 25–200% | How gradually it eases into and out of slowing down |
| **Speed-Up Response** | 25–200% | How gradually it eases into and out of speeding up |

Stuck in stop-and-go? Hold the following distance button for 2.5 seconds to turn on **"Traffic Mode"**, which keeps tighter gaps and reacts sooner, so you're not leaving room for every car to cut in. Tighter gaps mean less cushion, so stay ready to brake.

---

### <picture><img src="https://frogpilot.com/images/frogpilot-icon-extra-alerts.svg" width="36" height="36" align="absmiddle" alt=""></picture> Extra Alerts

Some moments are easy to miss. **"Extra Alerts"** adds chimes stock openpilot doesn't have, plus a louder version of one it does.

**"Green Light Alert"** works when you're stopped at a light with no car ahead.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-alert-green-light-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-alert-green-light-medium.svg"><img src="https://frogpilot.com/images/frogpilot-alert-green-light.svg" alt="A stretch of road seen from above with your car stopped at the left and facing right, at a stop line with a traffic light, and there is no car ahead. A dashed arrow runs down the empty road with the words: the driving model predicts it's time to move. Under your car a yellow box says FrogPilot chimes, followed by the words: while your car is still stopped. Two notes at the bottom say: Green Light Alert may chime before the light changes. Check the light and road before moving." width="800"></picture></p>

It chimes when the driving model predicts it's time to move, rather than reading the light's color, so it may chime before the light changes.

**"Lead Departing Alert"** chimes after the car in front of you pulls away.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-alert-lead-departing-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-alert-lead-departing-medium.svg"><img src="https://frogpilot.com/images/frogpilot-alert-lead-departing.svg" alt="A stretch of road seen from above with your car stopped at the left and facing right, behind another car. The car ahead has only just started to move: it sits a little ahead of a dashed outline that marks where it was stopped, and an arrow runs down the road in front of it with the words: the car ahead starts to pull away. Under your car a yellow box says FrogPilot chimes, followed by the words: while your car is still stopped." width="800"></picture></p>

It chimes while you're still stopped. Treat both of these as a nudge to check the light and road before moving.

**"Louder Blind Spot Alert"** isn't a new alert.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-alert-blind-spot-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-alert-blind-spot-medium.svg"><img src="https://frogpilot.com/images/frogpilot-alert-blind-spot.svg" alt="A two-lane road seen from above with traffic heading right. Your car is in the top lane with its right turn signal blinking, and a dashed arrow curves from its front down into the bottom lane, with the words: you signal for a lane change while openpilot is steering. In the bottom lane another car sits beside your car and slightly behind it, with the words: a car is beside you, in your blind spot. Below the road a gray box says quiet prompt chime, and an arrow leads to a yellow box that says louder warning chime, followed by the words: only the chime changes. Two notes at the bottom say: openpilot already alerts you with Car Detected in Blindspot; this only swaps in the louder chime. Needs Lane Changes on, at least your Minimum Lane Change Speed, and a car with blind-spot monitoring." width="800"></picture></p>

openpilot already chimes "Car Detected in Blindspot" when you signal for a lane change while it's steering and a car is beside you; this swaps that quiet chime for openpilot's louder warning chime. It needs a car with blind-spot monitoring, **"Lane Changes"** turned on, and at least your **"Minimum Lane Change Speed"**.

**"Speed Limit Changed Alert"** chimes whenever the speed limit FrogPilot is reading changes, such as entering a school zone or coming off the highway.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-alert-speed-limit-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-alert-speed-limit-medium.svg"><img src="https://frogpilot.com/images/frogpilot-alert-speed-limit.svg" alt="A stretch of road seen from above with your car driving to the right. Behind it stands a 45 mph speed limit sign; just ahead of it a 25 mph sign stands at a dashed line across the road where the limit changes. An arrow runs down the road from that line with the words: the speed limit openpilot reads changes. Above the far end of the road are the words: for example, entering a school zone. Under your car a yellow box says FrogPilot chimes, followed by the words: right as the limit changes. A note at the bottom says: the limit comes from your dashboard, downloaded maps, signs the camera reads or Mapbox, depending on your setup." width="800"></picture></p>

It appears when **"Show Speed Limits"** or **"Speed Limit Filler"** is on, or when **"Speed Limit Controller"** is on and openpilot controls the gas and brake.

Turn them on under **FrogPilot → Alerts and Sounds → Manage → Extra Alerts → Manage**.

---

### <picture><img src="https://frogpilot.com/images/frogpilot-icon-human-like-driving.svg" width="36" height="36" align="absmiddle" alt=""></picture> Human-Like Driving

**FrogPilot** works the gas and brake more like a careful driver, out of the box. The comparisons below are simulations run with the real openpilot and FrogPilot code.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-human-like-following-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-human-like-following-medium.svg"><img src="https://frogpilot.com/images/frogpilot-human-like-following.svg" alt="Two line graphs of your speed compare FrogPilot's Human-Like Following with stock openpilot in simulations run with the real code. When the car ahead slows down, FrogPilot eases off early and brakes 22% softer, keeping 67 ft of room after 20 seconds against stock openpilot's 48 ft. When the car ahead pulls away from a stop, FrogPilot sets off right away and gets moving 1.3 seconds sooner, ending 63 ft behind after 10 seconds against stock openpilot's 90 ft." width="800"></picture></p>

With **"Human-Like Following"**, **FrogPilot** plans around where the driving model expects the car ahead to go, so when traffic starts to slow, it lets off the gas sooner and brakes more gently. In these simulated comparisons, FrogPilot's hardest braking is 22% softer than stock openpilot's, and it gets moving 1.3 seconds sooner when the car ahead pulls away from a stop.

After 20 seconds in the braking comparison, FrogPilot has 67 ft of room to the car ahead and stock openpilot has 48 ft. After 10 seconds in the pull-away comparison, FrogPilot is 63 ft behind and stock openpilot is 90 ft behind.

In **"Chill Mode"**, **"Human-Like Acceleration"** eases into your set speed instead of rushing up to it.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-human-like-acceleration-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-human-like-acceleration-medium.svg"><img src="https://frogpilot.com/images/frogpilot-human-like-acceleration.svg" alt="A line graph of your speed pulling away from a stop on a slow street, set to 20 mph, from simulations run with the real code. Stock openpilot uses the same push at any set speed. FrogPilot's Human-Like Acceleration pulls away more gently, with its hardest push 19% softer. The lower your set speed, the gentler it pulls away, and on highways it uses full power." width="800"></picture></p>

The lower your set speed, the gentler it pulls away. In the simulation with nobody ahead and a set speed of 20 mph, FrogPilot's hardest push is 19% softer than stock openpilot's. On cars with a fixed launch push, it also replaces that push with openpilot's planned acceleration in either driving mode.

Adjust **"Human-Like Acceleration"** and **"Human-Like Following"** under **FrogPilot → Driving Controls → Gas / Brake → Acceleration and Braking → Manage** with your tuning level set to **Advanced** or **Developer**.

---

### <picture><img src="https://frogpilot.com/images/frogpilot-icon-model-manager.svg" width="36" height="36" align="absmiddle" alt=""></picture> Model Manager

The driving model is the AI at the heart of openpilot. It decides how to slow down, speed up and steer, so it shapes how every drive feels.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-model-manager-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-model-manager-medium.svg"><img src="https://frogpilot.com/images/frogpilot-model-manager.svg" alt="Two rows, each with a label at the left. Stock openpilot has a single driving model, comma's model: one model, the one comma ships with each release. Model Manager has a range of models: Model A, highlighted as selected, Model B, Model C, and a new model that downloads on its own; you download the ones you want and switch with Select Driving Model. The model names are stand-ins, not real models." width="800"></picture></p>

Stock openpilot runs the model comma ships with each release. **FrogPilot**'s **"Model Manager"** lets you choose from a range of driving models instead. Open **FrogPilot → Driving Controls → Driving Model** to download the ones you want and switch between them with **"Select Driving Model"**. New models download on their own as they're released.

Not sure which one you'll like? Turn on the **"Model Randomizer"**.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-model-randomizer-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-model-randomizer-medium.svg"><img src="https://frogpilot.com/images/frogpilot-model-randomizer.svg" alt="The Model Randomizer, for when you're not sure which one you'll like, in four steps joined by arrows: it picks a different model for you at the start of every drive, asks how it went after drives longer than 15 minutes, saves your ratings to compare under Manage Model Ratings, and never picks models you add to Manage Model Blacklist." width="800"></picture></p>

It picks a different model at the start of every drive, asks how it went after drives longer than 15 minutes, and saves your ratings under **"Manage Model Ratings"** so you can compare them. Add models you don't like to **"Manage Model Blacklist"** so the randomizer never picks them.

---

### <picture><img src="https://frogpilot.com/images/frogpilot-icon-speed-limit-controller.svg" width="36" height="36" align="absmiddle" alt=""></picture> Speed Limit Controller (SLC)

Matching your set speed to every new speed limit means constant fiddling with the cruise buttons, and it's easy to miss a sign. With **"Speed Limit Controller"**, you set your cruise speed once and **FrogPilot** follows the posted limit plus an offset you choose, but never goes above your set speed.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-speed-limit-sources-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-speed-limit-sources-medium.svg"><img src="https://frogpilot.com/images/frogpilot-speed-limit-sources.svg" alt="How Speed Limit Controller works. In this example, downloaded OpenStreetMap maps (priority 1) say 45 mph while your car's dashboard and the camera (2 and 3) say 40, so it uses 45 mph from your first choice, adds your 5 mph offset and drives 50 mph, never above your set speed. Mapbox online data, with your own key, is used only when none of them has a limit." width="800"></picture></p>

In the example, the downloaded maps are first in your source priority order, so their 45 mph limit wins over the 40 mph reported by the dashboard and camera. Adding your 5 mph offset gives 50 mph, still capped by your set speed.

It reads speed limit signs with the road camera and, on supported Ford, Genesis, Hyundai, Kia, Lexus and Toyota models, your car's dashboard. It also uses maps from **["OpenStreetMap"](https://www.openstreetmap.org)** and, with your own **["Mapbox"](https://www.mapbox.com)** key, fills gaps with Mapbox's online data. To download maps, open **FrogPilot → Maps and Navigation → Speed Limit Maps** and pick your countries or states in **Map Sources**. Go back, then tap **Download** beside **Download Maps** while parked and online. They update automatically every month. Add your **Public Mapbox Key** under **FrogPilot → Maps and Navigation → Navigation** to make **Use Mapbox as Fallback** available.

To choose how it handles limits, open **FrogPilot → Driving Controls → Gas / Brake → Speed Limit Controller → Manage**. The settings below note any submenus and tuning-level requirements.

| Setting | What it does |
|---------|--------------|
| **"Confirm New Speed Limits"**<br>Under **Speed Limit Changes → Manage** | Asks before switching to a new limit, keeping the old one until you accept. Choose whether to confirm lower limits, higher limits or both. It won't ask for confirmation until you select an option. |
| **"Fallback Speed"** | What it does where no limit can be found. You choose between using your set speed, keeping the last limit and letting **"Experimental Mode"** pick the speed. Out of the box it keeps the last limit, even one from an earlier drive. Requires the **Standard** tuning level or higher |
| **"Higher Limit Lookahead Time"** and **"Lower Limit Lookahead Time"**<br>Under **Speed Limit Changes → Manage** | Lets it start speeding up or slowing down for a new limit before you reach it, instead of right where the limit changes. Only works with limits from your downloaded maps. Requires the **Advanced** tuning level or higher |
| **"Match Speed Limit on Engage"**<br>Under **Speed Limit Changes → Manage** | When you engage without resuming an earlier set speed, it starts at the speed limit plus your offset, not your current speed. It's only offered on cars where openpilot sets the cruise speed itself. Requires the **Standard** tuning level or higher |
| **"Override Speed"** | What happens after you press the gas past the limit. Out of the box it keeps the faster speed you reached, even if the limit drops, until you disengage or the limit rises to meet it. Requires the **Standard** tuning level or higher |
| **"Speed Limit Offsets"**<br>Tap **Manage** beside it | How far over or under the posted limit you drive, with a separate amount for each range of limits. Out of the box it drives 5 mph over on roads posted under 55 mph and 10 mph over on faster ones |
| **"Speed Limit Source Priority"** | Which source it goes by when more than one has a limit. You choose the first in the order you set, or always the highest or lowest limit reported. Requires the **Advanced** tuning level or higher |
| **"Use Mapbox as Fallback"**<br>Under **Speed Limit Changes → Manage** | When none of your chosen sources has a limit, it sends your location to Mapbox to look one up online. Needs your own Mapbox key and an internet connection. Requires the **Standard** tuning level or higher |

**Speed limits are only as accurate as the available speed limit data. Always stay attentive and adjust your speed when necessary!**

---

### <picture><img src="https://frogpilot.com/images/frogpilot-icon-themes.svg" width="36" height="36" align="absmiddle" alt=""></picture> Themes

Make your screen yours. With **"Custom Themes"**, download theme packs and mix and match their color schemes, icon packs, sound packs, steering wheel icons and turn signal animations. To choose your packs, open **FrogPilot → Theme and Appearance → Theme** and tap **Manage** beside **"Custom Themes"**. Turn on **"Holiday Themes"** in the **Theme** menu and **FrogPilot** dresses up on its own for thirteen holidays through the year. Build your own theme with the **"Theme Maker"** in **The Pond** and share it with the community. Open The Pond in a browser on the same Wi-Fi using the address shown under **FrogPilot → Maps and Navigation → Navigation → Manage Your Settings At**. For extra fun, the **Theme** menu also offers the Mario Kart–style **"Rainbow Path"** and **"Random Events"** for the occasional joke alert when your tuning level is **Standard** or higher. Neither one changes how it drives!

<p align="center"><img src="https://frogpilot.com/images/frogpilot-themes.jpg" alt="FrogPilot's driving screen with the Frog theme, which has green lane lines and path, a green settings button, a frog as the flag button and a frog in a chauffeur cap as the steering wheel icon." width="800"></p>

---

**FrogPilot is driver assistance, not self-driving. Stay attentive and ready to take over. "Conditional Experimental Mode", "Curve Speed Controller", driving personality tuning, Human-Like Driving, "Speed Limit Controller" and "Traffic Mode" only work on cars where openpilot controls the gas and brake.**

And lots more!

openpilot vs **FrogPilot**
------

**FrogPilot** keeps stock openpilot's core features and adds:

#### Alerts & Screen
| Feature | openpilot | **FrogPilot** |
|---------|:---------:|:---------:|
| Set your own volume for each alert (**"Alert Volumes"**) | ❌ | ✅ |
| Turns the lane beside you red on screen when a vehicle is in your blind spot, not just when you signal (**"Blind Spot Path"**)* | ❌ | ✅ |
| Saves the last minute of the driving screen as a video with one tap, or records it whenever you like (**"Capture Recent Footage"**, **"Screen Recorder"**) | ❌ | ✅ |
| Change colors, icons, sounds and turn signals, with automatic holiday themes (**"Custom Themes"**, **"Holiday Themes"**) | ❌ | ✅ |
| Chimes when the model predicts it's time to move, which can be before the light changes, or when the car in front pulls away (**"Green Light Alert"**, **"Lead Departing Alert"**) | ❌ | ✅ |
| Shows the posted speed limit on screen from maps, road signs or your car's dashboard, even on cars where openpilot doesn't control the gas and brake (**"Show Speed Limits"**) | ❌ | ✅ |
| Chimes when the speed limit changes (**"Speed Limit Changed Alert"**) | ❌ | ✅ |
| Turns the screen off while driving and wakes it for alerts (**"Standby Mode"**) | ❌ | ✅ |

<details>
<summary>Settings paths and tuning levels</summary>

- **Alert Volumes:** **FrogPilot → Alerts and Sounds → Manage → Alert Volumes → Manage**. Requires the **Advanced** tuning level or higher.
- **Blind Spot Path:** **FrogPilot → Theme and Appearance → Driving View → Driving Screen Widgets → Manage**. Requires the **Standard** tuning level or higher and a car with blind spot monitoring.
- **Capture Recent Footage**, **Screen Recorder** and **Standby Mode:** **FrogPilot → System Settings → Device / Screen → Screen Settings → Manage**. **Capture Recent Footage** and **Standby Mode** require the **Standard** tuning level or higher, while **Screen Recorder** requires **Advanced** or higher.
- **Custom Themes** and **Holiday Themes:** **FrogPilot → Theme and Appearance → Theme**. Tap **Manage** beside **Custom Themes** to choose its packs.
- **Green Light Alert**, **Lead Departing Alert**, **Louder Blind Spot Alert** and **Speed Limit Changed Alert:** **FrogPilot → Alerts and Sounds → Manage → Extra Alerts → Manage**. The speed-limit alert appears when **Show Speed Limits** or **Speed Limit Filler** is on, or when **Speed Limit Controller** is on and openpilot controls the gas and brake.
- **Show Speed Limits:** **FrogPilot → Theme and Appearance → Driving View → Navigation Widgets → Manage**. This toggle is hidden when **Speed Limit Controller** is on and openpilot controls the gas and brake, because the controller already displays the limit. Requires the **Standard** tuning level or higher.

</details>

#### Comfort
| Feature | openpilot | **FrogPilot** |
|---------|:---------:|:---------:|
| Automatic door locks on Lexus and Toyota cars, locking when you shift into gear and unlocking in park (**"Automatically Lock/Unlock Doors"**)* | ❌ | ✅ |
| Brakes earlier and more gently behind other cars and, in **"Chill Mode"**, brakes half as hard with no car ahead and eases into your set speed, all on by default (**"Deceleration Profile"**, **"Human-Like Acceleration"**, **"Human-Like Following"**)† | ❌ | ✅ |
| Tune each driving personality's acceleration, braking and following distance (**"Driving Personalities"**)† | ❌ | ✅ |
| Set how much farther back it stops behind the car ahead, up to 10 extra feet (**"Increase Stopped Distance by:"**)† | ❌ | ✅ |
| More steering strength* | ❌ | ✅ |
| Smoother steering, on by default, from a model trained on cars like yours or, where there isn't one, a lighter version that anticipates curves (**"Neural Network Feedforward (NNFF)"**, **"Neural Network Feedforward (NNFF) Lite"**)* | ❌ | ✅ |
| Stop-and-go on cars whose factory cruise control can't do it* | ❌ | ✅ |
| Set extra following distance and gentler acceleration and cornering for low visibility, rain and snow (**"Weather Condition Offsets"**)† | ❌ | ✅ |

<details>
<summary>Settings paths and tuning levels</summary>

- **Automatically Lock/Unlock Doors:** **FrogPilot → Vehicle Settings → Vehicle Settings → Toyota/Lexus Settings → Manage**.
- **Deceleration Profile**, **Human-Like Acceleration** and **Human-Like Following:** **FrogPilot → Driving Controls → Gas / Brake → Acceleration and Braking → Manage**. Requires the **Advanced** tuning level or higher.
- **Driving Personalities:** **FrogPilot → Driving Controls → Gas / Brake → Driving Personalities → Manage** (requires the **Advanced** tuning level or higher), then **Manage** beside **Aggressive**, **Standard** or **Relaxed**. **Following Distance** requires the **Advanced** tuning level or higher, while the other profile controls require **Developer**.
- **Increase Stopped Distance by:** **FrogPilot → Driving Controls → Gas / Brake → Quality of Life → Manage**. Requires the **Standard** tuning level or higher.
- **Neural Network Feedforward (NNFF)** and **Neural Network Feedforward (NNFF) Lite:** **FrogPilot → Driving Controls → Steering → Lateral Tuning → Manage**. Which one appears depends on your car and whether full NNFF is on. Requires the **Advanced** tuning level or higher.
- **Stop-and-go:** **FrogPilot → Vehicle Settings → Vehicle Settings**, then **Manage** beside your brand's settings. Look for **Stop-and-Go Hack** on supported Chevrolet Volt, Toyota and Lexus cars, or **Stop and Go** on supported Subarus. Requires the **Advanced** tuning level or higher.
- **Weather Condition Offsets:** **FrogPilot → Driving Controls → Gas / Brake → Quality of Life → Manage → Weather Condition Offsets → Manage**, then **Manage** beside **Low Visibility**, **Rain**, **Rainstorms** or **Snow**. Requires the **Advanced** tuning level or higher.

</details>

#### Driving
| Feature | openpilot | **FrogPilot** |
|---------|:---------:|:---------:|
| Keeps steering when you brake or cancel cruise (**"Always On Lateral"**) | ❌ | ✅ |
| Changes lanes with just the turn signal, no wheel nudge (**"Automatic Lane Changes"**) | ❌ | ✅ |
| Switches between **"Chill Mode"** and **"Experimental Mode"** on its own (**"Conditional Experimental Mode"**)† | ❌ | ✅ |
| Takes curves at the speed you like, learning from how you drive them (**"Curve Speed Controller"**)† | ❌ | ✅ |
| Choose what the following distance button does on a tap, a hold and a long hold, and what the LKAS button does, such as toggling **"Experimental Mode"** or **"Traffic Mode"**†, or pausing steering (**"Distance Button"**, **"LKAS Button"**) | ❌ | ✅ |
| Comes to a stop in **"Chill Mode"** when the driving model expects to stop and there's no car ahead (**Force Stop at "Detected" Stop Lights/Signs**)† | ❌ | ✅ |
| Pauses steering below a speed you choose, either always or only while your turn signal is on (**"Pause Steering Below"**) | ❌ | ✅ |
| Follows posted speed limits from maps, road signs or your car's dashboard (**"Speed Limit Controller"**)† | ❌ | ✅ |

<details>
<summary>Settings paths and tuning levels</summary>

- **Always On Lateral:** **FrogPilot → Driving Controls → Steering**. Tap **Manage** beside it for its options (requires the **Standard** tuning level or higher).
- **Automatic Lane Changes:** **FrogPilot → Driving Controls → Steering → Lane Changes → Manage**.
- **Conditional Experimental Mode**, **Curve Speed Controller** and **Speed Limit Controller:** **FrogPilot → Driving Controls → Gas / Brake**. Tap **Manage** beside the feature you want to adjust. CEM and CSC require the **Standard** tuning level or higher. SLC is available at every tuning level, with more options at higher levels.
- **Distance Button** and **LKAS Button:** **FrogPilot → Vehicle Settings → Wheel Buttons**. Requires the **Advanced** tuning level or higher.
- **Force Stop at "Detected" Stop Lights/Signs:** **FrogPilot → Driving Controls → Gas / Brake → Quality of Life → Manage**. Requires the **Advanced** tuning level or higher.
- **Pause Steering Below:** **FrogPilot → Driving Controls → Steering → Quality of Life → Manage**. Requires the **Standard** tuning level or higher.

</details>

#### Models & Device
| Feature | openpilot | **FrogPilot** |
|---------|:---------:|:---------:|
| Fine-tune steering by hand (**"Advanced Lateral Tuning"**) | ❌ | ✅ |
| Fine-tune gas and brake by hand (**"Advanced Longitudinal Tuning"**)† | ❌ | ✅ |
| Runs on the older **comma 3** | ❌ | ✅ |
| Works with add-on hardware some owners install (**comma Pedal**, **SDSU** and **ZSS**)* | ❌ | ✅ |
| Automatic backups of your FrogPilot install and settings, ready to restore (**"FrogPilot Backups"**, **"Settings Backups"**) | ❌ | ✅ |
| High-quality recordings when uploads are off (**"High-Quality Recording"**) | ❌ | ✅ |
| No forced internet check-ins (stock openpilot won't start after 27 hours of driving and 84 drives without checking for updates) | ❌ | ✅ |
| Choose your driving model (**"Select Driving Model"**), or let the **"Model Randomizer"** pick one and rate it after your drives | ❌ | ✅ |
| Shows only as many settings as you want, from a short **"Minimal"** list to every option under **"Developer"**, and starts at a level that fits how much you've driven with openpilot (**"Tuning Level"**) | ❌ | ✅ |

<details>
<summary>Settings paths and tuning levels</summary>

- **Advanced Lateral Tuning:** **FrogPilot → Driving Controls → Steering → Advanced Lateral Tuning → Manage**. Requires the **Developer** tuning level.
- **Advanced Longitudinal Tuning:** **FrogPilot → Driving Controls → Gas / Brake → Advanced Longitudinal Tuning → Manage**. Requires the **Developer** tuning level.
- **comma Pedal**, **SDSU** and **ZSS:** supported hardware is detected automatically. Check **FrogPilot → Vehicle Settings → Vehicle Settings → Vehicle Info → View → 3rd Party Hardware Detected**. This shows which hardware was detected and isn't a toggle.
- **FrogPilot Backups** and **Settings Backups:** **FrogPilot → System Settings → Data**.
- **High-Quality Recording:** **FrogPilot → System Settings → Device / Screen → Device Settings → Manage**. It appears when **Disable Uploads** is on and **Disable Onroad Only** is off. These options are hidden on **FrogPilot-Development** and **FrogPilot-Vetting**. Requires the **Advanced** tuning level or higher where available.
- **Select Driving Model** and **Model Randomizer:** **FrogPilot → Driving Controls → Driving Model**.
- **Tuning Level:** directly under **FrogPilot → Tuning Level**.

</details>

\*Select vehicles only<br>
†Cars where openpilot controls the gas and brake

And much, much more!

<a id="-how-to-install"></a>

📥 How to install
------

You need three things:

1. **A supported car.** Check [the list of 335+ supported cars](https://github.com/commaai/openpilot/blob/master/docs/CARS.md). Its **ACC** (adaptive cruise control) column shows who controls the gas and brake. On cars marked **openpilot**, openpilot does. On cars marked **openpilot available**, the experimental option is **Settings → Developer → openpilot Longitudinal Control (Alpha)**. It is only shown on supported cars when you select the **Developer** tuning level under **FrogPilot → Tuning Level**. On cars marked **Stock**, your car's own cruise control does, and openpilot steers.
2. **A comma device.** **FrogPilot** runs on the **comma 3**, **comma 3X** and **comma four**. The **comma four** is available at [comma.ai/shop/comma-four](https://www.comma.ai/shop/comma-four). **FrogPilot** also runs on the [**Konik A1M**](https://konik.ai/shop/konik-a1/), a third-party device that comma doesn't make or support.
3. **A car harness** for your car, which connects the device to it. comma lets you pick your car's harness when you buy a **comma four**, or you can buy one at [comma.ai/shop/car-harness](https://comma.ai/shop/car-harness).

Then:

1. Connect the harness and mount the device by following [comma's setup guide](https://comma.ai/setup) (written for the **comma four**).
2. When the device asks what software to install, choose **Custom Software** and enter the URL below.
   ```
   frogpilot.download
   ```
3. Go for a drive!

To install a different branch, enter its install URL instead (see below).

🔀 Branches and updates
------

A branch is a version of **FrogPilot** you can install. They differ in how new and how tested they are.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-release-cycle-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-release-cycle-medium.svg"><img src="https://frogpilot.com/images/frogpilot-release-cycle.svg" alt="FrogPilot's release cycle, drawn as one big gray ring that runs clockwise and stands for comma's openpilot release cycle. At the top of the ring, comma starts testing its next openpilot version. Right after that, FrogPilot-Staging moves to the openpilot version comma is testing, with FrogPilot's features. At the bottom of the ring, comma releases the new openpilot version, usually after one to three weeks of testing. Right after that, FrogPilot (main branch) gets the new openpilot version the Saturday after comma releases it. In the middle, FrogPilot-Testing is its own orange loop, updated constantly. It gets new features as soon as they're finished, and new features move up to FrogPilot-Staging." width="800"></picture></p>

**FrogPilot**'s releases follow comma's openpilot release cycle. **FrogPilot-Testing** gets new features as soon as they're finished, and those features move up to **FrogPilot-Staging**.

| Branch                     | Install&nbsp;URL          | Description | Recommended&nbsp;For     |
|----------------------------|---------------------------|-------------|--------------------------|
| FrogPilot                  | frogpilot.download        | The release branch. Updated the Saturday after comma's official openpilot release, once it's been through Staging. | Everyone                 |
| FrogPilot&#8209;Staging    | staging.frogpilot.download| FrogPilot on comma's upcoming openpilot release, updated as soon as comma starts testing it. Usually gets one to three weeks of testing before it becomes the release. | Early&nbsp;Adopters      |
| FrogPilot&#8209;Testing    | testing.frogpilot.download| The newest features, as soon as they're finished. The least tested, so expect bugs! | Advanced&nbsp;Testers    |
| MAKE&#8209;PRS&#8209;HERE  | No :)                     | Workspace for pull requests. Do not install! | Contributors             |

⚠️ Safety
------

**FrogPilot** is driver assistance (adaptive cruise control and lane centering), not self-driving. The driver must stay alert and be ready to take over at all times.

openpilot's safety model is described in comma's [SAFETY.md](https://github.com/commaai/openpilot/blob/master/docs/SAFETY.md). **FrogPilot** modifies openpilot's safety code, for example to allow higher steering limits on some cars, and to let **"Always On Lateral"** keep steering after you press the brake.

❓ Common questions
------

<details>
<summary><b>Is FrogPilot free?</b></summary>

Yes. **FrogPilot** is free and open source under the MIT license. The hardware isn't. You'll need a comma device and a car harness for your car.

</details>

<details>
<summary><b>Does it drive the car by itself?</b></summary>

No. It steers for you, and on many cars it also brakes and speeds up, but you're always the driver. Keep your eyes on the road and be ready to take over. The device's driver camera checks that you're paying attention and alerts you if you aren't.

</details>

<details>
<summary><b>Will it work with my car?</b></summary>

Check [the supported cars list](https://github.com/commaai/openpilot/blob/master/docs/CARS.md). Some **FrogPilot** features also need openpilot to control your car's gas and brake. They're marked with † in the tables above.

</details>

<details>
<summary><b>Which branch should I install?</b></summary>

If you're new, start with the main **FrogPilot** branch. It's the most tested. The other branches get new features sooner, with more bugs.

</details>

<details>
<summary><b>Can I switch branches or go back to stock openpilot later?</b></summary>

Yes. **FrogPilot** is software on your comma device, so you can reinstall it with another branch's install URL, or reinstall stock openpilot on a device it supports, whenever you like.

</details>

<details>
<summary><b>What data does it share?</b></summary>

Like stock openpilot, it uploads your drives to comma by default (or to [Konik](https://konik.ai/), a separate company, on builds that aren't fully tested yet). **FrogPilot**'s own data sharing is off until you agree to it. Crash reports, device registration and a check-in each time the device starts and after each drive are always on. The **User Data** section at the bottom has the details.

</details>

💬 Bug reports, feature requests and help
------

If you run into a bug or have an idea for a new feature, please post it on the **[FrogPilot Discord](https://discord.frogpilot.com)**! Feedback helps improve **FrogPilot** and create a better experience for everyone!

To report a bug, please post it in [**#bug-reports**](https://discord.com/channels/1137853399715549214/1162100167110053888).  
To request a feature, please post it in [**#feature-requests**](https://discord.com/channels/1137853399715549214/1160318669839147259).  

Please include which branch you're on, your car and your device, and as much detail as possible! Log files, photos and videos that show the issue or idea are very helpful!

I'll do my best to respond promptly, but not every request can be addressed right away. Your feedback is always appreciated and helps make **FrogPilot** the best it can be!

🤝 Contributing
------

Pull requests are welcome! Please open them against the [**MAKE-PRS-HERE**](https://github.com/FrogAi/FrogPilot/tree/MAKE-PRS-HERE) branch, since pull requests to any other branch are closed automatically.

GitHub Issues are turned off for this repository, so please post bug reports and feature requests in the **FrogPilot Discord** channels above.

🙏 Special Thanks
------

<table>
<tr><td><a href="https://github.com/AlexandreSato">AlexandreSato</a></td><td><a href="https://github.com/bigboigahoy">bigboigahoy</a></td><td><a href="https://github.com/cfranyota">cfranyota</a></td><td><a href="https://github.com/CHaucke89">CHaucke89</a></td></tr>
<tr><td><a href="https://github.com/cydia2020">cydia2020</a></td><td><a href="https://github.com/darkerthan-black">darkerthan-black</a></td><td><a href="https://github.com/Efini">Efini</a></td><td><a href="https://github.com/eFiniLan">eFiniLan</a></td></tr>
<tr><td><a href="https://github.com/ErichMoraga">ErichMoraga</a></td><td><a href="https://github.com/garrettpall">garrettpall</a></td><td><a href="https://github.com/haraschax">haraschax</a></td><td><a href="https://github.com/henryccy">henryccy</a></td></tr>
<tr><td><a href="https://github.com/jakethesnake420">jakethesnake420</a></td><td><a href="https://github.com/jyoung8607">jyoung8607</a></td><td><a href="https://github.com/keefeere">keefeere</a></td><td><a href="https://github.com/krkeegan">krkeegan</a></td></tr>
<tr><td><a href="https://github.com/martinl">martinl</a></td><td><a href="https://github.com/mike8643">mike8643</a></td><td><a href="https://github.com/MoreTore">MoreTore</a></td><td><a href="https://github.com/multikyd">multikyd</a></td></tr>
<tr><td><a href="https://github.com/neokii">neokii</a></td><td><a href="https://github.com/nworb-cire">nworb-cire</a></td><td><a href="https://github.com/opgm">OPGM</a></td><td><a href="https://github.com/pencilpusher">pencilpusher</a></td></tr>
<tr><td><a href="https://github.com/pfeiferj">pfeiferj</a></td><td><a href="https://github.com/rav4kumar">rav4kumar</a></td><td><a href="https://github.com/sshane">sshane</a></td><td><a href="https://github.com/sunnyhaibin">sunnyhaibin</a></td></tr>
<tr><td><a href="https://github.com/syncword">syncword</a></td><td><a href="https://github.com/Thinkpad4by3">Thinkpad4by3</a></td><td><a href="https://github.com/twilsonco">twilsonco</a></td><td><a href="https://github.com/vincentw56">vincentw56</a></td></tr>
<tr><td><a href="https://github.com/xyuelin">xyuelin</a></td></tr>
</table>

⭐ Star History
------

[![Star History Chart](https://api.star-history.com/svg?repos=FrogAi/FrogPilot&type=Date)](https://www.star-history.com/#FrogAi/FrogPilot&Date)

<details>
<summary>MIT Licensed</summary>

FrogPilot, like openpilot, is released under the MIT license. Some parts of the software are released under other licenses as specified.

Any user of this software shall indemnify and hold harmless Comma.ai, Inc. and its directors, officers, employees, agents, stockholders, affiliates, subcontractors and customers from and against all allegations, claims, actions, suits, demands, damages, liabilities, obligations, losses, settlements, judgments, costs and expenses (including without limitation attorneys’ fees and costs) which arise out of, relate to or result from any use of this software by user.

**THIS IS ALPHA QUALITY SOFTWARE FOR RESEARCH PURPOSES ONLY. THIS IS NOT A PRODUCT.
YOU ARE RESPONSIBLE FOR COMPLYING WITH LOCAL LAWS AND REGULATIONS.
NO WARRANTY EXPRESSED OR IMPLIED.**
</details>

<details>
<summary>User Data</summary>

Your comma device sends data to comma or Konik, FrogPilot and Sentry.

<p align="center"><picture><source media="(max-width: 900px)" srcset="https://frogpilot.com/images/frogpilot-data-sharing-phone.svg"><source media="(max-width: 1279px)" srcset="https://frogpilot.com/images/frogpilot-data-sharing-medium.svg"><img src="https://frogpilot.com/images/frogpilot-data-sharing.svg" alt="A table of what your comma device sends, grouped by where it goes, with when it goes and what stops it. A legend marks each row as sent without asking, sent only if Share FrogPilot Data is on, or sent only if you turn the feature on. comma's servers, or Konik's when Use Konik Server is on, get drive logs with GPS, crash logs and low-res road video without asking, whenever the device is online, even while driving, though on cellular the video goes only for drives you open. Disable Uploads stops these uploads. Also without asking, they get system logs and device stats whenever online and, only on request, full video or live data such as your location. No setting stops these uploads. FrogPilot's servers get two things without asking, which no setting stops. One is a registration with the device type and software version at first start and after updates, and the other is a check-in at each start and after each drive that counts active devices and is empty if Share FrogPilot Data is off. Only if Share FrogPilot Data is on, they also get usage stats in the check-in, with your car, drive totals, settings, and nearest city, state and country, which turning Share FrogPilot Data off stops. With Share FrogPilot Data on, they also get driving logs with no video or GPS but raw CAN kept, and 10-second road video clips of speed limit signs on some Toyota and Lexus cars, sent while parked on Wi-Fi or Ethernet, which turning Share FrogPilot Data off, or Disable Uploads, stops. With Share FrogPilot Data on, they also get speed limits learned by Speed Limit Filler where they differ from your maps, sent while parked on Wi-Fi or Ethernet, which turning off Share FrogPilot Data or Speed Limit Filler stops. Weather Condition Offsets is off by default. If you turn it on, it sends your exact location while driving, every 15 minutes or 20 km. Sentry, a crash reporting service, gets crash and error reports tagged with your device ID when something fails, and no setting stops them." width="800"></picture></p>

By default, FrogPilot uploads driving data to comma's servers, like stock openpilot. You can access your data through [comma connect](https://connect.comma.ai/). Builds that aren't fully tested yet, such as **FrogPilot-Testing**, upload to the servers of [Konik](https://konik.ai/), a separate company, instead. They turn on the **"Use Konik Server"** setting, lock it on, and register your device with Konik. You can also turn on **"Use Konik Server"** yourself under **FrogPilot → System Settings → Device / Screen → Device Settings → Manage** (requires the **Advanced** tuning level or higher). You can view those drives at [stable.konik.ai](https://stable.konik.ai/).

openpilot logs the road-facing cameras, CAN, GPS, IMU, magnetometer, thermal sensors, crashes, and operating system logs. Under **Settings → Toggles**, **Record and Upload Driver Camera** controls driver-facing camera recording, and **Record and Upload Microphone Audio** controls microphone recording. Each is only logged if its toggle is on. It also logs the ambient sound level in decibels, which is a loudness reading, not a recording.

If you agree, FrogPilot also sends the following data to its servers.

- Filtered driving logs have no video, GPS coordinates are cleared, and the last six characters of the VIN (your car's ID number) are masked while the rest remains. Your car's raw CAN data (the messages its computers send each other) is kept and, on some cars, includes GPS or the VIN.
- Short road-camera clips of speed limit signs are shared on Toyota and Lexus cars that show the speed limit on their dashboard.
- When **"Speed Limit Filler"** reads a speed limit from your car's dashboard or Mapbox that differs from your downloaded maps, it shares the road, direction, speed limit and source. This setting is under **FrogPilot → Maps and Navigation → Navigation**.
- usage statistics, which are your car's details, your drive totals, your FrogPilot settings, and your nearest city, state and country, linked to your device.

FrogPilot asks during setup whether you agree, and you can change your answer later with **"Share FrogPilot Data"** under **Settings → Software**. Drives recorded before you agreed are never uploaded to FrogPilot. Clips, driving logs and speed limits only upload over Ethernet or Wi-Fi while your car is off.

FrogPilot also registers your device with FrogPilot's server (when it first starts and after updates), checks in each time the device starts and after each drive to count active devices (with nothing in it unless you agreed to share), and sends crash reports to Sentry, a crash-reporting service. No setting turns these off. When you turn on **"Weather Condition Offsets"** under **FrogPilot → Driving Controls → Gas / Brake → Quality of Life → Manage** (requires the **Advanced** tuning level or higher), it sends your location to FrogPilot's server to check the weather. To add your own OpenWeatherMap key, open **Weather Condition Offsets → Manage → Set Your Own Key → Add**. Once you've added your key, FrogPilot tries OpenWeatherMap first.

**"Disable Uploads"** is under **FrogPilot → System Settings → Device / Screen → Device Settings → Manage**. It requires the **Advanced** tuning level or higher, selected under **FrogPilot → Tuning Level**, and is hidden on **FrogPilot-Development** and **FrogPilot-Vetting**.

- It stops the automatic uploads of your drives to comma or Konik, and FrogPilot's clip and driving log uploads. Its **"Disable Onroad Only"** button on the same row only pauses the uploads to comma or Konik while you drive and lets them finish once you park.
- It doesn't stop crash reports, device registration, **"Speed Limit Filler"** sharing, the usage statistics, or the device's connection to comma or Konik, which still sends system logs and uploads files when the server requests them.

When you first set up FrogPilot, it asks you to accept comma's terms. Data sent to comma is covered by comma's [Terms & Privacy](https://comma.ai/terms), which say comma owns all data conveyed to it through openpilot. Konik's terms are at [konik.ai/privacy](https://konik.ai/privacy/).
</details>
