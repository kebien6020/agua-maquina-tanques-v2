#pragma once

#include <avr/eeprom.h>

template <int address>
struct PersistByte {
	auto save(uint8_t b) -> void {
		eeprom_write_byte(reinterpret_cast<uint8_t*>(address), b);
	}

	auto read() -> uint8_t {
		return eeprom_read_byte(reinterpret_cast<uint8_t*>(address));
	}
};
