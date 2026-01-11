# PDLC driver

Drives [polymer dispersed liquid crystal (PDLC) film](https://en.wikipedia.org/wiki/Smart_glass#Polymer-dispersed_liquid-crystal_devices) from a 12 V DC nominal power supply.

PDLC film is opaque when turned off and becomes clear when turned on.  For example, you can apply it to a window to replace a privacy curtain or to create a diffuse surface for a projection screen on demand.

## Features

Here are some of the features of this PDLC driver board:

- Runs on a 12 V DC battery or power supply at a maximum of 16 V DC
- Drives approximately 4 square meters of PDLC film depending on its electrical characteristics with 15 W of power at 60 V AC peak-to-peak nominal
- Shuts off automatically to protect the battery when the supply voltage falls below 10.6 V DC and resumes operation at 11.5 V
- Includes several forms of circuit protection for the supply and the film
- Detach the terminal blocks from the device for easier wiring
- Optional external I/O functions
  - Connect the mode input to an external momentary push button or a normally open switch to control the state of the film from a convenient location
  - Connect the output enable control signal to a switch to control whether the PDLC film is operable (e.g. can be wired to a door sensor to prevent activation of the film when the door is opened)
  - Connect the auxiliary load output to control another device in tandem with the PDLC film, supports low-side switching of a 200 mA load at up to 20 V maximum (e.g. can be wired to an external indicator)
  - Connect an external I2C controller to control the driver programmatically
  - Connect your own accessories to the expansion port

Please refer to the [design and errata](./hardware/v2/design-and-errata.md) for technical details and schematics.

## Usage

If you have not yet [installed](#installation) and [setup](#setup) the device, do that first then proceed with usage.

How you use the PDLC driver depends on how you have configured it and integrated it with other systems.

By default, the PDLC film toggles on and off when you press the `MODE` button on the device.  When the film is on, it will turn clear, and when it is off, it will become diffuse and provide privacy.  The transition usually happens in less than a second but the film may take longer to change depending on your configuration settings or when the window is very cold.

You can connect an external push button or switch to change the state of the film from a more convenient location. If you have one, use that actuator instead of the `MODE` button.

As a safety feature, the film will not turn on unless the output enable control signal is active, so you may also need to perform another action to enable the film (such as closing the door if you have connected the output enable to a door switch).

You can connect an auxiliary load to the PDLC driver and configure it such that it turns on or off based on the state of the PDLC film automatically. So turning on or off the PDLC film can have additional effects depending on how you have wired things up.

Also, you can programmatically control the PDLC driver using I2C and adjust many internal settings and use advanced functionality such as dimming.  You'll need to write code for this or use an integration that someone else has made.

The `POWER` indicator illuminates for 1 minute after you supply power to the device then turns off.

The `STATUS` indicator color indicates the status of the PDLC driver during normal operation.  The indicator may blink a certain number of times, pause, then repeat to encode information about a problem.  This table explains the significance of the patterns.

| Status color | Pattern    | Meaning |
| ------------ | ---------- | ------- |
| GREEN        | steady     | The PDLC driver is providing power to the PDLC film. |
| AMBER        | 1 blink    | The PDLC driver was commanded to turn the film on but the output enable control signal is inactive so the driver is not providing power to the PDLC film. |
| RED          | 1 blinks   | The PDLC high voltage power-good signal is inactive.  May indicate that the power supply is undervoltage or an electrical fault. |
| RED          | 2 blinks   | The PDLC output A power-good signal is inactive.  May indicate an electrical fault. |
| RED          | 3 blinks   | The PDLC output B power-good signal is inactive.  May indicate an electrical fault. |
| RED          | 4 blinks   | The PDLC output B power-good signal is inactive.  May indicate an electrical fault. |

Refer to the [setup](#setup) guide for information about how this indicator is used while configuring settings.

## Troubleshooting

When the PDLC film doesn't turn on when you think it should...

- Try turning the film off then on again and look at the `STATUS` indicator for clues.  Perhaps the output is not enabled.
- Disconnect and reconnect power to the device.  The `POWER` indicator should illuminate momentarily once power is connected.
- Ensure that your power supply or battery is supplying at least 11.5 V to the device.
- Check that the settings are compatible with your installation.
- If under programmatic control, disconnect the external I2C controller, factory reset the settings, and try again using the built-in buttons to confirm basic operation.
- Check all wiring connections, fuses, and external control inputs.

When the PDLC film doesn't turn off when you think it should...

- Check that the settings are compatible with your installation.
- Check all wiring connections, fuses, and external control inputs.

## Setup

If you have not yet [installed](#installation) the device, do that first then proceed with setup.

Start by locating the `MODE` button, `SET` button, and `STATUS` indicator on the PDLC driver [as shown during installation](#installation).  Here's how to navigate the setting menus.

1. If you have connected an external switch to the `CONTROL CTRL_MODE_IN` terminal, then turn the switch off before changing settings to prevent interference with the `MODE` push button of the PDLC driver.
2. Press the `SET` button to turn off the PDLC film and enter the first setting menu.
3. Look at the `STATUS` indicator.
  - The color of the status indicator tells you which setting menu is active.
  - The indicator will blink a certain number of times, pause, then repeat.  The number of blinks tells you which option of the setting menu is selected.
4. Press the `MODE` button to advance to the next option.  The indicator will begin blinking a different number of times corresponding to the newly selected option.
5. Press the `SET` button to advance to the next setting menu and the color of the `STATUS` indicator will change.  Repeat from step 3 to continue changing settings.
6. Once you have configured the last option, the `STATUS` indicator will turn off and return to normal function.  Similarly, if you wait 30 seconds without interacting with the setting menu the device will exit the setting menu automatically.

> ![IMPORTANT]
> To reset the device to factory default settings, press and hold the `SET` button for 10 seconds until the indicator cycles through a rainbow pattern to confirm.

| Menu color | Option blinks  | Behavior |
| ---------- | -------------- | -------- |
| BLUE       | 1 (default)    | The PDLC film toggles on and off each time `MODE` is pressed. Use this option when you connect a momentary push button to the `CONTROL CTRL_MODE_IN` terminal. |
| BLUE       | 2              | The PDLC film turns on while `MODE` is pressed and turns off while `MODE` is released. Use this option when you connect a normally open switch to the `CONTROL CTRL_MODE_IN` terminal. |
| BLUE       | 3              | The PDLC film state is always on whenever the output enable control signal is active. Use this option when you are controlling the PDLC driver programmatically and do not want the `MODE` button to interfere. |
| BLUE       | 4              | The PDLC film state is always off. Use this option when you are controlling the PDLC driver programmatically and do not want the `MODE` button to interfere. |
| MAGENTA    | 1 (default)    | The PDLC film transitions from opaque to clear as quickly as possible. |
| MAGENTA    | 2              | The PDLC film transitions from opaque to clear gradually over a duration of about 0.5 seconds. |
| MAGENTA    | 3              | The PDLC film transitions from opaque to clear gradually over a duration of about 1 second. |
| MAGENTA    | 4              | The PDLC film transitions from opaque to clear gradually over a duration of about 2 seconds. |
| CYAN       | 1 (default)    | The auxiliary load output turns on when the PDLC film is on and turns off otherwise (same state). |
| CYAN       | 2              | The auxiliary load output turns off when the PDLC film is on and turns on otherwise (opposite state). |
| CYAN       | 3              | The auxiliary load output is always on. |
| CYAN       | 4              | The auxiliary load output is always off. |

## Installation

To begin installing and using your PDLC driver, locate these parts of your device:

| Part                     | Function |
| ------------------------ | -------- |
| `POWER` terminal block   | Connect the power supply and the PDLC film as described in the [wiring guide](#wiring). |
| `CONTROL` terminal block | Connect the control inputs and outputs as described in the [wiring guide](#wiring). |
| `POWER` indicator        | Indicates that the device has power.  Turns off automatically after a few seconds of inactivity. |
| `STATUS` indicator       | Indicates the status of the device and provides visual feedback while you configure settings. |
| `MODE` button            | By default, toggles the PDLC panel on/off.  Can be configured to behave in other ways. |
| `SET` button             | Configures device settings. |
| `EN` solder jumper       | Solder closed to force the output enable control signal to be always active. |
| `EXT I2C` connector      | Allows an external device to control the PDLC driver programmatically using a [QWIIC-compatible](https://www.sparkfun.com/qwiic) I2C interface with galvanic isolation. Additional I2C devices can also be connected in a daisy-chain. |
| `EXPANSION` header       | Connect your own accessories to this port to take advantage of otherwise unused microcontroller pins. |

![PCB front](./hardware/v2/pdlc-front.png)

### Identify the terminals

Remove both terminal blocks from the device by grasping the sides of a block and gently pulling the block out from its socket.  Do not tug on any wires that you may have already connected to the terminal blocks because pulling on the wires could damage them.

Identify the terminals of each terminal block.  You may use the silkscreen legend on the back side of the PCB as a guide, taking care to note the orientation of the connector when it is plugged in.  The following sections will describe these connections in more detail.

We recommend crimping [electric wire ferrules](https://en.wikipedia.org/wiki/Electric_wire_ferrule) onto the wires before screwing them into place to make your connections more secure.  Only moderate screw tension is required.

| Block    | Terminal | Name         | Function |
| -------- | -------- | ------------ | -------- |
| POWER    | 1        | GND          | Power supply input with negative polarity. |
| POWER    | 2        | 12V_IN       | Power supply input with positive polarity. |
| POWER    | 3        | PDLC_OUT_A   | PDLC panel AC output A. |
| POWER    | 4        | PDLC_OUT_B   | PDLC panel AC output B. |
| CONTROL  | 1        | GND          | Connected to `POWER GND`. |
| CONTROL  | 2        | CTRL_EN_IN   | Output enable switch input.  Active when connected to `GND`. |
| CONTROL  | 3        | CTRL_MODE_IN | Mode switch input.  Active when connected to `GND`. |
| CONTROL  | 4        | GND          | Connected to `POWER GND`. |
| CONTROL  | 5        | LOAD_OUT     | Auxiliary load output. |
| CONTROL  | 6        | 12V_OUT      | Connected to the 12V rail downstream of the internal fuse circuit protection.  DO NOT CONNECT TO POWER SUPPLY. |

![PCB front](./hardware/v2/pdlc-back.png)

### Connect the power supply (required)

You must connect the driver to a suitable 12 V DC nominal power supply.  It must be capable of supplying at least 2 amps of current and have suitable overcurrent protection such as a fuse or circuit breaker.

Use 20 AWG (0.5 mm^2) wire or larger to make the connections.

Connect the `POWER GND` terminal to the negative (ground) side of your power supply.

Connect the `POWER 12V_IN` terminal to a 3 A fuse or circuit breaker attached to the positive side of your power supply.

### Connect the PDLC film (required)

Use 22 AWG (0.3 mm^2) wire or larger to make the connections, preferably double-insulated.

Connect the `POWER PDLC_OUT_A` and `POWER PDLC_OUT_B` terminals to the PDLC film.  Either terminal can be connected to the PDLC film in either orientation because the film uses AC and there is no distinct polarity.

> ![WARNING]
> The PDLC power wires carry high voltage similar to domestic electrical wiring systems.  They can deliver a small electric shock that could be harmful to people so please ensure that these wires are adequately insulated and without exposed connections that could accidentally be touched!
>
> If the connection passes through a contact surface that might intermittently be exposed to people or to the environment (such as through a sliding door contact plate) install a safety device to deactivate the output enable control signal (such as a door switch) when those contacts may be exposed to reduce the risk of harm.

### Connect the output enable control signal (required)

The PDLC driver only provides power to the PDLC film when the output enable control signal is connected to ground.  This mechanism can be used to implement safety features to automatically disable the film when it is not safe to operate (such as when the contacts may be exposed as described in the previous section) or to remotely enable or disable the film using other devices.

> ![IMPORTANT]
> By default, the output enable control signal is not connected to anything (it is inactive) and the driver will not operate the PDLC film.  So for the PDLC film to operate, you must make a connection to activate the output enable control signal.

Here are three ways to do activate the output enable control signal:

- Connect a switch between the `CONTROL CTRL_EN_IN` and `CONTROL GND` terminals that is closed whenever it is safe to operate the PDLC film and open otherwise.  Alternatively, you may use a relay, transistor, or an optocoupler to drive the signal low (connect it to ground).
- Connect a short piece of wire with bare ends between the `CONTROL CTRL_EN_IN` and `CONTROL GND` terminals to ensure that the signal is always active when there is no reason to disable the film.
- Solder the `EN` jumper closed to bypass the `CONTROL` terminal block and ensure that the signal is always active when there is no reason to disable the film.

### Connect an external mode actuator (optional)

Optionally, you can connect an external actuator to control the mode of the PDLC driver from a more convenient location such as when you install the PDLC driver inside of a cabinet.

Choose one of following actuators and make note of its corresponding `MODE` behavior.

| Actuator type              | Behavior | Description |
| -------------------------- | -------- | ----------- |
| SPST momentary push button | Toggle   | Toggle the film on or off each time the button is pressed. |
| SPST normally open switch  | On / Off | Turn the film on when the switch is closed and off when the switch is open. |

Use 24 AWG (0.2 mm^2) wire or larger to make the connections.

Connect the actuator to the `CONTROL CTRL_MODE_IN` and `CONTROL GND` terminals such that the circuit is closed when the button is pressed or the switch is in the on position.

In place of a button or switch, you may use different actuators that perform a similar electric function, such as a relay, transistor, or optocoupler to drive the signal low (connect it to ground).  And you may also connect multiple push button actuators in parallel to toggle the mode from several locations.

Later during [setup](#setup) you must set the mode button behavior for the type of actuator that you selected.

### Connect an auxiliary load (optional)

Optionally, you can connect an auxiliary load that will receive power when the PDLC film is either on or off according to the settings.  You can use this output to power an external indicator, relay, optocoupler, or other small load that can switch on and off based on the PDLC film state.

Use 24 AWG (0.2 mm^2) wire or larger to make the connections.

Connect the `CONTROL LOAD_OUT` terminal to the negative terminal of the load.

Connect the load's positive terminal to the load's power supply, preferably with adequate current limiting or a small fuse to stay within the rating of the load output.  The load's power supply must share a common ground with `CONTROL GND`.

> ![IMPORTANT]
> The auxiliary output is a low-side solid state switch (MOSFET).  When active, the `CONTROL LOAD_OUT` terminal behaves as if it has been shorted to `CONTROL GND` which completes the circuit to power the load.  The load output is rated for drive 200 mA at up to 20 V maximum.  Please pay attention to the current requirements of the load to avoid exceeding the rating and damaging your devices.

### Connect an external I2C controller (optional)

Optionally, you can connect the PDLC driver to an external I2C controller.

Connect the controller to either port labeled `EXT I2C` using a standard 4-pin QWIIC bus JST SH connector.  You may also connect additional devices downstream using the other port to form a daisy-chain.

The circuit board has an [ISO1640](https://www.ti.com/lit/ds/symlink/iso1640.pdf) to galvanically isolate the PDLC driver from the I2C port.  Here's what that means:

- The devices can be powered on and off independently (hot-plugging).
- The devices do not need to be connected to the same ground so they can be powered from different supplies and they will not form ground loops when connected.  The isolation barrier is rated for at least 450 Vrms which is suitable for many automotive applications where devices are nominally connected to the same chassis ground but there may be a small voltage difference between nodes.
- The PDLC driver's `EXT I2C` port is protected from ESD (electrostatic discharges).

(TODO: Link to the I2C target protocol)

### Plug the terminal blocks into the device (required)

Now that you have made all of the wiring connections, turn off or disconnect the power supply, gently plug the terminal block in until they are fully seated, then turn on or reconnect the power supply.

Congratulations!  Now you're ready to setup the device configuration (if you need to change the defaults).

## Electrical information

### Recommended operational parameters and circuit protection

Supply:

- Voltage: 12 V DC nominal and absolute maximum range from 10 to 16 V DC
- Wiring: minimum 20 AWG (0.5 mm²)
- Circuit protection
  - Internal: built-in 2 A polyfuse, transient voltage suppressor, reverse polarity protection, and under voltage lock-out below 10.6 V
  - External: add a 3 A fuse or circuit breaker to protect the wiring to the device

Load:

- Voltage: 60 V AC nominal at 100 Hz
- Current: 300 mA maximum continous load
- Power output: 15 W maximum continuous load
- Circuit protection
  - Internal: built-in overload, over voltage, and over temperature protection
  - External: ensure wires are adequately insulated and not exposed to touch

In case of an overload such as exceeding the maximum output current or a short circuit, the power supply will hiccup until the overload condition is removed.

Please test your set up carefully if you choose to operate the PDLC driver beyond these recommendations.

### PDLC film electrical characteristics

Here is some general information about the electrical characteristics of PDLC films for which this driver was design.  If your PDLC film has significantly different electrical characteristics, please follow the tuning instructions or contact the author for advice on using this driver with your film.

The film's capacitance is proportional to its surface area, around 10 uF per square meter.  It becomes clear when charged and opaque when discharged.

The film's resistance is inversely proportional to its surface area, generally in the tens to hundreds of kiloohms.  It self-discharges rapidly and becomes opaque when power is removed.

The film's clarity increases with voltage.  It is opaque at 0 V, begins to clear around 20 to 30 V, is nearly transparent around 50 V, and becomes slightly clearer with increasing voltage.

The film must be driven with AC to produce an alternating electric field to orient the liquid crystals and it may be damaged when driven in one polarity for too long.  At very low frequencies, the film acts as a shutter.  As the frequency rises to a few hertz, the film will appear to pulse between opaque and clear states.  At higher frequencies, the film appears clear.  Mains AC frequencies (50 / 60 Hz) seem adequate.

### PDLC film data

Here's some information about PDLC films that the driver has been tested with.  

- FilmBase PDLC self-adhesive grey film
  - Operating voltage: 60 V AC (manufacturer recommendation)
  - Frequency: 50/60 Hz (manufacturer recommendation)
  - Power: < 5W / m^2 (manufacturer claim)
  - Capacitance: ~12 uF / m^2 (measured)

## Design history

v2.0: Second prototype.  Uses a microcontroller to generate commutation waveforms in software and more careful circuitry to limit the output current.  Should work with larger panels and be safer to operate.  Can be controlled with two switch inputs and interface with a host device as an I2C target.

v1.0: Initial prototype.  Uses a 555 timer and inverter to generate commutation waveforms.  Works fine for small PDLC samples.  Can't drive larger panels without modification because it does not wait for the power-good signal to be asserted before energizing the panel and it does not limit the current so it hiccups.  During testing, the board was modified to replace the timer with a microcontroller to generate PWM signals in open loop control which helped it drive larger panels but it still could not achieve the expected output power.

## Notice

The PDLC driver software, documentation, design, and all copyright protected artifacts are released under the terms of the [MIT license](LICENSE).

The PDLC driver hardware is released under the terms of the [CERN-OHL-W license](hardware/LICENSE).
