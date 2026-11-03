// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Phoenix;
using Phoenix.Interop;

namespace Skyline;

/// <summary>
/// A second script on the "Game" object: draws the score as seven-segment digits straight into
/// the engine's frame memory (Engine.DrawRects), plus a pulse when a drop lands perfectly.
/// </summary>
public sealed class ScoreHud : MetaBehaviour
{
    // Segments a..g per digit, as bits 0..6.
    private static readonly byte[] Digits = [0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F];

    private StackGame? _game;

    protected override void Start() => _game = gameObject.GetScript<StackGame>();

    protected override void Update()
    {
        if (_game is null)
        {
            return;
        }

        var frame = Time.frame;
        var text = _game.Score.ToString(System.Globalization.CultureInfo.InvariantCulture);
        const float h = 56f, w = 30f, gap = 14f, thick = 7f;
        var total = (text.Length * w) + ((text.Length - 1) * gap);
        var x = (frame.ViewportWidth - total) / 2;
        var y = frame.ViewportHeight * 0.08f;
        var color = new Color(1, 1, 1, _game.GameOver ? 0.55f + (0.45f * MathF.Abs(MathF.Sin((float)Time.time * 3))) : 0.92f).ToRgba();

        var rects = Engine.DrawRects((text.Length * 7) + 1);
        var n = 0;
        foreach (var ch in text)
        {
            var bits = Digits[ch - '0'];
            for (var s = 0; s < 7; s++)
            {
                rects[n++] = (bits & (1 << s)) != 0 ? Segment(s, x, y, w, h, thick, color) : default;
            }

            x += w + gap;
        }

        // A thin bar under the score flashes on perfect drops.
        var pulse = MathF.Max(0, 1 - (_game.Flash * 2.5f));
        rects[n] = pulse > 0 && !_game.GameOver
            ? PhxDrawRect.Rect((frame.ViewportWidth - 120) / 2, y + h + 14, 120, 4, new Color(1, 1, 1, pulse).ToRgba())
            : default;
    }

    private static PhxDrawRect Segment(int s, float x, float y, float w, float h, float t, uint rgba)
    {
        var half = (h - t) / 2;
        return s switch
        {
            0 => PhxDrawRect.Rect(x, y, w, t, rgba),                     // a: top
            1 => PhxDrawRect.Rect(x + w - t, y, t, half + t, rgba),      // b: top right
            2 => PhxDrawRect.Rect(x + w - t, y + half, t, half + t, rgba), // c: bottom right
            3 => PhxDrawRect.Rect(x, y + h - t, w, t, rgba),             // d: bottom
            4 => PhxDrawRect.Rect(x, y + half, t, half + t, rgba),       // e: bottom left
            5 => PhxDrawRect.Rect(x, y, t, half + t, rgba),              // f: top left
            _ => PhxDrawRect.Rect(x, y + half, w, t, rgba),              // g: middle
        };
    }
}
