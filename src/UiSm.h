#pragma once

#include "Arduino.h"
#include "HardwareSerial.h"
#include "Log.h"
#include "NexHardware.h"
#include "NextionUtils.h"

enum struct UiState {
	// Underlying numbers double as page ids
	HOME = 0,
	WAITING_TANK_A = 1,
	WAITING_TANK_B = 2,
	STATUS = 3,
	ADVANCED = 4,

	LAST,
};

enum struct UiTank {
	A,
	B,
};

template <class SerialT = HardwareSerial>
struct UiSm {
	UiSm(SerialT& serial) : serial{serial} {}
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
	auto tick() -> void {
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
			log("Received press event, page = ", static_cast<uint8_t>(raw[1]),
				", id = ", static_cast<uint8_t>(raw[2]));
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

	UiState state = UiState::HOME;
	HardwareSerial& serial;
	Log<> log{"ui"};

	UiTank tank = UiTank::A;
};
