using UnityEngine;

[ExecuteInEditMode]
public class LODGradientOverlay : MonoBehaviour
{
    public LODGradient data;
    private Texture2D _texture;
    private GUIStyle _textStyle;

    [Header("Layout Settings")]
    public Vector2 offset = Vector2.zero; 
    [Range(0.1f, 2.0f)]
    public float scale = 1.0f;
    public int fontSize = 20;

    private readonly Vector2 _refRes = new Vector2(1920, 1080);

    void OnEnable()
    {
        _texture = new Texture2D(1, 1);
        _texture.SetPixel(0, 0, Color.white);
        _texture.Apply();

        _textStyle = new GUIStyle();
        _textStyle.normal.textColor = Color.black;
        _textStyle.alignment = TextAnchor.MiddleCenter;
        _textStyle.fontStyle = FontStyle.Bold;
    }

    void OnDisable()
    {
        if (_texture != null) DestroyImmediate(_texture);
    }

    void OnGUI()
    {
        if (data == null || data.val == null || data.labels == null) return;
        float horizRatio = Screen.width / _refRes.x;
        float vertRatio = Screen.height / _refRes.y;
        Matrix4x4 oldMatrix = GUI.matrix;
        GUI.matrix = Matrix4x4.TRS(Vector3.zero, Quaternion.identity, new Vector3(horizRatio, vertRatio, 1));
        _textStyle.fontSize = Mathf.RoundToInt(fontSize * scale);

        float width = _refRes.x * 0.7f * scale;
        float height = _refRes.y * 0.05f * scale;

        float x = ((_refRes.x - width) / 2) + offset.x;
        float y = (_refRes.y - height - 50.0f) - offset.y;

        int count = data.labels.Length;
        float segmentWidth = width / count;

        for (int i = 0; i < count; i++)
        {
            Rect segmentRect = new Rect(x + (i * segmentWidth), y, segmentWidth, height);
            
            float t = (count > 1) ? (float)i / (count - 1) : 0.5f;
            
            GUI.color = data.val.Evaluate(t);
            GUI.DrawTexture(segmentRect, _texture);
            
            GUI.color = Color.white;
            GUI.Label(segmentRect, data.labels[i], _textStyle);
        }

        GUI.matrix = oldMatrix;
    }
}