#pragma once

namespace kev {

template <class Reader>
class Edge {
   public:
	Edge(Reader& reader, bool prev = false, bool curr = false)
		: reader{reader}, prev{prev}, curr{curr} {}

	auto update() -> void {
		prev = curr;
		curr = reader.read();
	}

	auto risingEdge() -> bool {
		auto const retval = changed() && curr;
		return retval;
	}

	auto fallingEdge() -> bool {
		auto const retval = changed() && !curr;
		return retval;
	}

	auto changed() -> bool { return prev != curr; }

   private:
	bool prev;
	bool curr;
	Reader& reader;
};

}  // namespace kev
