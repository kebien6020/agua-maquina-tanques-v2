constexpr auto version = "Version 0.1 (30/Dic/2023)";

#include <Arduino.h>
#include "DirectIO.h"
#include "HardwareSerial.h"
#include "Persist.h"
#include "TankSM.h"
#include "Timer.h"
#include "UiSerial.h"
#include "UiSm.h"

using namespace kev::literals;
using kev::Timestamp;

auto led_output = Output<LED_BUILTIN>{};
auto out_fill_pump = Output<3>{};
auto out_recir_pump = Output<4>{};
auto out_ingress_valve = Output<5>{};

auto led_timer = kev::Timer{1_s};

auto persist_state_tank_a = PersistByte<0>{};
auto persist_state_tank_b = PersistByte<1>{};

auto tank_a_sm = TankSM<decltype(out_fill_pump),
						decltype(out_recir_pump),
						decltype(out_ingress_valve),
						decltype(persist_state_tank_a)>{
	"tank_a", out_fill_pump, out_recir_pump, out_ingress_valve,
	persist_state_tank_a};

auto tank_b_sm = TankSM<decltype(out_fill_pump),
						decltype(out_recir_pump),
						decltype(out_ingress_valve),
						decltype(persist_state_tank_b)>{
	"tank_b", out_fill_pump, out_recir_pump, out_ingress_valve,
	persist_state_tank_b};

auto ui = UiSm<>{Serial3};

auto ui_serial =
	UiSerial<decltype(tank_a_sm), decltype(tank_b_sm)>{tank_a_sm, tank_b_sm};

auto log_ = Log<>{"main"};
auto log_timer = kev::Timer{1_s};

auto led(Timestamp now) -> void;
auto serial_log(Timestamp now) -> void;

auto main() -> int {
	init();

	Serial.begin(115200);
	log_(version);
	tank_a_sm.restore_state();
	tank_b_sm.restore_state();
	ui.init();
	log_("Setup done");

	for (;;) {
		auto now = kev::Timestamp{millis()};

		led(now);

		tank_a_sm.tick(now);
		tank_b_sm.tick(now);
		ui.tick();
		ui_serial.tick();

		serial_log(now);

		serialEventRun();
	}
}

auto led(Timestamp now) -> void {
	if (led_timer.isDone(now)) {
		led_timer.reset(now);
		digitalWrite(13, !digitalRead(13));
	}
}

auto serial_log(Timestamp now) -> void {
	if (log_timer.isDone(now)) {
		log_timer.reset(now);
		tank_a_sm.log_debug(now);
		tank_b_sm.log_debug(now);
		ui.log_debug();
	}
}
