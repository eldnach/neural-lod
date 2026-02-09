using UnityEngine;
using UnityEngine.InputSystem; // Required for New Input System

[RequireComponent(typeof(Camera))]
public class FlyCam : MonoBehaviour
{
    [Header("Movement Settings")]
    public float mainSpeed = 20.0f;
    public float shiftMultiplier = 2.5f;
    public float lookSensitivity = 0.1f; // New Input System delta values are larger

    private float rotationX = 0;
    private float rotationY = 0;

    void Start()
    {
        Vector3 euler = transform.rotation.eulerAngles;
        rotationX = euler.y;
        rotationY = -euler.x;
    }

    void Update()
    {
        // --- 1. Rotation Logic (Right Click Hold) ---
        if (Mouse.current.rightButton.isPressed)
        {
            Cursor.visible = false;
            Cursor.lockState = CursorLockMode.Locked;

            Vector2 mouseDelta = Mouse.current.delta.ReadValue();
            
            rotationX += mouseDelta.x * lookSensitivity;
            rotationY += mouseDelta.y * lookSensitivity;
            rotationY = Mathf.Clamp(rotationY, -90, 90);

            transform.localRotation = Quaternion.Euler(-rotationY, rotationX, 0);
        }
        else
        {
            Cursor.visible = true;
            Cursor.lockState = CursorLockMode.None;
        }

        // --- 2. Translation Logic ---
        float p_Velocity = mainSpeed;
        if (Keyboard.current.leftShiftKey.isPressed)
        {
            p_Velocity *= shiftMultiplier;
        }

        Vector3 move = GetBaseInput();
        transform.Translate(move * p_Velocity * Time.deltaTime);
    }

    private Vector3 GetBaseInput()
    {
        Vector3 p_Velocity = new Vector3();
        var kbd = Keyboard.current;

        // WASD
        if (kbd.wKey.isPressed) p_Velocity += Vector3.forward;
        if (kbd.sKey.isPressed) p_Velocity += Vector3.back;
        if (kbd.aKey.isPressed) p_Velocity += Vector3.left;
        if (kbd.dKey.isPressed) p_Velocity += Vector3.right;

        // Vertical (Q/E)
        if (kbd.eKey.isPressed) p_Velocity += Vector3.up;
        if (kbd.qKey.isPressed) p_Velocity += Vector3.down;

        return p_Velocity;
    }
}