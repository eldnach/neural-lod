#ifndef METALLIB_H
#define METALLIB_H

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

struct Vertex {
    float position[3];
    float color[3];
    float normal[3];
    float texcoord[2];
};

class Mesh{
    public:
        struct Bounds {
            float height;
            float radius;
        };
        Mesh(std::string name);
        ~Mesh();
        void SetData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        size_t GetVertexCount() const;
        size_t GetIndexCount() const;
        void* GetRawVertexData() const;
        void* GetRawIndexData() const;
        void SetBuffers(MTL::Buffer* args, MTL::Buffer* pos, MTL::Buffer* col, MTL::Buffer* uv, MTL::Buffer* ind);
        MTL::Buffer* GetArgsBuffer();
        MTL::Buffer* GetPosBuffer();
        MTL::Buffer* GetColBuffer();
        MTL::Buffer* GetUVBuffer();
        MTL::Buffer* GetIndexBuffer();
        void SetBounds(float height, float radius);
        Bounds GetBounds();

    private:      
        size_t _vertexCount;
        size_t _indexCount;
        std::unique_ptr<Vertex[]> _vertices;
        std::unique_ptr<uint32_t[]> _indices;
        Bounds _bounds;
    
        MTL::Buffer* _argBuffer = nullptr;    // Argument Buffer
        MTL::Buffer* _posBuffer = nullptr;    // Vertex Positions
        MTL::Buffer* _colBuffer = nullptr;    // Vertex Colors
        MTL::Buffer* _uvBuffer = nullptr;     // Texture Coordinates
        MTL::Buffer* _indexBuffer = nullptr;  // Face Indices
      
        std::string _name;
    
};

class RenderPass{
    public:
        RenderPass(MTL::ClearColor clear, MTL::LoadAction load, MTL::StoreAction store, MTL::Texture* rt, MTL::Texture* dt, size_t layerCount);
        MTL::RenderPassDescriptor* GetPassDescriptor() const;
    private:
        MTL::RenderPassDescriptor* _renderPass;
        MTL::RenderPassColorAttachmentDescriptor* _outCol;
        MTL::RenderPassDepthAttachmentDescriptor* _outDepth;
        MTL::Texture* _rt;
};

class MetalContext{
    
    public:
        MetalContext(MTL::Device* device, CA::MetalLayer* layer, int perFrameBufferSize);
        ~MetalContext();
    
        void ResizeDrawable(int w, int h, float scale);
        float GetAspectRatio() const;
        MTL::Buffer* CreateBuffer(int elementCount, size_t elementSize, MTL::ResourceOptions bufferUsage);
        uint32_t CreateVersionedBuffer(int elementCount, size_t elementSize, MTL::ResourceOptions bufferUsage);
        MTL::Buffer* GetVersionedBuffer(uint32_t handle);
        void SetBufferData(MTL::Buffer* buffer, int elementCount, size_t elementSize, const void* data);
        void SetVersionedBufferData(uint32_t handle, int elementCount, size_t elementSize, void* array);
        void ClearBufferData(MTL::Buffer* buffer, int elementCount, size_t elementSize);
        void ClearVersionedBufferData(uint32_t handle, int elementCount, size_t elementSize);
        MTL::Buffer* CreateArgsBuffer(MTL::Library* lib, const NS::String* funcName, int bufferID, int bufferCount, MTL::Buffer** buffers);
        MTL::Texture* CreateTexture(int width, int height, MTL::PixelFormat format, MTL::TextureUsage usage, MTL::StorageMode mode);
        void SetTextureData(MTL::Texture* texture, const std::vector<uint8_t>& textData, uint32_t width, uint32_t height, uint32_t channels);
        MTL::SamplerState* CreateSampler(MTL::SamplerMinMagFilter minFilter, MTL::SamplerMinMagFilter magFilter);
        MTL::Texture* CreateRenderTexture(int width, int height, int layers, MTL::PixelFormat format, MTL::TextureType type,  MTL::TextureUsage usage, MTL::StorageMode mode);
        uint32_t CreateVersionedRenderTexture(int width, int height, int layers, MTL::PixelFormat format, MTL::TextureType type,  MTL::TextureUsage usage, MTL::StorageMode mode);
        MTL::Texture* GetVersionedRenderTexture(uint32_t handle);
        MTL::Library* CreateLibrary(const NS::String* src, const MTL::CompileOptions* options);
        MTL::RenderPipelineState* CreatePSO(MTL::PrimitiveTopologyClass primitive, MTL::PixelFormat rtFormat, MTL::PixelFormat dtFormat, MTL::Function* vStage, MTL::Function* fStage);
        MTL::ComputePipelineState* CreateComputePSO(MTL::Function* kernelFunc);
        MTL::Buffer* InitFrame();
        MTL::Texture* GetBackbufferColor();
        MTL::Texture* GetBackbufferDepth();
        void SetDepthStencilState(MTL::CompareFunction compareFunc, bool depthWrite);
        MTL::DepthStencilState* GetDepthStencilState();
        MTL::CommandBuffer* CreateCommandBuffer();
        MTL::RenderCommandEncoder* StartRenderPass(const RenderPass& renderPass);
        void EndRenderPass(MTL::RenderCommandEncoder* rasterBuffer);
        MTL::ComputeCommandEncoder* StartComputePass();
        void EndComputePass(MTL::ComputeCommandEncoder* computeEncoder);
        size_t CreateConstantBuffer(int elementCount, int elementSize);
        MTL::Buffer* GetConstantBuffer(size_t bufferID);

        using EndFrameCallback = std::function<void(const float*)>;
        void Execute(EndFrameCallback callback = nullptr, uint32_t handle = 0);
        int IncrementFrameCounter();
        void WaitUntilIdle();

    private:
        MTL::Device* _device;
    
        CA::MetalLayer* _layer;
        CA::MetalDrawable* _drawable;
        int _drawableWidth;
        int _drawableHeight;
        int _drawableScale;
        float _aspectRatio;
        MTL::Texture* _backbufferColor;
        MTL::ClearColor _CLEARCOLOR;
        MTL::Texture* _backbufferDepth;
        MTL::DepthStencilState* _depthStencilState;
    
        MTL::CommandQueue* _queue;
        MTL::CommandBuffer* _cmdBuffer;
    
        std::vector<MTL::Buffer*> _versionedBuffers;
        std::vector<MTL::Texture*> _versionedRenderTextures;
        std::vector<MTL::Buffer*> _frameConstantsBuffers;
        std::vector<std::vector<MTL::Buffer*>> _customConstantBuffers;
    
        int _frameID;
        std::vector<dispatch_semaphore_t> _frameSemaphore;
        const int _maxFrames;
        int _frameCounter;
};

#endif // METALLIB_H
