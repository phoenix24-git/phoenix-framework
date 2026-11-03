// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Phoenix;

namespace Skyline;

/// <summary>
/// Attached to "Main Camera": rises with the tower, tints the sky with the current level's hue,
/// and pulls back on game over to show the whole tower.
/// </summary>
public sealed class CameraRig : MetaBehaviour
{
    private StackGame? _game;
    private Vector3 _offset;
    private float _baseSize;
    private float _followSpeed = 3f;
    private float _heightAboveTop = 0.5f;

    protected override void Start()
    {
        _followSpeed = properties.GetFloat("followSpeed", _followSpeed);
        _heightAboveTop = properties.GetFloat("heightAboveTop", _heightAboveTop);
        _offset = transform.localPosition; // the scene's framing, relative to the tower top
        _baseSize = gameObject.GetComponent<Camera>().orthographicSize;
        _game = GameObject.Find("Game").GetScript<StackGame>();
    }

    protected override void Update()
    {
        if (_game is null)
        {
            return;
        }

        var camera = gameObject.GetComponent<Camera>();
        var t = Mathf.Clamp01(Time.deltaTime * _followSpeed);
        var size = _baseSize;
        var focusY = _game.TopY + _heightAboveTop;
        if (_game.GameOver)
        {
            // Frame the whole tower, from its base to the top.
            size = MathF.Max(_baseSize, (_game.TopY * 0.75f) + 2.5f);
            focusY = (_game.TopY / 2) + _heightAboveTop;
        }

        var target = _offset + new Vector3(0, focusY, 0);
        transform.localPosition = Vector3.Lerp(transform.localPosition, target, t);
        camera.orthographicSize = Mathf.Lerp(camera.orthographicSize, size, t);
        var sky = Color.HSVToRGB(_game.Hue + 0.1f, 0.5f, 0.24f); // a dusk tint of the tower color
        camera.backgroundColor = Color.Lerp(camera.backgroundColor, sky, t * 0.5f);
    }
}
