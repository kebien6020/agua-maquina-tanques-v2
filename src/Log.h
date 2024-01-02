#pragma once
#include "HardwareSerial.h"

#define INLINE __attribute__((always_inline)) inline

template <class... Args>
INLINE auto print(Args... args) -> void {
	(Serial.print(args), ...);
}

template <class... Args>
INLINE auto println(Args... args) -> void {
	print(args..., '\n');
}

template <bool enabled = true>
struct Log {
	Log(char const* name) : name{name} {}

	template <class... Args>
	INLINE auto operator()(Args... args) -> void {
		if constexpr (enabled) {
			println('[', name, ']', ' ', args...);
		}
	}

	template <class... Args>
	INLINE auto partial_start() -> void {
		if constexpr (enabled) {
			print('[', name, ']', ' ');
		}
	}

	template <class... Args>
	INLINE auto partial_end() -> void {
		if constexpr (enabled) {
			println();
		}
	}

	template <class... Args>
	INLINE auto partial(Args... args) -> void {
		if constexpr (enabled) {
			print(args...);
		}
	}

   private:
	char const* name;
};
