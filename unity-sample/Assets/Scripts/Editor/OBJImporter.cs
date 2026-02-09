using UnityEngine;
using UnityEditor;
using System.IO;
using System.Collections.Generic;
using System.Globalization;

public class OBJImporter : EditorWindow
{
        [MenuItem("Tools/Import OBJ and generate LODs")]
    public static void ImportWithColorsAndLOD()
    {
        string path = EditorUtility.OpenFilePanel("Select OBJ", "", "obj");
        if (string.IsNullOrEmpty(path)) return;

        string meshName = Path.GetFileNameWithoutExtension(path);
        
        List<Vector3> verts = new List<Vector3>();
        List<Vector2> uvs = new List<Vector2>(); // NEW: UV List
        List<Color> colors = new List<Color>();
        
        // We need a temporary list to hold the raw UVs from the file
        List<Vector2> temp_uvs = new List<Vector2>();
        List<int> tris = new List<int>();

        string[] lines = File.ReadAllLines(path);

        foreach (string line in lines)
        {
            string trimmed = line.Trim();
            if (trimmed.Length < 3 || trimmed.StartsWith("#")) continue;
            string[] parts = trimmed.Split(new[] { ' ', '\t' }, System.StringSplitOptions.RemoveEmptyEntries);

            if (parts[0] == "v") 
            {
                verts.Add(new Vector3(-float.Parse(parts[1], CultureInfo.InvariantCulture), 
                                    float.Parse(parts[2], CultureInfo.InvariantCulture), 
                                    float.Parse(parts[3], CultureInfo.InvariantCulture)));

                if (parts.Length >= 7)
                    colors.Add(new Color(float.Parse(parts[4], CultureInfo.InvariantCulture),
                                        float.Parse(parts[5], CultureInfo.InvariantCulture),
                                        float.Parse(parts[6], CultureInfo.InvariantCulture)));
            }
            else if (parts[0] == "vt") // NEW: Parse Texture Coordinates
            {
                temp_uvs.Add(new Vector2(float.Parse(parts[1], CultureInfo.InvariantCulture),
                                        float.Parse(parts[2], CultureInfo.InvariantCulture)));
            }
            else if (parts[0] == "f") 
            {
                List<int> faceIndices = new List<int>();
                List<int> uvIndices = new List<int>();

                for (int i = 1; i < parts.Length; i++)
                {
                    string[] subParts = parts[i].Split('/');
                    faceIndices.Add(int.Parse(subParts[0]) - 1);
                    
                    // If there's a UV index (e.g., f 1/1/1 or f 1/1)
                    if (subParts.Length > 1 && !string.IsNullOrEmpty(subParts[1]))
                        uvIndices.Add(int.Parse(subParts[1]) - 1);
                }

                // Simple Triangle Fan for faces
                for (int i = 1; i < faceIndices.Count - 1; i++) {
                    tris.Add(faceIndices[0]);
                    tris.Add(faceIndices[i + 1]);
                    tris.Add(faceIndices[i]);
                }
            }
        }

        Mesh mesh = new Mesh();
        mesh.name = meshName;
        if (verts.Count > 65535) mesh.indexFormat = UnityEngine.Rendering.IndexFormat.UInt32;

        mesh.SetVertices(verts);
        if (colors.Count == verts.Count) mesh.SetColors(colors);

        // NEW: Assign UVs to the mesh
        // Note: This logic assumes 1:1 vertex-to-UV mapping. 
        // If your OBJ has split UVs, you'd need a more complex Vertex dictionary.
        if (temp_uvs.Count >= verts.Count)
        {
            mesh.SetUVs(0, temp_uvs.GetRange(0, verts.Count));
        }

        mesh.SetIndices(tris.ToArray(), MeshTopology.Triangles, 0);
        
        mesh.RecalculateNormals();
        mesh.RecalculateBounds();

        // Generate LODs
        MeshLodUtility.GenerateMeshLods(mesh, 0, 10);

        AssetDatabase.CreateAsset(mesh, $"Assets/{meshName}_WithLODs.asset");
        AssetDatabase.SaveAssets();
        
        Debug.Log($"<color=green>SUCCESS:</color> {meshName} now has UVs and LODs.");
    }

}