// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Runs Godot Engine's own core/ unit tests (vendored in thirdparty/godot/tests, MIT) against
// Phoenix's build of Godot core: our CMake build, code generation, SDL3 OS/FileAccess/DirAccess
// layer, and runtime bring-up. Any failure here means our integration differs from Godot.
//
// The listener below is the core-only part of Godot's GodotTestCaseListener (tests/test_main.cpp).
// Test cases tagged [SceneTree] or [Editor] need Godot's scene/editor layers and are excluded.
#include "tests/signal_watcher.h"
#include "tests/test_macros.h"

#include "core/math/math_funcs.h"
#include "core/object/message_queue.h"
#include "platform/core_runtime.h"

#include <cstring>
#include <vector>

namespace {

struct FoundationTestListener : public doctest::IReporter {
	explicit FoundationTestListener(const doctest::ContextOptions &) {}

	SignalWatcher *signal_watcher = nullptr;

	void report_query(const doctest::QueryData &) override {}
	void test_run_start() override { signal_watcher = memnew(SignalWatcher); }
	void test_run_end(const doctest::TestRunStats &) override { memdelete(signal_watcher); }
	void test_case_start(const doctest::TestCaseData &) override { reinitialize(); }
	void test_case_reenter(const doctest::TestCaseData &) override { reinitialize(); }
	void test_case_end(const doctest::CurrentTestCaseStats &) override {
		if (MessageQueue::get_singleton()) {
			MessageQueue::get_singleton()->flush();
		}
	}
	void test_case_exception(const doctest::TestCaseException &) override {}
	void subcase_start(const doctest::SubcaseSignature &) override { reinitialize(); }
	void subcase_end() override {}
	void log_assert(const doctest::AssertData &) override {}
	void log_message(const doctest::MessageData &) override {}
	void test_case_skipped(const doctest::TestCaseData &) override {}

private:
	static void reinitialize() {
		Math::seed(0x60d07);
		SignalWatcher::get_singleton()->_clear_signals();
	}
};

} // namespace

REGISTER_LISTENER("FoundationTestListener", 1, FoundationTestListener);

int main(int argc, char **argv) {
	phoenix::foundation::CoreOptions options;
	options.app_name = "phoenix_phoenix_foundation_tests";
	// Godot's TestUtils::get_data_path() resolves <executable dir>/../tests/data.
	options.executable_path = String::utf8(PHOENIX_FOUNDATION_DIR) + "/tests/phoenix_tests";  // must be an existing dir
	options.resource_dir = String::utf8(PHOENIX_FOUNDATION_DIR) + "/tests/data";
	options.args.push_back("--test");  // Godot's runner always passes it; [OS] tests expect it
	for (int i = 1; i < argc; i++) {
		options.args.push_back(String::utf8(argv[i]));
	}
	phoenix::foundation::CoreRuntime::initialize(options);

	doctest::Context context;
	// [SceneTree]/[Editor]: need Godot's scene/editor layers.
	// [PCKPacker]: packs Godot's logo artwork (misc/logo, CC-BY-4.0), deliberately not vendored.
	// [OS] Execute (web only): a browser/Wasm runtime cannot spawn processes.
#ifdef __EMSCRIPTEN__
	context.setOption("test-case-exclude", "*[SceneTree]*,*[Editor]*,*[PCKPacker]*,[OS] Execute");
#else
	context.setOption("test-case-exclude", "*[SceneTree]*,*[Editor]*,*[PCKPacker]*");
#endif
	context.applyCommandLine(argc, argv);  // command-line filters override the default
	const int result = context.run();

	phoenix::foundation::CoreRuntime::finalize();
	return result;
}
