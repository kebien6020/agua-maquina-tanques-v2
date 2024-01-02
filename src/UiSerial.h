#pragma once

#include "HardwareSerial.h"
#include "Log.h"
#include "TankSM.h"

template <class TankASM, class TankBSM, class AqueductSM>
struct UiSerial {
	UiSerial(TankASM& tank_a_sm, TankBSM& tank_b_sm, AqueductSM& aqueduct_sm)
		: tank_a_sm{tank_a_sm},
		  tank_b_sm{tank_b_sm},
		  aqueduct_sm{aqueduct_sm} {}

	auto tick() {
		if (Serial.available()) {
			auto cmd = Serial.readStringUntil('\n');
			log("cmd = ", cmd);

			process(cmd);
		}
	}

	auto process(String const& cmd) -> void {
		// clang-format off
		if (cmd == "next a") tank_a_sm.event_next();
		if (cmd == "next b") tank_b_sm.event_next();
		if (cmd == "cancel a") tank_a_sm.event_cancel();
		if (cmd == "cancel b") tank_b_sm.event_cancel();
		if (cmd == "fill finish a") tank_a_sm.event_fill_finish();
		if (cmd == "fill finish b") tank_b_sm.event_fill_finish();
		if (cmd == "fnext a") tank_a_sm.event_force_next_stage();
		if (cmd == "fnext b") tank_b_sm.event_force_next_stage();
		if (cmd == "fprev a") tank_a_sm.event_force_prev_stage();
		if (cmd == "fprev b") tank_b_sm.event_force_prev_stage();
		if (cmd == "aq valve on") aqueduct_sm.event_valve_on();
		if (cmd == "aq valve off") aqueduct_sm.event_valve_off();
		if (cmd == "aq pump on") aqueduct_sm.event_pump_on();
		if (cmd == "aq pump off") aqueduct_sm.event_pump_off();
		if (cmd == "aq sensor hi") aqueduct_sm.event_sensor_hi();
		// clang-format on
	}

   private:
	Log<> log = {"serial"};
	TankASM& tank_a_sm;
	TankBSM& tank_b_sm;
	AqueductSM& aqueduct_sm;
};
