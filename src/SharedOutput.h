#pragma once

template <class WrappedT>
struct SharedOutput {
	SharedOutput(WrappedT& output) : output{output} {}

	auto set_a(bool out) {
		out_a = out;
		update();
	}

	auto set_b(bool out) {
		out_b = out;
		update();
	}

	[[nodiscard]] auto read_a() const -> bool { return out_a; }
	[[nodiscard]] auto read_b() const -> bool { return out_b; }

   private:
	auto update() -> void { output = out_a || out_b; }
	bool out_a = false;
	bool out_b = false;
	WrappedT& output;
};

template <class SharedT>
struct SharedOutputA {
	SharedOutputA(SharedT& shared) : shared{shared} {}

	auto operator=(bool value) -> SharedOutputA& {
		shared.set_a(value);
		return *this;
	}

	[[nodiscard]] auto read() const -> bool { return shared.read_a(); }
	explicit operator bool() const { return shared.read_a(); }

   private:
	SharedT& shared;
};

template <class SharedT>
struct SharedOutputB {
	SharedOutputB(SharedT& shared) : shared{shared} {}

	auto operator=(bool value) -> SharedOutputB& {
		shared.set_b(value);
		return *this;
	}

	[[nodiscard]] auto read() const -> bool { return shared.read_b(); }
	explicit operator bool() const { return shared.read_b(); }

   private:
	SharedT& shared;
};
