#pragma once

#include <stdint.h>
#include "Arduino.h"

template <class SerialT>
auto receiveRaw(SerialT& serial, int timeout = 1000) -> String {
	auto const initial = static_cast<long long>(millis());
	auto s = String{};
	auto consecutiveFF = 0;

	while (millis() - initial < timeout) {
		while (serial.available()) {
			auto const b = serial.read();
			s.concat((char)b);
			if (uint8_t{b} == 0xFF) {
				consecutiveFF++;
			} else {
				consecutiveFF = 0;
			}
			if (consecutiveFF >= 3) {
				return s;
			}
		}
	}

	return s;
}
