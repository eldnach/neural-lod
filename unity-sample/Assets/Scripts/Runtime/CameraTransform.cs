using UnityEngine;

public class CameraTransform : MonoBehaviour
{
    public Transform cameraTransform;
    public float minDistance = 0.5f;
    public float maxDistance = 10.0f;
    public float cycleDuration = 3.0f;

    private void Update()
    {
        float t = Mathf.PingPong(Time.time / cycleDuration, 1f);
        float smoothT = t * t * (3f - 2f * t);
        
        float currentDistance = Mathf.Lerp(minDistance, maxDistance, smoothT);
        cameraTransform.localPosition = cameraTransform.forward * -currentDistance;
    }
}