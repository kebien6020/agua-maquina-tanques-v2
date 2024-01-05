#pragma once
#include "Edge.h"
#include "Log.h"
#include "Mutex.h"
#include "Timer.h"

using namespace kev::literals;
using kev::Edge;
using kev::Timer;
using kev::Timestamp;

constexpr auto TIME_PRE_FILL = 3_s;
constexpr auto TIME_FILL_FAILSAFE = 27_min;
constexpr auto TIME_CHEM1 = 40_min;
constexpr auto TIME_CHEM2 = 5_min;

enum struct TankState {
	INITIAL,
	PRE_FILL,
	FILLING,
	WAITING_CHEM_1,
	CHEM_1,
	WAITING_CHEM_2,
	CHEM_2,
	WAITING_IN_PROCESS,
	IN_PROCESS,

	LAST,
};

template <class OutFillPump,
		  class OutRecirPump,
		  class OutIngressValve,
		  class OutProcessValve,
		  class InSensorHi,
		  class StateSaver>
struct TankSM {
	TankSM(char const* name,
		   OutFillPump& out_fill_pump,
		   OutRecirPump& out_recir_pump,
		   OutIngressValve& out_ingress_valve,
		   OutProcessValve& out_process_valve,
		   InSensorHi& in_sensor_hi,
		   StateSaver& state_saver,
		   Mutex& in_process_mutex)
		: log{name},
		  out_fill_pump{out_fill_pump},
		  out_recir_pump{out_recir_pump},
		  out_ingress_valve{out_ingress_valve},
		  out_process_valve{out_process_valve},
		  in_sensor_hi{in_sensor_hi},
		  state_saver{state_saver} {}

	auto event_next() -> void {
		switch (state) {
		case TankState::INITIAL: set_state(TankState::PRE_FILL); break;
		case TankState::WAITING_CHEM_1: set_state(TankState::CHEM_1); break;
		case TankState::WAITING_CHEM_2: set_state(TankState::CHEM_2); break;
		case TankState::WAITING_IN_PROCESS:
			set_state(TankState::INITIAL);
			break;
		default: log("Ignoring event_next in state ", state_text()); break;
		}
	}

	auto event_cancel() -> void {
		switch (state) {
		case TankState::PRE_FILL:
		case TankState::FILLING: set_state(TankState::INITIAL); break;
		case TankState::CHEM_1: set_state(TankState::WAITING_CHEM_1); break;
		case TankState::CHEM_2: set_state(TankState::WAITING_CHEM_2); break;
		default: log("Ignoring event_cancel in state ", state_text()); break;
		}
	}

	auto event_force_next_stage() -> void {
		switch (state) {
		case TankState::INITIAL: set_state(TankState::WAITING_CHEM_1); break;
		case TankState::WAITING_CHEM_1:
			set_state(TankState::WAITING_CHEM_2);
			break;
		case TankState::WAITING_CHEM_2:
			set_state(TankState::WAITING_IN_PROCESS);
			break;
		default:
			log("Ignoring event_force_next_stage in state ", state_text());
			break;
		}
	}

	auto event_force_prev_stage() -> void {
		switch (state) {
		case TankState::INITIAL:
			set_state(TankState::WAITING_IN_PROCESS);
			break;
		case TankState::WAITING_CHEM_1: set_state(TankState::INITIAL); break;
		case TankState::WAITING_CHEM_2:
			set_state(TankState::WAITING_CHEM_1);
			break;
		case TankState::WAITING_IN_PROCESS:
			set_state(TankState::WAITING_CHEM_2);
			break;
		default:
			log("Ignoring event_force_next_stage in state ", state_text());
			break;
		}
	}

	auto event_fill_finish() -> void {
		if (state != TankState::FILLING) {
			log("Ignoring fill finish event on state other than FILLING");
			return;
		}

		set_state(TankState::WAITING_CHEM_1);
	}

	auto tick(Timestamp now) -> void {
		in_sensor_hi_edge.update();

		if (changed()) {
			handle_state_changed(now);
		}
		state_transitions(now);
	}

	auto log_debug(Timestamp now) {
		log.partial_start();
		log.partial("State = ", state_text());
		log.partial(", In: ", in_sensor_hi_edge.value() ? "HI " : "   ");
		log.partial(", Out: ", out_fill_pump ? "FIL " : "    ",
					out_ingress_valve ? "VAL " : "    ",
					out_recir_pump ? "RCR " : "    ");
		if (state == TankState::PRE_FILL) {
			log.partial(", Pre-fill timer = ", pre_fill_timer.elapsedSec(now),
						"/", pre_fill_timer.totalSec());
		}
		if (state == TankState::FILLING) {
			log.partial(", Fill failsafe = ", fill_timer.elapsedSec(now), "/",
						fill_timer.totalSec());
		}
		if (state == TankState::CHEM_1) {
			log.partial(", Chem1 timer = ", chem1_timer.elapsedSec(now), "/",
						chem1_timer.totalSec());
		}
		if (state == TankState::CHEM_2) {
			log.partial(", Chem2 timer = ", chem2_timer.elapsedSec(now), "/",
						chem2_timer.totalSec());
		}
		log.partial_end();
	}

	auto display_fill_timer(Timestamp now) -> String {
		if (state != TankState::FILLING)
			return "N/A";
		return format_timer(fill_timer, now);
	}

	auto display_chem1_timer(Timestamp now) -> String {
		if (state != TankState::CHEM_1)
			return "N/A";
		return format_timer(chem1_timer, now);
	}

	auto display_chem2_timer(Timestamp now) -> String {
		if (state != TankState::CHEM_2)
			return "N/A";
		return format_timer(chem2_timer, now);
	}

	auto restore_state() -> void {
		auto const saved = state_saver.read();
		log("Restoring state, raw value = ", saved);

		auto parsed = static_cast<TankState>(saved);
		if (parsed >= TankState::LAST) {
			parsed = TankState::INITIAL;
		}
		set_state(parsed);
	}
	auto get_state() -> TankState { return state; }

   private:
	auto format_timer(Timer& t, Timestamp now) -> String {
		return format_seconds(t.elapsedSec(now)) + "/" +
			   format_seconds(t.totalSec());
	}

	auto format_seconds(long sec) -> String {
		auto min = sec / 60;
		auto s = sec % 60;
		return pad2(min) + ":" + pad2(s);
	}

	auto pad2(long n) -> String {
		if (n >= 10 || n < 0)
			return String{n};
		return "0" + String{n};
	}

	auto handle_state_changed(Timestamp now) -> void {
		switch (state) {
		case TankState::INITIAL: {
			stop_all();
		}; break;
		case TankState::PRE_FILL: {
			out_ingress_valve = true;
			pre_fill_timer.reset(now);
		}; break;
		case TankState::FILLING: {
			out_fill_pump = true;
			fill_timer.reset(now);
		}; break;
		case TankState::CHEM_1: {
			out_ingress_valve = false;
			out_fill_pump = false;
			out_recir_pump = true;
			chem1_timer.reset(now);
		}; break;
		case TankState::CHEM_2: {
			out_ingress_valve = false;
			out_fill_pump = false;
			out_recir_pump = true;
			chem2_timer.reset(now);
		}; break;
		case TankState::WAITING_CHEM_1:
		case TankState::WAITING_CHEM_2:
		case TankState::WAITING_IN_PROCESS: stop_all(); break;
		case TankState::LAST: log("Error, state LAST should not be set");
		}

		// On next tick do not handle as a change
		prev_state = state;
	}
	auto state_transitions(Timestamp now) -> void {
		switch (state) {
		case TankState::PRE_FILL: {
			if (pre_fill_timer.isDone(now)) {
				set_state(TankState::FILLING);
			}
		}; break;
		case TankState::FILLING: {
			if (fill_timer.isDone(now)) {
				log("Alerta: Finalizando llenado por tiempo de seguridad");
				set_state(TankState::WAITING_CHEM_1);
			}
			if (in_sensor_hi_edge.risingEdge()) {
				set_state(TankState::WAITING_CHEM_1);
			}
		}; break;
		case TankState::CHEM_1: {
			if (chem1_timer.isDone(now)) {
				set_state(TankState::WAITING_CHEM_2);
			}
		}; break;
		case TankState::CHEM_2: {
			if (chem2_timer.isDone(now)) {
				set_state(TankState::WAITING_CHEM_2);
			}
		}; break;
		case TankState::INITIAL:
		case TankState::WAITING_CHEM_1:
		case TankState::WAITING_CHEM_2:
		case TankState::WAITING_IN_PROCESS: break;
		case TankState::LAST: log("Error, state LAST should not be set");
		}
	}

	auto changed() -> bool { return state != prev_state; }

	auto stop_all() -> void {
		out_fill_pump = false;
		out_recir_pump = false;
		out_ingress_valve = false;
	}

	auto set_state(TankState s) -> void {
		prev_state = state;
		state = s;

		if (state_should_be_persisted()) {
			persist_state();
		}
	}

	auto state_should_be_persisted() -> bool {
		return state == TankState::INITIAL ||
			   state == TankState::WAITING_CHEM_1 ||
			   state == TankState::WAITING_CHEM_2 ||
			   state == TankState::WAITING_IN_PROCESS;
	}

	auto persist_state() -> void {
		state_saver.save(static_cast<uint8_t>(state));
	}

	auto state_text() -> char const* {
		switch (state) {
		case TankState::INITIAL: return "INITIAL";
		case TankState::PRE_FILL: return "PRE_FILL";
		case TankState::FILLING: return "FILLING";
		case TankState::WAITING_CHEM_1: return "WAITING_CHEM_1";
		case TankState::CHEM_1: return "CHEM_1";
		case TankState::WAITING_CHEM_2: return "WAITING_CHEM_2";
		case TankState::CHEM_2: return "CHEM_2";
		case TankState::WAITING_IN_PROCESS: return "WAITING_IN_USE";
		case TankState::LAST: return "LAST (ERROR)";
		}
		return "INVALID";
	}

	Log<> log;

	OutFillPump& out_fill_pump;
	OutRecirPump& out_recir_pump;
	OutIngressValve& out_ingress_valve;
	OutProcessValve& out_process_valve;
	InSensorHi& in_sensor_hi;
	StateSaver& state_saver;
	Mutex& in_process_mutex;

	Edge<InSensorHi> in_sensor_hi_edge{in_sensor_hi};

	TankState state = {};
	TankState prev_state = {};

	Timer pre_fill_timer{TIME_PRE_FILL};
	Timer fill_timer{TIME_FILL_FAILSAFE};
	Timer chem1_timer{TIME_CHEM1};
	Timer chem2_timer{TIME_CHEM2};
};
