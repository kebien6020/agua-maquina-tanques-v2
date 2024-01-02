#pragma once

#include "Edge.h"
#include "Log.h"

using kev::Edge;

enum class AqState {
	STOPPED,
	FILLING,
	FILLING_PUMP,
};

template <class OutIngressValve, class OutPump, class InLevelHi>
struct AqueductSM {
	AqueductSM(OutIngressValve& out_ingress_valve,
			   OutPump& out_pump,
			   InLevelHi& in_level_hi)
		: out_ingress_valve{out_ingress_valve},
		  out_pump{out_pump},
		  in_level_hi_edge{in_level_hi} {}

	auto tick() -> void {
		in_level_hi_edge.update();

		if (in_level_hi_edge.risingEdge()) {
			event_sensor_hi();
		}
	}

	auto event_pump_on() -> void {
		switch (state) {
		case AqState::STOPPED:
		case AqState::FILLING: return set_state(AqState::FILLING_PUMP);
		case AqState::FILLING_PUMP: return;
		}
		log("Invalid state on event_pump_on");
	}

	auto event_pump_off() -> void {
		switch (state) {
		case AqState::STOPPED:
		case AqState::FILLING: return;
		case AqState::FILLING_PUMP: return set_state(AqState::FILLING);
		}
		log("Invalid state on event_pump_off");
	}

	auto event_valve_on() -> void {
		switch (state) {
		case AqState::STOPPED: return set_state(AqState::FILLING);
		case AqState::FILLING:
		case AqState::FILLING_PUMP: return;
		}
		log("Invalid state on event_valve_on");
	}

	auto event_valve_off() -> void {
		switch (state) {
		case AqState::STOPPED: return;
		case AqState::FILLING:
		case AqState::FILLING_PUMP: return set_state(AqState::STOPPED);
		}
		log("Invalid state on event_valve_off");
	}

	auto event_sensor_hi() -> void {
		switch (state) {
		case AqState::STOPPED: return;
		case AqState::FILLING:
		case AqState::FILLING_PUMP: return set_state(AqState::STOPPED);
		}
		log("Invalid state on event_sensor_hi");
	}

	auto log_debug() -> void {
		log("State = ", state_text(), ", Sensor Hi = ", sensor_hi_text());
	}

   private:
	auto set_state(AqState s) {
		state = s;

		handle_state_change();
	}

	auto handle_state_change() -> void {
		switch (state) {
		case AqState::STOPPED: {
			out_ingress_valve = false;
			out_pump = false;
		};
			return;
		case AqState::FILLING: {
			out_ingress_valve = true;
			out_pump = false;
		};
			return;
		case AqState::FILLING_PUMP: {
			out_ingress_valve = true;
			out_pump = true;
		};
			return;
		}
		log("Invalid state on handle_state_change");
	}

	auto state_text() -> char const* {
		switch (state) {
		case AqState::STOPPED: return "STOPPED";
		case AqState::FILLING: return "FILLING";
		case AqState::FILLING_PUMP: return "FILLING_PUMP";
		}

		return "UNKNOWN (Error)";
	}

	auto sensor_hi_text() -> char const* {
		return in_level_hi_edge.value() ? "ON" : "OFF";
	}

	OutIngressValve& out_ingress_valve;
	OutPump& out_pump;
	Edge<InLevelHi> in_level_hi_edge;

	Log<> log = Log<>{"aqueduct"};

	AqState state = AqState::STOPPED;
};
