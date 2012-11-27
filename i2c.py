# in iPython:
#   %run -i i2c.py
# the -i means use existing global variables; in particular: ser
# (otherwise, we end up with multiple open serial connections and it gets ugly)

import sys
import os
import re
import serial
import time
from math import ceil
from functools import reduce
from itertools import chain

if len(sys.argv) < 2:
    print("Please specify a serial port as the first argument.", file=sys.stderr)
    exit()

ser_port = sys.argv[1]

class CommunicationsError(Exception):
    pass

def init_ser():
    global mapped_variables

    mapfile = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'esc_test.map')
    mapfile_lines = open(mapfile, 'r').readlines()
    mapped_variables = dict([(m.group(2), int(m.group(1), 0) - 0x800000) for s in mapfile_lines for m in [
        re.search('(0x00000000008[0-9a-fA-F]+)\s*([a-zA-Z_]\w+)', s)] if m])

    global ser
    
    is_defined = 'ser' in globals() # see comment at top
    is_open = is_defined and ser.isOpen()
    min_ser_timeout = 0.010 # less than 10ms and we're going too quickly
    
    if not is_open:
        if not is_defined:
            ser = serial.Serial(ser_port, 115200, timeout=0.01)
        else:
            ser.open()
        
        # give the serial interface a bit of time to return data received _before_ the Arduino
        # reset that we just caused by opening up the serial interface that is connected to
        # the auto-reset circuit
        time.sleep(.1)
        ser.flushInput()
        # now wait a max of one second for the initial serial dump from the Arduino; once we
        # get a single byte, we can just eat up the rest and be done
        for i in range(0,100):
            by = ser.read(1)
            if len(by) == 0:
                time.sleep(.01)
            else:
                ser.read(100)
                break

    write('-Vb') # turn verbose off and binary on
    
    if ser.timeout < min_ser_timeout:
        print("adjusting ser.timeout from %.3f to %.3f" % (ser.timeout, min_ser_timeout))
        ser.timeout = max(ser.timeout, min_ser_timeout)

[Ap, An, Bp, Bn, Cp, Cn, sense_common] = map(lambda n: 2**n, [0,1,2,3,4,5,6])

class serial_twi_bridge:
    MAX_READ_BYTES = 0x20

class mcu:
    F_CPU = 16e6
    ADC_PRESCALAR = [2, 2, 4, 8, 16, 32, 64, 128] # probably common to all ATmegas
    ADC_CYCLES_PER_SAMPLE = 13 # strictly speaking the first is 25 but we don't care

    def adc_sample_time(prescalar):
        return mcu.ADC_PRESCALAR[prescalar] / mcu.F_CPU * mcu.ADC_CYCLES_PER_SAMPLE

    class MCUCSR: 
        WDRF  = 3
        BORF  = 2
        EXTRF = 1
        PORF  = 0
    
class config:
    MAX_SAMPLE_COUNT = 0x100
    current_res = 0.02
    vcoil_scale = 2
    vsup_scale = 11
    # indexed by high_res = [0,1]
    Vrefs = [5, 2.56]
    resolutions = [256, 1024]
    # indexed by adc = [0..7]
    scales = [vcoil_scale, vcoil_scale, 0, 0, 0, 0, 1 / current_res, vsup_scale]

    def _adjust_sample_count(v):
        if v > config.MAX_SAMPLE_COUNT:
            raise ValueError("max sample_count is %d, which is less than %d" % 
                (config.MAX_SAMPLE_COUNT, v))
        
        # this is because the "do I make another ADC measurement?" if statement in the 
        # device looks like this:
        #   if (--copy_of_sample_count == 0)
        #     stop_adc();
        # so if copy_of_sample_count starts off as zero, it will return as 0xFF and
        # 0x100 samples will actually be taken
        if v == config.MAX_SAMPLE_COUNT:
            v = 0
        
        return v

    def set_sample_count(v):
        config.sample_count = config._adjust_sample_count(v)
        write_var('sample_count', config.sample_count)

    def set_prescalar(v):
        config.prescalar = v
        write_var('prescalar', v)
    def set_adc(v):
        config.adc = v
        write_var('which_adc', v)
    def set_high_res(v):
        config.high_res = v
        write_var('high_res', v)
    
    def set_vcoil():
        config.set_high_res(0)
        config.set_adc(0)
    def set_current():        
        config.set_high_res(1)
        config.set_adc(6)
    def set_vsup():
        config.set_high_res(0)
        config.set_adc(7)
            
    def convert_y():
        c = config
        return lambda adc: adc / c.resolutions[c.high_res] * c.Vrefs[c.high_res] * c.scales[c.adc]
    def x_values():
        spacing = mcu.adc_sample_time(config.prescalar)
        
        return [spacing * n for n in range(0, config.sample_count)]
        
    def trigger():
        read_var(0xFFF9, 1);
    
    def get_mcucsr():
        return read_var8('mcucsr_mirror')
    
    def check_reset_cause():
        mcucsr = config.get_mcucsr()
        m = mcu.MCUCSR
        
        if (mcucsr & (1<<m.WDRF)) != 0:
            print("RESET CAUSE: watchdog timer reset", file=sys.stderr)
        if (mcucsr & (1<<m.BORF)) != 0:
            print("RESET CAUSE: brown out", file=sys.stderr)

def flatten(a):
    return list(chain.from_iterable(a))
        
def data(s):
    return bytes(s, 'latin1')

def decode(by):
    return by.decode('latin1')

def write(s):
    ser.write(data(s))
    ser.write(data('\r'))

def get_addr(var_name):
    return mapped_variables[var_name]

def check_ack():
    c = decode(ser.read(1))
    if c != '~':
        raise CommunicationsError("ERROR: did not get ~ as ack: '%s%s'" % 
            (c, decode(ser.read(500)).strip()))

def write_var(var, bytes):
    addr = get_addr(var) if isinstance(var, str) else var
    if not isinstance(bytes, list):
        bytes = [bytes]
        
    cmd = '%04X w %s' % (addr, ' '.join('%02X' % y for y in bytes))
    #print("> %s" % cmd)
    write(cmd)
    check_ack()

def read_var8(var):
    return read_var(var)[0]

def read_var(var, count=1, binary=True):
    addr = get_addr(var) if isinstance(var, str) else var

    cmd = '%04X %02X' % (addr, count)
    #print("< %s" % cmd)
    write(cmd)
    check_ack()

    if binary:
        by = ser.read(count)

        if len(by) < count:
            raise CommunicationsError("Expected %d bytes, got %d: %s" % (count, len(by), by))

        return list(by)
    else:
        expected = count * len('00 ') + len('\r\n')
        by = ser.read(expected)
        s = decode(by.strip())
        
        if len(by) < expected:
            raise CommunicationsError("Expected %d bytes, got %d: '%s'" % (expected, len(by), s))
        
        try:
            return [int(str(y), 16) for y in s.split(' ')]
        except:
            print("read_var: %s" % s)
            raise

def get_waveform():
    waveform_addr = get_addr('waveform')
    v = []
    chunk = serial_twi_bridge.MAX_READ_BYTES
    for i in range(0, int(ceil(config.sample_count / chunk))):
        v += read_var(waveform_addr, min(chunk, config.sample_count - i * chunk))
        waveform_addr += chunk
    return v

def exec(t):
    expected_delay = mcu.adc_sample_time(config.prescalar) * config.sample_count
    
    write_var('states', [s for s, w in t])
    write_var('state_waits', flatten([w & 0xFF, (w & 0xFF00) >> 8] for s, w in t))
    
    config.trigger()

    time.sleep(expected_delay)

    return get_waveform()

def exec_save(t, file):
    with open(file, 'w') as f:
        by = exec(t)
        convert_y = config.convert_y()
        x_values = config.x_values()
        for idx, y in enumerate(by):
            f.write('%d %.3f %.3f\n' % (y, x_values[idx] * 1e6, convert_y(y)))

#ser.read(500)

# sys.exit()

init_ser()
config.check_reset_cause()

# exec_save([(sen,200),(sen+Ap,delta_t),(sen,200)], 'wave')
# exec_save([(Cp+An,delta_t)], 'wave')
        
for i in range(0,2):
    try:
        fet_on_time = sense_on_time = 800
        config.set_prescalar(6)
        config.set_sample_count(0x30)
        
        def with_sense(fet, file):
            exec_save([
                (sense_common, sense_on_time),
                (sense_common+fet, fet_on_time),
                (sense_common, sense_on_time)], file)
        
        def without_sense(fets, file):
            exec_save([(0, sense_on_time), (fets, fet_on_time)], file)
        
        pFETs = [(Ap, 'Ap'), (Bp, 'Bp'), (Bp, 'Cp')]
        nFETs = [(An, 'An'), (Bn, 'Bn'), (Cn, 'Cn')]

        def rotate(l, n):
            return l[n:] + l[0:n]
        def interleave(l1, l2):
            return flatten([a, b] for a,b in zip(l1, l2))
        def double(n):
            return [(pFET+nFET, '%s-%s' % (pS, nS)) for (pFET,pS),(nFET,nS) in zip(pFETs, rotate(nFETs, n))]        

        singles = pFETs + nFETs
        doubles = interleave(double(1), double(2))
        
        for f, ext in [
            (config.set_vcoil, 'vcoil'), 
            (config.set_current, 'current'), 
            (config.set_vsup, 'vsup')]:
            f()
            for fet, name in singles:
                with_sense(fet, '%s.%s' % (name, ext))
            for fets, name in doubles:
                without_sense(fets, '%s.%s' % (name, ext))
                    
        break
    except CommunicationsError as ex:
        print("ex: %s" % ex, file=sys.stderr)
        continue

def gnuplot():
    format_string = "set title '{0}'; plot " + \
        ', '.join(["'{{0}}.{0}' u 2:3 t '{1}' w lp{2}".format(*t) for t in [
            ('vcoil', 'V_{{coil}}', ''), 
            ('vsup', 'V_{{battery}}', ''), 
            ('current', 'I_{{battery}}', ' axes x1y2')]])

    # `singles` and `doubles` are defined in the non-function code above
    for fet, name in singles + doubles:
        print(format_string.format(name))

