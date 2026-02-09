using UnityEngine;

[ExecuteInEditMode]
public class LODGradientOverlay : MonoBehaviour
{
    public LODGradient data;
    private Texture2D _texture;
    private GUIStyle _textStyle;

    public Vector2 offset = Vector2.zero; // X/Y displacement from center/bottom
    [Range(0.1f, 2.0f)]
    public float scale = 1.0f;

    void Start()
    {
        _texture = new Texture2D(1, 1);
        _texture.SetPixel(0, 0, Color.white);
        _texture.Apply();

        _textStyle = new GUIStyle();
        _textStyle.normal.textColor = Color.black;
        _textStyle.alignment = TextAnchor.MiddleCenter;
        _textStyle.fontSize = 20;
        _textStyle.fontStyle = FontStyle.Bold;
    }

    void OnGUI()
    {
        if (data == null || data.val == null || data.labels == null) return;

        float width = Screen.width * 0.7f * scale;
        float height = Screen.height * 0.05f * scale;

        float x = ((Screen.width - width) / 2) + offset.x;
        float y = (Screen.height - height - 20.0f) - offset.y;

        int count = data.labels.Length;
        float segmentWidth = width / count;

        for (int i = 0; i < count; i++)
        {
            Rect segmentRect = new Rect(x + (i * segmentWidth), y, segmentWidth, height);
            
            float t = (float)i / (count - 1);
            GUI.color = data.val.Evaluate(t);
            GUI.DrawTexture(segmentRect, _texture);
            
            GUI.color = Color.white;
            GUI.Label(segmentRect, data.labels[i], _textStyle);
        }
    }
}