import torch
import torch.nn as nn
import json
import pandas as pd
import numpy as np
from sklearn.preprocessing import StandardScaler
import os
import argparse
import struct

def train(data_path):
    if not os.path.exists(data_path):
        print(f"Error: Could not find data file at {data_path}")
        return

    print(f"Loading data from: {data_path}")
    with open(data_path, 'r') as f:
        data = json.load(f)
    
    rows = []
    for frame in data['frames']:
        for inst in frame['instances']:
            rows.append([
                inst['lod'],
                inst['dist'], 
                inst['angle'],
                inst['size'],
                inst['error']
            ])
    
    cols = ['lod', 'dist', 'angle', 'size', 'error']
    df = pd.DataFrame(rows, columns=cols)

    X = df.drop('error', axis=1).values 
    y = df['error'].values.reshape(-1, 1)

    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(script_dir, 'scaler_config.json'), 'w') as f:
        json.dump({"mean": scaler.mean_.tolist(), "std": scaler.scale_.tolist()}, f)

    class LODModel(nn.Module):
        def __init__(self):
            super(LODModel, self).__init__()
            self.net = nn.Sequential(
                nn.Linear(4, 64),      
                nn.LeakyReLU(0.1),
                nn.Linear(64, 32),     
                nn.LeakyReLU(0.1),
                nn.Linear(32, 1)
            )
        def forward(self, x):
            return self.net(x)

    model = LODModel()
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)
    criterion = nn.L1Loss()

    print(f"Training on {len(df)} samples...")
    X_tensor = torch.FloatTensor(X_scaled)
    y_tensor = torch.FloatTensor(y)

    for epoch in range(10000):
        optimizer.zero_grad()
        output = model(X_tensor)
        loss = criterion(output, y_tensor)
        loss.backward()
        optimizer.step()
        if epoch % 500 == 0:
            print(f"Epoch {epoch}, Loss: {loss.item():.6f}")

    # --- WEIGHT EXPORT  ---
    print("\n--- Exporting Weights ---")
    
    # Extract weights 
    w1 = model.net[0].weight.data.numpy().flatten() # 64 * 4 = 256
    b1 = model.net[0].bias.data.numpy().flatten()   # 64
    w2 = model.net[2].weight.data.numpy().flatten() # 32 * 64 = 2048
    b2 = model.net[2].bias.data.numpy().flatten()   # 32
    w3 = model.net[4].weight.data.numpy().flatten() # 1 * 32 = 32
    b3 = model.net[4].bias.data.numpy().flatten()   # 1
    
    all_weights = np.concatenate([w1, b1, w2, b2, w3, b3]).astype(np.float32)

    binary_path = os.path.join(script_dir, "lod_weights.bytes")
    with open(binary_path, "wb") as f:
        f.write(all_weights.tobytes())

    json_path = os.path.join(script_dir, "lod_weights.json")
    weights_dict = {
        "w1": w1.tolist(),
        "b1": b1.tolist(),
        "w2": w2.tolist(),
        "b2": b2.tolist(),
        "w3": w3.tolist(),
        "b3": b3.tolist(),
        "metadata": {
            "total_floats": len(all_weights),
            "layout": "Row-Major [Output, Input]",
            "offsets": {
                "W1": 0,
                "B1": 256,
                "W2": 320,
                "B2": 2368,
                "W3": 2400,
                "B3": 2432
            }
        }
    }
    with open(json_path, 'w') as f:
        json.dump(weights_dict, f, indent=4)

    model.eval()
    dummy_input = torch.randn(1, 4)
    onnx_path = os.path.join(script_dir, "lod_predictor.onnx")
    torch.onnx.export(model, dummy_input, onnx_path,
                      input_names=['input'], output_names=['output'],
                      dynamic_axes={'input': {0: 'batch_size'}})
    
    print(f"Success! Files saved to: {script_dir}")
    print(f"Total weights: {len(all_weights)} floats")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    default_data = os.path.join(os.path.dirname(__file__), "training_data.json")
    parser.add_argument("--data", type=str, default=default_data)
    args = parser.parse_args()
    train(args.data)