#pragma once

enum class MutexError {
	SUCCESS,
	ALREADY_LOCKED,
};

struct Mutex {
	[[nodiscard]] inline auto try_lock() -> MutexError {
		if (!locked) {
			locked = true;
			return MutexError::SUCCESS;
		}
		return MutexError::ALREADY_LOCKED;
	}

	inline auto unlock() -> void { locked = false; }

	inline auto current() const -> bool { return locked; }

   private:
	bool locked = false;
};
