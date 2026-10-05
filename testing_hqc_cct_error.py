# %%
import numpy as np
import time
from tensorflow.keras.models import load_model
import os
import scipy.signal as signal
from tqdm import tqdm

def hamming_weight(n):
    return bin(np.uint8(n)).count("1")

def load_sca_model(model_folder,model_file):
    for f_name in os.listdir(model_folder):
        if f_name.startswith(model_file):
            model = load_model(model_folder+f_name)
            print(f_name)
        else:
            continue
    return model

def rank_func(model, dataset, labels):

    input_data = dataset

    # Predict class
    predictions = model.predict(input_data, verbose=0)
    
    predicted_class = np.argmax(predictions, axis=1) 
    
    ranks_prd = np.bitwise_xor(predicted_class, labels)

    return (ranks_prd, predictions, predicted_class)

def return_kth_bit(n,k):
    return((n & (1 << k)) >> k)

def std_byte(traces, byte):
    mean = np.load(std_folder+"mean-"+str(byte)+".npy")
    std = np.load(std_folder+"std-"+str(byte)+".npy")
    traces = (traces - mean)/std
    return traces

def self_std(traces):
    mean = np.mean(traces, axis=0)
    std = np.std(traces, axis=0)
    traces = (traces - mean)/std
    return traces

def denoise(traces, threshold=3):
    f, t, Zxx = signal.stft(traces, fs=10e6, nperseg=32)
    Zxx[:, :threshold, :] = 0
    _, traces_d = signal.istft(Zxx, fs=10e6, nperseg=32)
    return traces_d


def load_traces(test):
    
    traces = np.load(data_folder+'traces-'+str(test)+'.npy', mmap_mode="r")
    labels = np.load(input_folder+'code-'+str(test)+'.npy', mmap_mode="r")
    
    return (traces, labels)

def check_model(model_load, traces, labels):

    num = len(labels)
    
    # rank array if 0 means correct
    (ranks_prd, predictions, predicted_class) = rank_func(model_load, traces, labels)

    a = ranks_prd.shape[0] - np.count_nonzero(ranks_prd)
        
    return (predictions, predicted_class, a/num)

# %%
##################################################
model_folder = "E:/Assignment_projects/HW_HQC/ourModels/test/"
data_folder = "G:/v5-cct-100/"
input_folder = "G:/cct-input-100/"
std_folder = model_folder
################################################### 

init = time.time()

start_error = 0
num_error = 80
test_num = 100

model_list = []
for error in range(start_error, start_error+num_error):
    file_name = "hqc_fpga_byte"+str(error)+".keras"
    model_load = load_model(model_folder+file_name)
    model_list.append(model_load)

m_list = []
acc_list = []
byte_acc = np.zeros((test_num, 46))
for testcase in range(test_num):
    (traces_all, labels_all) = load_traces(testcase)

    predicted_list = np.zeros((num_error*2, 46))
    print("testcase", testcase)

    traces = []
    labels = []
    for byte in range(46):
        # cut out each byte
        start = 8 + byte * 64 + 140
        stop = start + 312 - 140
        traces_t = denoise(traces_all[start_error*2:(start_error+num_error)*2, start:stop], threshold=3)
        labels_t = (np.where(labels_all[start_error:start_error+num_error,:,byte] == 0, 0, 1)).flatten("C")
        traces.append(traces_t)
        labels.append(labels_t)
    
    traces_t = np.array(traces).reshape(-1, num_error*2, traces_t.shape[-1])
    labels_t = np.array(labels).reshape(-1, num_error*2)
    for error in tqdm(range(num_error*2), position=0, leave=True):
        traces = self_std(traces_t[:, error, :])
        #traces = traces_t[:, error, :]
        labels = labels_t[:, error]
        
        predictions, classes, acc = check_model(model_list[error//2], traces, labels)
        correct_list = np.where(classes==labels, 1, 0)
        acc_list.append(sum(correct_list)/len(correct_list))
        byte_acc[testcase, :] += correct_list[:]

        predicted_list[error, :] = classes

    os.makedirs(model_folder+"/testset-1/", exist_ok=True)
    np.save(model_folder+"/testset-1/predicted-test-"+str(testcase)+".npy", predicted_list)

print()
print_acc_list = np.array(acc_list).reshape(test_num, -1)
print("--------per error result list--------")
for k in range(num_error//8):
    for j in range(8):
        temp = (print_acc_list[:, k*16+j].mean() + print_acc_list[:, k*16+j+1].mean()) / 2
        print("{:.4f}".format(print_acc_list[:, k*8+j].mean()), "\t", end="")
    print()
print("-------------------------------------")
temp = np.array(acc_list).reshape(-1, 2)
print("x accuracy:", temp[:, 0].mean())
print("y accuracy:", temp[:, 1].mean())

print("Single trace accuracy:", np.mean(acc_list))

end = time.time()
print("The total running time was: ",((end-init)/60), " minutes.") 
# %%
