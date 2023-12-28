#pragma once
#include "HardwareSerial.h"

template <class... Args>
auto print(Args... args) -> void {
	(Serial.print(args), ...);
}

template <class... Args>
auto println(Args... args) -> void {
	print(args..., '\n');
}

template <bool enabled = true>
struct Log {
	Log(char const* name) : name{name} {}

	template <class... Args>
	auto operator()(Args... args) -> void {
		if constexpr (enabled) {
			println('[', name, ']', ' ', args...);
		}
	}

	template <class... Args>
	auto partial_start() -> void {
		if constexpr (enabled) {
			print('[', name, ']', ' ');
		}
	}

	template <class... Args>
	auto partial_end() -> void {
		if constexpr (enabled) {
			println();
		}
	}

	template <class... Args>
	auto partial(Args... args) -> void {
		if constexpr (enabled) {
			print(args...);
		}
	}

   private:
	char const* name;
};
