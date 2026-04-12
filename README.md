# Neural LOD Selector
Training a small network (using C++, Metal and Pytorch) to predict the pixel error of different LOD levels. The network is then embedded in a compute shader (HLSL), implementing neural LOD selection on the GPU. 

Combining this with Indirect Drawing, we can render 10,000's of instances more efficiency while applying GPU-side culling and LOD selection.

The model can automatically learn the relationships between different input features (e.g. object distance, screen size, velocity, FOV...) and the loss in image quality for each LOD level. We can then use the predicted pixel error to pick an optimal LOD. 

The neural network is first trained using a standalone C++ application:
<p align="center">
  <img width="100%" src="https://github.com/eldnach/neural-lod/blob/main/images/training_rock.gif?raw=true" alt="Training">
  <br>
  <em>Training LOD levels: pixel error highlighted in green </em>
</p>

<p align="left">
  <img width="100%" src="https://github.com/eldnach/neural-lod/blob/main/images/model.png?raw=true" alt="Assets">
</p>

We can use the model prediction to apply content-aware LODs based on the mesh characteristics. In this example, the delicate Sword asset maintains higher quality LODs at a distance, while the Rock uses more aggressive LODs. Feature selection is used to train the model based on combination of different parameters. By including the camera FOV as an input feature, the model can also switch to higher quality LODs when zooming on objects:
<p align="left">
  <img width="100%" src="https://github.com/eldnach/neural-lod/blob/main/images/lod-zoom.gif?raw=true" alt="LODs">
</p>

Neural inference is implemented in HLSL (compute) for GPU acceleration. The compute pass for ~130K instances is measured at ~0.5ms GPU time on a Macbook (M4). This includes inference, LOD selection and frustrum culling:
<p align="left">
  <img width="100%" src="https://github.com/eldnach/neural-lod/blob/main/images/gputime.png?raw=true" alt="Compute">
</p>

 By implementing culling and LOD selection on the GPU, we can indirectly render many mesh instances with very minimal overhead. Total GPU frame time for ~130K instances is measured at around 3.7ms: 
<p align="left">
  <img width="100%" src="https://github.com/eldnach/neural-lod/blob/main/images/fov.gif?raw=true" alt="Compute">
</p>

## Run the demo
1. Open the `unity-sample` project using the Unity Editor (version 6.4 beta and later)
2. Navigate to `Scenes` and open `SampleBasicZoom` / `SampleIndirectRendering` 
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
