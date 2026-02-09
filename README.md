# Neural LOD Selector
Training a small neural network for GPU-driven and content-aware LOD selection. The model predicts the pixel error for different LOD levels of a given 3D asset. This is used to pick more conservative (or aggressive) LODs based on predicted loss in image quality.

The model is trained using a standalone C++ (Metal) simulation and pytorch:
<p align="center">
  <img width="100%" src="https://github.com/eldnach/neural-lods/blob/main/images/training_rock.gif?raw=true" alt="Training">
  <br>
  <em>Training LOD levels: pixel error highlighted in green </em>
</p>

<p align="left">
  <img width="100%" src="https://github.com/eldnach/neural-lods/blob/main/images/model.png?raw=true" alt="Assets">
</p>

The model perdicts a higher pixel error for the thin Sword, applying more conservative LODs to avoid decimating the delicate silouhette. While the chunkier boulder asset is picking more agressive LODs:
<p align="left">
  <img width="100%" src="https://github.com/eldnach/neural-lods/blob/main/images/lods.gif?raw=true" alt="LODs">
</p>

Neural inference is implemented in a compute shader for GPU acceleration. By applying culling and LOD selection on the GPU, we can indirectly render 100,000+ instances with very minimal overhead:
<p align="left">
  <img width="100%" src="https://github.com/eldnach/neural-lods/blob/main/images/gpu-driven.png?raw=true" alt="LODs">
</p>

We can train the model to automatically learn error prediction based on combination of different input features, such as the camera FOV:
<p align="center">
  <img width="75%" src="https://github.com/eldnach/neural-lods/blob/main/images/fov.gif?raw=true" alt="LODs">
  <br>
  <em>Per-instance LOD level visualized on the top right of the screen </em>
</p>

## Run the demo
1. Open `neural-lods-unity` using the Unity Editor (version 6.4 beta and later)
2. Navigate to `Scenes` and open `SampleBasic` / `SampleIndirectRendering` 
3. Press the Play button

## Re-train neural network 
1. Run `cmake --preset xcode-macos` in the terminal to generate an XCode project
2. Open the Unity sample project and import your asset using the `OBJImporter` script (`Tools`->`Import Obj and generate LODs`)
3. Copy the generated files into the `assets/lods` folder of the XCode build directory
4. Build and run the XCode project
5. Once the training simulation is done, save the generated training data (`training_data.json`) from the build folder to the `model-training` folder
6. Open the model-training folder using VSCode (or other IDE), configure venv using `requirements.txt` and run the script
7. Copy the generated .ONNX model (`lod_predictor.onnx`), scaler config file (`scaler_config.json`) and model weights (`lod_weights.onnx`) from the model-training folder into the asset folder of the Unity sample project
8. CPU Inference: Add a `NeuralLOD` component to your asset's game object, and assign the .ONNX odel and scaler config file
9. GPU Inference: Add a `NeuralLODRendererFeature` component to your active Renderer Asset, and assign the Meshes and scaler config + model weights files.

Feature selection can be applied by modifying the `train-lods.py` script. This allows you to train the model on other properties such as velocity and camera FOV.  You can also modify the training simulation in `training-sim.cpp` (found inside the `metal-app`/`source` folder).
