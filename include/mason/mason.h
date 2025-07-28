#pragma once

#include <functional>
#include <string_view>
#include <variant>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <iosfwd>

namespace Mason {

class Value;

struct StringHash {
	using hash_type = std::hash<std::string_view>;
	using is_transparent = void;

	std::size_t operator()(const char *str) const { return hash_type{}(str); }
	std::size_t operator()(std::string_view str) const { return hash_type{}(str); }
	std::size_t operator()(const std::string &str) const { return hash_type{}(str); }
};

struct Null {};
using Bool = bool;
using Number = double;
using String = std::string;
using BString = std::vector<unsigned char>;
using Array = std::vector<std::shared_ptr<Value>>;
using Object = std::unordered_map<std::string, std::shared_ptr<Value>, StringHash, std::equal_to<>>;

class Value {
public:
	using V = std::variant<Null, Bool, Number, String, BString, Array, Object>;

	Value(): Value(Null{}) {}
	template<typename T>
	Value(T v): v_(std::move(v)) {}

	template<typename T>
	T *as() { return std::get_if<T>(&v_); }

	template<typename T>
	const T *as() const { return std::get_if<T>(&v_); }

	template<typename T>
	bool is() const { return as<T>(); }

	template<typename T>
	static std::shared_ptr<Value> make(T v)
	{
		return std::make_shared<Value>(std::move(v));
	}

	static std::shared_ptr<Value> makeNull()
	{
		return std::make_shared<Value>(Null{});
	}

	Null &set(Null &&v) { return setT(std::move(v)); }
	Bool &set(Bool &&v) { return setT(std::move(v)); }
	Number &set(Number &&v) { return setT(std::move(v)); }
	String &set(String &&v) { return setT(std::move(v)); }
	BString &set(BString &&v) { return setT(std::move(v)); }
	Array &set(Array &&v) { return setT(std::move(v)); }
	Object &set(Object &&v) { return setT(std::move(v)); }

	V &v() { return v_; }
	size_t index() { return index_; }

private:
	static size_t nextIndex();

	template<typename T>
	T &setT(T &&v)
	{
		v_ = std::move(v);
		return std::get<T>(v_);
	}

	V v_;
	size_t index_ = nextIndex();
};

bool parse(std::istream &is, Value &v, std::string *err = nullptr);

}
