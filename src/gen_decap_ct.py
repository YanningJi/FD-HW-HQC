# %%
import ctypes
import logging
import math
import os
import pickle
import numpy as np
import json
import sys
import random as random

# ==============================================================================
# HQC-128 Parameter Constants (Round 3 Reference)
# ==============================================================================
PARAM_N = 17669
PARAM_N2 = 384
PARAM_N1N2 = 17664
PARAM_OMEGA = 66
SHA512_BYTES = 64

VEC_N_SIZE_64 = (PARAM_N + 63) // 64
VEC_N1N2_SIZE_64 = (PARAM_N1N2 + 63) // 64
PARAM_N1 = 46
VEC_N1_SIZE_64 = (PARAM_N1 + 8 - 1) // 8
PARAM_K = 16
VEC_K_SIZE_64 = (PARAM_K + 7) // 8


# ==============================================================================
# C Structure Definitions
# ==============================================================================


class AES_XOF_struct(ctypes.Structure):
    _fields_ = [
        ("buffer", ctypes.c_ubyte * 16),
        ("buffer_pos", ctypes.c_int),
        ("length_remaining", ctypes.c_ulong),
        ("key", ctypes.c_ubyte * 32),
        ("ctr", ctypes.c_ubyte * 16),
    ]


# ==============================================================================
# Library Wrapper
# ==============================================================================
class HQCLibrary:
    CRYPTO_SECRETKEYBYTES = 2289
    CRYPTO_PUBLICKEYBYTES = 2249
    CRYPTO_CIPHERTEXTBYTES = 4481
    CRYPTO_BYTES = 64

    def __init__(self, lib_path):
        self._cdll = ctypes.CDLL(lib_path)
        self._bind_crypto_api()

    def _bind_crypto_api(self):
        u8_ptr = ctypes.POINTER(ctypes.c_ubyte)
        u64_ptr = ctypes.POINTER(ctypes.c_uint64)
        
        # Keypair generation
        self.crypto_kem_keypair = self._bind(
            "crypto_kem_keypair", ctypes.c_int, (u8_ptr, u8_ptr)
        )

        # Encapsulation
        self.crypto_kem_enc = self._bind(
            "crypto_kem_enc", ctypes.c_int, (u8_ptr, u8_ptr, u8_ptr)
        )
        
        # Decapsulation
        self.crypto_kem_dec = self._bind(
            "crypto_kem_dec", ctypes.c_int, (u8_ptr, u8_ptr, u8_ptr)
        )

        self.crypto_kem_dec_half = self._bind(
            "crypto_kem_dec_half", ctypes.c_int, (u8_ptr, u8_ptr, u8_ptr)
        )

        # Parsing / Key management
        self.hqc_public_key_from_string = self._bind(
            "hqc_public_key_from_string", None, (u64_ptr, u64_ptr, u8_ptr)
        )
        self.hqc_secret_key_from_string = self._bind(
            "hqc_secret_key_from_string", None, (u64_ptr, ctypes.POINTER(ctypes.c_uint32), u8_ptr, u8_ptr)
        )
        self.hqc_ciphertext_to_string = self._bind(
            "hqc_ciphertext_to_string", None, (u8_ptr, u64_ptr, u64_ptr, u8_ptr)
        )
        self.hqc_ciphertext_from_string = self._bind(
            "hqc_ciphertext_from_string", None, (u64_ptr, u64_ptr, u8_ptr, u8_ptr)
        )

        # Vector operations
        self.vect_add = self._bind(
            "vect_add", None, (u64_ptr, u64_ptr, u64_ptr, ctypes.c_uint32)
        )
        self.vect_mul = self._bind(
            "vect_mul", None, (u64_ptr, ctypes.POINTER(ctypes.c_uint32), u64_ptr, ctypes.c_uint16, ctypes.POINTER(AES_XOF_struct))
        )
        
        # Randomness / Utils
        self.seedexpander_init = self._bind(
            "seedexpander_init", ctypes.c_int, (ctypes.POINTER(AES_XOF_struct), u8_ptr, u8_ptr, ctypes.c_ulong)
        )
        self.set_seed = self._bind(
            "set_seed", None, (u8_ptr, u8_ptr)
        )
        self.reed_muller_decode_one_block = self._bind(
            "reed_muller_decode_one_block", None, (u64_ptr, u64_ptr)
        ) # this c function is added (not in the original library)

        self.y_value = self._bind(
            "read_y_value", None, (ctypes.POINTER(ctypes.c_uint32),)
        )

    def _bind(self, name, restype, argtypes):
        fn = getattr(self._cdll, name)
        fn.restype = restype
        fn.argtypes = argtypes
        return fn


# ==============================================================================
# Helper Functions
# ==============================================================================
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

def uint64_to_bits(array, length):
    """Convert HQC uint64 array into list of bits (index 0 -> bit 0)."""
    bits = []
    total = length if length is not None else len(array) * 64
    for idx in range(total):
        word = idx // 64
        offset = idx % 64
        if word >= len(array):
            break
        value = int(array[word])
        bits.append((value >> offset) & 1)
    return bits

def rm_decoder_result_wo_noise(eXORu, lib):
    """Output the RM decoder result for one block.""" 
    rm_decoder = lib.reed_muller_decode_one_block
    vector_64 = bit_array_to_uint64(eXORu)
    message = (ctypes.c_uint64 * 1)()
    rm_decoder(message, vector_64)
    byte_view = ctypes.cast(message, ctypes.POINTER(ctypes.c_uint8))
    return int(byte_view[0])

def obtain_rm_decoder_results(vector, n2, use_error_patterns, lib):
    vector_blocks = np.array_split(vector, int(len(vector)/n2))
    all_m = []
    for i, vector_block in enumerate(vector_blocks):
        for j, error_pattern in enumerate(use_error_patterns):
            vector_sum = np.bitwise_xor(vector_block, error_pattern)
            mtmp = rm_decoder_result_wo_noise(vector_sum, lib)
            all_m.append(mtmp)
    return all_m

def fd_pad_e(evec, n):
    """Pad error vector to length n by repeating it."""
    repeats = n // len(evec)
    remainder = n % len(evec)
    long_evec = np.tile(evec, repeats)
    if remainder > 0:
        long_evec = np.concatenate((long_evec, evec[:remainder]))
    return long_evec


def load_error_patterns_from_json(scheme, num_vec):

    template_path = f'../templates/{scheme}_80_rho0.7.json'
    
    logging.info(f"Loading error patterns from {template_path}")
    if not os.path.exists(template_path):
        raise FileNotFoundError(f"Template file not found: {template_path}")

    with open(template_path, 'r') as f:
        data = json.load(f)
        
    patterns = []
    for entry in data:
        patterns.append(np.array(entry['error_pattern'], dtype=int))
        
    # Return requested number of patterns
    if len(patterns) < num_vec:
        logging.warning(f"Requested {num_vec} patterns but found only {len(patterns)} in {template_path}")
    
    return patterns[:num_vec]

# %%
# ==============================================================================
# Main Execution
# ==============================================================================
dest_folder = "G:/profile-cct-input-4000/"
scheme='hqc128' 
num_error_patterns=80
key_num = 4000
lib = HQCLibrary("./libhqc-128.so")
setup_logging(script='gen_decap_ct', scheme=scheme)
# ==============================================================================

# Load error patterns from JSON template
use_error_patterns = load_error_patterns_from_json(scheme, num_error_patterns)
logging.info(f"Loaded {len(use_error_patterns)} error patterns")

logging.info(f"Generating cts for {key_num} key pairs with {num_error_patterns} error patterns...")

for key_idx in range(key_num):
    print(f"Processing key_idx {key_idx}")
    
    pk_seed = random.randbytes(40)
    sk_seed = random.randbytes(40)
    pk_seed_l = [int(x) for x in pk_seed]
    sk_seed_l = [int(x) for x in sk_seed]
    np.save(dest_folder+"seed-"+str(key_idx)+".npy", (pk_seed_l+sk_seed_l)[::-1])
    pk_seed = (ctypes.c_uint8 * len(pk_seed)).from_buffer_copy(pk_seed)
    sk_seed = (ctypes.c_uint8 * len(sk_seed)).from_buffer_copy(sk_seed)
    lib.set_seed(pk_seed, sk_seed)
    
    # 2. Generate Keypair
    pk = (ctypes.c_ubyte * lib.CRYPTO_PUBLICKEYBYTES)()
    sk = (ctypes.c_ubyte * lib.CRYPTO_SECRETKEYBYTES)()
    
    if lib.crypto_kem_keypair(pk, sk) != 0:
        raise RuntimeError("crypto_kem_keypair failed")

    # 3. Parse Public Key (get h, s)
    h = (ctypes.c_uint64 * VEC_N_SIZE_64)()
    s = (ctypes.c_uint64 * VEC_N_SIZE_64)()
    lib.hqc_public_key_from_string(h, s, pk)

    y = (ctypes.c_uint32 * PARAM_OMEGA)()
    lib.y_value(y)
    y = np.array(y, dtype=np.uint32)
    
    # 4. Parse Secret Key (get x and y coordinates)
    x = (ctypes.c_uint64 * VEC_N_SIZE_64)()
    y_coords = (ctypes.c_uint32 * PARAM_OMEGA)()
    pk_buf = (ctypes.c_uint8 * lib.CRYPTO_PUBLICKEYBYTES)()
    dk_pke_ptr = ctypes.cast(sk, ctypes.POINTER(ctypes.c_uint8))
    lib.hqc_secret_key_from_string(x, y_coords, pk_buf, dk_pke_ptr)

    y_bits = np.zeros(PARAM_N, dtype=int)
    for idx in y: 
        y_bits[int(idx)] = 1
    y = bit_array_to_uint64(y_bits)
    
    x_bits = uint64_to_bits(x, PARAM_N)
    
    # Initialize record for this key
    key_record = {
        "sk": bytes(sk),
        "pk": bytes(pk),
        "h": bytes(h),
        "s": bytes(s),
        "x": bytes(x),
        "y": bytes(y),
        "ct details": []
    }

    u_list = []
    v_list = []
    m_list = []
    # Loop through all error patterns to gen cts for this key
    for pat_idx, this_error_pattern in enumerate(use_error_patterns):
        
        # 6. Prepare Error Vector
        padded_e = fd_pad_e(this_error_pattern, PARAM_N1N2)
        padded_e_64 = bit_array_to_uint64(padded_e)

        # 7. Generate Ciphertexts (manually constructed)
        # X0 shift (equivalent to 1)
        X0_bit = np.zeros(PARAM_N, dtype=int)
        X0_bit[0] = 1
        X0 = bit_array_to_uint64(X0_bit)
        d = (ctypes.c_uint8 * SHA512_BYTES)()  # zeroed

        # --- x part --- r1=0, r2=1, u=r1+h*r2, v = mG+s*r2+e. m default to 0
        ux = h
        vx = (ctypes.c_uint64 * VEC_N1N2_SIZE_64)()
        lib.vect_add(vx, s, padded_e_64, VEC_N1N2_SIZE_64) 
        
        ct_bytes_x = (ctypes.c_ubyte * lib.CRYPTO_CIPHERTEXTBYTES)()
        lib.hqc_ciphertext_to_string(ct_bytes_x, ux, vx, d)

        ss_x = (ctypes.c_ubyte * lib.CRYPTO_BYTES)()
        lib.crypto_kem_dec(ss_x, ct_bytes_x, sk)

    
        # --- y part --- r1=1, r2=0
        uy = X0 
        vy = padded_e_64
        
        ct_bytes_y = (ctypes.c_ubyte * lib.CRYPTO_CIPHERTEXTBYTES)()
        lib.hqc_ciphertext_to_string(ct_bytes_y, uy, vy, d)
        
        ss_y = (ctypes.c_ubyte * lib.CRYPTO_BYTES)()
        lib.crypto_kem_dec(ss_y, ct_bytes_y, sk)

        # 8. Check RM Decode Results

        # Truncate
        
        x_bits = x_bits[:PARAM_N1N2]
        y_bits = y_bits[:PARAM_N1N2]
        
        m_x = obtain_rm_decoder_results(vector=x_bits, n2=PARAM_N2, use_error_patterns=[this_error_pattern], lib=lib)
        m_y = obtain_rm_decoder_results(vector=y_bits, n2=PARAM_N2, use_error_patterns=[this_error_pattern], lib=lib)
        
        binary_m_x = [0 if val == 0 else 1 for val in m_x]
        binary_m_y = [0 if val == 0 else 1 for val in m_y] 
        
        # Append result for this pattern
        key_record["ct details"].append({
            "error_pattern_idx": pat_idx, # index of this error pattern in the list of patterns used/in the template
            "error_pattern": this_error_pattern,
            "ct_x": bytes(ct_bytes_x),
            "ct_y": bytes(ct_bytes_y),
            "u_x": bytes(ux),
            "v_x": bytes(vx),
            "u_y": bytes(uy),
            "v_y": bytes(vy),
            "binary_m_x": binary_m_x,
            "binary_m_y": binary_m_y
        })

        # save cct and messag to send to FPGA
        ux_pad = np.zeros((32*70-len(ux)*8,))
        vx_pad = np.zeros((32*70-len(vx)*8,))
        uy_pad = np.zeros((32*70-len(uy)*8,))
        vy_pad = np.zeros((32*70-len(vy)*8,))
        ux_np = np.append(np.array((ctypes.c_uint8 * (VEC_N_SIZE_64*8)).from_buffer_copy(ux)), ux_pad)
        vx_np = np.append(np.array((ctypes.c_uint8 * (VEC_N1N2_SIZE_64*8)).from_buffer_copy(vx)), vx_pad)
        uy_np = np.append(np.array((ctypes.c_uint8 * (VEC_N_SIZE_64*8)).from_buffer_copy(uy)), uy_pad)
        vy_np = np.append(np.array((ctypes.c_uint8 * (VEC_N1N2_SIZE_64*8)).from_buffer_copy(vy)), vy_pad)
        u_np = np.append([ux_np], [uy_np], axis=0)
        v_np = np.append([vx_np], [vy_np], axis=0)
        m_np = np.append([m_x], [m_y], axis=0)
        u_list.append(u_np)
        v_list.append(v_np)
        m_list.append(m_np)

    np.save(dest_folder+"u-"+str(key_idx)+".npy", u_list)
    np.save(dest_folder+"v-"+str(key_idx)+".npy", v_list)
    np.save(dest_folder+"code-"+str(key_idx)+".npy", m_list)

    # Save record for this key
    os.makedirs("../my_output_2/ct", exist_ok=True)
    filename = f"../my_output_2/ct/hqc128_decap_ct_key{key_idx}.pkl"
    with open(filename, "wb") as fh:
        pickle.dump(key_record, fh, protocol=pickle.HIGHEST_PROTOCOL)
    logging.info(f"Saved {len(key_record['ct details'])} records to {filename}")

