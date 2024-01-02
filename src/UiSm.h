#pragma once

#include "Arduino.h"
#include "HardwareSerial.h"
#include "Log.h"
#include "NexHardware.h"
#include "NextionUtils.h"
#include "TankSM.h"
#include "Timer.h"

using namespace kev::literals;
using kev::Timer;
using kev::Timestamp;

enum struct UiState {
	// Underlying numbers double as page ids
	HOME = 0,
	WAITING_TANK_A = 1,
	WAITING_TANK_B = 2,
	STATUS = 3,
	ADVANCED = 4,
	AQUEDUCT = 5,

	LAST,
};

enum struct UiTank {
	A,
	B,
};

template <class TankASM, class TankBSM, class SerialT = HardwareSerial>
struct UiSm {
	UiSm(SerialT& serial, TankASM& tank_a_sm, TankBSM& tank_b_sm)
		: serial{serial}, tank_a_sm{tank_a_sm}, tank_b_sm{tank_b_sm} {}
	auto init() -> void {
		serial.begin(9600);
		serial.print("baud=115200\xFF\xFF\xFF");
		recvRetCommandFinished(serial);
		serial.begin(115200);
		serial.print("bkcmd=1\xFF\xFF\xFF");
		recvRetCommandFinished(serial);
		serial.print("page 0\xFF\xFF\xFF");
		recvRetCommandFinished(serial);
		log("Initialized");
	}

	auto tick(Timestamp now) -> void {
		if (serial.available()) {
			process_ui_command();
		}

		switch (state) {
		case UiState::WAITING_TANK_A: {
			tank = UiTank::A;
			page_status();
			set_state(UiState::STATUS);
		}; break;
		case UiState::WAITING_TANK_B: {
			tank = UiTank::B;
			page_status();
			set_state(UiState::STATUS);
		}; break;
		case UiState::STATUS: {
			if (status_update_timer.isDone(now)) {
				status_update_timer.reset(now);
				update_status(now);
			}
		}; break;
		default: break;  // noop
		}
	}

	auto log_debug() -> void {
		log("UI State = ", state_text(), ", Current Tank = ", tank_text());
	}

   private:
	auto page_status() -> void {
		serial.print("page ");
		serial.print(static_cast<int>(UiState::STATUS));
		serial.print("\xFF\xFF\xFF");
		recvRetCommandFinished(serial);
	}

	auto update_status(Timestamp now) -> void {
		log("Updating status screen");
		set_text("t1", tank_display());
		set_text("t3", state_display());
		set_text("t4", additional_display(now));
		set_text("b0", confirm_display());
	}

	template <class StringT>
	auto set_text(char const* id, StringT text) -> void {
		serial.print(id);
		serial.print(".txt=\"");
		serial.print(text);
		serial.print("\"\xFF\xFF\xFF");
	}

	auto process_ui_command() -> void {
		auto const raw = receiveRaw(serial);
		if (!raw.endsWith("\xFF\xFF\xFF")) {
			log("Invalid command");
			log_raw(raw);
			return;
		}

		if (raw[0] == 0x66) {
			auto const page = static_cast<uint8_t>(raw[1]);
			log("Current page = ", page);
			event_page_change(page);
			return;
		}

		if (raw[0] == 0x65) {
			auto const page = static_cast<uint8_t>(raw[1]);
			auto const id = static_cast<uint8_t>(raw[2]);
			log("Received press event, page = ", page, ", id = ", id);
			handle_button_press(page, id);
			return;
		}

		log_raw(raw);
	}

	auto event_page_change(int page) -> void {
		if (page >= static_cast<int>(UiState::LAST)) {
			log("Error invalid page number");
			return;
		}

		set_state(static_cast<UiState>(page));
	}

	auto handle_button_press(int page, int id) -> void {
		if (page == 3 && id == 5)
			event_next();
		if (page == 3 && id == 6)
			event_cancel();
		if (page == 4 && id == 2)
			event_force_prev();
		if (page == 4 && id == 1)
			event_force_next();
	}

	auto event_next() -> void {
		switch (tank) {
		case UiTank::A: return tank_a_sm.event_next();
		case UiTank::B: return tank_b_sm.event_next();
		}
		log("Error during event_next");
	}

	auto event_cancel() -> void {
		switch (tank) {
		case UiTank::A: return tank_a_sm.event_cancel();
		case UiTank::B: return tank_b_sm.event_cancel();
		}
		log("Error during event_cancel");
	}

	auto event_force_prev() -> void {
		switch (tank) {
		case UiTank::A: return tank_a_sm.event_force_prev_stage();
		case UiTank::B: return tank_b_sm.event_force_prev_stage();
		}
		log("Error during event_force_prev");
	}

	auto event_force_next() -> void {
		switch (tank) {
		case UiTank::A: return tank_a_sm.event_force_next_stage();
		case UiTank::B: return tank_b_sm.event_force_next_stage();
		}
		log("Error during event_force_next");
	}

	auto set_state(UiState s) { state = s; }

	auto log_raw(String const& raw) {
		log.partial_start();
		log.partial("Raw UI command = ");
		for (byte c : raw) {
			Serial.print(c, HEX);
			log.partial(" ");
		}
		log.partial_end();
	}

	auto state_text() -> char const* {
		switch (state) {
		case UiState::HOME: return "HOME";
		case UiState::WAITING_TANK_A: return "WAITING_TANK_A";
		case UiState::WAITING_TANK_B: return "WAITING_TANK_B";
		case UiState::STATUS: return "STATUS";
		case UiState::ADVANCED: return "ADVANCED";
		case UiState::AQUEDUCT: return "AQUEDUCT";
		case UiState::LAST: return "LAST (error)";
		}
		return "UNKNOWN (error)";
	}

	auto tank_text() -> char const* {
		switch (tank) {
		case UiTank::A: return "A";
		case UiTank::B: return "B";
		}
		return "UNKNOWN (error)";
	}

	auto tank_display() -> char const* {
		switch (tank) {
		case UiTank::A: return "Tanque 3 (3500L)";
		case UiTank::B: return "Tanque 4 (2000L)";
		}
		return "UNKNOWN (error)";
	}

	auto state_display() -> char const* {
		switch (tank) {
		case UiTank::A: return state_display_impl(tank_a_sm);
		case UiTank::B: return state_display_impl(tank_b_sm);
		}
		log("Error during state_display");
		return "Error de programa, informar";
	}

	template <class TankSM>
	auto state_display_impl(TankSM& tank_sm) -> char const* {
		switch (tank_sm.get_state()) {
		case TankState::INITIAL: return "Espera inicial";
		case TankState::PRE_FILL: return "Pre-llenado";
		case TankState::FILLING: return "Llenando";
		case TankState::CHEM_1:
			return "Recirculando hidroxicloruro de aluminio";
		case TankState::CHEM_2: return "Recirculando hipoclorito de sodio";
		case TankState::WAITING_CHEM_1:
		case TankState::WAITING_CHEM_2:
		case TankState::WAITING_IN_USE: return "Esperando confirmacion";
		case TankState::LAST: return "Error de programa, informar";
		}
		return "Error de programa, informar";
	}

	auto additional_display(Timestamp now) -> String {
		switch (tank) {
		case UiTank::A: return additional_display_impl(tank_a_sm, now);
		case UiTank::B: return additional_display_impl(tank_b_sm, now);
		}
		log("Error during additional_display");
		return "Error de programa, informar";
	}

	template <class TankSM>
	auto additional_display_impl(TankSM& tank_sm, Timestamp now) -> String {
		switch (tank_sm.get_state()) {
		case TankState::INITIAL:
		case TankState::PRE_FILL: return "";
		case TankState::FILLING:
			return "Tiempo de seguridad = " + tank_sm.display_fill_timer(now);
		case TankState::CHEM_1:
			return "Tiempo = " + tank_sm.display_chem1_timer(now);
		case TankState::CHEM_2:
			return "Tiempo = " + tank_sm.display_chem2_timer(now);
		case TankState::WAITING_CHEM_1:
			return "Confirmar hidroxicloruro de aluminio";
		case TankState::WAITING_CHEM_2: return "Confirmar hipoclorito de sodio";
		case TankState::WAITING_IN_USE: return "Confirmar tanque vacio";
		case TankState::LAST: return "Error de programa, informar";
		}
		return "Error de programa, informar";
	}

	auto confirm_display() -> char const* {
		switch (tank) {
		case UiTank::A: return confirm_display_impl(tank_a_sm);
		case UiTank::B: return confirm_display_impl(tank_b_sm);
		}
		log("Error during confirm_display");
		return "Error de programa, informar";
	}

	template <class TankSM>
	auto confirm_display_impl(TankSM& tank_sm) -> char const* {
		switch (tank_sm.get_state()) {
		case TankState::INITIAL: return "Llenar";
		case TankState::PRE_FILL:
		case TankState::FILLING:
		case TankState::CHEM_1:
		case TankState::CHEM_2: return "";
		case TankState::WAITING_CHEM_1:
		case TankState::WAITING_CHEM_2:
		case TankState::WAITING_IN_USE: return "Confirmar";
		case TankState::LAST: return "Error de programa, informar";
		}
		return "Error de programa, informar";
	}

	UiState state = UiState::HOME;
	HardwareSerial& serial;
	TankASM& tank_a_sm;
	TankBSM& tank_b_sm;
	Log<> log{"ui"};

	UiTank tank = UiTank::A;
	Timer status_update_timer = {1_s};
};
