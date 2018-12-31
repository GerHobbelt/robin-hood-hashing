#include "test_base.h"

#include <bitset>
#include <iomanip>
#include <list>
#include <map>
#include <tuple>
#include <unordered_map>

// Counter for only swaps & equals. Used for optimizing.
// Can't use static counters here because I want to do it in parallel.
class Counter {
public:
	static size_t staticDefaultCtor;
	static size_t staticDtor;
	struct Counts {
		size_t ctor{};
		size_t defaultCtor{};
		size_t copyCtor{};
		size_t dtor{};
		size_t equals{};
		size_t less{};
		size_t assign{};
		size_t swaps{};
		size_t get{};
		size_t constGet{};
		size_t hash{};
		size_t moveCtor{};
		size_t moveAssign{};

		static void printHeader() {
			printf(
				"     ctor  defctor  cpyctor     dtor   assign    swaps      get  cnstget     hash   equals     less   ctormv assignmv |    total\n");
		}
		void reset() {
			ctor = 0;
			copyCtor = 0;
			dtor = 0;
			equals = 0;
			less = 0;
			assign = 0;
			swaps = 0;
			get = 0;
			constGet = 0;
			hash = 0;
			moveCtor = 0;
			moveAssign = 0;
		}

		void printCounts(std::string const& title) {
			size_t total = ctor + staticDefaultCtor + copyCtor + (dtor + staticDtor) + equals + less + assign + swaps + get + constGet + hash +
						   moveCtor + moveAssign;

			printf("%9zu%9zu%9zu%9zu%9zu%9zu%9zu%9zu%9zu%9zu%9zu%9zu%9zu |%9zu %s\n", ctor, staticDefaultCtor, copyCtor, dtor + staticDtor, assign,
				   swaps, get, constGet, hash, equals, less, moveCtor, moveAssign, total, title.c_str());
		}
	};

	// required for operator[]
	Counter()
		: mData(0)
		, mCounts(nullptr) {
		++staticDefaultCtor;
	}

	Counter(const size_t& data, Counts& counts)
		: mData(data)
		, mCounts(&counts) {
		if (mCounts) {
			++mCounts->ctor;
		}
	}

	Counter(const Counter& o)
		: mData(o.mData)
		, mCounts(o.mCounts) {
		if (mCounts) {
			++mCounts->copyCtor;
		}
	}

	Counter(Counter&& o)
		: mData(std::move(o.mData))
		, mCounts(std::move(o.mCounts)) {
		if (mCounts) {
			++mCounts->moveCtor;
		}
	}

	~Counter() {
		if (mCounts) {
			++mCounts->dtor;
		} else {
			++staticDtor;
		}
	}

	bool operator==(const Counter& o) const {
		if (mCounts) {
			++mCounts->equals;
		}
		return mData == o.mData;
	}

	bool operator<(const Counter& o) const {
		if (mCounts) {
			++mCounts->less;
		}
		return mData < o.mData;
	}

	Counter& operator=(const Counter& o) {
		mCounts = o.mCounts;
		if (mCounts) {
			++mCounts->assign;
		}
		mData = o.mData;
		return *this;
	}

	Counter& operator=(Counter&& o) {
		if (o.mCounts) {
			mCounts = std::move(o.mCounts);
		}
		mData = std::move(o.mData);
		if (mCounts) {
			++mCounts->moveAssign;
		}
		return *this;
	}

	size_t const& get() const {
		if (mCounts) {
			++mCounts->constGet;
		}
		return mData;
	}

	size_t& get() {
		if (mCounts) {
			++mCounts->get;
		}
		return mData;
	}

	void swap(Counter& other) {
		using std::swap;
		swap(mData, other.mData);
		swap(mCounts, other.mCounts);
		if (mCounts) {
			++mCounts->swaps;
		}
	}

	size_t getForHash() const {
		if (mCounts) {
			++mCounts->hash;
		}
		return mData;
	}

private:
	size_t mData;
	Counts* mCounts;
};

size_t Counter::staticDefaultCtor = 0;
size_t Counter::staticDtor = 0;

void swap(Counter& a, Counter& b) {
	a.swap(b);
}

namespace std {

template <>
class hash<Counter> {
public:
	size_t operator()(const Counter& c) const {
		return robin_hood::hash<uint64_t>{}(c.getForHash());
	}
};

} // namespace std

template <typename K, typename V>
const char* name(std::unordered_map<K, V> const&) {
	return "std::unordered_map";
}

template <typename K, typename V>
const char* name(std::map<K, V> const&) {
	return "std::map";
}

template <typename K, typename V>
const char* name(robin_hood::flat_map<K, V> const&) {
	return "robin_hood::flat_map";
}

template <typename K, typename V>
const char* name(robin_hood::node_map<K, V> const&) {
	return "robin_hood::node_map";
}

TEST_CASE("prefix", "[display]") {
	Counter::Counts::printHeader();
}

TEMPLATE_TEST_CASE("map ctor & dtor", "[display]", (std::map<Counter, Counter>), (std::unordered_map<Counter, Counter>),
				   (robin_hood::flat_map<Counter, Counter>), (robin_hood::node_map<Counter, Counter>)) {

	Counter::Counts counts;
	// counts.printHeader();
	{ TestType map; }
	counts.printCounts(std::string("ctor & dtor ") + name(TestType{}));
	REQUIRE(counts.dtor == counts.ctor + counts.defaultCtor + counts.copyCtor + counts.moveCtor);
	REQUIRE(counts.dtor == 0);
}

TEMPLATE_TEST_CASE("1 emplace", "[display]", (std::map<Counter, Counter>), (std::unordered_map<Counter, Counter>),
				   (robin_hood::flat_map<Counter, Counter>), (robin_hood::node_map<Counter, Counter>)) {
	Counter::Counts counts;
	{
		TestType map;
		map.emplace(std::piecewise_construct, std::forward_as_tuple((size_t)1, counts), std::forward_as_tuple((size_t)2, counts));
	}
	counts.printCounts(std::string("1 emplace ") + name(TestType{}));
	REQUIRE(counts.dtor == counts.ctor + counts.defaultCtor + counts.copyCtor + counts.moveCtor);
}

TEMPLATE_TEST_CASE("10k random insert & erase", "[display]", (std::map<Counter, Counter>), (std::unordered_map<Counter, Counter>),
				   (robin_hood::flat_map<Counter, Counter>), (robin_hood::node_map<Counter, Counter>)) {

	Counter::Counts counts;
	Counter::staticDefaultCtor = 0;
	Counter::staticDtor = 0;
	{
		Rng rng(321);
		TestType map;
		for (size_t i = 1; i < 10000; ++i) {
			for (size_t j = 0; j < 10; ++j) {
				map[Counter{rng.uniform<size_t>(i), counts}] = Counter{i, counts};
				map.erase(Counter{rng.uniform<size_t>(i), counts});

				map.emplace(std::piecewise_construct, std::forward_as_tuple(rng.uniform<size_t>(i), counts), std::forward_as_tuple(i, counts));
				map.erase(Counter{rng.uniform<size_t>(i), counts});

				map.insert(typename TestType::value_type{Counter{rng.uniform<size_t>(i), counts}, Counter{i, counts}});
				map.erase(Counter{rng.uniform<size_t>(i), counts});
			}
		}
	}

	counts.printCounts(std::string("10k random insert & erase - ") + name(TestType{}));
	REQUIRE(counts.dtor + Counter::staticDtor == Counter::staticDefaultCtor + counts.ctor + counts.defaultCtor + counts.copyCtor + counts.moveCtor);
}

TEMPLATE_TEST_CASE("10 insert erase", "[display]", (robin_hood::node_map<Counter, Counter>)) {
	for (size_t i = 23; i < 25; ++i) {
		Rng rng(12);
		Counter::Counts counts;
		Counter::staticDefaultCtor = 0;
		Counter::staticDtor = 0;
		{
			TestType map;
			for (size_t j = 0; j < 24; ++j) {
				map[Counter{rng.uniform<size_t>(i), counts}] = Counter{i, counts};
				map.erase(Counter{rng.uniform<size_t>(i), counts});
			}
		}

		REQUIRE(counts.dtor + Counter::staticDtor ==
				Counter::staticDefaultCtor + counts.ctor + counts.defaultCtor + counts.copyCtor + counts.moveCtor);
	}
}

TEMPLATE_TEST_CASE("100k [] and erase", "[display]", (std::map<Counter, Counter>), (std::unordered_map<Counter, Counter>),
				   (robin_hood::flat_map<Counter, Counter>), (robin_hood::node_map<Counter, Counter>)) {

	Counter::Counts counts;
	Counter::staticDefaultCtor = 0;
	Counter::staticDtor = 0;
	{
		static const size_t maxVal = 5000;
		Rng rng(123);
		TestType map;
		for (size_t i = 1; i < 100000; ++i) {
			map[Counter{rng.uniform<size_t>(maxVal), counts}] = Counter{i, counts};
			map.erase(Counter{rng.uniform<size_t>(maxVal), counts});
		}
	}

	counts.printCounts(std::string("100k [] and erase ") + name(TestType{}));
	REQUIRE(counts.dtor + Counter::staticDtor == Counter::staticDefaultCtor + counts.ctor + counts.defaultCtor + counts.copyCtor + counts.moveCtor);
}

TEMPLATE_TEST_CASE("100k emplace and erase", "[display]", (std::map<Counter, Counter>), (std::unordered_map<Counter, Counter>),
				   (robin_hood::flat_map<Counter, Counter>), (robin_hood::node_map<Counter, Counter>)) {

	Counter::Counts counts;
	// Counter::Counts::printHeader();
	Counter::staticDefaultCtor = 0;
	Counter::staticDtor = 0;
	{
		static const size_t maxVal = 5000;
		Rng rng(123);
		TestType map;
		for (size_t i = 1; i < 100000; ++i) {
			map.emplace(std::piecewise_construct, std::forward_as_tuple(rng.uniform<size_t>(i), counts), std::forward_as_tuple(i, counts));
			map.erase(Counter{rng.uniform<size_t>(maxVal), counts});
		}
	}

	counts.printCounts(std::string("100k emplace and erase ") + name(TestType{}));
	REQUIRE(counts.dtor + Counter::staticDtor == Counter::staticDefaultCtor + counts.ctor + counts.defaultCtor + counts.copyCtor + counts.moveCtor);
}

#define PRINT_SIZEOF(x, A, B) std::cout << sizeof(x<A, B>) << " bytes for " #x "<" #A ", " #B ">" << std::endl

struct BigObject {
	std::string str;
	std::vector<int> vec;
	std::shared_ptr<int> ptr;
	std::list<int> list;
};

namespace std {

template <>
class hash<BigObject> {
public:
	size_t operator()(BigObject const&) const {
		return 0;
	}
};

} // namespace std

TEST_CASE("show datastructure sizes") {
	PRINT_SIZEOF(robin_hood::unordered_map, int, int);
	PRINT_SIZEOF(std::map, int, int);
	PRINT_SIZEOF(std::unordered_map, int, int);
	std::cout << std::endl;

	PRINT_SIZEOF(robin_hood::unordered_map, int, BigObject);
	PRINT_SIZEOF(std::map, int, BigObject);
	PRINT_SIZEOF(std::unordered_map, int, BigObject);
	std::cout << std::endl;

	PRINT_SIZEOF(robin_hood::unordered_map, BigObject, BigObject);
	PRINT_SIZEOF(std::map, BigObject, BigObject);
	PRINT_SIZEOF(std::unordered_map, BigObject, BigObject);
	std::cout << std::endl;

	PRINT_SIZEOF(robin_hood::unordered_map, BigObject, int);
	PRINT_SIZEOF(std::map, BigObject, int);
	PRINT_SIZEOF(std::unordered_map, BigObject, int);
}

struct hex {
	hex(int bits)
		: bits(bits) {}

	int const bits;
};

std::ostream& operator<<(std::ostream& os, hex const& h) {
	os << "0x" << std::setfill('0') << std::setw(h.bits * 2 / 8) << std::hex;
	return os;
}

void showHash(size_t val) {
	auto sh = std::hash<size_t>{}(val);
	auto rh = robin_hood::hash<size_t>{}(val);
	std::cout << hex(ROBIN_HOOD_BITNESS) << val << " ->  " << hex(ROBIN_HOOD_BITNESS) << sh << "   " << hex(ROBIN_HOOD_BITNESS) << rh << std::endl;
}

TEST_CASE("show hash distribution") {
	std::cout << "input                 std::hash            robin_hood::hash" << std::endl;
	for (size_t i = 0; i < 16; ++i) {
		showHash(i);
	}

	for (size_t i = 0; i < 10; ++i) {
		size_t s = ((0x23d7 + i) << (ROBIN_HOOD_BITNESS / 2)) + (size_t)63;
		showHash(s);
	}

	for (size_t i = 1; i < 8; ++i) {
		showHash(i * ((size_t)1 << (ROBIN_HOOD_BITNESS - 4)));
	}

	for (size_t i = 1; i != 0; i *= 2) {
		showHash(i);
	}
}

struct ConfigurableCounterHash {

	// 234679895032 masksum, 1.17938e+06 geomean for 0xbdcbaec81634e906 0xa309d159626eef52
	ConfigurableCounterHash() {
#if ROBIN_HOOD_BITNESS == 64
		m_values[0] = UINT64_C(0x5e1caf9535ce6811);
		m_values[1] = UINT64_C(0xbb1039b2f223f0af);
#else
		m_values[0] = UINT64_C(0xa1ac131cae0b3f71);
#endif
	}

	ConfigurableCounterHash(ConfigurableCounterHash&& o) = default;
	ConfigurableCounterHash(ConfigurableCounterHash const& o) = default;

	ConfigurableCounterHash& operator=(ConfigurableCounterHash&& o) {
		m_values = std::move(o.m_values);
		return *this;
	}

	size_t operator()(Counter const& c) const {
		size_t const h = c.getForHash();
#if ROBIN_HOOD_HAS_UMULH
		size_t result = static_cast<size_t>(robin_hood::detail::umulh(m_values[0], h * m_values[1]));
#else
		uint64_t const factor = m_values[0];
		size_t result = static_cast<size_t>((h * factor) >> 32);
#endif
		return result;
	}

#if ROBIN_HOOD_BITNESS == 64
	std::array<uint64_t, 2> m_values;
	const std::array<uint64_t, 2> m_max_values = {(uint64_t)(-1), (uint64_t)(-1)};
#else
	std::array<uint64_t, 1> m_values;
	const std::array<uint64_t, 1> m_max_values = {(uint64_t)(-1)};
#endif
};

template <size_t S>
void mutate(std::array<uint64_t, S>& vals, Rng& rng, RandomBool<>& rbool) {
	do {
		if (rbool(rng)) {
			auto mask_bits = rng(24) + 1;
			uint64_t mask = rng((UINT64_C(1) << mask_bits) - 1) + 1;
			// uint64_t mask = (UINT64_C(1) << mask_bits) - 1;
			vals[rng.uniform<size_t>(vals.size())] ^= mask << rng(64 - mask_bits);
		} else {
			vals[rng.uniform<size_t>(vals.size())] = rng();
		}
	} while (rbool(rng));
}

template <typename A>
void eval(int const iters, A const& current_values, size_t& num_usecases, uint64_t& current_mask_sum, double& current_ops_sum) {
	using Map = robin_hood::flat_map<Counter, Counter, ConfigurableCounterHash, std::equal_to<Counter>, 128>;
	try {
		Rng rng(((uint64_t)iters) * 0x135ff36020fe7455);
		Counter::Counts counts;
		size_t const num_iters = 33000;

		{
			// this tends to be very slow because of lots of shifts
			counts.reset();
			Map map;
			map.m_values = current_values;

			for (size_t n = 1; n < 10000; n += (500 * 10000 / num_iters)) {
				for (size_t i = 0; i < 500; ++i) {
					map[Counter{rng.uniform<size_t>(n), counts}] = Counter{i, counts};
					current_mask_sum += map.mask();
					map.erase(Counter{rng.uniform<size_t>(n), counts});
				}
			}
		}
		current_ops_sum += std::log(counts.moveAssign + counts.moveCtor + counts.equals + counts.hash);
		++num_usecases;

		{
			counts.reset();
			Map map;
			map.m_values = current_values;

			for (size_t i = 0; i < num_iters; ++i) {
				map[Counter{rng.uniform<size_t>(i + 1), counts}] = Counter{rng.uniform<size_t>(i + 1), counts};
				map.erase(Counter{rng.uniform<size_t>(i + 1), counts});
				current_mask_sum += map.mask();
			}
		}
		current_ops_sum += std::log(counts.moveAssign + counts.moveCtor + counts.equals + counts.hash);
		++num_usecases;

		{
			counts.reset();
			Map map;
			map.m_values = current_values;

			for (size_t i = 0; i < num_iters; ++i) {
				map.emplace(std::piecewise_construct, std::forward_as_tuple(rng.uniform<size_t>(), counts), std::forward_as_tuple(i, counts));
				current_mask_sum += map.mask();
			}
		}
		current_ops_sum += std::log(counts.moveAssign + counts.moveCtor + counts.equals + counts.hash);
		++num_usecases;

		{
			counts.reset();
			Map map;
			map.m_values = current_values;

			for (size_t i = 0; i < num_iters; ++i) {
				map.emplace(std::piecewise_construct, std::forward_as_tuple(rng.uniform<size_t>(10000) << (ROBIN_HOOD_BITNESS / 2), counts),
							std::forward_as_tuple(i, counts));
				current_mask_sum += map.mask();
				map.erase(Counter{rng.uniform<size_t>(10000) << (ROBIN_HOOD_BITNESS / 2), counts});
			}
		}
		current_ops_sum += std::log(counts.moveAssign + counts.moveCtor + counts.equals + counts.hash);
		++num_usecases;

		{
			counts.reset();
			Map map;
			map.m_values = current_values;

			static const size_t maxVal = 100000 / (ROBIN_HOOD_BITNESS - 8);
			for (size_t i = 0; i < num_iters / 8; ++i) {
				for (size_t sh = 0; sh < (ROBIN_HOOD_BITNESS - 8); ++sh) {
					map[Counter{rng.uniform<size_t>(maxVal) << sh, counts}] = Counter{i, counts};
					current_mask_sum += map.mask();
					map.erase(Counter{rng.uniform<size_t>(maxVal) << sh, counts});
				}
			}
		}
		current_ops_sum += std::log(counts.moveAssign + counts.moveCtor + counts.equals + counts.hash);
		++num_usecases;

		// just sequential insertion
		{
			counts.reset();
			Map map;
			map.m_values = current_values;

			for (size_t i = 0; i < num_iters; ++i) {
				map.emplace(std::piecewise_construct, std::forward_as_tuple(i, counts), std::forward_as_tuple(i, counts));
				current_mask_sum += map.mask();
			}
		}
		current_ops_sum += std::log(counts.moveAssign + counts.moveCtor + counts.equals + counts.hash);
		++num_usecases;

		// sequential shifted
		{
			counts.reset();
			Map map;
			map.m_values = current_values;

			for (size_t i = 0; i < num_iters; ++i) {
				map.emplace(std::piecewise_construct, std::forward_as_tuple(i << ROBIN_HOOD_BITNESS / 2, counts), std::forward_as_tuple(i, counts));
				current_mask_sum += map.mask();
			}
		}
		current_ops_sum += std::log(counts.moveAssign + counts.moveCtor + counts.equals + counts.hash);
		++num_usecases;
	} catch (std::overflow_error const&) {
		current_mask_sum += (std::numeric_limits<uint64_t>::max)() / 100;
		current_ops_sum += (std::numeric_limits<double>::max)() / 100;
		++num_usecases;
	}
}

TEST_CASE("quickmixoptimizer", "[!hide]") {
	Rng factorRng(std::random_device{}());
	RandomBool<> rbool;

	using Map = robin_hood::flat_map<Counter, Counter, ConfigurableCounterHash, std::equal_to<Counter>, 126>;
	Map startup_map;
	auto best_values = startup_map.m_values;
	auto global_best_values = best_values;

	std::cout << "initializing with random data" << std::endl;
	for (size_t i = 0; i < best_values.size(); ++i) {
		best_values[i] = factorRng();
	}

	uint64_t best_mask_sum = (std::numeric_limits<uint64_t>::max)();
	double best_ops_sum = (std::numeric_limits<double>::max)();

	uint64_t global_best_mask_sum = best_mask_sum;
	double global_best_ops_sum = best_ops_sum;

	auto current_values = best_values;
	int num_unsuccessful_tries = 0;
	while (true) {

		size_t num_usecases = 0;
		uint64_t current_mask_sum = 0;
		double current_ops_sum = 0;
#pragma omp parallel for reduction(+ : num_usecases, current_mask_sum, current_ops_sum)
		for (int iters = 0; iters < 12 * 4; ++iters) {
			eval(iters, current_values, num_usecases, current_mask_sum, current_ops_sum);
		}
		std::cout << ".";
		std::cout.flush();

		++num_unsuccessful_tries;

		// also assign when we are equally good, should lead to a bit more exploration
		if (num_unsuccessful_tries == 1000 || std::tie(current_mask_sum, current_ops_sum) <= std::tie(best_mask_sum, best_ops_sum)) {
			best_mask_sum = current_mask_sum;
			best_ops_sum = current_ops_sum;
			best_values = current_values;

			if (std::tie(best_mask_sum, best_ops_sum) <= std::tie(global_best_mask_sum, global_best_ops_sum)) {
				global_best_mask_sum = best_mask_sum;
				global_best_ops_sum = best_ops_sum;
				global_best_values = best_values;
			}

			num_unsuccessful_tries = 0;

			std::cout << std::endl
					  << std::dec << global_best_mask_sum << " masksum, "
					  << std::exp(static_cast<double>(global_best_ops_sum) / static_cast<double>(num_usecases)) << " geomean globalbest: ";
			for (auto const x : global_best_values) {
				std::cout << hex(64) << x << " ";
			}

			std::cout << "  |  " << std::dec << best_mask_sum << " masksum, "
					  << std::exp(static_cast<double>(best_ops_sum) / static_cast<double>(num_usecases)) << " geomean current best: ";
			for (auto const x : best_values) {
				std::cout << hex(64) << x << " ";
			}

			std::cout << std::endl;
		}

		// mutate *after* evaluation & setting best, so initial value is tried too
		current_values = best_values;
		mutate(current_values, factorRng, rbool);
	}
}