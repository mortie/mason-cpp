#include <mason/mason.h>
#include <iostream>
#include <fstream>

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

	Mason::serialize(std::cout, val);
	return 0;
}
