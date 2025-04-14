#define NOMINMAX
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ios>
#include <iostream>
#include <Windows.h>
#include <memory>
#include <string>
#include <wingdi.h>
#include <fstream>
#include <filesystem>
#include <map>



int main(int argc, char *argv[]){
	if(argc < 3){
		std::cout << "usage: bmp2arr infile outfile [bit depth]";
		return 1;
	}

	std::filesystem::path p(argv[1]);
	std::filesystem::path op(argv[2]);

	BITMAPFILEHEADER file_header;
	BITMAPINFOHEADER bmp_header;

	std::ifstream f(p, std::ios::binary);
	f.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));
	f.read(reinterpret_cast<char*>(&bmp_header), sizeof(bmp_header));

	if(bmp_header.biBitCount > 8 || bmp_header.biCompression != BI_RGB){
		std::cout << "unsupported format";
		return 1;
	}

	unsigned int bit_depth = bmp_header.biBitCount;
	if(argc > 3){
		bit_depth = std::stoi(argv[3]);
	}

	uint32_t stride = (((bmp_header.biWidth * bmp_header.biBitCount) + 0x1f) & ~0x1f) >> 0x3;
	uint32_t pixels_per_byte = 8 / bmp_header.biBitCount;
	uint32_t actual_pixels_per_byte = 8 / bit_depth;

	bool bottom_up = bmp_header.biHeight > 0 ? true : false;
	if(!bottom_up){
		bmp_header.biHeight *= -1;
	}
	uint32_t lut_entries = bmp_header.biClrUsed ? bmp_header.biClrUsed : 1 << bmp_header.biBitCount;
	uint32_t sz = stride * bmp_header.biHeight;
	std::vector<RGBQUAD> lut(lut_entries, RGBQUAD());
	std::vector<unsigned char> img_data(sz, 0);


	f.read(reinterpret_cast<char *>(lut.data()), lut_entries * sizeof(RGBQUAD));
	f.read(reinterpret_cast<char *>(img_data.data()), sz);

	std::ofstream o(op);

	std::string guard_name = "_" + p.filename().string();
	std::replace(guard_name.begin(), guard_name.end(), '.', '_');
	std::transform(guard_name.begin(), guard_name.end(), guard_name.begin(), ::toupper);
	guard_name += "_H";

	std::string var_base_name = p.stem().string();
	std::transform(var_base_name.begin(), var_base_name.end(), var_base_name.begin(), ::tolower);

	o << "#ifndef " << guard_name << "\n";
	o << "#define " << guard_name << "\n\n";

	o << "static const unsigned char " << var_base_name << "_bpp = " << bit_depth << ";\n";
	o << "static const unsigned int " << var_base_name << "_width = " << bmp_header.biWidth << "; \n";
	o << "static const unsigned int " << var_base_name << "_height = " << bmp_header.biHeight << "; \n\n";

	o << "static const unsigned char " << var_base_name << "_arr[] = {\n    ";

	uint32_t count = 0;
	unsigned char next = 0;

	o << std::hex;
	for(int32_t i = bottom_up ? bmp_header.biHeight - 1 : 0; bottom_up ? i >= 0 : i < bmp_header.biHeight; i += bottom_up ? -1 : 1){
		for(uint32_t j = 0; j < bmp_header.biWidth; j++){
			count++;

			bool is_last = count == (bmp_header.biWidth * bmp_header.biHeight);
			bool is_last_in_line = count % (8 * actual_pixels_per_byte) == 0;
			bool is_last_for_next = count  % actual_pixels_per_byte == 0;

			uint32_t idx = i * stride + j / (pixels_per_byte);
			unsigned char cur = img_data[idx];

			cur >>= ((pixels_per_byte - 1) - (j & (pixels_per_byte - 1))) * bmp_header.biBitCount;
			cur &= (1 << bit_depth) - 1;

			next = (next << bit_depth) | cur;

			if(is_last){
				next <<= bit_depth * ((actual_pixels_per_byte - (count % actual_pixels_per_byte + 1)) % actual_pixels_per_byte); 
			}

			if(is_last_for_next || is_last){
				o << "0x" << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(next);

				if(!is_last){
					o << ",";
				}

				if(!is_last_in_line && !is_last){
					o << " ";
				}
			}

			if(is_last_in_line){
				o << "\n";
				if(!is_last){
					o << "    ";
				}
			}
		}
	}
	
	if(count % 8 != 0){
		o << "\n";
	}

	o << "};\n\n";

	o << "static const unsigned int " << var_base_name << "_lut[] = {\n";

	for(uint32_t i = 0; i < std::min(lut_entries, static_cast<uint32_t>(1 << bit_depth)); i++){
		auto cur = lut[i];

		uint32_t bgra = (cur.rgbBlue) << 16 | (cur.rgbGreen) << 8 | (cur.rgbRed);

		o << "    0x" << std::setw(8) << std::setfill('0') << bgra;
		if(i + 1 < lut_entries){
			o << ",";
		}
		o << "\n";
	}

	o << "};\n\n";

	o << "#endif" << "\n";

	o.flush();
}