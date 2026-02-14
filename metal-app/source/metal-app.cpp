#include <iostream>
#include <fstream>
#include <cmath>
#include <unordered_map>
#include <chrono>

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "metal-renderer.h"
#include "training-sim.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <simd/simd.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;
const Uint32 WINDOW_FLAGS = SDL_WINDOW_METAL | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_RESIZABLE;
MTL::PixelFormat FRAMEBUFFER_FORMAT = MTL::PixelFormatRGBA8Unorm;

// Utility
// --------------------------------------------------------------
struct IndexKey {
    int v, n, t;
    bool operator==(const IndexKey& other) const {
        return v == other.v && n == other.n && t == other.t;
    }
};

namespace std {
    template<> struct hash<IndexKey> {
        size_t operator()(const IndexKey& k) const {
            // "Combining" hashes using bit-shifting
            return ((hash<int>{}(k.v) ^ (hash<int>{}(k.n) << 1)) >> 1) ^ (hash<int>{}(k.t) << 1);
        }
    };
}

bool operator==(const Vertex& a, const Vertex& b) {
    return std::memcmp(&a, &b, sizeof(Vertex)) == 0;
}

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(const Vertex& v) const {
            size_t h1 = hash<float>{}(v.position[0]);
            size_t h2 = hash<float>{}(v.position[1]);
            size_t h3 = hash<float>{}(v.position[2]);
            size_t h4 = hash<float>{}(v.texcoord[0]);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}

std::string readShaderSource(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::unique_ptr<Mesh> loadObj(std::string path){
    float minY = 1e9f;
    float maxY = -1e9f;
    float maxRadiusSq = 0.0f;
    
    std::vector<Vertex> tempVertices;
    std::vector<uint32_t> tempIndices;
    std::unordered_map<IndexKey, uint32_t> uniqueVertices;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string modelPath = path;
    std::string loader_warn, loader_err;
    if (tinyobj::LoadObj(&attrib, &shapes, &materials, &loader_warn, &loader_err, modelPath.c_str())) {
        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {

                IndexKey key = { index.vertex_index, index.normal_index, index.texcoord_index };
                
                if (uniqueVertices.find(key) == uniqueVertices.end()) {

                    Vertex v;
                    v.position[0] = attrib.vertices[3 * index.vertex_index + 0];
                    v.position[1] = attrib.vertices[3 * index.vertex_index + 1];
                    v.position[2] = attrib.vertices[3 * index.vertex_index + 2];
                    
                    if (!attrib.colors.empty()) {
                        v.color[0] = attrib.colors[3 * index.vertex_index + 0];
                        v.color[1] = attrib.colors[3 * index.vertex_index + 1];
                        v.color[2] = attrib.colors[3 * index.vertex_index + 2];
                    } else {
                        v.color[0] = 1.0f; v.color[1] = 1.0f; v.color[2] = 1.0f;
                    }
                    if (index.normal_index >= 0) {
                        v.normal[0] = attrib.normals[3 * index.normal_index + 0];
                        v.normal[1] = attrib.normals[3 * index.normal_index + 1];
                        v.normal[2] = attrib.normals[3 * index.normal_index + 2];
                    }
                    if (index.texcoord_index >= 0) {
                        v.texcoord[0] = attrib.texcoords[2 * index.texcoord_index + 0];
                        // Flip the UV.y coordinate
                        v.texcoord[1] = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];
                    } else {
                        v.texcoord[0] = 0.0f;
                        v.texcoord[1] = 0.0f;
                    }
                    uniqueVertices[key] = static_cast<uint32_t>(tempVertices.size());
                    tempVertices.push_back(v);
                }
                
                tempIndices.push_back(uniqueVertices[key]);
                
                float y = attrib.vertices[3 * index.vertex_index + 1];
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                
                float x = attrib.vertices[3 * index.vertex_index + 0];
                float z = attrib.vertices[3 * index.vertex_index + 2];
                float distSq = x*x + y*y + z*z;
                if (distSq > maxRadiusSq) maxRadiusSq = distSq;
            }
        }
    }
    auto mesh = std::make_unique<Mesh>("Mesh:" + path);
    mesh->SetData(tempVertices, tempIndices);
    mesh->SetBounds(maxY - minY, sqrt(maxRadiusSq));
    return mesh;
}

// --------------------------------------------------------------
struct FrameConstants{
    float progress;
};

struct InstanceConstants{
    simd::float4x4 viewProjMatrix;
    simd::float4x4 worldMatrix;
    simd::float3 color;
};
 
struct MaterialConstants{
    float matID;
};

namespace fs = std::filesystem;

int main( int argc, char* argv[] )
{
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    NS::AutoreleasePool* autoPool = NS::AutoreleasePool::alloc()->init();

    SDL_Window* window = NULL;
    SDL_Surface* screenSurface = NULL;
 
    std::string pathPrefix = "";

    if (!SDL_Init(SDL_INIT_VIDEO)){
        printf("SDL could not initialize! SDL_Error: %s \n", SDL_GetError());
        return -1;
    } else {
        window = SDL_CreateWindow("SDL Window: Metal App", SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_FLAGS);
        if (window == NULL){
            printf("SDL Window not created! SDL_Error: %s \n", SDL_GetError() );
        } else {
            
            const char* base_path = SDL_GetBasePath();
            if (base_path) {
                pathPrefix = base_path;
                SDL_free((void*)base_path);
            }
            
            // Create metal device
            // -------------------------------------------------------------------------
            MTL::Device* metalDevice = MTL::CreateSystemDefaultDevice();
            
            // Create metal view (CAMetalLayer-backed NS/UI View) and attach view to window
            void* metalView = SDL_Metal_CreateView(window);
            // Return pointer to metal view's backing CAMetalLayer
            void* caMetalLayer = SDL_Metal_GetLayer(metalView);
            CA::MetalLayer* metalLayer = (CA::MetalLayer*) caMetalLayer; // cast it to CA::MetalLayer
            
            metalLayer->setPixelFormat(FRAMEBUFFER_FORMAT); // set framebuffer format
            metalLayer->setDevice(metalDevice); // assign metal device to metal view
            
            // Create graphics context
            // -------------------------------------------------------------------------
            std::unique_ptr<MetalContext> mtlContext = std::make_unique<MetalContext>(metalDevice, metalLayer, sizeof(FrameConstants));
            
            int winWidth;
            int winHeight;
            SDL_GetWindowSize(window, &winWidth, &winHeight);
            float winScale = SDL_GetWindowDisplayScale(window);
            mtlContext->ResizeDrawable(winWidth, winHeight, winScale);

            // Load mesh from disk
            // -------------------------------------------------------------------------
            std::string assetPath = pathPrefix + "assets/lods/";
            std::vector<std::string> modelPaths;
            try {
                for (const auto& entry : fs::directory_iterator(assetPath)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".obj") {
                        modelPaths.push_back(entry.path().string());
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error accessing folder: " << e.what() << std::endl;
                return -1;
            }
            std::sort(modelPaths.begin(), modelPaths.end());
            if (modelPaths.empty()) {
                std::cerr << "No .obj files found in " << assetPath << std::endl;
                return -1;
            }
            
            if (modelPaths.size() > 5) {
                std::cout << "Attempted to load more than 5 mesh LODs. Simulation is limited to up to 5 LOD levels" << std::endl;
                modelPaths.resize(5);
            }
            
            std::vector<std::unique_ptr<Mesh>> meshes;
            for (const auto& path : modelPaths) {
                std::unique_ptr<Mesh> mesh = loadObj(path);
                meshes.push_back(std::move(mesh));
            }
            
            // Load texture from disk
            // -------------------------------------------------------------------------
            int texWidth, texHeight, texChannels;
            int texBPP = 4; // STBI_rgb_alpha = 4 bytes per pixel (8 bit per channel)
            std::string texturePath = pathPrefix + "assets/texture.png";
            unsigned char* rawPixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            size_t texDataSize;

            if (!rawPixels) {
                std::cout << "Failed to load texture: " << texturePath << std::endl;
                texWidth = 512;
                texHeight = 512;
                texChannels = 4;
                texDataSize = texWidth * texHeight * texChannels;
                rawPixels = new unsigned char[texDataSize];

                for (size_t i = 0; i < texDataSize; i++)
                {
                    rawPixels[i] = 0;
                }
            } 
            texDataSize = texWidth * texHeight * texBPP;
            std::vector<uint8_t> textureData(rawPixels, rawPixels + texDataSize);
            stbi_image_free(rawPixels);

            // Load shaders from disk
            // -------------------------------------------------------------------------
            std::string shaderID = std::to_string(meshes.size());
            std::vector<std::string> shaderPaths = {
                pathPrefix + "assets/shader.metal",
                pathPrefix + "assets/debug_" + shaderID + "lods.metal"
            };
            std::vector<NS::String*> shaderSource;
            for (const auto& path : shaderPaths) {
                std::string source = readShaderSource(path);
                if (source.empty()) continue;
                NS::String* nSrc = NS::String::string(source.c_str(), NS::UTF8StringEncoding);
                shaderSource.push_back(nSrc);
            }

            std::vector<std::string> kernelPaths = {
                pathPrefix + "assets/pixel_error_" + shaderID + "lods.metal"
            };
            std::vector<NS::String*> kernelSource;
            for (const auto& path : kernelPaths) {
                std::string source = readShaderSource(path);
                if (source.empty()) continue;
                NS::String* nSrc = NS::String::string(source.c_str(), NS::UTF8StringEncoding);
                kernelSource.push_back(nSrc);
            }
            
            // Create libraries
            // -------------------------------------------------------------------------
            std::vector<MTL::Library*> libraries;
            std::vector<MTL::Function*> vertexShaders;
            std::vector<MTL::Function*> fragmentShaders;

            for (NS::String* pSrc : shaderSource) {
                const MTL::CompileOptions* compileOptions = nullptr;
                MTL::Library* lib = mtlContext->CreateLibrary(pSrc, compileOptions);
                libraries.push_back(lib);
                
                const NS::String* vertStageName = NS::String::string("VertexStage", NS::UTF8StringEncoding);
                const NS::String* fragStageName = NS::String::string("FragmentStage", NS::UTF8StringEncoding);
                MTL::Function* vStage = lib->newFunction(vertStageName);
                MTL::Function* fStage = lib->newFunction(fragStageName);
                vertexShaders.push_back(vStage);
                fragmentShaders.push_back(fStage);
            }

            std::vector<MTL::Library*> computeLibraries;
            std::vector<MTL::Function*> computeKernels;

            for (NS::String* pSrc : kernelSource) {
                const MTL::CompileOptions* compileOptions = nullptr;
                MTL::Library* lib = mtlContext->CreateLibrary(pSrc, compileOptions);
                computeLibraries.push_back(lib);
                
                NS::String* kernelName = NS::String::string("ComputeKernel", NS::UTF8StringEncoding);;
                MTL::Function* kernelFunc = lib->newFunction(kernelName);
                computeKernels.push_back(kernelFunc);
            }
            
            // Create PSOs
            // -------------------------------------------------------------------------
            std::vector<MTL::RenderPipelineState*> psos;
            size_t psoID = 0;
            for (const auto& library : libraries) {
                MTL::RenderPipelineState* pso = mtlContext->CreatePSO(MTL::PrimitiveTopologyClassTriangle,
                                                                    FRAMEBUFFER_FORMAT, MTL::PixelFormatDepth32Float, vertexShaders[psoID], fragmentShaders[psoID]);
                psos.push_back(pso);
                psoID++;
            }

            std::vector<MTL::ComputePipelineState*> cpsos;
            size_t cpsoID = 0;
            for (const auto& library : computeLibraries) {
                MTL::ComputePipelineState* cpso = mtlContext->CreateComputePSO(computeKernels[cpsoID]);
                cpsos.push_back(cpso);
                cpsoID++;
            }

            // Create vertex buffers
            // ------------------------------------------------------------------------
            for (auto& meshPtr : meshes) {
                Mesh* mesh = meshPtr.get();

                int vertexCount = static_cast<int>(mesh->GetVertexCount());

                std::vector<simd::float3> positions(vertexCount);
                std::vector<simd::float3> colors(vertexCount);
                std::vector<simd::float2> texcoords(vertexCount);

                const Vertex* meshVertices = (const Vertex*)mesh->GetRawVertexData();
                for (int i = 0; i < vertexCount; ++i) {
                    positions[i] = { meshVertices[i].position[0], meshVertices[i].position[1], meshVertices[i].position[2] };
                    colors[i]    = { meshVertices[i].color[0],    meshVertices[i].color[1],    meshVertices[i].color[2] };
                    texcoords[i] = { meshVertices[i].texcoord[0], meshVertices[i].texcoord[1]};
                }

                MTL::Buffer* posBuffer = mtlContext->CreateBuffer(vertexCount, sizeof(simd::float3), MTL::ResourceStorageModeManaged);
                mtlContext->SetBufferData(posBuffer, vertexCount, sizeof(simd::float3), positions.data());

                MTL::Buffer* colBuffer = mtlContext->CreateBuffer(vertexCount, sizeof(simd::float3), MTL::ResourceStorageModeManaged);
                mtlContext->SetBufferData(colBuffer, vertexCount, sizeof(simd::float3), colors.data());

                MTL::Buffer* uvBuffer = mtlContext->CreateBuffer(vertexCount, sizeof(simd::float2), MTL::ResourceStorageModeManaged);
                mtlContext->SetBufferData(uvBuffer, vertexCount, sizeof(simd::float2), texcoords.data());

                int indexCount = static_cast<int>(mesh->GetIndexCount());

                MTL::Buffer* indexBuffer = mtlContext->CreateBuffer(indexCount, sizeof(uint32_t), MTL::ResourceStorageModeManaged);
                mtlContext->SetBufferData(indexBuffer, indexCount, sizeof(uint32_t), mesh->GetRawIndexData());

                // Create argument buffer
                const NS::String* funcName = NS::String::string("VertexStage", NS::UTF8StringEncoding);
                MTL::Buffer* vertBuffers[] = { posBuffer, colBuffer, uvBuffer };
                MTL::Buffer* argBuffer = mtlContext->CreateArgsBuffer(libraries[0], funcName, 0, 3, vertBuffers);
                
                mesh->SetBuffers(argBuffer, posBuffer, colBuffer, uvBuffer, indexBuffer);
            }
            
            
            // Create constant buffers
            // -------------------------------------------------------------------------
            FrameConstants frameConstants;
            MaterialConstants material;;
            size_t materialID = mtlContext->CreateConstantBuffer(1, sizeof(MaterialConstants));
            
            size_t instanceCount = 50;
            std::vector<InstanceConstants> instances(instanceCount);
            size_t instanceID = mtlContext->CreateConstantBuffer((int)instances.size(), sizeof(InstanceConstants));

            // Create textures and samples
            // ------------------------------------------------------------------------
            int texID = 0;
            MTL::Texture* texture = mtlContext->CreateTexture(texWidth, texHeight,
                MTL::PixelFormatRGBA8Unorm, MTL::TextureUsageShaderRead, MTL::StorageModeManaged);
            mtlContext->SetTextureData(texture, textureData, texWidth, texHeight, texBPP);
            
            int samplerID = 0;
            MTL::SamplerState* samplerState = mtlContext->CreateSampler(MTL::SamplerMinMagFilterLinear, MTL::SamplerMinMagFilterLinear);

            // Create render textures
            // ------------------------------------------------------------------------
            size_t rtexLayerCount = instanceCount;
            std::vector<uint32_t> colorBufferHandles;
            std::vector<uint32_t> depthBufferHandles;
            int meshID = 0;
            for (auto& meshPtr : meshes) {
                uint32_t rtHandle = mtlContext->CreateVersionedRenderTexture(
                winWidth, winHeight, (int)rtexLayerCount, FRAMEBUFFER_FORMAT,
                MTL::TextureType2DArray, MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModePrivate);
                colorBufferHandles.push_back(rtHandle);
                
                uint32_t dtHandle = mtlContext->CreateVersionedRenderTexture(
                winWidth, winHeight, (int)rtexLayerCount, MTL::PixelFormatDepth32Float,
                MTL::TextureType2DArray, MTL::TextureUsageRenderTarget, MTL::StorageModePrivate);
                depthBufferHandles.push_back(dtHandle);
            }
            
            // Create compute buffer
            // --------------------------------------------------------------------------------
            MTL::Buffer* pixelErrorBuffer = mtlContext->CreateBuffer((int)instanceCount, sizeof(float) * meshes.size(), MTL::ResourceStorageModeShared);
            
            uint32_t pixelErrorBufferHandle = mtlContext->CreateVersionedBuffer((int)instanceCount, sizeof(float) * meshes.size(), MTL::ResourceStorageModeShared);

            int stride = 4; // downsample the frame buffer
            MTL::Size gridSize = MTL::Size(winWidth / stride, winHeight / stride, instanceCount);
            MTL::Size threadGroupSize = MTL::Size(16, 16, 1);

            // Create training simulation
            // --------------------------------------------------------------------------------
            std::shared_ptr<TrainingSim> simulation = std::make_shared<TrainingSim>((int)meshes.size(), instanceCount, meshes[0]->GetBounds().height, mtlContext->GetAspectRatio());
            simulation->Start(startTime);
            
            // Main loop
            // -------------------------------------------------------------------------
            SDL_Event event;
            bool quit = false;
            while (!quit){
                
                // Poll event queue
                int eventsCount = SDL_PollEvent(&event);
                while (eventsCount > 0) {
                    switch (event.type) {
                        case SDL_EVENT_QUIT: {
                            quit = true;
                            break;
                        }
                        case SDL_EVENT_WINDOW_RESIZED: {
                            winWidth = event.window.data1;
                            winHeight = event.window.data2;
                            
                            // 2.0 for retina, 1.0 for standard
                            winScale = SDL_GetWindowDisplayScale(window);
                            
                            mtlContext->ResizeDrawable(winWidth, winHeight, winScale);
                            break;
                        }
                    }
                    eventsCount = SDL_PollEvent(&event);
                }
                
                std::shared_ptr<TrainingData> trainingData = simulation->Update(winWidth, winHeight);
                if (trainingData == nullptr) {
                    quit = true;
                }

                if (quit) break;

                // Metal render loop
                // ------------------------------------------------------
                
                NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
                MTL::Buffer* frameConstantsBuffer = mtlContext->InitFrame();

                frameConstants.progress = simulation->GetProgress();
                mtlContext->SetBufferData(frameConstantsBuffer, 1, sizeof(FrameConstants), &frameConstants);
                
                // Set material constant buffer
                MTL::Buffer* materialConstantsBuffer = mtlContext->GetConstantBuffer(materialID);
                material.matID = 0.0f;
                mtlContext->SetBufferData(materialConstantsBuffer, 1, sizeof(MaterialConstants), &material);
                
                const auto& worldMatrices = simulation->GetWorldMatrices();
                const auto& viewProjMatrices = simulation->GetViewProjMatrices();
                const auto& colors = simulation->GetColors();
                
                for (size_t i = 0; i < instances.size(); ++i) {
                    instances[i].worldMatrix    = worldMatrices[i];
                    instances[i].viewProjMatrix = viewProjMatrices[i];
                    instances[i].color          = colors[i];
                }
                
                // Set instance constant buffer
                MTL::Buffer* instanceConstantsBuffer = mtlContext->GetConstantBuffer(instanceID);
                mtlContext->SetBufferData(instanceConstantsBuffer, (int)instances.size(), sizeof(InstanceConstants), instances.data());
                
                std::vector<std::unique_ptr<RenderPass>> rasterPasses;
                int passID = 0;
                for (auto rtHandle : colorBufferHandles) {
                    std::unique_ptr<RenderPass> rasterPass = std::make_unique<RenderPass>(MTL::ClearColor::Make(1.0, 0.0, 1.0, 1.0), MTL::LoadActionClear, MTL::StoreActionStore,
                                                                                          mtlContext->GetVersionedRenderTexture(colorBufferHandles[passID]), mtlContext->GetVersionedRenderTexture(depthBufferHandles[passID]), rtexLayerCount);
                    rasterPasses.push_back(std::move(rasterPass));
                    passID++;
                }

                RenderPass debugPass(MTL::ClearColor::Make(1.0, 0.0, 1.0, 1.0), MTL::LoadActionClear, MTL::StoreActionStore,
                                     mtlContext->GetBackbufferColor(), mtlContext->GetBackbufferDepth(), 1);
                
                mtlContext->SetDepthStencilState(MTL::CompareFunction::CompareFunctionLess, true);
                    
                mtlContext->ClearVersionedBufferData(pixelErrorBufferHandle, (int)instanceCount, sizeof(float) * meshes.size());

                int i = 0;
                for (auto& pass : rasterPasses) {

                    // Begin render pass 0 (render to texture array)
                    MTL::RenderCommandEncoder* rasterBuffer = mtlContext->StartRenderPass(*pass);
                    
                    rasterBuffer->setDepthStencilState(mtlContext->GetDepthStencilState());
                    
                    // Bind shader data
                    rasterBuffer->setRenderPipelineState(psos[0]);
                    texID = 0;
                    samplerID = 0;
                    rasterBuffer->setFragmentTexture(texture, 0);
                    rasterBuffer->setFragmentSamplerState(samplerState, 0);
                    rasterBuffer->setFrontFacingWinding(MTL::Winding::WindingCounterClockwise);
                    rasterBuffer->setCullMode(MTL::CullModeBack);
                    
                    int bufferID = 0;
                    int byteOffset = 0;
                    rasterBuffer->setVertexBuffer(meshes[i]->GetArgsBuffer(), byteOffset, bufferID);
                    rasterBuffer->useResource(meshes[i]->GetPosBuffer(), MTL::ResourceUsageRead);
                    rasterBuffer->useResource(meshes[i]->GetColBuffer(), MTL::ResourceUsageRead);
                    rasterBuffer->useResource(meshes[i]->GetUVBuffer(), MTL::ResourceUsageRead);
                    
                    bufferID = 1;
                    rasterBuffer->setVertexBuffer(frameConstantsBuffer, byteOffset, bufferID);
                    bufferID = 2;
                    rasterBuffer->setVertexBuffer(materialConstantsBuffer, byteOffset, bufferID);
                    bufferID = 3;
                    rasterBuffer->setVertexBuffer(instanceConstantsBuffer, byteOffset, bufferID);

                    // Draw
                    int drawIndexCount = static_cast<int>(meshes[i]->GetIndexCount());
                    int instanceCount = (int)instances.size();
                    rasterBuffer->drawIndexedPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle,
                                                        drawIndexCount,
                                                        MTL::IndexType::IndexTypeUInt32, // Must be UInt32 to match uint32_t
                                                        meshes[i]->GetIndexBuffer(),
                                                        0,
                                                        instanceCount);
                    mtlContext->EndRenderPass(rasterBuffer);
                    i++;
                }
                
                // Begin compute pass 0
                MTL::ComputeCommandEncoder* compEnc = mtlContext->StartComputePass();
                compEnc->setComputePipelineState(cpsos[0]);
                
                int rtID = 0;
                for (auto rt : colorBufferHandles) {
                    compEnc->setTexture(mtlContext->GetVersionedRenderTexture(colorBufferHandles[rtID]), rtID);
                    rtID++;
                }
                compEnc->setBuffer(mtlContext->GetVersionedBuffer(pixelErrorBufferHandle), 0, 0);
                compEnc->dispatchThreads(gridSize, threadGroupSize);
                mtlContext->EndComputePass(compEnc);
                                
                // Begin render pass 1 (blit texture array grid to back buffer)
                MTL::RenderCommandEncoder* debugBuffer = mtlContext->StartRenderPass(debugPass);
                debugBuffer->setRenderPipelineState(psos[1]);
                rtID = 0;
                for (auto rt : colorBufferHandles) {
                    debugBuffer->setFragmentTexture(mtlContext->GetVersionedRenderTexture(colorBufferHandles[rtID]), rtID);
                    rtID++;
                }
                //int debugSlice = 0;
                //debugBuffer->setFragmentBytes(&debugSlice, sizeof(int), 0);
                int bufferID = 0;
                int byteOffset = 0;
                debugBuffer->setFragmentBuffer(frameConstantsBuffer, byteOffset, bufferID);
                debugBuffer-> drawPrimitives(MTL::PrimitiveTypeTriangle, (NS::UInteger)0, (NS::UInteger)3);
                
                mtlContext->EndRenderPass(debugBuffer);
                
                // "GPU Finished Rendering" callback
                mtlContext->Execute([simulation,
                                     trainingData,
                                     winWidth, winHeight, stride
                                    ](const float* renderingResults) mutable {
                    
                    simulation->ProcessFrameData(trainingData, renderingResults, winWidth, winHeight, stride);
                });
                
                pool->release();
            }
            
            mtlContext->WaitUntilIdle();

            simulation->SaveData(pathPrefix);

            meshes.clear();
            instances.clear();
            texture->release();
            samplerState->release();
            
            mtlContext.reset();
            
            SDL_Metal_DestroyView(metalView);
            autoPool->release();

        }
    }
    
    SDL_DestroyWindow(window);
    //SDL_Quit();
    return 0;
}
