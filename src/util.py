import ctypes
import logging
import numpy as np
import random
import matplotlib
import json
import math
import os


def load_config(scheme):
    with open('./src/config.json', 'r') as file:
        config = json.load(file)
    return config.get(scheme, {})



def load_lib(lib_path):
    lib = ctypes.CDLL(lib_path)
    return lib



def sample_binary_vector(n2, min_weight, max_weight):
    #Sample a binary random vector of length n2 with Hamming weight between min_weight and max_weight.

    # Calculate actual Hamming weight bounds
    lower_bound = int(min_weight * n2)
    upper_bound = int(max_weight * n2)
    
    # Generate a random Hamming weight within the bounds
    weight = random.randint(lower_bound, upper_bound)
    
    # Create the vector
    vector = np.zeros(n2, dtype=int)
    vector[:weight] = 1
    np.random.shuffle(vector)
    
    return vector

def mutate_bit_by_bit(vector): # only flip one bit
    flip_index = random.randint(0, len(vector) - 1)
    # Flip the bit: XOR with 1 will toggle the bit at flip_index
    vector[flip_index] ^= 1
    return vector

def mutate2bits(vector): # flip two bits

    flip_indices = random.sample(range(len(vector)), 2)

    # Flip the bits at the selected indices: XOR with 1 will toggle the bits
    for flip_index in flip_indices:
        vector[flip_index] ^= 1
    return vector

# Global scheme data, distribution of u
scheme_data = {
    'hqc128': {'n2': 384, 'probabilities': [23.44, 34.38, 24.83, 11.77, 4.12, 1.45]},
    'hqc192': {'n2': 640, 'probabilities': [16.50, 30.00, 27.00, 16.04, 7.07, 3.40]},
    'hqc256': {'n2': 640, 'probabilities': [23.14, 34.06, 24.87, 12.02, 4.32, 1.59]}
}

def sample_vector_from_scheme(scheme):
    if scheme not in scheme_data:
        raise ValueError(f"Scheme {scheme} not recognized. Available schemes are: {list(scheme_data.keys())}.")
    
    data = scheme_data[scheme]
    n2 = data['n2']
    probabilities = np.array(data['probabilities'])
    probabilities /= 100  # Convert percentages to a proper probability sum
    probabilities /= probabilities.sum()

    # Generate the number of ones in the vector
    number_of_ones = np.random.choice(np.arange(len(probabilities)), p=probabilities)

    # Create the binary vector
    vector = np.zeros(n2, dtype=int)
    vector[:number_of_ones] = 1
    np.random.shuffle(vector)
    
    return vector



def bernoulli_noise(rho):
    """
    Generate Bernoulli noise.
    
    Returns 0 with probability rho and 1 with probability 1 - rho.
    
    :param rho: Probability of returning 0.
    :return: int (0 or 1)
    """
    return 0 if random.random() < rho else 1

def setup_logging(script, scheme):
    LOG_FORMAT = '%(asctime)s - %(levelname)s - %(message)s'
    DATE_FORMAT = '%m/%d/%Y %H:%M:%S'
    # Clean up existing handlers
    for handler in logging.root.handlers[:]:
        logging.root.removeHandler(handler)
    os.makedirs("logs", exist_ok=True)
    logging.basicConfig(level=logging.DEBUG, format=LOG_FORMAT, datefmt=DATE_FORMAT, handlers=[
        logging.FileHandler(f'./logs/{script}_{scheme}.log'),
    ])

def bit_array_to_uint64(vector):
    """Convert a binary array (LSB-first) into packed uint64 words."""
    len_64 = max(1, math.ceil(len(vector) / 64))
    uint64_values = []
    for word_index in range(len_64):
        word = 0
        base = word_index * 64
        for bit_offset in range(64):
            bit_index = base + bit_offset
            if bit_index < len(vector) and vector[bit_index] & 1:
                word |= 1 << bit_offset
        uint64_values.append(word)
    return (ctypes.c_uint64 * len_64)(*uint64_values)

def rm_decoder_result_wo_noise(eXORu, lib):
    """Output the RM decoder result for one block."""
    rm_decoder = lib.reed_muller_decode_one_block
    vector_64 = bit_array_to_uint64(eXORu)
    message = (ctypes.c_uint64 * 1)()
    rm_decoder(message, vector_64)
    byte_view = ctypes.cast(message, ctypes.POINTER(ctypes.c_uint8))
    return int(byte_view[0])