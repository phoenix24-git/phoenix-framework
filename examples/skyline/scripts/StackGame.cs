// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using Phoenix;

namespace Skyline;

/// <summary>
/// The game, attached to the "Game" object by res/scenes/main.pscn. Every slab is a GameObject
/// with a Transform, a MeshFilter (cube) and a MeshRenderer; the engine renders them in 3D.
/// Tap (or click) to drop the sliding slab.
/// </summary>
public sealed class StackGame : MetaBehaviour
{
    private const float SlideRange = 3.6f;

    private readonly List<GameObject> _slabs = [];
    private readonly List<Debris> _debris = [];
    private Footprint _top;
    private GameObject _moving;
    private SlideAxis _axis;
    private float _phase;
    private int _perfectStreak;
    private float _flash;
    private bool _autoplay;

    // Tuning, from the scene file's [script] section.
    private float _speed = 2.4f;
    private float _slabHeight = 0.42f;
    private float _startSize = 3f;
    private float _tolerance = 0.09f;
    private float _hueStart = 0.6f;
    private float _hueStep = 0.032f;

    /// <summary>Slabs placed this run (the score).</summary>
    public int Score { get; private set; }

    public bool GameOver { get; private set; }

    /// <summary>World height of the tower's top surface.</summary>
    public float TopY => Score * _slabHeight;

    /// <summary>The hue of the current level (the camera tints the sky with it).</summary>
    public float Hue => _hueStart + (Score * _hueStep);

    /// <summary>Seconds since the last perfect drop's flash started (for the HUD).</summary>
    public float Flash => _flash;

    private sealed class Debris(GameObject gameObject, Vector3 velocity, Vector3 spin)
    {
        public GameObject GameObject { get; } = gameObject;
        public Vector3 Velocity { get; set; } = velocity;
        public Vector3 Spin { get; } = spin;
        public float Age { get; set; }
    }

    protected override void Awake()
    {
        _speed = properties.GetFloat("speed", _speed);
        _slabHeight = properties.GetFloat("slabHeight", _slabHeight);
        _startSize = properties.GetFloat("startSize", _startSize);
        _tolerance = properties.GetFloat("perfectTolerance", _tolerance);
        _hueStart = properties.GetFloat("hueStart", _hueStart);
        _hueStep = properties.GetFloat("hueStep", _hueStep);
        // Headless smoke runs (CI) play by themselves: SKYLINE_AUTOPLAY=1.
        _autoplay = Environment.GetEnvironmentVariable("SKYLINE_AUTOPLAY") == "1";
    }

    protected override void Start() => NewGame();

    protected override void Update()
    {
        var dt = Time.deltaTime;
        _flash += dt;
        UpdateDebris(dt);

        var tapped = Input.GetMouseButtonDown(0);
        if (GameOver)
        {
            if (tapped || (_autoplay && _flash > 1.5f))
            {
                NewGame();
            }

            return;
        }

        // Slide back and forth across the tower.
        _phase += dt * StackRules.SpeedFor(Score, _speed);
        var offset = Mathf.PingPong(_phase, 2 * SlideRange) - SlideRange;
        var footprint = _top.With(_axis, _top.Center(_axis) + offset, _top.Size(_axis));
        Place(_moving, footprint, (Score + 0.5f) * _slabHeight);

        if (_autoplay)
        {
            tapped = MathF.Abs(offset) < _tolerance * 0.5f;
        }

        if (tapped)
        {
            Drop(footprint);
        }
    }

    private void NewGame()
    {
        foreach (var slab in _slabs)
        {
            Destroy(slab);
        }

        foreach (var d in _debris)
        {
            Destroy(d.GameObject);
        }

        _slabs.Clear();
        _debris.Clear();
        Score = 0;
        _perfectStreak = 0;
        GameOver = false;
        _top = new Footprint(0, 0, _startSize, _startSize);
        _axis = SlideAxis.Z;
        SpawnMoving();
    }

    private void SpawnMoving()
    {
        _axis = _axis == SlideAxis.X ? SlideAxis.Z : SlideAxis.X;
        _phase = 0;
        _moving = CreateSlab("Slab " + (Score + 1), Color.HSVToRGB(Hue + _hueStep, 0.55f, 0.95f));
    }

    private void Drop(Footprint footprint)
    {
        var result = StackRules.Drop(_top, footprint, _axis, _tolerance);
        var y = (Score + 0.5f) * _slabHeight;
        if (result.Missed)
        {
            // The whole slab tumbles off: the run is over.
            Fall(_moving, footprint, y);
            GameOver = true;
            _flash = 0;
            Engine.Log($"Skyline: game over, score {Score}");
            return;
        }

        _perfectStreak = result.Perfect ? _perfectStreak + 1 : 0;
        var placed = result.Perfect ? StackRules.Reward(result.Placed, _axis, _perfectStreak, _startSize) : result.Placed;
        Place(_moving, placed, y);
        _slabs.Add(_moving);
        if (result.CutOff is { } cut)
        {
            var piece = CreateSlab("Debris", _moving.GetComponent<MeshRenderer>().color);
            Fall(piece, cut, y);
        }

        if (result.Perfect)
        {
            _flash = 0;
        }

        _top = placed;
        Score++;
        if (_autoplay || Score % 10 == 0)
        {
            Engine.Log($"Skyline: score {Score}{(result.Perfect ? " (perfect)" : "")}");
        }

        SpawnMoving();
    }

    private void Fall(GameObject piece, Footprint footprint, float y)
    {
        Place(piece, footprint, y);
        var away = _axis == SlideAxis.X ? new Vector3(MathF.Sign(footprint.X - _top.X), 0, 0) : new Vector3(0, 0, MathF.Sign(footprint.Z - _top.Z));
        _debris.Add(new Debris(piece, (away * 1.2f) + new Vector3(0, 1.5f, 0), new Vector3(away.z * 160, 0, -away.x * 160)));
    }

    private void UpdateDebris(float dt)
    {
        for (var i = _debris.Count - 1; i >= 0; i--)
        {
            var d = _debris[i];
            d.Age += dt;
            d.Velocity += new Vector3(0, -12f * dt, 0);
            var t = d.GameObject.transform;
            t.Translate(d.Velocity * dt);
            t.Rotate(d.Spin * dt);
            ref var color = ref d.GameObject.GetComponent<MeshRenderer>().color;
            color.a = MathF.Max(0, 1 - (d.Age / 1.6f));
            if (d.Age > 1.6f)
            {
                Destroy(d.GameObject);
                _debris.RemoveAt(i);
            }
        }
    }

    private static GameObject CreateSlab(string name, Color color)
    {
        var slab = GameObject.Create(name);
        slab.AddComponent<MeshFilter>();
        slab.AddComponent<MeshRenderer>().color = color;
        return slab;
    }

    private void Place(GameObject slab, Footprint footprint, float centerY)
    {
        var t = slab.transform;
        t.localPosition = new Vector3(footprint.X, centerY, footprint.Z);
        t.localScale = new Vector3(footprint.Width, _slabHeight, footprint.Depth);
    }
}
