embedded-esc-testFET API
========================

required setup
--------------

**WARNING**: it is highly recommended to use a current-limiting power supply when performing FET testing. A small error can have a FET combination open for up to 15ms, which is enough time to fry a FET if it is connected to a lipo battery. The watchdog timer is used to ensure that no pulses longer than this can be applied, but it is of limited use: 15ms is plenty of time to fry a FET.

     computer            MPU (e.g. a random Arduino)            MPU (the ESC) 
    ----------            -------------------------           ----------------
    | i2c.py | <- UART -> | twi_serial_bridge.hex | <- I2C -> | esc_test.hex |
    ----------            -------------------------           ----------------

[twi_serial_bridge] is an example in our [embedded-atmel-twi] library. `esc_test.hex` comes from [esc_test.cpp], where [i2c.py] is also located. The reason that a bridge between UART and I2C is needed is because the ESC we're using only has an I2C interface exposed, while most computers only have UART (via USB).

**WARNING**: as of 2012-11-26, you must modify the `Select_*` macros in [esc_test.cpp]. They tell the ESC which pins are connected to which FETs. See the code comments near these macros.

[twi_serial_bridge]: https://github.com/ashima/embedded-atmel-twi/tree/master/example/twi_serial_bridge
[embedded-atmel-twi]: https://github.com/ashima/embedded-atmel-twi
[esc_test.cpp]: https://github.com/ashima/embedded-esc-testFET/blob/master/esc_test.cpp
[i2c.py]: https://github.com/ashima/embedded-esc-testFET/blob/master/i2c.py

running via the Python code
---------------------------

Currently, Python 3.x is required to run [i2c.py]. You can check your version by running `python --version`. You can use the python utility directly to execute-and-exit:

    > python i2c.py /dev/tty.usbmodem3a21

You will want to replace `/dev/tty.usbmodem3a21` with the name of your serial port with the \[Arduino\] board running [twi_serial_bridge]. After successful execution, you will find files in your working directory, as described in [standard battery of tests].

Another way to run the standard tests, and then to run your own tests easily, is to use [IPython] \(again, version 3.x). For example, the invocations below would both run the [standard battery of tests], as well as a custom test.

    > ipython
    In [1]: %run i2c.py /dev/tty.usbmodem3a21
    In [2]: exec_save([(0,200),(Ap,600),(0,200)], 'wave.txt')

'wave.txt' will be saved to the working directory.

**WARNING**: if you alter [esc_test.cpp] sufficiently, where the variables are stored in memory will change. If you are running code via [IPython], you will need to call `init_ser()` explicitly to reload the mapping file (see [TWI interface](#twi-interface)). 

[standard battery of tests]: #standard_tests
[gnuplot]: http://www.gnuplot.info/
[IPython]: http://ipython.org/

<a name="standard_tests"></a>
standard battery of tests 
-------------------------

By default, running `./i2c.py serialport` will generate a full "battery of tests":
1. firing each FET individually with `sense_common` raised high so that it is possible to see the action of both p and nFETs.
2. firing each nFET-pFET pair (where each FET is on a different phase)

Three files are generated for each:
1. `*.vcoil` voltage on the motor coil ([example][vcoil])
2. `*.vsup` voltage of the supply ([example][vsup])
3. `*.current` current drawn from the supply ([example][current])

[vcoil]: https://github.com/ashima/embedded-esc-testFET/blob/master/example_data/all_good_fets/Ap.vcoil
[vsup]: https://github.com/ashima/embedded-esc-testFET/blob/master/example_data/all_good_fets/Ap-Bn.vsup
[current]: https://github.com/ashima/embedded-esc-testFET/blob/master/example_data/all_good_fets/Ap-Bn.current

The format of all files generated via the `exec_save` function (which is what is called to run the standard battery of tests) is (raw_adc_value, time_ms, computed_adc_units). The ADC units are controlled by `config.scales` in [i2c.py]; they'll probably be V or A, although temperature is a possibility.

The file [plot] plots the standard battery of tests and may be useful for seeing how to plot data using [gnuplot]. You would invoke it something like this:

    bash> cd /path/to/embedded-esc-testFET/
    bash> mkdir data && cd data
    bash> python ../i2c.py serialport
    bash> gnuplot ../../plot

[plot]: https://github.com/ashima/embedded-esc-testFET/blob/master/plot

[example_data/] contains [two][good_single] [plots][good_double] from a good ESC and [two][bad_single] [plots][bad_double] from an ESC with a bad pFET.

[example_data/]: https://github.com/ashima/embedded-esc-testFET/tree/master/example_data
[good_single]: https://github.com/ashima/embedded-esc-testFET/blob/master/example_data/all_good_fets/single.png
[good_double]: https://github.com/ashima/embedded-esc-testFET/blob/master/example_data/all_good_fets/double.png
[bad_single]: https://github.com/ashima/embedded-esc-testFET/blob/master/example_data/one_bad_fet/single.png
[bad_double]: https://github.com/ashima/embedded-esc-testFET/blob/master/example_data/one_bad_fet/double.png

TWI interface
-------------

The ESC FET tester works via a TWI (I2C) interface. It consistes of a memory map for configuration and resultant waveforms, plus a special address that when written to or read from, will trigger an individual test. The user will generally write to the following variables before triggering a test:

    uint8_t states[]        // bits [0..5] are [Ap, An, Bp, Bn, Cp, Cn]; bit 6
                            // is sense_common (a standard ESC term)
    uint16_t state_waits[]  // number of clocks to maintain the associated state
                            // in states[], times 8 (prescalar is clk/8)
    uint8_t prescalar       // see ADPS2:0 in the ATmega docs; is a divisor
                            // which controlshow many CPU clocks per ADC clock
                            // [0..7] = [2, 2, 4, 8, 16, 32, 64, 128];
                            // for highest accuracy, use 7 
    uint8_t which_adc       // see MUX3:0 in the ATmega docs;
                            // [0..7] = [ADC0..ADC7]
    bool high_res           // see the ADLAR bit in ADMUX in the ATmega docs;
                            // determines whether the high 8 bits or low 8 bits
                            // of the 10-bit ADC result are stored;
                            // true means the low 8 bits are stored;
                            // at lower prescalars, the two LSB will be noise
    uint8_t sample_count    // number of samples to capture, up to 100h

To trigger a test, read or write one byte from/to address FFF9h. The results of the test are found in:

    uint8_t waveform[]      // up to 100h samples, depending on sample_count

Note that the test will take `sample_count * adc_divisor(prescalar) * ADC_CYCLES_PER_SAMPLE / CPU_MHZ`. (It takes 13 ADC clock cycles per sample.) For example, if the sample count is 100h, prescalar 7, and CPU speed 16MHz, complete waveform capture will take ~27ms. (Actually, it will take a bit longer, due to set up time.) 

Note that as of 2012-11-26, the C++ variables above are not pinned to any particular addresses in SRAM; this means that one must consult [esc_test.map]. You can see where [i2c.py] extracts this information by searching it for `mapped_variables`.

[esc_test.map]: https://github.com/ashima/embedded-esc-testFET/blob/master/esc_test.map

errors
------

I often find that the first time I plug everything in, I get TWI errors as documented below. This is likely due to not everything starting up in a known state. Power cycling the ESC and sometimes the [twi_serial_bridge] board usually solves the problem. Re-flashing one or both MPUs might also be necesasry. Furthermore, sometimes the OS can get weird with the serial port; unplugging and reconnecting the USB cables solves this.

These messages will be the first thing to be printed after running [i2c.py], assuming that the serial connection was established properly and [twi_serial_bridge] is working correctly, if the reset cause isn't a power-on or external reset:

    RESET CAUSE: brown out
    RESET CAUSE: watchdog timer reset

If you see 'brown out', I suggest you reset the ESC cleanly. Brown outs can cause weird conditions that probably are not worth debugging. If you see 'watchdog timer reset', the problem is more serious: either you fed in test code with delays that were too long, or there is a bug in the code. Either way, there is imminent threat of burning out a FET, if it has not already happened.

    ex: ERROR: did not get ~ as ack: 'TWI ERROR: addr=0x50, TWSR=0x20, SCL=1, SDA=1'

If you see an error like this, consult the [twi_serial_bridge twi errors] documentation. But you might first try running [i2c.py] once or twice more, power cycle both MPUs, and unplug & reconnect USB devices, first.

[twi_serial_bridge twi errors]: https://github.com/ashima/embedded-atmel-twi/blob/master/example/twi_serial_bridge/API.md#twi-errors
