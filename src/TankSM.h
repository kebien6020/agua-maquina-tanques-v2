#pragma once
#include "Log.h"
#include "Timer.h"

using namespace kev::literals;
using kev::Timestamp;

constexpr auto TIME_PRE_FILL = 3_s;
constexpr auto TIME_FILL_FAILSAFE = 27_min;
constexpr auto TIME_CHEM1 = 5_min;
constexpr auto TIME_CHEM2 = 40_min;

enum struct TankState {
	INITIAL,
	PRE_FILL,
	FILLING,
	WAITING_CHEM_1,
	CHEM_1,
	WAITING_CHEM_2,
	CHEM_2,
	WAITING_IN_USE,

	LAST,
};

template <class OutFillPump,
		  class OutRecirPump,
		  class OutIngressValve,
		  class StateSaver>
struct TankSM {
	TankSM(char const* name,
		   OutFillPump& out_fill_pump,
		   OutRecirPump& out_recir_pump,
		   OutIngressValve& out_ingress_valve,
		   StateSaver& state_saver)
		: log{name},
		  out_fill_pump{out_fill_pump},
		  out_recir_pump{out_recir_pump},
		  out_ingress_valve{out_ingress_valve},
		  state_saver{state_saver} {}

	auto event_next() -> void {
		switch (state) {
		case TankState::INITIAL: set_state(TankState::PRE_FILL); break;
		case TankState::WAITING_CHEM_1: set_state(TankState::CHEM_1); break;
		case TankState::WAITING_CHEM_2: set_state(TankState::CHEM_2); break;
		case TankState::WAITING_IN_USE: set_state(TankState::INITIAL); break;
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
			set_state(TankState::WAITING_IN_USE);
			break;
		default:
			log("Ignoring event_force_next_stage in state ", state_text());
			break;
		}
	}

	auto event_force_prev_stage() -> void {
		switch (state) {
		case TankState::INITIAL: set_state(TankState::WAITING_IN_USE); break;
		case TankState::WAITING_CHEM_1: set_state(TankState::INITIAL); break;
		case TankState::WAITING_CHEM_2:
			set_state(TankState::WAITING_CHEM_1);
			break;
		case TankState::WAITING_IN_USE:
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
		if (changed()) {
			handle_state_changed(now);
		}
		state_transitions(now);
	}

	auto log_debug(Timestamp now) {
		log.partial_start();
		log.partial("State = ", state_text());
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

	auto restore_state() -> void {
		auto const saved = state_saver.read();
		log("Restoring state, raw value = ", saved);

		auto parsed = static_cast<TankState>(saved);
		if (parsed >= TankState::LAST) {
			parsed = TankState::INITIAL;
		}
		set_state(parsed);
	}

   private:
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
			   state == TankState::WAITING_IN_USE;
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
		case TankState::WAITING_IN_USE: return "WAITING_IN_USE";
		case TankState::LAST: return "LAST (ERROR)";
		}
		return "INVALID";
	}

	Log<> log;

	OutFillPump& out_fill_pump;
	OutRecirPump& out_recir_pump;
	OutIngressValve& out_ingress_valve;
	StateSaver& state_saver;

	TankState state = {};
	TankState prev_state = {};

	kev::Timer pre_fill_timer{TIME_PRE_FILL};
	kev::Timer fill_timer{TIME_FILL_FAILSAFE};
	kev::Timer chem1_timer{TIME_CHEM1};
	kev::Timer chem2_timer{TIME_CHEM2};
};
