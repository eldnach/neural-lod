using UnityEngine;
using UnityEditor;

[CustomEditor(typeof(ModelScatter))]
public class TownScattererEditor : Editor
{
    public override void OnInspectorGUI()
    {
        DrawDefaultInspector();

        ModelScatter script = (ModelScatter)target;

        EditorGUILayout.Space(10);
        
        // Make the Rescatter button blue to stand out
        GUI.backgroundColor = new Color(0.5f, 0.8f, 1f);
        if (GUILayout.Button("Rescatter (Randomize)", GUILayout.Height(30)))
        {
            script.Rescatter();
        }

        // Standard buttons
        GUI.backgroundColor = Color.white;
        if (GUILayout.Button("Add More Houses"))
        {
            script.GenerateTown();
        }

        GUI.backgroundColor = new Color(1f, 0.6f, 0.6f);
        if (GUILayout.Button("Clear All"))
        {
            script.ClearTown();
        }
    }
}