constexpr auto version = "Version 0.1 (27/Dic/2023)";

#include <Arduino.h>
#include "DirectIO.h"
#include "HardwareSerial.h"
#include "TankSM.h"
#include "Timer.h"
#include "UiSerial.h"

using namespace kev::literals;

auto led_output = Output<LED_BUILTIN>{};
auto out_fill_pump = Output<3>{};
auto out_recir_pump = Output<4>{};
auto out_ingress_valve = Output<5>{};

auto led_timer = kev::Timer{1_s};

auto tank_a_sm = TankSM<decltype(out_fill_pump),
						decltype(out_recir_pump),
						decltype(out_ingress_valve)>{
	"tank_a", out_fill_pump, out_recir_pump, out_ingress_valve};

auto ui_serial = UiSerial<decltype(tank_a_sm)>{tank_a_sm};

auto log_ = Log<>{"main"};
auto log_timer = kev::Timer{1_s};

auto main() -> int {
	init();

	Serial.begin(115200);
	log_(version);
	log_("Setup done");

	for (;;) {
		auto now = kev::Timestamp{millis()};
		if (led_timer.isDone(now)) {
			led_timer.reset(now);
			digitalWrite(13, !digitalRead(13));
			// led_output.write(HIGH);
		}

		tank_a_sm.tick(now);
		if (log_timer.isDone(now)) {
			log_timer.reset(now);
			tank_a_sm.log_debug(now);
		}

		ui_serial.tick();

		serialEventRun();
	}
}
