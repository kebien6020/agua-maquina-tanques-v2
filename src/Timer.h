#pragma once
#include "Time.h"

namespace kev {

struct Timer {
	Timer(Duration setting) : setting{setting} {}

	auto reset(Timestamp now) -> void { last = now; }

	auto isDone(Timestamp now) -> bool { return (now - last) > setting; }

	auto elapsedSec(Timestamp now) -> long {
		return (now - last).unsafeGetValue() / 1000;
	}
	auto totalSec() -> long { return setting.unsafeGetValue() / 1000; }

   private:
	Timestamp last = {};
	const Duration setting;
};

}  // namespace kev
