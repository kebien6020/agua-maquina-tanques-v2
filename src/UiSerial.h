#pragma once

#include "HardwareSerial.h"
#include "Log.h"
#include "TankSM.h"

template <typename TankASM>
struct UiSerial {
	UiSerial(TankASM& tank_a_sm) : tank_a_sm{tank_a_sm} {}

	auto tick() {
		if (Serial.available()) {
			auto cmd = Serial.readStringUntil('\n');
			log("cmd = ", cmd);

			process(cmd);
		}
	}

	auto process(String const& cmd) -> void {
		if (cmd == "fill a") {
			tank_a_sm.event_fill();
		}
		if (cmd == "fill finish a") {
			tank_a_sm.event_fill_finish();
		}
	}

   private:
	Log<> log = {"serial"};
	TankASM& tank_a_sm;
};
