// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Minimal IP singleton until Phoenix ships its network driver (known gap in the README):
// hostname resolution and interface listing return nothing instead of the foundation logging
// an error at start-up because no platform registered IP::_create.
#include "core/io/ip.h"

namespace {

class IP_Phoenix : public IP {
	GDSOFTCLASS(IP_Phoenix, IP);

public:
	void _resolve_hostname(List<IPAddress> &r_addresses, const String &p_hostname, Type p_type) const override {}
	void get_local_interfaces(HashMap<String, Interface_Info> *r_interfaces) const override {}
	static IP *create_phoenix() { return memnew(IP_Phoenix); }
	static void install() { _create = &create_phoenix; }  // _create is protected in IP
};

} // namespace

// Called by CoreRuntime before register_core_types() (an explicit call, because static
// registrars in static libraries are dropped by the linker when nothing references them).
void phoenix_foundation_install_ip() {
	IP_Phoenix::install();
}
