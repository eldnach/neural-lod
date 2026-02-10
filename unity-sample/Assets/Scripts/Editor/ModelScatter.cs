using UnityEngine;
using UnityEditor;
using System.Collections.Generic;

[System.Serializable]
public class SpawnGroup
{
    public GameObject prefab;
    public int count = 5;
}

public class ModelScatter : MonoBehaviour
{
    [Header("Spawn Groups")]
    public List<SpawnGroup> spawnGroups = new List<SpawnGroup>();

    [Header("Global Settings")]
    public float baseYOffset = 0f;
    public Vector2 scaleRange = new Vector2(0.9f, 1.2f);
    public float rotationSnap = 45f;

    [Header("Area Selection")]
    public BoxCollider scatterVolume;

    public void Rescatter()
    {
        ClearTown();
        GenerateTown();
    }

    public void GenerateTown()
    {
        if (scatterVolume == null || spawnGroups == null || spawnGroups.Count == 0)
        {
            Debug.LogError("ModelScatter: Please assign a Box Collider and at least one Spawn Group!");
            return;
        }

        Bounds bounds = scatterVolume.bounds;

        foreach (SpawnGroup group in spawnGroups)
        {
            if (group.prefab == null) continue;

            for (int i = 0; i < group.count; i++)
            {
                // 1. Instantiate robustly
                GameObject newHouse;
                if (PrefabUtility.IsPartOfPrefabAsset(group.prefab))
                    newHouse = (GameObject)PrefabUtility.InstantiatePrefab(group.prefab);
                else
                    newHouse = Instantiate(group.prefab);

                // 2. Set Parent
                newHouse.transform.SetParent(transform);

                // 3. Position Logic (Scatter X/Z, Base Y + Prefab Local Y)
                float randomX = Random.Range(bounds.min.x, bounds.max.x);
                float randomZ = Random.Range(bounds.min.z, bounds.max.z);
                
                Vector3 finalPos = new Vector3(
                    randomX + group.prefab.transform.localPosition.x,
                    baseYOffset + group.prefab.transform.localPosition.y,
                    randomZ + group.prefab.transform.localPosition.z
                );
                newHouse.transform.position = finalPos;

                // 4. Rotation Logic (Prefab Base + Random Snap)
                int steps = Mathf.Max(1, Mathf.RoundToInt(360f / rotationSnap));
                float randomYRotation = Random.Range(0, steps) * rotationSnap;
                
                newHouse.transform.eulerAngles = new Vector3(
                    group.prefab.transform.eulerAngles.x, 
                    group.prefab.transform.eulerAngles.y + randomYRotation, 
                    group.prefab.transform.eulerAngles.z
                );

                // 5. Scale Logic
                float randomFactor = Random.Range(scaleRange.x, scaleRange.y);
                newHouse.transform.localScale = Vector3.Scale(group.prefab.transform.localScale, Vector3.one * randomFactor);

                // 6. Support Undo
                Undo.RegisterCreatedObjectUndo(newHouse, "Generate Town");
            }
        }
    }

    public void ClearTown()
    {
        while (transform.childCount > 0)
        {
            Undo.DestroyObjectImmediate(transform.GetChild(0).gameObject);
        }
    }
}