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
};

template <class OutFillPump, class OutRecirPump, class OutIngressValve>
struct TankSM {
	TankSM(char const* name,
		   OutFillPump& out_fill_pump,
		   OutRecirPump& out_recir_pump,
		   OutIngressValve& out_ingress_valve)
		: log{name},
		  out_fill_pump{out_fill_pump},
		  out_recir_pump{out_recir_pump},
		  out_ingress_valve{out_ingress_valve} {}

	auto event_fill() -> void {
		if (state != TankState::INITIAL) {
			log("Ignoring fill event on state other than INITIAL");
			return;
		}

		set_state(TankState::PRE_FILL);
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

	// For use with saving state
	auto restore_state(TankState state, Timestamp now) -> void {
		this->state = state;
		handle_state_changed(now);
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
		log.partial_end();
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
		}

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
		}
	}

	OutFillPump& out_fill_pump;
	OutRecirPump& out_recir_pump;
	OutIngressValve& out_ingress_valve;

	TankState state = {};
	TankState prev_state = {};

	kev::Timer pre_fill_timer{TIME_PRE_FILL};
	kev::Timer fill_timer{TIME_FILL_FAILSAFE};

	Log<> log;
};
