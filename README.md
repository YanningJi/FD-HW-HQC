# Beyond Microcontrollers: End-to-End Full Decryption Oracle Power Attacks of HQC on FPGAs

This is the repo for the paper **Beyond Microcontrollers: End-to-End Full Decryption Oracle Power Attacks of HQC on FPGAs**.

## Folder structure
```
main folder/
├─ hqc-128/                                 # Third-round submisson of hqc-128, for generating linhqc-128.so
├─ src/
│      cal_stats_using_oracle_output.py     # Script for testing predicted results
│      config.json
│      gen_decap_ct.py                      # Script for generating chosen ciphertexts as inputs to FPGA
│      libhqc-128.so
│      util.py
├─templates/                                # Template for generating chosen ciphertexts
├─top_wrapper/                              # FPGA wrapper to communicate between chipwhisperer and the crypto core
│
│  hqc_capture.py                           # Script for capturing power traces from FPGA
│  my_models.py
│  plot_t-test.py                           # Script for ploting t-test figure
│  testing_hqc_cct_error.py                 # Script for testing MLP models and recoding prediction results
│  training_hqc.py                          # Script for training MLP models 
```
