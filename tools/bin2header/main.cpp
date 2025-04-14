#include <iomanip>
#define NOMINMAX
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ios>
#include <iostream>
#include <Windows.h>
#include <string>
#include <wingdi.h>
#include <fstream>
#include <filesystem>



int main(int argc, char *argv[]){
	if(argc < 3){
		std::cout << "usage: bin2header infile outfile";
		return 1;
	}

	std::filesystem::path p(argv[1]);
	std::filesystem::path op(argv[2]);

	std::ifstream f(p, std::ios::binary);
	auto sz = std::filesystem::file_size(p);

	std::ofstream o(op);

	std::string guard_name = "_" + p.filename().string();
	std::replace(guard_name.begin(), guard_name.end(), '.', '_');
	std::transform(guard_name.begin(), guard_name.end(), guard_name.begin(), ::toupper);
	guard_name += "_H";

	std::string var_base_name = p.stem().string();
	std::transform(var_base_name.begin(), var_base_name.end(), var_base_name.begin(), ::tolower);

	o << "#ifndef " << guard_name << "\n";
	o << "#define " << guard_name << "\n\n";

	o << "static const unsigned char " << var_base_name << "_arr[] = {\n    ";

	size_t count = 0;

	o << std::hex;
	unsigned char c;
	while(f.read(reinterpret_cast<char*>(&c), 1)){
		count++;

		bool is_last = count == sz;
		bool is_last_in_line = count % 16 == 0;

		o << "0x" << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(c);

		if(!is_last){
			o << ",";
		}

		if(!is_last_in_line && !is_last){
			o << " ";
		}

		if(is_last_in_line){
			o << "\n";
			if(!is_last){
				o << "    ";
			}
		}
	}
	
	if(count % 16 != 0){
		o << "\n";
	}

	o << "};\n";

	o << "#endif" << "\n";

	o.flush();
}