# %%
# set up cw pro
import chipwhisperer as cw
import numpy as np
import time

scope = cw.scope()
scope.gain.db = 25
scope.adc.samples = 35000 
scope.adc.offset = 0  
scope.adc.basic_mode = 'rising_edge'
scope.clock.clkgen_freq = 10e6
scope.clock.adc_src = 'extclk_x1'
scope.trigger.triggers = 'tio4'
scope.io.tio1 = 'serial_rx'
scope.io.tio2 = 'serial_tx'
scope.io.hs2 = 'disabled'

# program target
bitstream = "./FPGA_HQC/HW_HQC.runs/impl_1/cw305_top.bit"
target = cw.target(scope, cw.targets.CW305, bsfile=bitstream, force=True)
time.sleep(0.1)

# we only need PLL1:
target.pll.pll_enable_set(True)
target.pll.pll_outenable_set(False, 0)
target.pll.pll_outenable_set(True, 1)
target.pll.pll_outenable_set(False, 2)

# run at 10 MHz:
target.pll.pll_outfreq_set(10E6, 1)

# 1ms is plenty of idling time
target.clkusbautooff = True
target.clksleeptime = 1

# %%
# ensure ADC is locked:
scope.clock.reset_adc()
assert (scope.clock.adc_locked), "ADC failed to lock"

# %%
# helper functions
### using cipherout(r) for output, use textin(t), cipherin(c) for data0_i & data1_i, use key(k) for key_i

def wait_for_fpga(target):
    i = 0
    while not target.is_done():
        i += 1
        time.sleep(0.05)
        if i > 100:
            raise ValueError("Target did not finish operation")
        
def my_readOutput(target, cmd):
    """"Read output from FPGA"""
    if cmd == 'r':
        if target.REG_CRYPT_CIPHEROUT is None:
            raise ValueError("target.REG_CRYPT_CIPHEROUT unset. Have you given target a verilog defines file?")
        data = target.fpga_read(target.REG_CRYPT_CIPHEROUT, 128)
    elif cmd == 't':
        if target.REG_CRYPT_TEXTOUT is None:
            raise ValueError("target.REG_CRYPT_TEXTOUT unset. Have you given target a verilog defines file?")
        data = target.fpga_read(target.REG_CRYPT_TEXTOUT, 128)
    data = data[::-1]
    return data

def my_loadInput(target, inputtext, cmd):
    """Write input to FPGA"""
    text = inputtext[::-1]
    if cmd == 't':
        if target.REG_CRYPT_TEXTIN is None:
            raise ValueError("target.REG_CRYPT_TEXTIN unset. Have you given target a verilog defines file?")
        target.fpga_write(target.REG_CRYPT_TEXTIN, text)
    elif cmd == 'c':
        if target.REG_CRYPT_CIPHERIN is None:
            raise ValueError("target.REG_CRYPT_CIPHERIN unset. Have you given target a verilog defines file?")
        target.fpga_write(target.REG_CRYPT_CIPHERIN, text)
    elif cmd == 'p':
        if target.REG_USER_PORT0 is None:
            raise ValueError("target.REG_USER_PORT0 unset. Have you given target a verilog defines file?")
        target.fpga_write(target.REG_USER_PORT0, text)
    elif cmd == 'q':
        if target.REG_USER_PORT1 is None:
            raise ValueError("target.REG_USER_PORT1 unset. Have you given target a verilog defines file?")
        target.fpga_write(target.REG_USER_PORT1, text)

def my_loadKey(target, inputkey):
    """Write cmd to FPGA"""
    text = inputkey[::-1]
    if target.REG_CRYPT_KEY is None:
        raise ValueError("target.REG_CRYPT_KEY unset. Have you given target a verilog defines file?")
    target.fpga_write(target.REG_CRYPT_KEY, text)
    target.go()

def my_capture_hqc(scope, target, seed:bytearray=None, msg:bytearray=None, ciphertext:tuple=None, decap:bool=False):
    ret = None
    zeros = bytearray(np.zeros((32,), dtype=np.uint8))

    # run keygen
    if seed != None:
        scope.arm()
        sk_seed = seed[:40]
        pk_seed = seed[40:]
        #sk_seed, pk_seed = seed
        my_loadInput(target, sk_seed, 't')                          # load sk seed to data0_i
        my_loadInput(target, pk_seed, 'c')                          # load pk seed to data1_i
        my_loadKey(target, bytearray.fromhex("00000000123c0de1"))   # run keygen, donot capture trace
        ret = scope.capture(poll_done=False)
    
    # run encap
    elif msg != None:
        scope.arm()
        my_loadInput(target, msg, 't')                              # load msg to data0_i
        my_loadKey(target, bytearray.fromhex("00000000123c0de2"))   # run encap, donot capture trace
        ret = scope.capture(poll_done=False)

    # load ciphertext and run decap
    elif ciphertext:
        u, v, d = ciphertext
        # load u
        step = 128
        q_step = 128 // 4
        for i in range(len(u)//step):   # {p0, upper}, {p1, lower}
            upper = u[i*step : i*step+q_step]
            p0 = u[i*step+q_step : i*step+q_step*2]
            lower = u[i*step+q_step*2 : i*step+q_step*3]
            p1 = u[i*step+q_step*3 : i*step+step]
            my_loadInput(target, bytearray(upper[::-1]), 't')           # load u to data0_i
            my_loadInput(target, bytearray(p0[::-1]), 'p')
            my_loadInput(target, bytearray(p1[::-1]), 'q')
            my_loadInput(target, bytearray(lower[::-1]), 'c')           # load u to data1_i
            my_loadKey(target, bytearray.fromhex("00000000123c0de4"))   # load u start
            wait_for_fpga(target)

        # last chunk
        i = i+1
        upper = u[i*step : i*step+q_step]
        p0 = u[i*step+q_step : i*step+q_step*2]
        my_loadInput(target, bytearray(upper[::-1]), 't')               # load u to data0_i
        my_loadInput(target, bytearray(p0[::-1]).zfill(32), 'p')
        my_loadInput(target, zeros, 'q')
        my_loadInput(target, zeros, 'c')
        #my_loadInput(target, bytearray(lower[::-1]), 'c')              # load u to data1_i
        my_loadKey(target, bytearray.fromhex("00000000123c0de4"))       # load u start
        wait_for_fpga(target)

        for i in range(len(v)//step):
            upper = v[i*step : i*step+q_step]
            p0 = v[i*step+q_step : i*step+q_step*2]
            lower = v[i*step+q_step*2 : i*step+q_step*3]
            p1 = v[i*step+q_step*3 : i*step+step]
            my_loadInput(target, bytearray(upper[::-1]), 't')           # load v to data0_i
            my_loadInput(target, bytearray(p0[::-1]), 'p')
            my_loadInput(target, bytearray(p1[::-1]), 'q')
            my_loadInput(target, bytearray(lower[::-1]), 'c')           # load v to data1_i
            my_loadKey(target, bytearray.fromhex("00000000123c0de5"))   # load v start
            wait_for_fpga(target)

        # last chunk
        i = i+1
        upper = v[i*step : i*step+q_step]
        p0 = v[i*step+q_step : i*step+q_step*2]
        #upper = v[i*step : i*step + step//2]
        my_loadInput(target, bytearray(upper[::-1]).zfill(32), 't')     # load v to data0_i
        my_loadInput(target, bytearray(p0[::-1]).zfill(32), 'p')
        my_loadInput(target, zeros, 'q')
        my_loadInput(target, zeros, 'c')                                # load v to data1_i
        my_loadKey(target, bytearray.fromhex("00000000123c0de5"))       # load v start
        wait_for_fpga(target)

        # prepare for capture and wait for target.go()
        scope.arm()
        #my_loadInput(target, bytearray(d[::-1]), 't')
        my_loadInput(target, zeros, 't')
        my_loadKey(target, bytearray.fromhex("00000000123c0de6"))       # load d and start decap
        ret = scope.capture(poll_done=False)                            # capture trace

    # run decap
    elif decap:
        # prepare for capture and wait for target.go()
        scope.arm()
        my_loadKey(target, bytearray.fromhex("00000000123c0de3"))           # run decap
        ret = scope.capture(poll_done=False)                                # capture trace

    # run reset all
    else:
        my_loadKey(target, bytearray.fromhex("00000000123c0de0"))       # run reset all, donot capture trace


    wait_for_fpga(target)
    if ret:
        raise ValueError("Timeout happened during capture")

    response = my_readOutput(target, 'r')

    if decap or ciphertext:
        wave = scope.get_last_trace()
        return (wave, response)
    else:
        return (None, response)

# %%
# capture trace 10K code

import numpy as np
from tqdm import tnrange
import numpy as np
import matplotlib.pyplot as plt
import random as random

def run_capture_10K(no, rep = 0, num = 10000):

    project_file = "G:/v5-random-test-long/"
    num_of_message = num

    # capture trace
    traces = []
    pk_seed_list = []
    sk_seed_list = []
    message_list = []

    for i in tnrange(num_of_message, desc='Capturing traces'):
        sk_seed = random.randbytes(40)
        pk_seed = random.randbytes(40)
        msg = random.randbytes(16)

        my_capture_hqc(scope, target) #reset all
        _, _ = my_capture_hqc(scope, target, seed=sk_seed+pk_seed) # keygen
        _, _ = my_capture_hqc(scope, target, msg=msg) # encap
        wave, res = my_capture_hqc(scope, target, decap=True) # decap
        traces.append(wave)

        if rep>0:
            for j in range(rep-1):
                wave, res = my_capture_hqc(scope, target, decap=True) # decap
                traces.append(wave)

        pk_seed = [int(x) for x in pk_seed]
        sk_seed = [int(x) for x in sk_seed]
        #msg = [int(x) for x in msg]
        msg = res[-16:]
        
        pk_seed_list.append(pk_seed)
        sk_seed_list.append(sk_seed)
        message_list.append(msg)

    print("Capture finished.")

    # save project
    traces = np.array(traces, dtype='float32')[:,:12800]    # cut after RM decode ends
    np.save(project_file + "traces-"+str(no)+".npy", traces)
    np.save(project_file + "pkseed-"+str(no)+".npy", pk_seed_list)
    np.save(project_file + "skseed-"+str(no)+".npy", sk_seed_list)
    np.save(project_file + "msg-"+str(no)+".npy", message_list)

    traces = []
    message_list = []
    pk_seed_list = []
    sk_seed_list = []

# %%
# run capture trace 10000
# 10k ~ 14min
for i in range(100):
    run_capture_10K(i, rep=0, num=10000)


# %%
# capture cct traces

import numpy as np
from tqdm import tnrange
import numpy as np
import matplotlib.pyplot as plt
import random as random

def run_capture_cct(no, rep=0):

    project_file = "G:/v6-profile-cct-4000/"
    input_folder = "G:/profile-cct-input-4000/"

    # capture trace
    traces = []
    seed = np.array(np.load(input_folder+'seed-'+str(no)+'.npy'), dtype=np.uint8)
    u_list = np.array(np.load(input_folder+'u-'+str(no)+'.npy'), dtype=np.uint8)
    v_list = np.array(np.load(input_folder+'v-'+str(no)+'.npy'), dtype=np.uint8)
    m_list = np.load(input_folder+'code-'+str(no)+'.npy')
    n = u_list.shape[0]

    my_capture_hqc(scope, target) #reset all
    _, _ = my_capture_hqc(scope, target, seed=bytearray(seed)) # keygen
    for i in tnrange(n, desc='Capturing traces'):
        wave, res = my_capture_hqc(scope, target, ciphertext=(u_list[i][0], v_list[i][0], [])) # decap
        traces.append(wave)
        if rep>0:
            for j in range(rep-1):
                wave, res = my_capture_hqc(scope, target, ciphertext=(u_list[i][0], v_list[i][0], [])) # decap
                traces.append(wave)

        wave, res = my_capture_hqc(scope, target, ciphertext=(u_list[i][1], v_list[i][1], [])) # decap
        traces.append(wave)        
        if rep>0:
            for j in range(rep-1):
                wave, res = my_capture_hqc(scope, target, ciphertext=(u_list[i][1], v_list[i][1], [])) # decap
                traces.append(wave)

    print("Capture finished.")

    # save project
    traces = np.array(traces, dtype='float32')[:,9600:12800]    # cut out RM decode
    np.save(project_file + "traces-"+str(no)+".npy", traces)

# %%
# run capture cct trace
# ~30s per set (160 traces)
# ~9.8hr per 1000 sets
for i in range(4000):
    run_capture_cct(i)
