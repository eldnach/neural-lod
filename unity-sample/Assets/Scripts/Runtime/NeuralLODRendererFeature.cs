using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using UnityEngine.Rendering.RenderGraphModule;
using Unity.Mathematics;
using Unity.Collections;
using System;

public class NeuralLODRendererFeature : ScriptableRendererFeature
{
    [SerializeField] private RenderPassEvent injectionPoint = RenderPassEvent.AfterRenderingOpaques;
    [SerializeField] private Mesh[] mesh;
    [SerializeField] private int instanceCount = 128 * 128;
    [SerializeField] private Material material;
    [SerializeField] private TextAsset[] scalerJson;
    [SerializeField] private TextAsset[] modelWeights;
    [SerializeField] private float pixelErrorThreshold = 10.0f;
    [SerializeField] private bool debug = true;
    [SerializeField] private LODGradient lodGradient;
    [SerializeField] private DebugMode debugMode = DebugMode.LodTex;

    public enum DebugMode
    {
        LodTex,
        LodMat,
        LodBoth,
        PixelErrorPrediction
    }
    
    private IndirectRenderPass _renderPass;
    private DebugPass _debugPass;
    public class DebugData : ContextItem {
        public TextureHandle debugTexture;

        public override void Reset()
        {
            debugTexture = TextureHandle.nullHandle;
        }
    }  

    public override void Create()
    {

        if (mesh == null || mesh.Length == 0)
        {
            Debug.LogError("IndirectRendererFeature: No mesh assigned.");
            return;
        } else if (mesh.Length != scalerJson.Length || mesh.Length != modelWeights.Length) {
            Debug.LogError("IndirectRendererFeature: Must assign an equal number of meshes, scaler JSONs, and model weights");
            return;
        }

        _renderPass = new IndirectRenderPass(mesh, instanceCount, material, scalerJson, modelWeights, lodGradient, debug, debugMode, pixelErrorThreshold);
        _renderPass.renderPassEvent = injectionPoint;

        if (debug)
        {
            _debugPass = new DebugPass(debugMode);
            _debugPass.renderPassEvent = RenderPassEvent.AfterRenderingPostProcessing;
        }
    }

    public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
    {
        renderer.EnqueuePass(_renderPass);
        if(debug){
            renderer.EnqueuePass(_debugPass);
        }
    }

    protected override void Dispose(bool disposing)
    {
        _renderPass?.Dispose();
    }

    class IndirectRenderPass : ScriptableRenderPass
    {
        private Mesh[] _mesh;
        private Vector4[] _scalerMean;
        private Vector4[] _scalerStd;
        private float[][] _modelWeights;
        private float _pixelErrorThreshold;

        private struct LodData {
            public GraphicsBuffer posBuffer, uvBuffer, colorBuffer, idxBuffer;
            public uint indexCount;
        }
        private bool _debugLods = false;
        private DebugMode _debugMode;
        private LocalKeyword _debugKeyword;
        private float4[] _lodColor = new float4[5];
        public Vector4 lodDistances = new Vector4(20.0f, 50.0f, 100.0f, 200.0f);

        private struct RendererData {
            public LodData[] lods;
            public GraphicsBuffer instancePosBuffer;
            public GraphicsBuffer[] culledPosBuffers;
            public GraphicsBuffer[] argsBuffers;
            public GraphicsBuffer weightsBuffer;

            public RendererData(Mesh mesh, float[] modelWeights, int instanceCount, Vector3 offset) 
            {
                lods = new LodData[5];
                culledPosBuffers = new GraphicsBuffer[5];
                argsBuffers = new GraphicsBuffer[5];
                instancePosBuffer = null;
                weightsBuffer = null;

                InitializeModelBuffers(mesh, modelWeights);
                InitializeInstanceBuffers(instanceCount, offset);
            } 
            
            public void InitializeModelBuffers(Mesh mesh, float[] modelWeights)
            {
                int actualLodCount = mesh.lodCount;
                lods[0].posBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Structured, mesh.vertexCount, 12);
                lods[0].posBuffer.SetData(mesh.vertices);
                lods[0].uvBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Structured, mesh.vertexCount, 8);
                lods[0].uvBuffer.SetData(mesh.uv);
                lods[0].colorBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Structured, mesh.vertexCount, 16);
                lods[0].colorBuffer.SetData(mesh.colors.Length > 0 ? mesh.colors : new Color[mesh.vertexCount]);

                int maxLods = mesh.lodCount; 
                for (int i = 0; i < 5; i++)
                {
                    if (i > 0) {
                        lods[i].posBuffer = lods[0].posBuffer;
                        lods[i].uvBuffer = lods[0].uvBuffer;
                        lods[i].colorBuffer = lods[0].colorBuffer;
                    }
                    int targetLod = Mathf.Min(i, maxLods - 1);
                    int[] lodIndices = mesh.GetIndices(0, targetLod, true); 

                    lods[i].indexCount = (uint)lodIndices.Length;
                    lods[i].idxBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Index, lodIndices.Length, 4);
                    lods[i].idxBuffer.SetData(lodIndices);
                }

                weightsBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Structured, modelWeights.Length, sizeof(float));
                weightsBuffer.SetData(modelWeights);      
            }
            
            void InitializeInstanceBuffers(int instanceCount, Vector3 offset)
            {
                float spacing = 2.5f;
                int gridSize = Mathf.CeilToInt(Mathf.Sqrt(instanceCount));
                float offsetX = (gridSize - 1) * spacing * 0.5f + offset.x;
                float offsetY = offset.y;
                float offsetZ = (gridSize - 1) * spacing * 0.5f + offset.z;

                instancePosBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Structured, instanceCount, sizeof(float) * 4);
                Vector4[] positions = new Vector4[instanceCount];
                for (int y = 0; y < gridSize; y++)
                {
                    for (int x = 0; x < gridSize; x++)
                    {
                        float shift = (y % 2 == 1) ? 1.0f : 0.0f;

                        float posX = (x * spacing) - offsetX + shift;
                        float posy = offsetY;
                        float posZ = (y * spacing) - offsetZ;
                        
                        positions[y * gridSize + x] = new Vector4(posX, posy, posZ, 1f);
                    }
                }
                instancePosBuffer.SetData(positions);

                for (int i = 0; i < 5; i++)
                {
                    culledPosBuffers[i] = new GraphicsBuffer(GraphicsBuffer.Target.Append, instanceCount, sizeof(float) * 4);
                    argsBuffers[i] = new GraphicsBuffer(GraphicsBuffer.Target.IndirectArguments | GraphicsBuffer.Target.CopyDestination, 1, 5 * 4);
                    uint[] args = new uint[5] { lods[i].indexCount, 0, 0, 0, 0 };
                    argsBuffers[i].SetData(args);
                }
            }             
        }
        private RendererData[] _renderers;

        private TextureHandle _debugTexture;

        private ComputeShader _computeShader;
        private Material _material;
        private int _gridSize;

        private class ScalerConfig {
            public float[] mean;
            public float[] std;
        }

        public IndirectRenderPass(Mesh[] mesh, int instances, Material mat, TextAsset[] scalerJson, TextAsset[] modelWeights, LODGradient lodGradient, bool debug, DebugMode debugMode, float pixelErrorThreshold)
        {
            _mesh = new Mesh[mesh.Length];
            _scalerMean = new Vector4[mesh.Length];
            _scalerStd = new Vector4[mesh.Length];
            _modelWeights = new float[mesh.Length][];
            _renderers = new RendererData[mesh.Length];
            _gridSize = Mathf.CeilToInt(Mathf.Sqrt(instances));

            for (int i = 0; i < mesh.Length; i++)
            {
                _mesh[i] = mesh[i];
                byte[] bytes = modelWeights[i].bytes;
                _modelWeights[i] = new float[bytes.Length / 4];
                System.Buffer.BlockCopy(bytes, 0, _modelWeights[i], 0, bytes.Length);

                _renderers[i] = new RendererData(_mesh[i], _modelWeights[i], instances, new Vector3(0.0f, (float)i / (float)mesh.Length * -2.0f, 0.0f));

                ScalerConfig scaler = JsonUtility.FromJson<ScalerConfig>(scalerJson[i].text);
                _scalerMean[i] = new Vector4(scaler.mean[0], scaler.mean[1], scaler.mean[2], scaler.mean[3]);
                _scalerStd[i] = new Vector4(scaler.std[0], scaler.std[1], scaler.std[2], scaler.std[3]);
            }

            if (_computeShader == null) _computeShader = Resources.Load<ComputeShader>("Shaders/Kernels");
            if (_debugKeyword == null)  _debugKeyword = new LocalKeyword(_computeShader, "DEBUG");

            _material = mat;
            _debugLods = debug;
            _debugMode = debugMode;
            _pixelErrorThreshold = pixelErrorThreshold;

            for (int i = 0; i < 5; i++)
            {
                float t = (float)i / (5 - 1);
                Color col = lodGradient.val.Evaluate(t);
                _lodColor[i] = new float4(col.r, col.g, col.b, col.a);
            }      
        }
    
        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
        {
            UniversalCameraData cameraData = frameData.Get<UniversalCameraData>();
            UniversalResourceData resourceData = frameData.Get<UniversalResourceData>();

            BufferHandle[] instancePosHandle = new BufferHandle[_renderers.Length];
            BufferHandle[] weightsHandle = new BufferHandle[_renderers.Length];
            BufferHandle[,] culledPosHandle = new BufferHandle[_renderers.Length,5];   
            BufferHandle[,] argsHandle = new BufferHandle[_renderers.Length,5];

            for (int i = 0; i < _renderers.Length; i++)
            {
                instancePosHandle[i] =  renderGraph.ImportBuffer(_renderers[i].instancePosBuffer);
                weightsHandle[i] = renderGraph.ImportBuffer(_renderers[i].weightsBuffer);
                for (int j = 0; j < 5; j++) {
                    culledPosHandle[i,j] = renderGraph.ImportBuffer(_renderers[i].culledPosBuffers[j]);
                    argsHandle[i,j] = renderGraph.ImportBuffer(_renderers[i].argsBuffers[j]);
                }
            }

            DebugData debugData = null;
            if (_debugLods)
            {
                debugData = frameData.Create<DebugData>(); 
            }
            
            // Neural Inference Pass
            using (var builder = renderGraph.AddComputePass("Neural Inference Pass", out ComputePassData passData))
            {
                if (_debugLods)
                {
                    RenderTextureDescriptor textureProperties = new RenderTextureDescriptor((int)_gridSize, (int)_gridSize, RenderTextureFormat.Default, 0);
                    textureProperties.enableRandomWrite = true;
                    _debugTexture = UniversalRenderer.CreateRenderGraphTexture(renderGraph, textureProperties, "DebugLODsTexture", false);
                    builder.UseTexture(_debugTexture, AccessFlags.Write);
                }

                for (int i = 0; i < _renderers.Length; i++) {
                    builder.UseBuffer(instancePosHandle[i], AccessFlags.Read);
                    builder.UseBuffer(weightsHandle[i], AccessFlags.Read);
                    for (int j = 0; j < 5; j++) {
                        builder.UseBuffer(culledPosHandle[i,j], AccessFlags.Write);
                        builder.UseBuffer(argsHandle[i,j], AccessFlags.ReadWrite);
                    }
                }

                builder.AllowPassCulling(false);
                builder.AllowGlobalStateModification(true);

                int kernelClear = _computeShader.FindKernel("ClearDebugTexture");
                int kernelLODs = _computeShader.FindKernel("ComputeLODs");
                _computeShader.GetKernelThreadGroupSizes(kernelLODs, out uint threadGroupSizeX, out uint threadGroupSizeY, out uint threadGroupSizeZ);

                int groupsX = Mathf.CeilToInt(_gridSize / threadGroupSizeX);
                int groupsY = Mathf.CeilToInt(_gridSize / threadGroupSizeY);  

                var camera = cameraData.camera;
                Matrix4x4 vp = camera.projectionMatrix * camera.worldToCameraMatrix;
                Plane[] frustumPlanes = GeometryUtility.CalculateFrustumPlanes(camera);
                Vector4[] planes = new Vector4[6];
                for (int i = 0; i < 6; i++) 
                {
                    planes[i] = new Vector4(frustumPlanes[i].normal.x, frustumPlanes[i].normal.y, frustumPlanes[i].normal.z, frustumPlanes[i].distance);
                }

                builder.SetRenderFunc((ComputePassData data, ComputeGraphContext context) =>
                {
                    int rID = 0;
                    foreach (var r in _renderers)
                    {
                        for (int i = 0; i < 5; i++) r.culledPosBuffers[i].SetCounterValue(0);

                        context.cmd.SetComputeBufferParam(_computeShader, kernelLODs, "positionsBuffer", r.instancePosBuffer);

                        context.cmd.SetComputeBufferParam(_computeShader, kernelLODs, "culledLOD0", r.culledPosBuffers[0]);
                        context.cmd.SetComputeBufferParam(_computeShader, kernelLODs, "culledLOD1", r.culledPosBuffers[1]);
                        context.cmd.SetComputeBufferParam(_computeShader, kernelLODs, "culledLOD2", r.culledPosBuffers[2]);
                        context.cmd.SetComputeBufferParam(_computeShader, kernelLODs, "culledLOD3", r.culledPosBuffers[3]);
                        context.cmd.SetComputeBufferParam(_computeShader, kernelLODs, "culledLOD4", r.culledPosBuffers[4]);

                        context.cmd.SetComputeIntParam(_computeShader, "instanceCountX", (int)_gridSize);
                        context.cmd.SetComputeIntParam(_computeShader, "totalInstances", (int)(_gridSize * _gridSize));

                        context.cmd.SetComputeVectorArrayParam(_computeShader, "frustumPlanes", planes);
                        context.cmd.SetComputeMatrixParam(_computeShader, "viewProjMat", vp);
                        context.cmd.SetComputeFloatParam(_computeShader, "minSolidAngleRad", 0.001f);
                        context.cmd.SetComputeVectorParam(_computeShader, "lodDistances", lodDistances);
                        context.cmd.SetComputeFloatParam(_computeShader, "errorThreshold", _pixelErrorThreshold);

                        float yScale = 1.0f / Mathf.Tan(cameraData.camera.fieldOfView * 0.5f * Mathf.Deg2Rad);
                        context.cmd.SetComputeFloatParam(_computeShader, "yScale", yScale);
                        context.cmd.SetComputeVectorParam(_computeShader, "cameraPos", cameraData.camera.transform.position);
                        context.cmd.SetComputeVectorParam(_computeShader, "cameraDir", cameraData.camera.transform.forward);
                        context.cmd.SetComputeFloatParam(_computeShader, "modelHeight", 1.0f);

                        context.cmd.SetComputeBufferParam(_computeShader, kernelLODs, "weightsBuffer", r.weightsBuffer);
                        context.cmd.SetComputeVectorParam(_computeShader, "scalerMean", _scalerMean[rID]);
                        context.cmd.SetComputeVectorParam(_computeShader, "scalerStd",  _scalerStd[rID]);

                        if (_debugLods)
                        {
                            context.cmd.SetComputeTextureParam(_computeShader, kernelLODs, "debugTexture", _debugTexture);
                            context.cmd.SetComputeTextureParam(_computeShader, kernelClear, "debugTexture", _debugTexture);
                            context.cmd.SetComputeVectorParam(_computeShader, "lod0Color",  _lodColor[0]);    
                            context.cmd.SetComputeVectorParam(_computeShader, "lod1Color",  _lodColor[1]); 
                            context.cmd.SetComputeVectorParam(_computeShader, "lod2Color",  _lodColor[2]); 
                            context.cmd.SetComputeVectorParam(_computeShader, "lod3Color",  _lodColor[3]);    
                            context.cmd.SetComputeVectorParam(_computeShader, "lod4Color",  _lodColor[4]);   
                            context.cmd.SetKeyword(_computeShader, _debugKeyword, true);   
                            context.cmd.DispatchCompute(_computeShader, kernelClear, groupsX, groupsY, 1);
                        } else
                        {
                            context.cmd.SetKeyword(_computeShader, _debugKeyword, false);
                        }
                        context.cmd.DispatchCompute(_computeShader, kernelLODs, groupsX, groupsY, 1);
                        
                        // Copy counter from AppendBuffer to ArgsBuffer (instance count)
                        for (int i = 0; i < 5; i++)
                        {
                            context.cmd.CopyCounterValue(culledPosHandle[rID, i], r.argsBuffers[i], 4);
                        }
                        rID++;
                    }
                });
            }
            if (_debugLods)
            {
                debugData.debugTexture = _debugTexture;
            }

            // Raster Pass
            using (var builder = renderGraph.AddRasterRenderPass("Indirect Draw Pass", out RasterPassData passData))
            {
                
                for (int i = 0; i < _renderers.Length; i++) {
                    for (int j = 0; j < 5; j++) {
                        builder.UseBuffer(culledPosHandle[i,j], AccessFlags.Read);
                        builder.UseBuffer(argsHandle[i,j], AccessFlags.Read);
                    }
                }
                
                builder.SetRenderAttachment(resourceData.activeColorTexture, 0);
                builder.SetRenderAttachmentDepth(resourceData.activeDepthTexture, AccessFlags.Write);

                builder.SetRenderFunc((RasterPassData data, RasterGraphContext ctx) => {

                    MaterialPropertyBlock props = new MaterialPropertyBlock();

                    foreach (var r in _renderers)
                    {
                        for (int i = 0; i < 5; i++) {
                            props.SetBuffer("vertexBuffer", r.lods[i].posBuffer);
                            props.SetBuffer("idxBuffer", r.lods[i].idxBuffer);
                            props.SetBuffer("colorBuffer", r.lods[i].colorBuffer);
                            props.SetBuffer("culledPositionsBuffer", r.culledPosBuffers[i]);
                            props.SetVector("lodLevel", new Vector4(i / 4.0f, 1.0f - (i / 4.0f), 0, 0));

                            props.SetVector("lod0Color", _lodColor[0]);
                            props.SetVector("lod1Color", _lodColor[1]);
                            props.SetVector("lod2Color", _lodColor[2]);
                            props.SetVector("lod3Color", _lodColor[3]);
                            props.SetVector("lod4Color", _lodColor[4]);

                            if (_debugLods && _debugMode == DebugMode.LodMat || _debugMode == DebugMode.LodBoth)
                                _material.EnableKeyword("_DEBUG_LODS");
                            else
                            {
                                _material.DisableKeyword("_DEBUG_LODS");
                            }
                            
                            ctx.cmd.DrawProceduralIndirect(r.lods[i].idxBuffer, Matrix4x4.identity, _material, 0, MeshTopology.Triangles, r.argsBuffers[i], 0, props);  
                        }
                    }

                });

            }
        }

        private class ComputePassData { }
        private class RasterPassData { }

        public void Dispose()
        {
            for (int rID = 0; rID < _renderers.Length; rID++)
            {
                for (int i = 0; i < _renderers[rID].lods.Length; i++)
                {
                    BufferRelease(ref _renderers[rID].lods[i].posBuffer);
                    BufferRelease(ref _renderers[rID].lods[i].uvBuffer);
                    BufferRelease(ref _renderers[rID].lods[i].colorBuffer);
                    BufferRelease(ref _renderers[rID].lods[i].idxBuffer);
                }
                BufferRelease(ref _renderers[rID].instancePosBuffer);
                BufferRelease(ref _renderers[rID].weightsBuffer);
                for (int j = 0; j < 5; j++)
                {
                    BufferRelease(ref _renderers[rID].culledPosBuffers[j]);
                    BufferRelease(ref _renderers[rID].argsBuffers[j]);
                }
            }
        }
    }

    private static void BufferRelease(ref GraphicsBuffer buffer)
    {
        if (buffer != null && buffer.IsValid())
        {
            buffer.Release();
        }
        buffer = null; // Essential to stop double-disposal
    }

    class DebugPass : ScriptableRenderPass
    {
        private Material _material;
        private TextureHandle _lodTextureHandle;
        private DebugMode _mode;

        public DebugPass(DebugMode mode)
        {
            _mode = mode;   
        }

        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData) 
        {
            const string passName = "LOD Debug Pass";

            if (_material == null)
                _material = new Material(Resources.Load<Shader>("Shaders/DebugLODs"));

            var debugData = frameData.Get<DebugData>();

            using (var builder = renderGraph.AddRasterRenderPass<DebugPassData>(passName, out var passData))
            {
                if (_mode == DebugMode.LodMat)
                {
                    return;
                }
                builder.AllowPassCulling(false); 
                _lodTextureHandle = debugData.debugTexture; 

                builder.UseTexture(_lodTextureHandle, AccessFlags.Read);
                UniversalResourceData resourceData = frameData.Get<UniversalResourceData>();
                builder.SetRenderAttachment(resourceData.activeColorTexture, 0);

                //Blit
                builder.SetRenderFunc((DebugPassData data, RasterGraphContext context) =>
                {
                    _material.SetFloat("debugMode", (float)_mode);
                    RasterCommandBuffer cmd = context.cmd;
                    Blitter.BlitTexture(cmd, _lodTextureHandle, new Vector4(1, 1, 0, 0), _material, 0);
                });
            }
        }
    }
    private class DebugPassData { }
}

