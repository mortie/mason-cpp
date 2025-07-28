#include <mason/mason.h>
#include <iostream>
#include <fstream>
#include <string>

void printJSON(const Mason::Value &val, std::ostream &os);

void printJSONString(const Mason::String &s, std::ostream &os)
{
	const char *hexAlphabet = "0123456789abcdef";

	os << '"';
	for (unsigned char ch: s) {
		if (ch == '"' || ch == '\\') {
			os << "\\" << ch;
		} else if (ch == '\n') {
			os << "\\n";
		} else if (ch == '\r') {
			os << "\\r";
		} else if (ch < 0x20) {
			os << "\\u00";
			os << hexAlphabet[ch >> 4];
			os << hexAlphabet[ch & 0x0f];
		} else {
			os << ch;
		}
	}
	os << '"';
}

void printJSONArray(const Mason::Array &arr, std::ostream &os)
{
	os << '[';
	bool first = true;
	for (auto &val: arr) {
		if (!first) {
			os << ',';
		}
		first = false;
		printJSON(*val, os);
	}
	os << ']';
}

void printJSONObject(const Mason::Object &obj, std::ostream &os)
{
	os << '{';
	bool first = true;
	for (auto &[k, v]: obj) {
		if (!first) {
			os << ',';
		}
		first = false;

		printJSONString(k, os);
		os << ':';
		printJSON(*v, os);
	}
	os << '}';
}

void printJSON(const Mason::Value &val, std::ostream &os)
{
	if (val.is<Mason::Null>()) {
		os << "null";
	} else if (auto *b = val.as<Mason::Bool>(); b) {
		os << (*b ? "true" : "false");
	} else if (auto *n = val.as<Mason::Number>(); n) {
		os << *n;
	} else if (auto *s = val.as<Mason::String>(); s) {
		printJSONString(*s, os);
	} else if (auto *bs = val.as<Mason::BString>(); bs) {
		os << "\"TODO\"";
	} else if (auto *arr = val.as<Mason::Array>(); arr) {
		printJSONArray(*arr, os);
	} else if (auto *obj = val.as<Mason::Object>(); obj) {
		printJSONObject(*obj, os);
	} else {
		abort();
	}
}

int main(int argc, char **argv)
{
	std::fstream fstream;
	std::istream *is;

	if (argc == 1) {
		is = &std::cin;
	} else if (argc == 2) {
		fstream.open(argv[1]);
		if (!fstream) {
			std::cerr << "Failed to open " << argv[1] << '\n';
			return 1;
		}
		is = &fstream;
	} else {
		std::cerr << "Usage: " << argv[0] << " <file>\n";
		return 1;
	}

	std::string err;
	Mason::Value val;
	if (!Mason::parse(*is, val, &err)) {
		std::cerr << "Failed to parse: " << err << '\n';
		return 1;
	}

	printJSON(val, std::cout);
	return 0;
}
