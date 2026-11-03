// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// On the web Godot's platform/web provides HTTPClientWeb (Fetch API) and sets
// HTTPClient::_create; core/io/http_client_tcp.cpp is compiled out under WEB_ENABLED.
// Phoenix has no web HTTP client yet, so HTTPClient::create() returns null there.
#ifdef WEB_ENABLED
#include "core/io/http_client.h"

HTTPClient *(*HTTPClient::_create)(bool p_notify_postinitialize) = nullptr;
#endif
