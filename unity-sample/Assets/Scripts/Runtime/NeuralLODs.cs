using UnityEngine;
using Unity.InferenceEngine;
using System;

public class NeuralLODs : MonoBehaviour
{
    public enum LODMode { Traditional, AI }
    [Header("Mode Selection")]
    public LODMode lodMode = LODMode.AI;

    [Header("AI Assets")]
    public ModelAsset onnxModel;
    public TextAsset scalerJson;

    [Header("LOD Settings")]
    public float errorThreshold = 1.0f;

    [Header("Debug Visuals")]
    public bool debugLods = false;
    public LODGradient gradient; 
    private Worker _worker;
    private ScalerConfig _scaler;
    private Vector3 _lastPos;
    private Vector3 _velocity;

    [System.Serializable]
    public class ScalerConfig {
        public float[] mean;
        public float[] std;
    }

    private MeshRenderer _renderer;
    private MaterialPropertyBlock _propBlock;

    void Start()
    {
        _renderer = GetComponent<MeshRenderer>();
        _propBlock = new MaterialPropertyBlock();
        
        _scaler = JsonUtility.FromJson<ScalerConfig>(scalerJson.text);
        Model runtimeModel = ModelLoader.Load(onnxModel);
        
        _worker = new Worker(runtimeModel, BackendType.CPU);
        _lastPos = transform.position;
    }

    void Update()
    {
        _velocity = (transform.position - _lastPos) / Time.deltaTime;
        _lastPos = transform.position;

        if (lodMode == LODMode.AI)
        {
            if (Time.frameCount % 5 == 0) {
                int prediction = PredictLOD();
                int lodIndex = prediction;
        
                Debug.Log($"Prediction: {prediction}");
                _renderer.forceMeshLod = (short)lodIndex;
                UpdateMaterialDebug(lodIndex);
            }
        } 
        else
        {
            if (_renderer.forceMeshLod != -1)
            {
                _renderer.forceMeshLod = -1;
                UpdateMaterialDebug(-1);
            }
        }
    }

    void UpdateMaterialDebug(int lodIndex)
    {
        if (_renderer == null) return;

        if (debugLods)
            _renderer.material.EnableKeyword("_DEBUG_LODS");
        else
        {
            _renderer.material.DisableKeyword("_DEBUG_LODS");
            return;
        }

        if (debugLods && gradient.val != null)
        {
            int count = gradient.labels.Length;
            float t = (lodIndex + 0.5f) / count;
            Color evaluatedColor = gradient.val.Evaluate(t);

            _renderer.GetPropertyBlock(_propBlock);
            _propBlock.SetColor("_DebugColor", evaluatedColor);
            _renderer.SetPropertyBlock(_propBlock);
        }
    }

    int PredictLOD()
    {
        Camera cam = Camera.main;
        Vector3 pos = transform.position;
        float speed = _velocity.magnitude; 
        float dist = Vector3.Distance(pos, cam.transform.position);
        float yScale = 1.0f / Mathf.Tan(cam.fieldOfView * 0.5f * Mathf.Deg2Rad);
        Vector3 toObjNorm = (pos - cam.transform.position).normalized;
        float angle = Mathf.Acos(Mathf.Clamp(Vector3.Dot(toObjNorm, cam.transform.forward), -1f, 1f));
        
        Bounds bounds = _renderer.bounds;;
        float objectWorldHeight = (bounds.max.y - bounds.min.y)  *  transform.localScale.y;
        float projHeight = (objectWorldHeight * yScale) / Mathf.Max(0.1f, dist);

        // Optional: early out with lowest LOD for objects that are too far or not visible
        if (projHeight < 0.05f)
        {
            return 4;
        }

        // Evaluate LODs using neural network
        float[] errors = new float[5];
        for (int i = 4; i >= 0; i--)
        {
            float lodInput = (float)i / 4.0f; 

            float[] rawFeatures = new float[4] {
                lodInput, dist, angle, projHeight
            };

            for (int f = 0; f < rawFeatures.Length; f++) {
                rawFeatures[f] = (rawFeatures[f] - _scaler.mean[f]) / _scaler.std[f];
            }

            using Tensor<float> inputTensor = new Tensor<float>(new TensorShape(1, rawFeatures.Length), rawFeatures);
            _worker.Schedule(inputTensor);
            
            var outputTensor = _worker.PeekOutput() as Tensor<float>;
            float[] result = outputTensor.DownloadToArray();
            errors[i] = Mathf.Max(0, result[0]);
            if (debugLods) {Debug.Log($"Distance: {dist}, ProjectHeight:{projHeight} LOD {i}: Predicted Error = {errors[i]}");}

            if (errors[i] < errorThreshold) {
                if (debugLods) {Debug.Log($"LOD {i}: is below threshold {errorThreshold}");}
                return i;
            }
        }
        
        if (debugLods) {Debug.Log("All LODs predicted error above threshold. Applying the highest LOD,");}
        return 0; 
    }

    void OnDisable() => _worker?.Dispose();
}