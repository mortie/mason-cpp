#include "mason.h"

#include <charconv>
#include <iostream>
#include <stdint.h>

namespace Mason {

struct Location {
	int line = 1;
	int ch = 1;
};

class Reader {
public:
	Reader(std::istream &is): is_(is)  {
		fill();
	}

	int peek() { return peekN(0); }
	int peek2() { return peekN(1); }

	int get() {
		int ch = peek();
		index_ += 1;
		loc_.ch += 1;
		if (ch == '\n') {
			loc_.ch = 1;
			loc_.line += 1;
		}
		return ch;
	}

	Location loc() {
		return loc_;
	}

private:
	int peekN(int n = 0) {
		if (index_ + n >= size_) {
			fill();
		}

		if (index_ + n >= size_) {
			return EOF;
		}

		return buffer_[index_ + n];
	}

	void fill() {
		if (index_ > size_) {
			return;
		}

		memmove(buffer_, buffer_ + index_, size_ - index_);
		size_ -= index_;
		index_ = 0;
		size_ += is_.read((char *)buffer_ + size_, sizeof(buffer_) - size_).gcount();
	}

	std::istream &is_;
	unsigned char buffer_[128];
	size_t index_ = 0;
	size_t size_ = 0;
	Location loc_;
};

static bool parseValue(
	Reader &r, Value &v, int depth,
	String *err, bool topLevel = false);
static void serializeValue(std::ostream &os, Value &val, int indent);

static void error(Location loc, String *err, const char *what)
{
	if (!err) {
		return;
	}

	*err = std::to_string(loc.line);
	*err += ':';
	*err += std::to_string(loc.ch);
	*err += ": ";
	*err += what;
}

static bool skipBlockComment(Reader &r, String *err)
{
	r.get(); // '/'
	r.get(); // '*'
	while (true) {
		int ch = r.get();
		if (ch == EOF) {
			error(r.loc(), err, "Unexpected EOF");
			return false;
		}

		if (ch == '*' && r.peek() == '/') {
			r.get();
			return true;
		}
	}
}

static bool skipWhitespace(Reader &r, String *err)
{
	while (true) {
		int ch = r.peek();
		if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
			r.get();
			continue;
		}

		if (ch == '/' && r.peek2() == '/') {
			r.get();
			r.get();
			while (true) {
				ch = r.get();
				if (ch == '\n' || ch == EOF) {
					break;
				}
			}
			continue;
		}

		if (ch == '/' && r.peek2() == '*') {
			if (!skipBlockComment(r, err)) {
				return false;
			}
			continue;
		}

		break;
	}

	return true;
}

static bool skipSpace(Reader &r, String *err)
{
	while (true) {
		int ch = r.peek();
		if (ch == ' ' || ch == '\t') {
			r.get();
			continue;
		}

		if (ch == '/' && r.peek2() == '*') {
			if (!skipBlockComment(r, err)) {
				return false;
			}
			continue;
		}

		break;
	}

	return true;
}

static bool skipSep(Reader &r, bool &foundSep, String *err)
{
	if (!skipSpace(r, err)) {
		return false;
	}

	int ch = r.peek();

	if (ch == ',') {
		r.get();
		foundSep = true;
		return skipWhitespace(r, err);
	}

	if (ch == '\n') {
		r.get();
		foundSep = true;
		return skipWhitespace(r, err);
	}

	if (ch == '\r' && r.peek2() == '\n') {
		r.get();
		r.get();
		foundSep = true;
		return skipWhitespace(r, err);
	}

	if (ch == '/' && r.peek2() == '/') {
		r.get();
		r.get();
		foundSep = true;
		while (true) {
			ch = r.get();
			if (ch == EOF || ch == '\n') {
				return true;
			}
		}
	}

	foundSep = false;
	return true;
}

static bool parseHex(Reader &r, int n, uint32_t &ret, String *err)
{
	uint32_t num = 0;
	while (n > 0) {
		auto loc = r.loc();
		int ch = r.get();
		if (ch == EOF) {
			error(r.loc(), err, "Unexpected EOF");
			return false;
		}

		num *= 16;
		if (ch >= '0' && ch <= '9') {
			num += ch - '0';
		} else if (ch >= 'a' && ch <= 'f') {
			num += ch - 'a' + 10;
		} else if (ch >= 'A' && ch <= 'F') {
			num += ch - 'A' + 10;
		} else {
			error(loc, err, "Invalid hex character");
			return false;
		}

		n -= 1;
	}

	ret = num;
	return true;
}

static bool parseIdentifier(Reader &r, String &ident, String *err)
{
	int ch = r.peek();
	if (ch == EOF) {
		error(r.loc(), err, "Unexpected EOF");
		return false;
	}

	auto isFirstIdent = [](char ch) {
		return
			(ch >= 'a' && ch <= 'z') ||
			(ch >= 'A' && ch <= 'Z') ||
			ch == '_';
	};
	if (!isFirstIdent(ch)) {
		error(r.loc(), err, "Unexpected character for identifier");
		return false;
	}

	auto isIdent = [](char ch) {
		return
			(ch >= 'a' && ch <= 'z') ||
			(ch >= 'A' && ch <= 'Z') ||
			(ch >= '0' && ch <= '9') ||
			ch == '_' || ch == '-';
	};

	ident = "";

	do {
		ident += r.get();
	} while (isIdent(r.peek()));
	return true;
}

template<typename T>
static bool parseStringEscapeChar(char ch, T &str) {
	if (ch == '"') {
		str.push_back('"');
		return true;
	} else if (ch == '\\') {
		str.push_back('\\');
		return true;
	} else if (ch == '/') {
		str.push_back('/');
		return true;
	} else if (ch == 'b') {
		str.push_back('\b');
		return true;
	} else if (ch == 'f') {
		str.push_back('\f');
		return true;
	} else if (ch == 'n') {
		str.push_back('\n');
		return true;
	} else if (ch == 'r') {
		str.push_back('\r');
		return true;
	} else if (ch == 't') {
		str.push_back('\t');
		return true;
	}

	return false;
}

static void writeUTF8(uint32_t num, String &str)
{
	if (num >= 0x10000u) {
		str += 0xf0u | ((num & 0x1c0000u) >> 18u);
		str += 0x80u | ((num & 0x03f000u) >> 12u);
		str += 0x80u | ((num & 0x000fc0u) >> 6u);
		str += 0x80u | ((num & 0x00003fu) >> 0u);
	} else if (num >= 0x0800u) {
		str += 0xe0u | ((num & 0x00f000u) >> 12u);
		str += 0x80u | ((num & 0x000fc0u) >> 6u);
		str += 0x80u | ((num & 0x00003fu) >> 0u);
	} else if (num >= 0x0080u) {
		str += 0xc0u | ((num & 0x0007c0u) >> 6);
		str += 0x80u | ((num & 0x00003fu) >> 0);
	} else {
		str += num;
	}
}

static bool parseStringEscape(Reader &r, String &str, String *err)
{
	int ch = r.get();
	if (ch == EOF) {
		error(r.loc(), err, "Unexpected EOF");
		return false;
	}

	if (parseStringEscapeChar(ch, str)) {
		return true;
	}

	if (ch == 'x') {
		unsigned int num;
		if (!parseHex(r, 2, num, err)) {
			return false;
		}

		str += (char)num;
		return true;
	}

	if (ch == 'u') {
		uint32_t codepoint;
		auto loc = r.loc();
		if (!parseHex(r, 4, codepoint, err)) {
			return false;
		}

		if (codepoint >= 0xd800 && codepoint <= 0xdfff) {
			error(loc, err, "UTF-16 surrogate pair escapes are not allowed");
			return false;
		}

		writeUTF8(codepoint, str);
		return true;
	}

	if (ch == 'U') {
		uint32_t codepoint;
		auto loc = r.loc();
		if (!parseHex(r, 6, codepoint, err)) {
			return false;
		}

		if (codepoint >= 0xd800 && codepoint <= 0xdfff) {
			error(loc, err, "UTF-16 surrogate pair escapes are not allowed");
			return false;
		}

		writeUTF8(codepoint, str);
		return true;
	}

	error(r.loc(), err, "Unknown escape character");
	return false;
}

static bool parseString(Reader &r, String &str, String *err) 
{
	str = "";
	r.get(); // '"'

	while (true) {
		int ch = r.get();
		if (ch == EOF) {
			error(r.loc(), err, "Unexpected EOF");
			return false;
		}

		if (ch == '"') {
			return true;
		}

		if (ch == '\\') {
			if (!parseStringEscape(r, str, err)) {
				return false;
			}

			continue;
		}

		str += ch;
	}
}

static bool parseBinaryString(Reader &r, BString &bytes, String *err)
{
	bytes.clear();
	r.get(); // 'b'
	r.get(); // '"'

	while (true) {
		auto loc = r.loc();
		int ch = r.get();
		if (ch == EOF) {
			error(r.loc(), err, "Unexpected EOF");
			return false;
		}

		if (ch == '"') {
			return true;
		}

		if (ch == '\\') {
			ch = r.get();
			if (ch == EOF) {
				error(r.loc(), err, "Unexpected EOF");
				return false;
			}

			if (ch == 'x') {
				uint32_t num;
				if (!parseHex(r, 2, num, err)) {
					return false;
				}
				bytes.push_back(num);
				continue;
			}

			if (!parseStringEscapeChar(ch, bytes)) {
				error(r.loc(), err, "Unknown escape character");
				return false;
			}

			continue;
		}

		if (ch > 127) {
			error(loc, err, "Binary strings can only contain ASCII");
			return false;
		}

		bytes.push_back(ch);
	}
}

static bool parseRawString(Reader &r, String &str, String *err)
{
	str.clear();
	r.get(); // 'r'

	int hashes = 0;
	int ch;
	while ((ch = r.get()) == '#') {
		hashes += 1;
	}
	if (ch != '"') {
		error(r.loc(), err, "Expected '\"'");
		return false;
	}

	int hashState = -1;
	while (true) {
		ch = r.get();
		if (ch == EOF) {
			error(r.loc(), err, "Unexpected EOF");
			return false;
		}

		str += ch;
		if (ch == '"') {
			hashState = 0;
		} else if (ch == '#' && hashState >= 0) {
			hashState += 1;
		} else {
			hashState = -1;
		}

		if (hashState == hashes) {
			str.resize(str.size() - hashes - 1);
			return true;
		}
	}
}

static bool charValue(int ch, int &num)
{
	if (ch >= '0' && ch <= '9') {
		num = ch - '0';
		return true;
	} else if (ch >= 'a' && ch <= 'f') {
		num = ch - 'a' + 10;
		return true;
	} else if (ch >= 'A' && ch <= 'F') {
		num = ch - 'A' + 10;
		return true;
	}

	return false;
}

static bool parseInteger(Reader &r, double &ret, int radix, String *err)
{
	int digit;

	auto loc = r.loc();
	if (!charValue(r.peek(), digit)) {
		error(loc, err, "Expected digit");
		return false;
	}

	if (digit >= radix) {
		error(loc, err, "Invalid digit");
		return false;
	}

	r.get();
	double num = digit;
	while (true) {
		int ch = r.peek();
		if (ch == EOF) {
			ret = num;
			return true;
		}

		if (ch == '\'') {
			r.get();
			continue;
		}

		if (!charValue(ch, digit)) {
			ret = num;
			return true;
		}

		if (digit >= radix) {
			ret = num;
			return true;
		}

		num *= radix;
		num += digit;
		r.get();
	}
}

static bool parseNumber(Reader &r, Number &ret, String *err)
{
	auto loc = r.loc();
	const char *sign = "";
	int ch = r.peek();
	if (ch == '-') {
		sign = "-";
		r.get();
		ch = r.peek();
	} else if (ch == '+') {
		r.get();
		ch = r.peek();
	}

	int radix = 10;
	if (ch == '0') {
		int ch2 = r.peek2();
		if (ch2 == 'x') {
			radix = 16;
			r.get();
			r.get();
			ch = r.peek();
		} else if (ch2 == 'o') {
			radix = 8;
			r.get();
			r.get();
			ch = r.peek();
		} else if (ch2 == 'b') {
			radix = 2;
			r.get();
			r.get();
			ch = r.peek();
		}
	}

	double integral = 0;
	if (ch != '.') {
		if (!parseInteger(r, integral, radix, err)) {
			return false;
		}
		ch = r.peek();
	}

	std::string fractional;
	if (radix == 10 && ch == '.') {
		r.get();
		fractional += '.';

		ch = r.peek();
		if (!(ch >= '0' && ch <= '9')) {
			error(r.loc(), err, "Expected digit");
			return false;
		}

		while (true) {
			int ch = r.peek();
			if (ch == '\'') {
				r.get();
				continue;
			}

			if (ch >= '0' && ch <= '9') {
				fractional += ch;
				r.get();
				continue;
			}

			break;
		}

		ch = r.peek();
	}

	double exponent = 0;
	if (radix == 10 && (ch == 'e' || ch == 'E')) {
		r.get();
		ch = r.peek();
		int exponentSign = 1;
		if (ch == '-') {
			exponentSign = -1;
			r.get();
			ch = r.peek();
		} else if (ch == '+') {
			r.get();
			ch = r.peek();
		}

		if (!parseInteger(r, exponent, 10, err)) {
			return false;
		}

		if (exponentSign < 0) {
			exponent = -exponent;
		}
	}

	char number[256];
	int n = snprintf(
		number, sizeof(number), "%s%.0f%se%.0f",
		sign, integral, fractional.c_str(), exponent);
	if (size_t(n) >= sizeof(number)) {
		error(loc, err, "Number too long");
		return false;
	}

	char *ep;
	ret = strtod(number, &ep);
	return true;
}

static bool parseKey(Reader &r, String &key, String *err)
{
	if (r.peek() == '"') {
		return parseString(r, key, err);
	} else {
		return parseIdentifier(r, key, err);
	}
}

static bool parseKeyValuePairsAfterKey(
	Reader &r, String key, Object &obj, int depth, String *err)
{
	obj.clear();

	size_t index = 0;
	while (true) {
		if (r.peek() != ':') {
			error(r.loc(), err, "Expected ':'");
			return false;
		}
		r.get();

		if (!skipWhitespace(r, err)) {
			return false;
		}

		auto &val = obj[std::move(key)] = Value::makeNull();
		val->index(index++);
		if (!parseValue(r, *val, depth, err)) {
			return false;
		}

		bool hasSep;
		if (!skipSep(r, hasSep, err)) {
			return false;
		}

		if (!skipWhitespace(r, err)) {
			return false;
		}

		int ch = r.peek();
		if (ch == '}' || ch == EOF) {
			return true;
		}

		if (!hasSep) {
			error(r.loc(), err, "Expected separator, '}' or EOF");
			return false;
		}

		key.clear();
		if (!parseKey(r, key, err)) {
			return false;
		}

		if (!skipWhitespace(r, err)) {
			return false;
		}
	}
}

static bool parseKeyValuePairs(Reader &r, Object &obj, int depth, String *err)
{
	String key;
	if (!parseKey(r, key, err)) {
		return false;
	}

	if (!skipWhitespace(r, err)) {
		return false;
	}

	return parseKeyValuePairsAfterKey(r, std::move(key), obj, depth, err);
}

static bool parseObject(Reader &r, Object &obj, int depth, String *err)
{
	if (r.peek() != '{') {
		error(r.loc(), err, "Expected '{'");
		return false;
	}
	r.get();

	if (!skipWhitespace(r, err)) {
		return false;
	}

	if (r.peek() == '}') {
		r.get();
		return true;
	}

	if (!parseKeyValuePairs(r, obj, depth, err)) {
		return false;
	}

	if (!skipWhitespace(r, err)) {
		return false;
	}

	if (r.peek() != '}') {
		error(r.loc(), err, "Expected '{'");
		return false;
	}
	r.get();
	return true;
}

static bool parseArray(Reader &r, Array &arr, int depth, String *err)
{
	if (r.peek() != '[') {
		error(r.loc(), err, "Expected '['");
		return false;
	}
	r.get();

	if (!skipWhitespace(r, err)) {
		return false;
	}

	if (r.peek() == ']') {
		r.get();
		return true;
	}

	size_t index = 0;
	while (true) {
		arr.push_back(Value::makeNull());
		arr.back()->index(index++);
		if (!parseValue(r, *arr.back(), depth, err)) {
			return false;
		}

		bool hasSep;
		if (!skipSep(r, hasSep, err)) {
			return false;
		}

		int ch = r.peek();
		if (ch == ']') {
			r.get();
			return true;
		}

		if (ch == EOF) {
			error(r.loc(), err, "Unexpected EOF");
			return false;
		}

		if (!hasSep) {
			error(r.loc(), err, "Expected separator or ']'");
			return false;
		}
	}
}

static bool parseValue(
	Reader &r, Value &v, int depth,
	String *err, bool topLevel)
{
	if (depth <= 0) {
		error(r.loc(), err, "Nesting limit exceeded");
		return false;
	}

	int ch = r.peek();
	if (ch == EOF) {
		error(r.loc(), err, "Unexpected EOF");
		return false;
	}

	if (ch == '[') {
		return parseArray(r, v.set(Array{}), depth - 1, err);
	} else if (ch == '{') {
		return parseObject(r, v.set(Object{}), depth - 1, err);
	} else if (ch == '"') {
		String ident;
		if (!parseString(r, ident, err)) {
			return false;
		}

		if (topLevel) {
			if (!skipWhitespace(r, err)) {
				return false;
			}

			if (r.peek() == ':') {
				return parseKeyValuePairsAfterKey(
					r, std::move(ident), v.set(Object{}),
					depth - 1, err);
			}
		}

		v.set(std::move(ident));
		return true;
	} else if (ch == 'r' && (r.peek2() == '"' || r.peek2() == '#')) {
		return parseRawString(r, v.set(String{}), err);
	} else if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '+' || ch == '-') {
		return parseNumber(r, v.set(Number{}), err);
	} else if (ch == 'b' && r.peek2() == '"') {
		return parseBinaryString(r, v.set(BString{}), err);
	}

	auto loc = r.loc();
	String ident;
	if (!parseIdentifier(r, ident, err)) {
		return false;
	}

	if (topLevel) {
		if (!skipWhitespace(r, err)) {
			return false;
		}

		if (r.peek() == ':') {
			return parseKeyValuePairsAfterKey(
				r, std::move(ident), v.set(Object{}),
				depth - 1, err);
		}
	}

	if (ident == "null") {
		v.set(Null{});
		return true;
	} else if (ident == "true") {
		v.set(true);
		return true;
	} else if (ident == "false") {
		v.set(false);
		return true;
	} else if (ident.size() > 0) {
		error(loc, err, "Unexpected keyword");
		return false;
	} else {
		error(loc, err, "Unexpected character");
		return false;
	}
}

bool parse(
	std::istream &is, Value &v,
	String *err, int maxDepth)
{
	Reader r(is);

	if (!skipWhitespace(r, err)) {
		return false;
	}

	if (!parseValue(r, v, maxDepth, err, true)) {
		return false;
	}

	if (!skipWhitespace(r, err)) {
		return false;
	}

	if (r.peek() != EOF) {
		error(r.loc(), err, "Trailing garbage after document");
		return false;
	}

	return true;
}

static void serializeString(std::ostream &os, const String &ident)
{
	os << '"';
	for (char ch: ident) {
		if (ch == '"' || ch == '\\') {
			os << '\\' << ch;
		} else if (ch == '\n') {
			os << "\\n";
		} else if (ch == '\r') {
			os << "\\r";
		} else if (ch == '\t') {
			os << "\\t";
		} else {
			os << ch;
		}
	}
	os << '"';
}

static void serializeBString(std::ostream &os, const BString &ident)
{
	const char *alphabet = "0123456789abcdef";

	os << "b\"";
	for (unsigned char ch: ident) {
		if (ch >= 32 && ch < 127) {
			os << char(ch);
		} else {
			os << "\\x";
			os << alphabet[ch >> 4];
			os << alphabet[ch & 0x0f];
		}
	}
	os << '"';
}

static void serializeKey(std::ostream &os, const String &ident)
{
	if (ident == "") {
		os << "\"\"";
		return;
	}

	unsigned char ch = ident[0];
	bool isValidIdent =
		(ch >= 'a' && ch <= 'z') ||
		(ch >= 'A' && ch <= 'Z') ||
		ch == '_';
	if (!isValidIdent) {
		serializeString(os, ident);
		return;
	}

	for (unsigned char ch: ident) {
		isValidIdent =
			(ch >= 'a' && ch <= 'z') ||
			(ch >= 'A' && ch <= 'Z') ||
			(ch >= '0' && ch <= '9') ||
			ch == '_' || ch == '-';
		if (!isValidIdent) {
			serializeString(os, ident);
			return;
		}
	}

	os << ident;
}

static void serializeKeyValues(std::ostream &os, Object &obj, int indent)
{
	std::vector<std::pair<const std::string *, Value *>> values;
	values.reserve(obj.size());
	for (auto &[key, val]: obj) {
		values.push_back({&key, val.get()});
	}

	std::sort(values.begin(), values.end(), [](const auto &a, const auto &b) {
		return a.second->index() < b.second->index();
	});

	for (auto [key, val]: values) {
		for (int i = 0; i < indent; ++i) {
			os << "  ";
		}

		serializeKey(os, *key);
		os << ": ";
		serializeValue(os, *val, indent);
		os << '\n';
	}
}

static void serializeObject(std::ostream &os, Object &obj, int indent)
{
	if (obj.size() == 0) {
		os << "{}";
		return;
	}

	os << "{\n";
	serializeKeyValues(os, obj, indent + 1);
	os << '}';
}

static void serializeArray(std::ostream &os, Array &arr, int indent)
{
	if (arr.size() == 0) {
		os << "[]";
		return;
	}

	os << "[\n";
	indent += 1;
	for (auto &val: arr) {
		for (int i = 0; i < indent; ++i) {
			os << "  ";
		}

		serializeValue(os, *val, indent);
		os << '\n';
	}
	os << ']';
}

static void serializeNumber(std::ostream &os, Number num)
{
	char buf[64];
	auto res = std::to_chars(buf, &buf[sizeof(buf) - 1], num);
	*res.ptr = '\0';
	os << buf;
}

static void serializeValue(std::ostream &os, Value &val, int indent)
{
	if (val.is<Null>()) {
		os << "null";
	} else if (auto *b = val.as<Bool>(); b) {
		os << (*b ? "true" : "false");
	} else if (auto *n = val.as<Number>(); n) {
		serializeNumber(os, *n);
	} else if (auto *s = val.as<String>(); s) {
		serializeString(os, *s);
	} else if (auto *b = val.as<BString>(); b) {
		serializeBString(os, *b);
	} else if (auto *a = val.as<Array>(); a) {
		serializeArray(os, *a, indent);
	} else if (auto *o = val.as<Object>(); o) {
		serializeObject(os, *o, indent);
	}
}

void serialize(std::ostream &os, Value &v)
{
	if (auto *obj = v.as<Object>(); obj) {
		serializeKeyValues(os, *obj, 0);
	} else {
		serializeValue(os, v, 0);
	}
}

}
