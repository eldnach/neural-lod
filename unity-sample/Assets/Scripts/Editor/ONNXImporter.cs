using UnityEngine;
using UnityEditor;
using Unity.InferenceEngine; 
using System.Collections.Generic;
using System.IO;
using System.Linq;

public class ONNXImporter : EditorWindow
{
    [MenuItem("Tools/Import ONNX and Export Weights")]
    public static void ShowWindow()
    {
        ModelAsset modelAsset = Selection.activeObject as ModelAsset;

        if (modelAsset == null)
        {
            EditorUtility.DisplayDialog("Selection Error", 
                "Please select your .onnx model file in the Project window first.", "OK");
            return;
        }

        Model model = ModelLoader.Load(modelAsset);
        List<float> flatWeights = new List<float>();
        var dataChunks = new List<float[]>();

        Debug.Log($"--- Extracting Weights from: {modelAsset.name} ---");

        foreach (var constant in model.constants)
        {
            int count = constant.weights.Length;
            float[] data = new float[count];
            
            NativeTensorArray.Copy(constant.weights, 0, data, 0, count);
            
            dataChunks.Add(data);
            Debug.Log($"Found Chunk: {count} floats");
        }

        // 3. Match the Architecture (4 -> 64 -> 32 -> 1)
        if (!TryPopChunk(dataChunks, 256, flatWeights, "L1 Weights (4x64)")) return;
        if (!TryPopChunk(dataChunks, 64,  flatWeights, "L1 Bias (64)")) return;
        if (!TryPopChunk(dataChunks, 2048, flatWeights, "L2 Weights (64x32)")) return;
        if (!TryPopChunk(dataChunks, 32,   flatWeights, "L2 Bias (32)")) return;
        if (!TryPopChunk(dataChunks, 32,   flatWeights, "L3 Weights (32x1)")) return;
        if (!TryPopChunk(dataChunks, 1,    flatWeights, "L3 Bias (1)")) return;

        string savePath = EditorUtility.SaveFilePanel("Save Weights", "Assets", "LODWeights", "bytes");
        if (!string.IsNullOrEmpty(savePath))
        {
            byte[] bytes = new byte[flatWeights.Count * 4];
            System.Buffer.BlockCopy(flatWeights.ToArray(), 0, bytes, 0, bytes.Length);
            File.WriteAllBytes(savePath, bytes);
            AssetDatabase.Refresh();
            
            Debug.Log($"<color=cyan><b>SUCCESS:</b> Exported {flatWeights.Count} weights to {savePath}</color>");
        }
    }

    private static bool TryPopChunk(List<float[]> chunks, int size, List<float> target, string label)
    {
        for (int i = 0; i < chunks.Count; i++)
        {
            if (chunks[i].Length == size)
            {
                target.AddRange(chunks[i]);
                Debug.Log($"<color=green>[MATCHED]</color> {label}");
                chunks.RemoveAt(i);
                return true;
            }
        }
        
        string found = string.Join(", ", chunks.Select(c => c.Length));
        Debug.LogError($"[FAILED] {label} (Expected {size}). Found sizes in model: {found}");
        return false;
    }
}