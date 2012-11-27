embedded-esc-testFET
====================

This project was created to test the [FET]s on the kinds of [ESC]s used in the aradio-controlled hobby world, with arbitrary waveforms. These [FET]s have a habit of burning out, especially when developing new [ESC]s. Due to the nature of [BLDC] motors, commonly used with [quadrotor]s and other RC multi-rotor craft, the [ESC] driving circuitry has the ability to do a good deal of troubleshooting of the [FET]s, without the need to hook up an oscilloscope and/or logic analyzer.

This documentation assumes at least a decent knowledge of ESC design. You will need to be able to determine which pins on your ATmega8 ESC MPU are connected to which FETs. You also are expected to know the terms `An`, `Ap`, `Bn`, `Bp`, `Cn`, `Cp`, and `sense_common`. These refer to the six FETs on the three phases in a standard ESC that uses back-EMF to sense motor position, plus the 'common' sense line. You will need to understand the difference between [good waveforms] and [bad waveforms].

[ESC]: http://en.wikipedia.org/wiki/Electronic_Speed_Control
[FET]: http://en.wikipedia.org/wiki/Field-effect_transistor
[BLDC]: http://en.wikipedia.org/wiki/Brushless_DC_electric_motor
[quadrotor]: http://en.wikipedia.org/wiki/Quadrotor
[good waveforms]: https://github.com/ashima/embedded-esc-testFET/blob/master/example_data/all_good_fets/single.png
[bad waveforms]: https://github.com/ashima/embedded-esc-testFET/blob/master/example_data/one_bad_fet/single.png
