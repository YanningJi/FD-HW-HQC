import time
import numpy as np
import tensorflow as tf
from tensorflow.keras.utils import to_categorical
from tensorflow.keras.callbacks import ModelCheckpoint, EarlyStopping
import my_models as mm
import gc
from tensorflow.keras import mixed_precision
import scipy.signal as signal

################################################
# for wsl
model_folder = "/mnt/e/Assignment_projects/HW_HQC/ourModels/test/"
model_name = 'hqc_fpga_byte'

data_folders = ["/mnt/d/data/errorpattern-v5-profile-cct-2000/",
                "/mnt/d/data/errorpattern-v5-profile-cct-2000-2/",
                "/mnt/d/data/errorpattern-v5-profile-cct-4000-2/",
                "/mnt/d/data/errorpattern-v5-profile-cct/",
                "/mnt/d/data/errorpattern-v5-profile-cct-2/",
                "/mnt/d/data/errorpattern-v5-profile-cct-2000-3/",
                "/mnt/d/data/errorpattern-v5-profile-cct-4000-3/",
                "/mnt/d/data/errorpattern-v5-profile-cct-2000-4/",
                "/mnt/d/data/errorpattern-v5-profile-cct-4000-4/",
                "/mnt/d/data/errorpattern-v5-profile-cct-2000-5-(1000)/",
                "/mnt/d/data/errorpattern-v5-profile-cct-4000-5/"]

#################################################

def std(traces, byte=0):
    mean = np.mean(traces, axis=0)
    std = np.std(traces, axis=0)
    np.save(model_folder+'mean-'+str(byte)+'.npy', mean)
    np.save(model_folder+'std-'+str(byte)+'.npy', std)
    traces = (traces - mean)/std
    return traces

def denoise(traces, threshold=3, nperseg=32):
    f, t, Zxx = signal.stft(traces, fs=10e6, nperseg=nperseg)
    Zxx[:, :threshold, :] = 0
    _, traces_d = signal.istft(Zxx, fs=10e6, nperseg=nperseg)
    return traces_d

def traces_append(traces_all, labels_all, folder, cut_interval, is_even):
    if is_even == 1:
        traces_t = np.load(folder+'traces-error'+str(error*2)+'.npy', mmap_mode="r")[:, cut_interval]
        labels_t = np.load(folder+'labels-error'+str(error*2)+'.npy', mmap_mode="r")
    else:
        traces_t = np.load(folder+'traces-error'+str(error*2+1)+'.npy', mmap_mode="r")[:, cut_interval]
        labels_t = np.load(folder+'labels-error'+str(error*2+1)+'.npy', mmap_mode="r")
    traces_all = np.append(traces_all, traces_t, axis=0)
    labels_all = np.append(labels_all, labels_t, axis=0)
    return traces_all, labels_all

def load_traces_errorpattern(error):
    cut_interval = list(range(0, 312))
    traces_all = np.load(data_folders[0]+'traces-error'+str(error*2)+'.npy', mmap_mode="r")[:, cut_interval]
    labels_all = np.load(data_folders[0]+'labels-error'+str(error*2)+'.npy', mmap_mode="r")
    traces_all, labels_all = traces_append(traces_all, labels_all, data_folder, cut_interval, 0)
    for data_folder in data_folders[1:]:
        traces_all, labels_all = traces_append(traces_all, labels_all, data_folder, cut_interval, 1)
        traces_all, labels_all = traces_append(traces_all, labels_all, data_folder, cut_interval, 0)

    traces_all = denoise(std(traces_all, error), threshold=3)
    labels_all = np.where(labels_all==0, 0, 1)
    
    p = np.random.permutation(len(traces_all))
    print("creating a permlation of length", len(traces_all))
    traces_shuffled = traces_all[p]
    labels_shuffled = labels_all[p]

    print("shape of traces shuffled",traces_shuffled.shape)
    print("shape of labels shuffled",labels_shuffled.shape)

    return(traces_shuffled, labels_shuffled, traces_shuffled.shape[1])

def train_model(X_profiling, Y_profiling, save_file_name, byte, input_size, epochs=200, batch_size=1024, num_classes=256, error=None):  

    model = mm.create_model(num_classes, input_size, 0.001, 1024, 512, 256)

    if error == None:
        save_model = ModelCheckpoint(save_file_name+str(byte)+'.keras',monitor='val_accuracy',verbose=1,save_best_only=True,mode='max')
    else:
        save_model = ModelCheckpoint(save_file_name+str(byte)+'_'+str(error)+'.keras',monitor='val_accuracy',verbose=1,save_best_only=True,mode='max')
    es = EarlyStopping(monitor='val_accuracy', mode='max', verbose=1, patience=10)

    history = model.fit(x=X_profiling, y=to_categorical(Y_profiling, num_classes=num_classes), batch_size=batch_size, verbose=0, epochs=epochs, callbacks=[es, save_model], validation_split=0.3)

    return history
	
if __name__ == '__main__':

    init = time.time()
    mixed_precision.set_global_policy('mixed_float16')
    for error in range(80):
        (traces, labels, input_size) = load_traces_errorpattern(error)
    
        history = train_model(traces, labels, model_folder + model_name, error, input_size, epochs=200, batch_size=256, num_classes=2)
        gc.collect()

    end = time.time()

    print("The total running time was: ",((end-init)/60), " minutes.")  