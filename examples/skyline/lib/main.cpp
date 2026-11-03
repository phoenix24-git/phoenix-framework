// Skyline: the whole game is data + C# scripts.
//
//   res/scenes/main.pscn   the scene: camera, light, tower base, and the "Game" object
//                          with the Skyline.StackGame C# script attached
//   scripts/               the C# scripts (NativeAOT shared library)
//
// The C++ side only starts the scene, like a project's main scene in Godot.
#include <phoenix/phoenix.h>

void phoenix_main() {
    phoenix::runScene("res://scenes/main.pscn");
}
