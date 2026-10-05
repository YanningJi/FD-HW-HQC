import numpy as np
import matplotlib.pyplot as plt
import scipy.signal as signal
from scipy import stats

def denoise(traces, threshold=3):
    f, t, Zxx = signal.stft(traces, fs=10e6, nperseg=32)
    Zxx[:, :threshold, :] = 0
    _, traces_d = signal.istft(Zxx, fs=10e6, nperseg=32)
    return traces_d

def t_test(traces_0,traces_1, byte=0):
    print(len(traces_0),len(traces_1))
    (statistic,pvalue) = stats.ttest_ind(traces_0, traces_1, equal_var = False)
    m_index = statistic.argsort()[-5:][::]
    print(statistic[m_index])
    m_index = statistic.argsort()[0:5][::]
    print(statistic[m_index])
    return (statistic)

data_folder = "G:/v5-profile-cct-4000-2/"
label_folder = "G:/profile-cct-input-4000-2/"

traces = []
labels = []
for j in range(4000):
    traces_t = np.load(data_folder+'traces-'+str(j)+'.npy', mmap_mode="r")[:2, :]
    labels_t = np.load(label_folder+'code-'+str(j)+'.npy', mmap_mode="r")[:1, :2, :]
    traces.append(traces_t)
    labels.append(labels_t)

traces = np.array(traces)
labels = np.array(labels)
traces = traces.reshape(-1, traces.shape[-1])
labels = labels.reshape(-1, labels.shape[-1])
traces = denoise(np.array(traces), threshold=3)
labels = np.array(labels)

print("final shape of traces",traces.shape)
print("final shape of labels",labels.shape)
n = traces.shape[0]

for j in range(3, 46): 
    trace_0 = traces[np.where(labels[:,j] == 0)]
    trace_1 = traces[np.where(labels[:,j] != 0)]

    stat = t_test(trace_0,trace_1, j)
    plt.plot(stat, color='grey')

for j in range(3):
    trace_0 = traces[np.where(labels[:,j] == 0)]
    trace_1 = traces[np.where(labels[:,j] != 0)]

    stat = t_test(trace_0,trace_1, j)
    plt.plot(stat)

plt.axhline(y=4.5, color='r', linestyle='--')
plt.axhline(y=-4.5, color='r', linestyle='--')

plt.xlabel("Sample Points", fontsize='x-large')
plt.ylabel("T-Value", fontsize='x-large')
plt.show()