import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import LayerNormalization, Dense, ReLU, Softmax, Conv1D, BatchNormalization, MaxPooling1D
from tensorflow.keras.optimizers import AdamW

def create_model(classes=256, input_size=800, learning_rate=0.01, dense1=256, dense2=256, dense3=256):
    input_shape = (input_size,)
    
    # Create model.
    model = Sequential()    
    model.add(LayerNormalization(input_shape=input_shape))

    model.add(Dense(dense1, kernel_initializer='he_uniform'))
    model.add(LayerNormalization())
    model.add(ReLU())

    if (dense2 != 0):
        model.add(Dense(dense2, kernel_initializer='he_uniform'))
        model.add(LayerNormalization())
        model.add(ReLU())

    if (dense3 != 0):
        model.add(Dense(dense3, kernel_initializer='he_uniform'))
        model.add(LayerNormalization())
        model.add(ReLU())

    model.add(Dense(classes, kernel_initializer='he_uniform'))
    model.add(Softmax())

    optimizer = AdamW(learning_rate=learning_rate, weight_decay=1e-4)  

    model.compile(loss='categorical_crossentropy', optimizer=optimizer, metrics=['accuracy'])

    print(model.summary())
    return model

