// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
using System.Runtime.CompilerServices;

// Zero-overhead interop, rule 1: no runtime marshalling. Every type crossing the
// engine boundary must be blittable; anything else is a compile-time error.
[assembly: DisableRuntimeMarshalling]
