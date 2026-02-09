#include "training-sim.h"

namespace MathUtils {

    simd::float4x4 calcLookAtMatrix(simd::float3 eye, simd::float3 target, simd::float3 up) {
        simd::float3 z = simd::normalize(eye - target); // Forward (points away from target in RH)
        simd::float3 x = simd::normalize(simd::cross(up, z)); // Right
        simd::float3 y = simd::cross(z, x); // Up

        simd::float4x4 m = {
            (simd::float4){ x.x, y.x, z.x, 0 },
            (simd::float4){ x.y, y.y, z.y, 0 },
            (simd::float4){ x.z, y.z, z.z, 0 },
            (simd::float4){ -simd::dot(x, eye), -simd::dot(y, eye), -simd::dot(z, eye), 1 }
        };
        return m;
    }

    simd::float4x4 calcProjectionMatrix(float fovRadians, float aspect, float nearZ, float farZ) {
        float ys = 1.0f / tanf(fovRadians * 0.5f);
        float xs = ys / aspect;
        float zs = farZ / (nearZ - farZ);
        
        return simd::float4x4(
            (simd::float4){ xs,   0,    0,           0  },  // Column 0
            (simd::float4){ 0,    ys,   0,           0  },  // Column 1
            (simd::float4){ 0,    0,    zs,          -1 },  // Column 2
            (simd::float4){ 0,    0,    nearZ * zs,  0  }   // Column 3
        );
    }

    simd::float4x4 calcRotXMatrix(float radians) {
        float c = cosf(radians);
        float s = sinf(radians);
        return simd_matrix(
        (simd::float4){ 1,  0,  0, 0 },
        (simd::float4){ 0,  c,  s, 0 },
        (simd::float4){ 0, -s,  c, 0 },
        (simd::float4){ 0,  0,  0, 1 }
        );
    }

    simd::float4x4 calcRotYMatrix(float radians) {
        float c = cosf(radians);
        float s = sinf(radians);
        return simd_matrix(
            (simd::float4){  c, 0, -s, 0 },
            (simd::float4){  0, 1,  0, 0 },
            (simd::float4){  s, 0,  c, 0 },
            (simd::float4){  0, 0,  0, 1 }
        );
    }
}

TrainingSim::TrainingSim(int meshCount, int instanceCount, float bounds, float aspectRatio) {
    _meshCount = meshCount;
    _instanceCount = instanceCount;
    _instanceBounds = bounds;
    _aspectRatio = aspectRatio;
    _instanceData.resize(_instanceCount);
    _viewProjMatrices.resize(_instanceCount);
    _yScales.resize(_instanceCount);
    _cameraPositions.resize(_instanceCount);
    _worldMatrices.resize(_instanceCount);
    _colors.resize(_instanceCount);
    _trainingData.reserve(TRAINING_SAMPLES);
}

void TrainingSim::Start(TimePoint appStartTime) {
    _appStartTime = appStartTime;
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = currentTime - _appStartTime;
    float currentTotalTime = elapsed.count();
    _startTime = currentTotalTime;

    _frameCounter = 0;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> _posRange(0.0f, 0.0f);
    std::uniform_real_distribution<float> _scaleRange(1.0f, 1.0f);
    std::uniform_real_distribution<float> _rotRange((float)M_PI * 0.05f, (float)M_PI * 0.8f);

    _angleAccumulation = 0.0f;

    for (int i = 0; i < (int)_instanceData.size(); i++) {
        auto& state = _instanceData[i];
        state.rotation = _rotRange(gen);
        state.scale = _scaleRange(gen);
        state.position = _posRange(gen);
        state.velocity = { 0.0f, 0.0f, 0.0f };
    }
}

std::shared_ptr<TrainingData> TrainingSim::Update(int screenWidth, int screenHeight) {

    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = currentTime - _appStartTime;
    float currentTotalTime = elapsed.count();
    
    _elapsedTime = currentTotalTime - _startTime;
    float deltaTime = _elapsedTime - _lastElapsedTime;
    _lastElapsedTime = _elapsedTime;
    
    simd::float3 target = {0.0f, 0.0f, 0.0f};
    simd::float3 up      = {0.0f, 1.0f, 0.0f};
    simd::float3 rotAxis = {0.0f, 1.0f, 0.0f};
    
    float maxRotation = 2.0f * M_PI;
    _angleAccumulation = _angleAccumulation + M_PI * 0.1f * deltaTime;
    float rotAngle = _angleAccumulation;
    _progress = rotAngle / maxRotation;
    
    if (_angleAccumulation >= maxRotation) {
        std::cout << "Training completed in " + std::to_string(_elapsedTime) + "s. Shutting down training simulation..." << std::endl;
        return nullptr;
    }
    
    float rotSin = sinf(rotAngle);
    float rotCos = cosf(rotAngle);
    simd::float3x3 rotMatrix = simd::float3x3(
        (simd::float3){ rotCos, 0, -rotSin},
        (simd::float3){ 0,      1,       0},
        (simd::float3){ rotSin, 0,   rotCos}
    );
    
    float aspect = _aspectRatio;
    float fov = 45.0f * (M_PI / 180.0f);
    simd::float4x4 projMatrix = MathUtils::calcProjectionMatrix(fov, aspect, 0.1f, 100.0f);
    
    simd::float2 zoomRange = {-1.5f, -100.0f};
    
    for (int i=0; i<_instanceCount; i++){
        float t = ((float)i / (float)_instanceCount);
        float radius = zoomRange[0] + zoomRange[1] * t * t * t;
        simd::float3 dir = {0.0f, 0.0f, -1.0f};
        _cameraPositions[i] = target + (rotMatrix * dir) * radius;
        simd::float4x4 viewMatrix = MathUtils::calcLookAtMatrix(_cameraPositions[i], target, up);;
        _viewProjMatrices[i] = (simd_mul(projMatrix, viewMatrix));
        _yScales[i] = projMatrix.columns[1][1];
    }
        
    for (int i = 0; i < _instanceCount; i++) {
        auto& s = _instanceData[i];
        
        simd::float4x4 T = simd::float4x4(
                                          (simd::float4){ 1, 0, 0, 0 },
                                          (simd::float4){ 0, 1, 0, 0 },
                                          (simd::float4){ 0, 0, 1, 0 },
                                          (simd::float4){ s.position.x, s.position.y, s.position.z, 1 }
                                          );
        
        s.rotation += s.rotVelocity;
        simd::float4x4 R = MathUtils::calcRotXMatrix(s.rotation);
        
        simd::float4x4 S = simd::float4x4(
                                          (simd::float4){ s.scale, 0, 0, 0 },
                                          (simd::float4){ 0, s.scale, 0, 0 },
                                          (simd::float4){ 0, 0, s.scale, 0 },
                                          (simd::float4){ 0, 0, 0,       1 }
                                          );
        
        _worldMatrices[i] = simd_mul(T, simd_mul(R, S));
        _colors[i] = simd::float3{ (float)i/_instanceCount, 0.5f, 1.0f };
    }
        
    std::shared_ptr<TrainingData> frameData = std::make_shared<TrainingData>();
    // Record input features
    // ------------------------------------------------------
    if (_trainingData.size() < TRAINING_SAMPLES) {
        frameData->lodCount = _meshCount;
        frameData->frameIndex = _frameCounter;
        frameData->screenWidth = screenWidth;
        frameData->screenHeight = screenHeight;
        
        for (int i = 0; i < _instanceCount; i++) {
            auto& ins = _instanceData[i];
            
            // Calculate Screen Position
            simd::float4 clipPos = simd_mul(_viewProjMatrices[i], (simd::float4){ins.position.x, ins.position.y, ins.position.z, 1.0f});
            float w = clipPos.w;
            float ndcX = ((clipPos.x / w) + 1.0f) * 0.5f;
            float ndcY = ((clipPos.y / w) + 1.0f) * 0.5f;
            
            float dist = simd::distance(ins.position, _cameraPositions[i]);
            simd::float3 cameraToObj = simd::normalize(ins.position - _cameraPositions[i]);
            simd::float3 cameraForward = simd::float3{0, 0, -1};
            
            // View angle (radians):  acos(dot(cameraToObject, cameraForward))
            float angle = acos(simd::clamp(simd::dot(cameraToObj, cameraForward), -1.0f, 1.0f));
            
            // Calculate projected height on screen
            float objectWorldHeight = _instanceBounds * ins.scale;
            float projHeight = (objectWorldHeight * _yScales[i]) / fmaxf(0.1f, dist);
            
            /*
             struct TrainingEntry {
             float lod; // LOD index (0-3)
             float velX, velY, velZ; // World pos velocity
             float distance; // Distance from the camera (world space)
             float viewAngle; // View angle (radians):  acos(dot(cameraToObject, cameraForward))
             float yScale; // ProjectionMatrix[1][1]
             float projectedHeight; // Screen size
             float ndcX, ndcY; // Screen pos
             float pixelError; // Pixel error
             };
             */
            frameData->entries.push_back({
                0.0f,
                ins.velocity.x, ins.velocity.y, ins.velocity.z,
                dist,
                angle,
                _yScales[i],
                projHeight,
                ndcX, ndcY,
                0.0f
            });
        }
    }

    return frameData;
}
    
float TrainingSim::GetProgress(){
    return _progress;
}

const std::vector<simd::float4x4>& TrainingSim::GetWorldMatrices() const {
    return _worldMatrices;
}

const std::vector<simd::float4x4>& TrainingSim::GetViewProjMatrices() const{
    return _viewProjMatrices;
}

const std::vector<simd::float3>& TrainingSim::GetColors() const{
    return _colors;
}

void TrainingSim::ProcessFrameData(std::shared_ptr<TrainingData> trainingData, const float* outputData, int screenWidth, int screenHeight, int stride){
    if (_trainingData.size() > TRAINING_SAMPLES) {
        return;
    }

    std::vector<TrainingEntry> initialEntries = std::move(trainingData->entries);
        
    trainingData->entries.clear();
    trainingData->entries.reserve(initialEntries.size() * trainingData->lodCount);

    for (int i = 0; i < (int)initialEntries.size(); i++) {
        int baseIndex = i * trainingData->lodCount;
        for (int j = 0; j < trainingData->lodCount; j++) {
            TrainingEntry entry = initialEntries[i];
            entry.lod = (float)j / (float)(trainingData->lodCount - 1);
            float rawError = outputData[baseIndex + j];
            float pixelError = (rawError >= 0.01f) ? rawError : 0.0f;
            entry.pixelError = pixelError;
            trainingData->entries.push_back(entry);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _trainingData.push_back(std::move(*trainingData));
    }
}

void TrainingSim::SaveData(const std::string pathPrefix) {
    {
    std::lock_guard<std::mutex> lock(_mutex);

    // Sort the data by frame index
    std::sort(_trainingData.begin(), _trainingData.end(),
        [](const TrainingData& a, const TrainingData& b) {
            return a.frameIndex < b.frameIndex;
    });

    SaveDataToJson(pathPrefix + "training_data.json");
    }
}

void TrainingSim::SaveDataToJson(const std::string& filename) {
    if (_trainingData.empty()) {
        printf("Warning: No training history to save.\n");
        return;
    }

    std::ofstream jsonFile(filename);
    if (!jsonFile.is_open()) {
        std::cerr << "Error: Could not open " << filename << " for writing!" << std::endl;
        return;
    }

    // Set precision to ensure floating point values are preserved for AI training
    jsonFile << std::fixed << std::setprecision(6);

    jsonFile << "{\n  \"frames\": [\n";

    for (size_t f = 0; f < _trainingData.size(); ++f) {
        const auto& frame = _trainingData[f];
        jsonFile << "    {\n";
        jsonFile << "      \"frame\": " << frame.frameIndex << ",\n";
        jsonFile << "      \"screen\": [" << frame.screenWidth << "," << frame.screenHeight << "],\n";
        jsonFile << "      \"instances\": [\n";
        
        for (size_t i = 0; i < frame.entries.size(); ++i) {
            const auto& e = frame.entries[i];

            jsonFile << "        { \"lod\":" << std::fixed << std::setprecision(2) << e.lod;
            jsonFile << ", \"vel\":[" << e.velX << "," << e.velY << "," << e.velZ << "]";
            jsonFile << ", \"dist\":" << e.distance;
            jsonFile << ", \"angle\":" << e.viewAngle;
            jsonFile << ", \"size\":" << e.projectedHeight;
            jsonFile << ", \"ndc\":[" << e.ndcX << "," << e.ndcY << "]";
            jsonFile << ", \"error\":" << std::setprecision(8) << e.pixelError << " }";
            
            if (i < frame.entries.size() - 1) jsonFile << ",";
            jsonFile << "\n";

            if (f == 0 && i < frame.lodCount) {
                printf("DEBUG: Frame %zu, Entry %zu, LOD: %f, Error: %f\n", f, i, e.lod, e.pixelError);
            }
        }
        
        jsonFile << "      ]\n    }";
        if (f < _trainingData.size() - 1) jsonFile << ",";
        jsonFile << "\n";
    }

    jsonFile << "  ]\n}";
    jsonFile.close();
    
    printf("Successfully exported %zu frames of training data to: %s\n", _trainingData.size(), filename.c_str());
}

TrainingSim::~TrainingSim(){
}

