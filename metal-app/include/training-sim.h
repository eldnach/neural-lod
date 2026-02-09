#ifndef TRAININGSIM_H
#define TRAININGSIM_H

#include <simd/simd.h>
#include <chrono>
#include <random>
#include <iostream>
#include <fstream>
#include <mutex>

using TimePoint = std::chrono::high_resolution_clock::time_point;

const int TRAINING_SAMPLES = 1000;

struct InstanceState {
    simd::float3 position;
    simd::float3 velocity;
    float rotation;
    float rotVelocity;
    float scale;
    
};

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

struct TrainingData {
    int frameIndex;
    float screenWidth;
    float screenHeight;
    int lodCount;
    std::vector<TrainingEntry> entries;
};

class TrainingSim{
    public:
        TrainingSim(int meshCount, int instanceCount, float bounds, float aspectRatio);
        ~TrainingSim();
        void Start(TimePoint appStartTime);
        std::shared_ptr<TrainingData> Update(int screenWidth, int screenHeight);
        const std::vector<simd::float4x4>& GetWorldMatrices() const;
        const std::vector<simd::float4x4>& GetViewProjMatrices() const;
        const std::vector<simd::float3>& GetColors() const;
        void ProcessFrameData(std::shared_ptr<TrainingData> trainingData, const float* outputData, int screenWidth, int screenHeight, int samples);
        void SaveData(const std::string pathPrefix);
        float GetProgress();
    private:
        int _meshCount;
        int _instanceCount;
        float _instanceBounds;
        std::vector<InstanceState> _instanceData;
        std::vector<TrainingData> _trainingData;
        
        int _frameCounter;
        TimePoint _appStartTime;
        float _startTime;
        float _elapsedTime;
        float _lastElapsedTime;

        std::vector<simd::float3> _colors;
        std::vector<simd::float4x4> _worldMatrices;
        std::vector<simd::float4x4> _viewProjMatrices;
        std::vector<simd::float3> _cameraPositions;
        std::vector<float> _yScales;
        float _angleAccumulation;
        float _aspectRatio;

        TrainingData _currentFrame;
        float _progress;
        std::mutex _mutex;

        void SaveDataToJson(const std::string& filename);
};

namespace MathUtils {
    simd::float4x4 calcLookAtMatrix(simd::float3 eye, simd::float3 target, simd::float3 up);
    simd::float4x4 calcProjectionMatrix(float fovRadians, float aspect, float nearZ, float farZ);
    simd::float4x4 calcRotXMatrix(float radians);
    simd::float4x4 calcRotYMatrix(float radians);
}


#endif // TRAININGSIM_H
