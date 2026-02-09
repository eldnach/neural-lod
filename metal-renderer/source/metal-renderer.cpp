#include "metal-renderer.h"
#include <cstdio>
#include <iostream>

Mesh::Mesh(std::string name){
    _name = name;
}

void Mesh::SetData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices){
    _vertexCount = vertices.size();
    _vertices = std::make_unique<Vertex[]>(_vertexCount);
    std::memcpy(_vertices.get(), vertices.data(), _vertexCount * sizeof(Vertex));

    _indexCount = indices.size();
    _indices = std::make_unique<uint32_t[]>(_indexCount);
    std::memcpy(_indices.get(), indices.data(), _indexCount * sizeof(uint32_t));
}

size_t Mesh::GetVertexCount() const {
    return _vertexCount;
}

size_t Mesh::GetIndexCount() const {
    return _indexCount;
}

void* Mesh::GetRawVertexData() const {
    return _vertices.get();
}

void* Mesh::GetRawIndexData() const {
    return _indices.get();
}

void Mesh::SetBuffers(MTL::Buffer* args, MTL::Buffer* pos, MTL::Buffer* col, MTL::Buffer* uv, MTL::Buffer* ind){
    _argBuffer = args;
    _posBuffer = pos;
    _colBuffer = col;
    _uvBuffer = uv;
    _indexBuffer = ind;
}

MTL::Buffer* Mesh::GetArgsBuffer(){
    return _argBuffer;
}
MTL::Buffer* Mesh::GetPosBuffer(){
    return _posBuffer;
}
MTL::Buffer* Mesh::GetColBuffer(){
    return _colBuffer;
}
MTL::Buffer* Mesh::GetUVBuffer(){
    return _uvBuffer;
}
MTL::Buffer* Mesh::GetIndexBuffer(){
    return _indexBuffer;
}

void Mesh::SetBounds(float height, float radius){
    _bounds.height = height;
    _bounds.radius = radius;
}

Mesh::Bounds Mesh::GetBounds(){
    return _bounds;
}

RenderPass::RenderPass(MTL::ClearColor clear, MTL::LoadAction load, MTL::StoreAction store, MTL::Texture* rt, MTL::Texture* dt, size_t layerCount){
    _renderPass = MTL::RenderPassDescriptor::alloc()->init();

    _outCol = _renderPass->colorAttachments()->object(0);
    _outCol->setClearColor(clear);
    _outCol->setLoadAction(load);
    _outCol->setStoreAction(store);
    _outCol->setTexture(rt);
    
    _outDepth = _renderPass->depthAttachment();
    _outDepth->setLoadAction(MTL::LoadActionClear);
    _outDepth->setStoreAction(MTL::StoreActionDontCare);
    _outDepth->setClearDepth(1.0);
    _outDepth->setTexture(dt);
    
    _renderPass->setRenderTargetArrayLength(layerCount);
}

Mesh::~Mesh() {
    // Release GPU (Metal) buffers
    if (_argBuffer)   _argBuffer->release();
    if (_posBuffer)   _posBuffer->release();
    if (_colBuffer)   _colBuffer->release();
    if (_uvBuffer)    _uvBuffer->release();
    if (_indexBuffer) _indexBuffer->release();
}

MTL::RenderPassDescriptor* RenderPass::GetPassDescriptor() const{
    return _renderPass;
}

MetalContext::MetalContext(MTL::Device* device, CA::MetalLayer* layer, int frameConstantsSize) : _maxFrames(3), _frameID(0), _frameCounter(-1) {
    _device = device;
    _layer = layer;
    _queue = device->newCommandQueue();
    _CLEARCOLOR = MTL::ClearColor::Make(0.2, 0.2, 0.2, 1.0);
    
    for (int i=0; i<_maxFrames; i++){
        _frameConstantsBuffers.push_back(CreateBuffer(1, frameConstantsSize, MTL::ResourceStorageModeManaged));
        _frameSemaphore.push_back(dispatch_semaphore_create(1));
    }
}

MetalContext::~MetalContext(){
    if (_queue) {
        _queue->release();
    }

    if (_backbufferDepth) {
        _backbufferDepth->release();
    }

    for (auto* buffer : _versionedBuffers) {
        if (buffer) buffer->release();
    }
    for (auto* texture : _versionedRenderTextures) {
        if (texture) texture->release();
    }
    for (auto* buffer : _frameConstantsBuffers) {
        if (buffer) buffer->release();
    }
    for (auto& bufferList : _customConstantBuffers) {
        for (auto* buffer : bufferList) {
            if (buffer) buffer->release();
        }
    }
    
    if (_device) {
        _device->release();
    }
}

void MetalContext::ResizeDrawable(int w, int h, float scale){
    
    _layer->setDrawableSize(CGSize{
        (double)w * scale,
        (double)h * scale
    });
    
    _drawableWidth = w;
    _drawableHeight = h;
    _drawableScale = scale;
    
    _aspectRatio = (float)_drawableWidth / (float)_drawableHeight;
}

float MetalContext::GetAspectRatio() const{
    return _aspectRatio;
}

MTL::Buffer* MetalContext::CreateBuffer(int elementCount, size_t elementSize, MTL::ResourceOptions bufferUsage){
    
    const size_t arraySize = elementCount * elementSize;

    MTL::Buffer* buffer = _device->newBuffer(arraySize, bufferUsage);
    if(!buffer){
        std::cout << "Failed to create buffer" << "\n" << std::flush;
        assert(0);
    }
    //std::cout << "Created buffer" << "\n" << std::flush;
    
    return buffer;
}

uint32_t MetalContext::CreateVersionedBuffer(int elementCount, size_t elementSize, MTL::ResourceOptions bufferUsage){
    
    const size_t arraySize = elementCount * elementSize;
    
    for (int i=0; i<_maxFrames; i++){
        MTL::Buffer* buffer = _device->newBuffer(arraySize, bufferUsage);
        if(!buffer){
            std::cout << "Failed to create buffer" << "\n" << std::flush;
            assert(0);
        }
        //std::cout << "Created buffer" << "\n" << std::flush;
        _versionedBuffers.push_back(buffer);
    }
    
    uint32_t handle = static_cast<uint32_t>(_versionedBuffers.size() - _maxFrames);
    return handle;
}

MTL::Buffer* MetalContext::GetVersionedBuffer(uint32_t handle){
    return _versionedBuffers[handle + _frameID];
}

void MetalContext::SetBufferData(MTL::Buffer* buffer, int elementCount, size_t elementSize, const void* data){
    
    const size_t arraySize = elementCount * elementSize;
    memcpy(buffer->contents(), data, arraySize);
    buffer->didModifyRange(NS::Range::Make(0, buffer->length()));
    //std::cout << "Set buffer data" << "\n" << std::flush;
    return;
}

void MetalContext::SetVersionedBufferData(uint32_t handle, int elementCount, size_t elementSize, void* array){
    
    const size_t arraySize = elementCount * elementSize;
    memcpy(_versionedBuffers[handle+_frameID]->contents(), array, arraySize);
    _versionedBuffers[handle+_frameID]->didModifyRange(NS::Range::Make(0, _versionedBuffers[handle+_frameID]->length()));
    //std::cout << "Set buffer data" << "\n" << std::flush;
    return;
}

void MetalContext::ClearBufferData(MTL::Buffer* buffer, int elementCount, size_t elementSize){
    MTL::BlitCommandEncoder* blitEnc = _cmdBuffer->blitCommandEncoder();
    blitEnc->fillBuffer(buffer, NS::Range(0, elementCount * elementSize), 0);
    blitEnc->endEncoding();
}

void MetalContext::ClearVersionedBufferData(uint32_t handle, int elementCount, size_t elementSize){
    MTL::BlitCommandEncoder* blitEnc = _cmdBuffer->blitCommandEncoder();
    blitEnc->fillBuffer(_versionedBuffers[handle + _frameID], NS::Range(0, elementCount * elementSize), 0);
    blitEnc->endEncoding();
}

MTL::Buffer* MetalContext::CreateArgsBuffer(MTL::Library* lib, const NS::String* funcName, int bufferID, int bufferCount, MTL::Buffer** buffers){
    
    MTL::Function* func = lib->newFunction(funcName);
    MTL::ArgumentEncoder* argsEncoder = func->newArgumentEncoder(bufferID);
    
    MTL::Buffer* buffer = _device->newBuffer(argsEncoder->encodedLength(), MTL::ResourceStorageModeManaged);
    int byteoffset = 0;
    argsEncoder->setArgumentBuffer(buffer, byteoffset);
    
    for(int i=0; i<bufferCount; i++){
        argsEncoder->setBuffer(buffers[i], byteoffset, i);
    }
    
    buffer->didModifyRange(NS::Range::Make(0, buffer->length()));
    func->release();
    argsEncoder->release();
    
    //std::cout << "Created args buffer" << "\n" << std::flush;
    return buffer;
}

MTL::Texture* MetalContext::CreateTexture(int width, int height, MTL::PixelFormat format, MTL::TextureUsage usage, MTL::StorageMode mode){
    MTL::TextureDescriptor* texDesc = MTL::TextureDescriptor::alloc()->init();
    texDesc->setPixelFormat(format);
    texDesc->setWidth(width);
    texDesc->setHeight(height);
    texDesc->setUsage(usage);
    texDesc->setStorageMode(mode);

    MTL::Texture* texture = _device->newTexture(texDesc);
    texDesc->release();

    return texture;
}

void MetalContext::SetTextureData(MTL::Texture* texture, const std::vector<uint8_t>& texData, uint32_t width, uint32_t height, uint32_t bytesPerPixel){
    size_t expectedSize = width * height * bytesPerPixel;
    
    if (texData.size() < expectedSize) {
        std::cerr << "SetTextureData: Texture data buffer is too small!" << std::endl;
        return;
    }
    
    MTL::Region region = MTL::Region::Make2D(0, 0, width, height);
    texture->replaceRegion(region, 0, texData.data(), width * bytesPerPixel);
}

MTL::SamplerState* MetalContext::CreateSampler(MTL::SamplerMinMagFilter minFilter, MTL::SamplerMinMagFilter magFilter){
    MTL::SamplerDescriptor* samplerDesc = MTL::SamplerDescriptor::alloc()->init();
    samplerDesc->setMinFilter(minFilter);
    samplerDesc->setMagFilter(magFilter);
    MTL::SamplerState* samplerState = _device->newSamplerState(samplerDesc);
    return samplerState;
}

MTL::Texture* MetalContext::CreateRenderTexture(int width, int height, int layers, MTL::PixelFormat format, MTL::TextureType type,  MTL::TextureUsage usage, MTL::StorageMode mode){
    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType(type);
    desc->setPixelFormat(format);
    desc->setWidth(width);
    desc->setHeight(height);
    desc->setArrayLength(layers);
    desc->setUsage(usage);
    desc->setStorageMode(mode);

    MTL::Texture* textureArray = _device->newTexture(desc);
    return textureArray;
}

uint32_t MetalContext::CreateVersionedRenderTexture(int width, int height, int layers, MTL::PixelFormat format, MTL::TextureType type,  MTL::TextureUsage usage, MTL::StorageMode mode){
    
    for (int i=0; i<_maxFrames; i++){
        MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
        desc->setTextureType(type);
        desc->setPixelFormat(format);
        desc->setWidth(width);
        desc->setHeight(height);
        desc->setArrayLength(layers);
        desc->setUsage(usage);
        desc->setStorageMode(mode);
        MTL::Texture* textureArray = _device->newTexture(desc);
        _versionedRenderTextures.push_back(textureArray);
    }

    uint32_t handle = static_cast<uint32_t>(_versionedRenderTextures.size() - _maxFrames);
    return handle;
}

MTL::Texture* MetalContext::GetVersionedRenderTexture(uint32_t handle){
    return _versionedRenderTextures[handle + _frameID];
}

MTL::Library* MetalContext::CreateLibrary(const NS::String* src, const MTL::CompileOptions* options){
    
    NS::Error* err = nullptr;
    
    MTL::Library* lib = _device->newLibrary(src, options, &err);
    if (!lib){
        if (err) {
            std::cerr << "METAL COMPILATION ERROR:\n"
                      << err->localizedDescription()->utf8String() << std::endl;
        }
        assert(0);
    }
    std::cout << "CreateLibrary: Metal library created \n" << std::flush;\
    
    return lib;
}

MTL::RenderPipelineState* MetalContext::CreatePSO(MTL::PrimitiveTopologyClass primitive, MTL::PixelFormat rtFormat, MTL::PixelFormat dtFormat, MTL::Function* vStage, MTL::Function* fStage){
    
    NS::Error* err = nullptr;

    MTL::RenderPipelineDescriptor* psoDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    psoDesc->setVertexFunction(vStage);
    psoDesc->setInputPrimitiveTopology(primitive);
    psoDesc->setFragmentFunction(fStage);
    psoDesc->colorAttachments()->object(0)->setPixelFormat(rtFormat);
    psoDesc->setDepthAttachmentPixelFormat(dtFormat);
    
    MTL::RenderPipelineState* _pso = _device->newRenderPipelineState(psoDesc, &err);
    if(!_pso){
        std::printf("Failed to create PSO: %s", err->localizedDescription()->utf8String());
        assert(0);
    }
    std::cout << "PSO created \n" << std::flush;
    
    vStage->release();
    fStage->release();
    psoDesc->release();
    
    return _pso; 
    
}

MTL::ComputePipelineState* MetalContext::CreateComputePSO(MTL::Function* kernelFunc){
    
    NS::Error* err = nullptr;
    MTL::ComputePipelineState* computePSO = _device->newComputePipelineState(kernelFunc, &err);

    if (!computePSO) {
        std::printf("Failed to create PSO: %s", err->localizedDescription()->utf8String());
        assert(0);
    }
    
    kernelFunc->release();
    return computePSO; 
}

MTL::Texture* MetalContext::GetBackbufferColor(){
    return _backbufferColor;
}

MTL::Texture* MetalContext::GetBackbufferDepth(){
    return _backbufferDepth;
}

void MetalContext::SetDepthStencilState(MTL::CompareFunction compareFunc, bool depthWrite){
    MTL::DepthStencilDescriptor* dsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    dsDesc->setDepthCompareFunction(compareFunc);
    dsDesc->setDepthWriteEnabled(depthWrite);

    _depthStencilState = _device->newDepthStencilState(dsDesc);

    dsDesc->release();
}

MTL::DepthStencilState* MetalContext::GetDepthStencilState(){
    return _depthStencilState;
}

MTL::Buffer* MetalContext::InitFrame(){
    
    // wait on frameID's "finished rendering" semaphore to signal, before re-recording it's command buffer and updating it's resources
    dispatch_semaphore_wait(_frameSemaphore[_frameID], DISPATCH_TIME_FOREVER);
    
    // get the next available swapchain image
    _drawable = _layer->nextDrawable();
    if (!_drawable) {
        // Skip this frame
        dispatch_semaphore_signal(_frameSemaphore[_frameID]);
        return nullptr;
    }
    _backbufferColor = _drawable->texture();
    
    MTL::TextureDescriptor* depthDesc = MTL::TextureDescriptor::alloc()->init();
    depthDesc->setTextureType(MTL::TextureType2D);
    depthDesc->setPixelFormat(MTL::PixelFormatDepth32Float);
    depthDesc->setWidth(_drawableWidth);
    depthDesc->setHeight(_drawableHeight);
    depthDesc->setUsage(MTL::TextureUsageRenderTarget);
    depthDesc->setStorageMode(MTL::StorageModePrivate);
    _backbufferDepth = _device->newTexture(depthDesc);
    
    // cache the current frameID (avoid signaling the wrong sempahore in the command buffer completion callback)
    int constantFrameID = _frameID;

    // initialize the frame's global command buffer
    _cmdBuffer = this->CreateCommandBuffer();
    
    _cmdBuffer->addCompletedHandler( ^void(MTL::CommandBuffer* cmdBuffer){

        double start = cmdBuffer->GPUStartTime();
        double end = cmdBuffer->GPUEndTime();
        std::cout << "frame " << std::to_string(constantFrameID) << " GPU time:" << std::to_string(end-start) << "\n" << std::flush;
        
        // signal frameID semaphore when command buffer finishes executing on the GPU (rendering done)
        dispatch_semaphore_signal(_frameSemaphore[constantFrameID]);
    });
    
    MTL::Buffer* frameConstantbuffer = _frameConstantsBuffers[_frameID];

    // increment for the next render loop iteration
    _frameID = (_frameID + 1) % _maxFrames;
    
    return frameConstantbuffer;
}

MTL::CommandBuffer* MetalContext::CreateCommandBuffer(){
    MTL::CommandBuffer* cmdBuffer = _queue->commandBuffer();
    return cmdBuffer;
}

MTL::RenderCommandEncoder* MetalContext::StartRenderPass(const RenderPass& renderPass){
    MTL::RenderCommandEncoder* rasterEncoder = _cmdBuffer->renderCommandEncoder(renderPass.GetPassDescriptor());
    return rasterEncoder;
}

void MetalContext::EndRenderPass(MTL::RenderCommandEncoder* rasterEncoder){
    rasterEncoder->endEncoding();
}

MTL::ComputeCommandEncoder* MetalContext::StartComputePass(){
    MTL::ComputeCommandEncoder* computeEncoder = _cmdBuffer->computeCommandEncoder();
    return computeEncoder;
}

void MetalContext::EndComputePass(MTL::ComputeCommandEncoder* computeEncoder){
    computeEncoder->endEncoding();
}

void MetalContext::Execute(MetalContext::EndFrameCallback callback, uint32_t handle){
    
    _cmdBuffer->presentDrawable(_drawable);
    
    if (callback != nullptr) {
        
        MTL::Buffer* buffer = _versionedBuffers[handle+_frameID];

        _cmdBuffer->addCompletedHandler([callback, buffer](MTL::CommandBuffer* pBuffer) {

            if (pBuffer->status() == MTL::CommandBufferStatusCompleted) {
                // Map the GPU buffer to a CPU-readable pointer
                const float* results = static_cast<float*>(buffer->contents());
                
                callback(results);
            }
        });
    }
    
   _cmdBuffer->commit();
   _cmdBuffer = nullptr;

}

int MetalContext::IncrementFrameCounter(){
    _frameCounter++;
    return _frameCounter;
}

size_t MetalContext::CreateConstantBuffer(int elementCount, int elementSize){
    std::vector<MTL::Buffer*> constantBuffer;
    for (int i=0; i<_maxFrames; i++){
        constantBuffer.push_back(CreateBuffer(elementCount, elementSize, MTL::ResourceStorageModeManaged));
    }
    size_t bufferID = _customConstantBuffers.size();
    _customConstantBuffers.push_back(constantBuffer);
    return bufferID;
}

MTL::Buffer* MetalContext::GetConstantBuffer(size_t bufferID){
    return _customConstantBuffers[bufferID][_frameID];
}

void MetalContext::WaitUntilIdle() {
    MTL::CommandBuffer* pWaitBuf = _queue->commandBuffer();
    pWaitBuf->commit();
    pWaitBuf->waitUntilCompleted();
}
