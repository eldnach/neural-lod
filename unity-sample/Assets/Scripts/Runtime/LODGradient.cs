using UnityEngine;

[CreateAssetMenu(fileName = "LODData", menuName = "ScriptableObjects/LODData")]
public class LODGradient : ScriptableObject
{
    public Gradient val;
    public string[] labels = { "LOD 0", "LOD 1", "LOD 2", "LOD 4" };
}