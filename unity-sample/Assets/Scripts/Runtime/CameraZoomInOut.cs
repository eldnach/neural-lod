using UnityEngine;

public class CameraZoom : MonoBehaviour
{
    public Camera targetCamera; 
    public float minFOV = 40.0f;
    public float maxFOV = 90.0f;
    public float cycleDuration = 3.0f;

    private void Start()
    {
        if (targetCamera == null)
        {
            targetCamera = GetComponent<Camera>();
        }
    }

    private void Update()
    {
        float t = Mathf.PingPong(Time.time / cycleDuration, 1f);
        float smoothT = t * t * (3f - 2f * t);
        targetCamera.fieldOfView = Mathf.Lerp(minFOV, maxFOV, smoothT);
    }
}