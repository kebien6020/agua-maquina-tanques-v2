constexpr auto version = "Version 1.5 (04/Ene/2026)";

#include <Arduino.h>
#include "AqueductSM.h"
#include "DirectIO.h"
#include "HardwareSerial.h"
#include "Mutex.h"
#include "Persist.h"
#include "SharedOutput.h"
#include "TankSM.h"
#include "Timer.h"
#include "UiSerial.h"
#include "UiSm.h"

using namespace kev::literals;
using kev::Timestamp;

auto led_output = Output<LED_BUILTIN>{};
auto out_fill_pump = OutputLow<35>{};
auto out_fill_pump_shared =
	SharedOutput<decltype(out_fill_pump)>{out_fill_pump};
auto out_fill_pump_shared_a =
	SharedOutputA<decltype(out_fill_pump_shared)>{out_fill_pump_shared};
auto out_fill_pump_shared_b =
	SharedOutputB<decltype(out_fill_pump_shared)>{out_fill_pump_shared};

auto in_process_mutex = Mutex{};

auto out_recir_pump_a = OutputLow<29>{};
auto out_ingress_valve_a = OutputLow<23>{};
auto out_process_valve_a = OutputLow<9>{};
auto in_sensor_hi_a = InputLow<22>{};

auto out_recir_pump_b = OutputLow<31>{};
auto out_ingress_valve_b = OutputLow<25>{};
auto out_process_valve_b = OutputLow<10>{};
auto in_sensor_hi_b = InputLow<24>{};

auto out_aq_ingress_valve = OutputLow<27>{};
auto out_aq_pump = OutputLow<33>{};
auto in_aq_sensor_hi = InputLow<26>{};
auto in_aq_sensor_lo = InputLow<32>{};

auto led_timer = kev::Timer{1_s};

auto persist_state_tank_a = PersistByte<0>{};
auto persist_state_tank_b = PersistByte<1>{};

auto tank_a_sm = TankSM<decltype(out_fill_pump_shared_a),
						decltype(out_recir_pump_a),
						decltype(out_ingress_valve_a),
						decltype(out_process_valve_a),
						decltype(in_sensor_hi_a),
						decltype(in_aq_sensor_lo),
						decltype(persist_state_tank_a)>{
	"tank_a",
	out_fill_pump_shared_a,
	out_recir_pump_a,
	out_ingress_valve_a,
	out_process_valve_a,
	in_sensor_hi_a,
	in_aq_sensor_lo,
	persist_state_tank_a,
	in_process_mutex,
};

auto tank_b_sm = TankSM<decltype(out_fill_pump_shared_b),
						decltype(out_recir_pump_b),
						decltype(out_ingress_valve_b),
						decltype(out_process_valve_b),
						decltype(in_sensor_hi_b),
						decltype(in_aq_sensor_lo),
						decltype(persist_state_tank_b)>{
	"tank_b",
	out_fill_pump_shared_b,
	out_recir_pump_b,
	out_ingress_valve_b,
	out_process_valve_b,
	in_sensor_hi_b,
	in_aq_sensor_lo,
	persist_state_tank_b,
	in_process_mutex,
};

auto aqueduct_sm = AqueductSM<decltype(out_aq_ingress_valve),
							  decltype(out_aq_pump),
							  decltype(in_aq_sensor_hi)>{
	out_aq_ingress_valve, out_aq_pump, in_aq_sensor_hi};

auto ui = UiSm<decltype(tank_a_sm), decltype(tank_b_sm), decltype(aqueduct_sm)>{
	Serial3, tank_a_sm, tank_b_sm, aqueduct_sm};

auto ui_serial =
	UiSerial<decltype(tank_a_sm), decltype(tank_b_sm), decltype(aqueduct_sm)>{
		tank_a_sm, tank_b_sm, aqueduct_sm};

auto log_ = Log<>{"main"};
auto log_timer = kev::Timer{2_s};

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
		aqueduct_sm.tick(now);
		ui.tick(now);
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
		aqueduct_sm.log_debug();
		ui.log_debug();
		println();
	}
}
