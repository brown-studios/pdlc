# PDLC driver

Drives [polymer dispersed liquid crystal (PDLC) film](https://en.wikipedia.org/wiki/Smart_glass#Polymer-dispersed_liquid-crystal_devices) from a 12 V DC nominal power supply.

PDLC film is opaque when turned off and becomes clear when turned on.  For example, you can apply it to a window to replace a privacy curtain or to create a diffuse surface for a projection screen on demand.

## Features

Here are some of the features of this PDLC driver board:

- Drives approximately 4 square meters of PDLC film depending on its electrical characteristics with 15 W of power at 60 V AC peak-to-peak nominal
- Designed to run on a 12 V DC battery or power supply at a maximum of 16 V DC
- Shuts off automatically to protect the battery when the supply voltage falls below 10.6 V DC and resumes operation at 11.5 V
- Has several forms of circuit protection for the supply and the load
- Power on indicator

## Installation

Wire the 6-pin connector according to the labels shown on the back side of the circuit board as follows.  We recommend using crimped wire ferrules to secure stranded wires before screwing them into the terminal block.

Make sure the supply is turned off while making these connections.

Connect `12V` and one `GND` terminal to a 10 to 16 V DC supply using 20 AWG (0.5 mm^2) wire or larger.  Pay attention to the polarity: `12V` is positive and `GND` is negative.  We recommend adding a 3 A fuse or circuit breaker to protect the wiring to the device.

Connect `OUTA` and `OUTB` to the PDLC film using 22 AWG (0.3 mm^2) wire or larger.  Either terminal can be connected to the PDLC film in either orientation because the film uses AC and there is no distinct polarity.  These wires carry enough voltage to deliver a small electric shock that could be harmful to people so please ensure that the wires are adequately insulated and not exposed to touch.

Connect `EN` to one `GND` terminal via a switch circuit, via a short jumper wire, or by soldering the `ENABLE` jumper closed.  The `EN` terminal is active low so the driver turns on when `EN` is grounded and turn offs otherwise.  Use 24 AWG (0.2 mm^2) wire or larger to make the connection.  Smaller wires will work but may be too fragile for the terminals.

If you would like to disable the power on indicator, cut the `JP1` jumper trace.

## Usage

After [installing the PDLC driver](#installation), provide power to the driver and activate the switch circuit.

If everything is working as intended and the circuit is enabled, the PDLC film will become clear and the power on indicator labeled `ON` will glow red (unless it has been disabled).

If this does not happen, disconnect power then recheck your connections and the [recommended operational parameters](#recommended-operational-parameters-and-circuit-protection).

## Recommended operational parameters and circuit protection

Supply:

- Voltage: 12 V DC nominal and absolute maximum range from 10 to 16 V DC
- Wiring: minimum 20 AWG (0.5 mm²)
- Circuit protection
  - Internal: built-in 2 A polyfuse, transient voltage suppressor, reverse polarity protection, and under voltage lock-out below 10 V
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

## PDLC film electrical characteristics

Here is some general information about the electrical characteristics of PDLC films for which this driver was design.  If your PDLC film has significantly different electrical characteristics, please follow the tuning instructions or contact the author for advice on using this driver with your film.

The film's capacitance is proportional to its surface area, around 10 uF per square meter.  It becomes clear when charged and opaque when discharged.

The film's resistance is inversely proportional to its surface area, generally in the tens to hundreds of kiloohms.  It self-discharges rapidly and becomes opaque when power is removed.

The film's clarity increases with voltage.  It is opaque at 0 V, begins to clear around 20 to 30 V, is nearly transparent around 50 V, and becomes slightly clearer with increasing voltage.

The film must be driven with AC to produce an alternating electric field to orient the liquid crystals and it may be damaged when driven in one polarity for too long.  At very low frequencies, the film acts as a shutter.  As the frequency rises to a few hertz, the film will appear to pulse between opaque and clear states.  At higher frequencies, the film appears clear.  Mains AC frequencies (50 / 60 Hz) seem adequate.

### Data

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
