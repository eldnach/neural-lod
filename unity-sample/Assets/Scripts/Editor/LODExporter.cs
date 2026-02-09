using UnityEngine;
using UnityEditor;
using System.IO;

public class LODExporter : EditorWindow
{
    private string folderName = "LOD_Export";

    [MenuItem("Tools/LOD Exporter")]
    public static void ShowWindow() => GetWindow<LODExporter>("LOD Export");

    void OnGUI()
    {
        GUILayout.Label("LOD Exporter", EditorStyles.boldLabel);
        
        EditorGUILayout.Space();
        folderName = EditorGUILayout.TextField("Export Folder Name", folderName);
        EditorGUILayout.Space();

        // Target the currently selected object in the Hierarchy
        GameObject selected = Selection.activeGameObject;

        if (selected == null)
        {
            EditorGUILayout.HelpBox("Please select a GameObject in the Hierarchy.", MessageType.Warning);
            return;
        }

        MeshFilter mf = selected.GetComponentInChildren<MeshFilter>();
        if (mf == null || mf.sharedMesh == null)
        {
            EditorGUILayout.HelpBox("Selected object has no MeshFilter.", MessageType.Error);
            return;
        }

        GUILayout.Label($"Target: {selected.name}", EditorStyles.miniLabel);
        GUILayout.Label($"LOD Count: {mf.sharedMesh.lodCount}", EditorStyles.miniLabel);

        GUI.backgroundColor = Color.cyan;
        if (GUILayout.Button("EXPORT LODS", GUILayout.Height(40)))
        {
            ExportLODs(selected, mf.sharedMesh);
        }
        GUI.backgroundColor = Color.white;
    }

    void ExportLODs(GameObject go, Mesh mesh)
    {
        int totalLods = mesh.lodCount;
        string dirPath = Path.Combine(Application.dataPath, folderName);
        if (!Directory.Exists(dirPath)) Directory.CreateDirectory(dirPath);

        float startTime = Time.realtimeSinceStartup;

        for (int i = 0; i < totalLods; i++)
        {
            string fileName = $"{go.name}_LOD{i}.obj";
            string fullPath = Path.Combine(dirPath, fileName);
            
            SaveLODToObj(mesh, i, fullPath);
        }

        float elapsed = Time.realtimeSinceStartup - startTime;
        Debug.Log($"<color=green><b>Success!</b></color> Exported {totalLods} LODs in {elapsed:F2} seconds.");
        
        AssetDatabase.Refresh();
        EditorUtility.RevealInFinder(dirPath);
    }

    private void SaveLODToObj(Mesh mesh, int level, string path)
    {
        using (StreamWriter sw = new StreamWriter(path, false, System.Text.Encoding.UTF8, 65536))
        {
            sw.WriteLine("#LOD Export - Textured + Vertex Colors");
            
            Vector3[] verts = mesh.vertices;
            Vector2[] uvs = mesh.uv;
            Color[] colors = mesh.colors;
            
            bool hasColors = (colors != null && colors.Length == verts.Length);
            bool hasUVs = (uvs != null && uvs.Length == verts.Length);

            // 1. Write Vertices (with optional colors)
            for (int i = 0; i < verts.Length; i++)
            {
                if (hasColors)
                    sw.WriteLine($"v {-verts[i].x:F6} {verts[i].y:F6} {verts[i].z:F6} {colors[i].r:F6} {colors[i].g:F6} {colors[i].b:F6}");
                else
                    sw.WriteLine($"v {-verts[i].x:F6} {verts[i].y:F6} {verts[i].z:F6}");
            }

            // 2. Write ALL UVs - Ensuring 1:1 mapping with vertices
            if (hasUVs)
            {
                foreach (Vector2 uv in uvs)
                {
                    sw.WriteLine($"vt {uv.x:F6} {uv.y:F6}");
                }
            }
            else
            {
                // FALLBACK: If mesh has no UVs, write dummy UVs 
                for (int i = 0; i < verts.Length; i++) sw.WriteLine("vt 0.000000 0.000000");
            }

            // 3. Write Faces for specific LOD level
            for (int s = 0; s < mesh.subMeshCount; s++)
            {
                int[] indices = mesh.GetIndices(s, level);
                for (int i = 0; i < indices.Length; i += 3)
                {
                    // OBJ is 1-indexed. Winding order flip: i1, i2, i3
                    int i1 = indices[i] + 1;
                    int i2 = indices[i + 1] + 1;
                    int i3 = indices[i + 2] + 1;
                    
                    // Format: v/vt (VertexIndex/UVIndex)
                    // This tells tinyobjloader exactly which UV pair to use for each vertex
                    sw.WriteLine($"f {i3}/{i3} {i2}/{i2} {i1}/{i1}");
                }
            }
        }
    }
}