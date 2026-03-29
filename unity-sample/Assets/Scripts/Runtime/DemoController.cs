using UnityEngine;

public class DemoController : MonoBehaviour
{
    [Header("References")]
    public Camera playerCamera; 
    public Material blitMaterial; 

    [Header("Movement")]
    public float walkSpeed = 5f;
    public float lookSensitivity = 2f;

    [Header("Zoom Settings")]
    public float normalFOV = 60f;
    public float zoomFOV = 20f;
    public float zoomLerpSpeed = 10f;
    
    private float xRotation = 0f;
    private float currentZoomAlpha = 0f;

    void Start()
    {
        // 1. Force-find the camera if the slot is empty
        if (playerCamera == null) 
            playerCamera = GetComponentInChildren<Camera>();

        // 2. Final check to prevent the NullReferenceException
        if (playerCamera == null)
        {
            Debug.LogError("<b>DemoController:</b> No Camera found! Attach this script to the Player parent and ensure the Camera is a child.");
            enabled = false; // Disable script to stop errors
            return;
        }

        Cursor.lockState = CursorLockMode.Locked;
    }

    void Update()
    {
        // Movement & Rotation
        HandleRotation();
        HandleMovement();
        
        // Zoom Logic
        HandleZoom();
    }

    void HandleRotation()
    {
        float mouseX = Input.GetAxis("Mouse X") * lookSensitivity;
        float mouseY = Input.GetAxis("Mouse Y") * lookSensitivity;

        xRotation -= mouseY;
        xRotation = Mathf.Clamp(xRotation, -90f, 90f);
        
        playerCamera.transform.localRotation = Quaternion.Euler(xRotation, 0f, 0f);
        transform.Rotate(Vector3.up * mouseX);
    }

    void HandleMovement()
    {
        float moveX = Input.GetAxis("Horizontal");
        float moveZ = Input.GetAxis("Vertical");

        Vector3 move = (transform.right * moveX) + (transform.forward * moveZ);
        // Normalize so diagonal movement isn't faster
        transform.position += move.normalized * walkSpeed * Time.deltaTime;
    }

    void HandleZoom()
    {
        // Right-click is index 1
        bool isZooming = Input.GetMouseButton(1); 
        float targetFOV = isZooming ? zoomFOV : normalFOV;
        
        // Smoothly interpolate FOV
        playerCamera.fieldOfView = Mathf.Lerp(playerCamera.fieldOfView, targetFOV, Time.deltaTime * zoomLerpSpeed);

        // Update Shader Alpha for the scope blit
        if (blitMaterial != null)
        {
            currentZoomAlpha = Mathf.Lerp(currentZoomAlpha, isZooming ? 1f : 0f, Time.deltaTime * zoomLerpSpeed);
            blitMaterial.SetFloat("_ZoomAmount", currentZoomAlpha);
        }
    }
}