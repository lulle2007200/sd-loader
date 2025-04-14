from Crypto.Cipher import AES
from Crypto.Signature import pss
from Crypto.Signature import pkcs1_15
from Crypto.Hash import SHA256
from Crypto.PublicKey import RSA
import os
import struct
import argparse


try:
	bek = os.environ["BEK"]
except:
	print("BEK environment variable not set")
	exit(-1)

bek = bytes.fromhex(bek)


rsa_e_custom      = int.from_bytes(bytes.fromhex("010001"), byteorder = 'little', signed = False)
rsa_n_custom      = int.from_bytes(bytes.fromhex("596969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969696969FF"), byteorder = 'little', signed = False)
rsa_d_custom      = 6512128715229088976470211610075969347035078304643231077895577077900787352712063823560162578441773733649014439616165727455431015055675770987914713980812453585413988983206576233689754710500864883529402371292948326392791238474661859182717295176679567362482790015587820446999760239570255254879359445627372805817473978644067558931078225451477635089763009580492462097185005355990612929951162366081631888011031830742459571000203341001926135389508196521518687349554188686396554248868403128728646457247197407637887195043486221295751496667162366700477934591694110831002874992896076061627516220934290742867397720103040314639313
rsa_key_custom    = RSA.construct((rsa_n_custom, rsa_e_custom, rsa_d_custom))

rsa_n_nintendo = int.from_bytes(bytes.fromhex("9BCD635886EBD2F3D0842D3666BE09366E12122BF53A2E7DF944D7C03D4BFD5A22B35C1956EECE88201E062BC26B492F5F74C2DFBCD076A177F9A25949D2F6FF1EFE340BEF5BEDD086156404871EC4F5FC2F59D977C18757A3CB593B8B6255B2B1485123ECFCA9EF10408E46CCA11BE69A476003EFE7C4B0DBC893DED86266EAE0A860D3F801BB218194E8DB4AC5428F3B5EE999038CEDB2E47CBFB694CC10D6972D785CAE5ECC4C2AEDD36B248FDFA2FA68653986E82B65EC10AB8959AD51CC8CDFE5D5CD6BDDC43495E3A962A827E49E3EB2A6991DCC0B226ECBD2D5388810CF7519E3A51D787587E527AB83CF98F17ED76BD88136DA636B76FDFD8529B1B9"), byteorder = 'little', signed = False)
rsa_e_nintendo = int.from_bytes(bytes.fromhex("010001"), byteorder = 'little', signed = False)
rsa_key_nintendo = RSA.construct((rsa_n_nintendo, rsa_e_nintendo))

def get_cipher_pss(key):
	return pss.new(key, salt_bytes = 32)

def get_cipher_aes():
	iv = bytearray(0x10)
	return AES.new(bek, AES.MODE_CBC, iv = iv)

def decrypt_bytes(data):
	return get_cipher_aes().decrypt(data)

def encrypt_bytes(data):
	return get_cipher_aes().encrypt(data)

def get_sha256(data):
	return SHA256.new(data)

def check_size(data, size):
	if(len(data) != size):
		raise ValueError(f"Size != {size}")

class Pkg1:
	def __init__(self, payload, loader = None, is_encrypted = False):
		self.payload = payload
		self.loader = loader
		self.is_encrypted = is_encrypted

	@classmethod
	def from_file(cls, payload_path, loader_path = None, is_encrypted = False):
		payload = None
		loader = None
		with open(payload_path, "rb") as f_payload:
			payload = bytearray(f_payload.read())
		if loader_path:
			with open(loader_path, "rb") as f_loader:
				loader = bytearray(f_loader.read())
		return cls(payload, loader, is_encrypted)

	@classmethod
	def from_bytes(cls, payload, loader = None, is_encrypted = False):
		return cls(payload, loader, is_encrypted)

	def to_bytes(self):
		ret = bytearray()
		if self.loader:
			ret += self.loader
			if not self.is_encrypted:
				ret = encrypt_bytes(ret)
			ret += bytearray(0x20)
			ret += self.payload
		else:
			ret = self.payload
			if not self.is_encrypted:
				ret = encrypt_bytes(ret)

		return ret

	def write_to_file(self, out_path):
		with open(out_path, "wb") as f:
			f.write(self.to_bytes())

class Payload:
	def __init__(self, signature, salt, data_hash, version, length, load_address, entry_point, data, key = rsa_key_custom):
		check_size(signature, 0x100)
		check_size(salt, 0x20)
		check_size(data_hash, 0x20)
		self.signature    = signature
		self.salt         = salt
		self.data_hash    = data_hash
		self.version      = int.from_bytes(version, byteorder = 'little', signed = False)
		self.length       = int.from_bytes(length, byteorder = 'little', signed = False)
		self.load_address = int.from_bytes(load_address, byteorder = 'little', signed = False)
		self.entry_point  = int.from_bytes(entry_point, byteorder = 'little', signed = False)
		self.data         = data

		self.key = key

	@classmethod
	def from_bytes(cls, bytes, key = rsa_key_custom):
		signature = bytes[0x10:0x110][::-1]
		salt = bytes[0x110:0x130]
		data_hash = bytes[0x130:0x150]
		version = bytes[0x150:0x154]
		length = bytes[0x154:0x158]
		load_address = bytes[0x158:0x15c]
		entry_point = bytes[0x15c:0x160]
		data = bytes[0x170:]

		return cls(signature, salt, data_hash, version, length, load_address, entry_point, data, key = key)

	@classmethod
	def from_file(cls, path_in, key = rsa_key_custom):
		with open(path_in, 'rb') as f:
			return cls.from_bytes(bytearray(f.read()), key = key)

	@classmethod
	def from_components(cls, payload, entry_point = None, load_address = None, length = None, version = None, salt = None, signature = None, data_hash = None, key = rsa_key_custom):
		data = payload.to_bytes()

		if not load_address:
			load_address = 0x40010000

		if not entry_point:
			entry_point = load_address
			# on erista bct, need to set entry point to after loader
			# if loader:
			# 	entry_point += len(loader)
		entry_point = struct.pack("<I", entry_point)
		load_address = struct.pack("<I", load_address)

		if not length:
			length = len(data)
		length = struct.pack("<I", length)

		if not version:
			version = 19
		version = struct.pack("<I", version)

		if not salt:
			salt = bytes.fromhex("F723F425B471F859FCF4D3DA0AF7633C38BEBD51EEB70CCA5428DED2FCE61769")
		else:
			salt = salt.to_bytes(0x20, byteorder = 'little')

		if data_hash:
			if len(data_hash) != 0x20:
				raise ValueError("Data hash must be a 64 byte SHA256 hash")
		else:
			data_hash = get_sha256(data).digest()

		if signature:
			if len(signature) != 0x100:
				raise ValueError("Signature must be a 256 byte RSASSA-PSS signature")
		else:
			data_to_sign = bytearray()
			data_to_sign += salt
			data_to_sign += data_hash
			data_to_sign += version
			data_to_sign += length
			data_to_sign += load_address
			data_to_sign += entry_point
			data_to_sign += bytearray(0x10)

			payload_hash = get_sha256(data_to_sign)

			signature = get_cipher_pss(key).sign(payload_hash)

		return cls(signature, salt, data_hash, version, length, load_address, entry_point, data, key = key)

	def to_bytes(self):
		ret = bytearray(0x10)
		ret += self.signature[::-1]
		ret += self.salt
		ret += self.data_hash
		ret += struct.pack("<I", self.version)
		ret += struct.pack("<I", self.length)
		ret += struct.pack("<I", self.load_address)
		ret += struct.pack("<I", self.entry_point)
		ret += bytearray(0x10)
		ret += self.data
		return ret

	def write_to_file(self, path_out, pad = None):
		with open(path_out, 'wb') as f:
			data = self.to_bytes()
			if pad:
				if len(data) % pad:
					data += bytearray(-len(data) % pad)
			f.write(data)

	def make_signature(self):
		h = get_sha256(self.get_data_to_sign())
		return get_cipher_pss(self.key).sign(h)

	def get_data_to_sign(self):
		data_to_sign = bytearray()
		data_to_sign += self.salt
		data_to_sign += self.data_hash
		data_to_sign += struct.pack("<I", self.version)
		data_to_sign += struct.pack("<I", self.length)
		data_to_sign += struct.pack("<I", self.load_address)
		data_to_sign += struct.pack("<I", self.entry_point)
		data_to_sign += bytearray(0x10)
		return data_to_sign

	def is_signature_valid(self):
		h = get_sha256(self.get_data_to_sign())
		try:
			get_cipher_pss(self.key).verify(h, self.signature)
		except ValueError as e:
			return False
		return True

	def is_hash_valid(self):
		calculated_data_hash = get_sha256(self.data[:self.length]).digest()
		return self.data_hash == calculated_data_hash

	def verify(self):
		errs = []
		if not self.is_hash_valid():
			errs += [ValueError("Invalid hash")]
		if not self.is_signature_valid():
			errs += [ValueError("Invalid signature")]
		if not self.length == len(self.data):
			errs += [ValueError("Invalid length")]
		if errs:
			raise ExceptionGroup("Package invalid", errs)
		else:
			return True

	def get_data_decrypt(self):
		return decrypt_bytes(self.data)

	def __str__(self):
		ret = ""
		ret += f"Version:               {self.version}\n"
		ret += f"Data Size:             0x{self.length:x}\n"
		ret += f"Entrypoint:            0x{self.entry_point:x}\n"
		ret += f"Load addr.:            0x{self.load_address:x}\n"
		ret += f"Salt:                  {self.salt.hex()}\n"
		ret += f"Data hash:             {self.data_hash.hex()}\n"
		prefix = "Signature:             "
		width = 64
		for i in range(0, len(self.signature.hex()), width):
			ret += f"{prefix if i == 0 else ' ' * len(prefix)}{self.signature.hex()[i:i+width]}\n"
		prefix = "Is valid:              "
		try:
			self.verify()
		except ExceptionGroup as eg:
			i = 0
			for e in eg.exceptions:
				ret += f"{prefix if i == 0 else ' ' * len(prefix)}{e}\n"
				i += 1
		else:
			ret += f"{prefix}{True}"
		return ret


class EristaBlEntry:
	def __init__(self, signature, version, start_block, length, load_address, entry_point, payload, attributes):
		self.version = version
		self.start_block = start_block
		self.length = length
		self.load_address = load_address
		self.entry_point = entry_point
		self.attributes = attributes
		self.payload = payload
		self.signature = signature
		pass

	@classmethod
	def from_file(cls, path, path_loader, version, start_block, load_address):
		if version is None:
			version = 9
		if start_block is None:
			start_block = 0xfc
		if load_address is None:
			load_address  = 0x40010000
		with open(path, "rb") as f:
			with open(path_loader, "rb") as fl:
				data = bytearray(f.read())
				data_ldr = bytearray(fl.read())
				h = get_sha256(data)
				signature = get_cipher_pss(rsa_key_custom).sign(h)
				return cls(signature, version, start_block, len(data), load_address, load_address + len(data_ldr) + 0x20, data, 0x3)

	@classmethod
	def from_bytes(cls, data, payload):
		version = int.from_bytes(data[0:0x4], byteorder = 'little', signed = False)
		start_block = int.from_bytes(data[0x4:0x8], byteorder = 'little', signed = False)
		length = int.from_bytes(data[0xc:0x10], byteorder = 'little', signed = False)
		load_address = int.from_bytes(data[0x10:0x14], byteorder = 'little', signed = False)
		entry_point = int.from_bytes(data[0x14:0x18], byteorder = 'little', signed = False)
		attributes = int.from_bytes(data[0x18:0x1c], byteorder = 'little', signed = False)
		signature = data[0x20:0x120][::-1]
		return cls(signature, version, start_block, length, load_address, entry_point, payload, attributes)

	def to_bytes(self):
		data = bytearray()
		data += struct.pack("<I", self.version)
		data += struct.pack("<I", self.start_block)
		data += struct.pack("<I", 0)
		data += struct.pack("<I", self.length)
		data += struct.pack("<I", self.load_address)
		data += struct.pack("<I", self.entry_point)
		data += struct.pack("<I", self.attributes)
		data += bytearray(0x10)
		data += self.signature[::-1]

		return data

	def is_signature_valid(self):
		h = get_sha256(self.payload)
		try:
			get_cipher_pss(rsa_key_custom).verify(h, self.signature)
		except ValueError as e:
			return False
		return True

	def verify(self):
		errs = []
		if not self.is_signature_valid():
			errs += [ValueError("Invalid signature")]
		if not self.length == len(self.payload):
			errs += [ValueError("Invalid length")]
		if errs:
			raise ExceptionGroup("Package invalid", errs)
		else:
			return True

	def __str__(self):
		ret = ""
		ret += f"Version:               {self.version}\n"
		ret += f"Data Size:             0x{self.length:x}\n"
		ret += f"Entrypoint:            0x{self.entry_point:x}\n"
		ret += f"Load addr.:            0x{self.load_address:x}\n"
		prefix = "Signature:             "
		width = 64
		for i in range(0, len(self.signature.hex()), width):
			ret += f"{prefix if i == 0 else ' ' * len(prefix)}{self.signature.hex()[i:i+width]}\n"
		prefix = "Is valid:              "
		try:
			self.verify()
		except ExceptionGroup as eg:
			i = 0
			for e in eg.exceptions:
				ret += f"{prefix if i == 0 else ' ' * len(prefix)}{e}\n"
				i += 1
		else:
			ret += f"{prefix}{True}"
		return ret

class EristaBct:
	def __init__(self, bl_entry):
		self.bl_entry = bl_entry
		self.loaders_used = 1

		self.entries_used = 0x200
		self.virtual_block_size_log2 = 0xf
		self.block_size_log2 = 0xe
		self.key = rsa_n_custom

		self.boot_data_version = 0x210001
		self.partition_size = 0x1000000
		self.num_param_sets = 0x1
		self.type = 0x4
		self.page_size_log2 = 0x9
		self.clock_divider = 0x9
		self.data_width = 0x2

		h = get_sha256(self.data_to_sign())
		self.signature = get_cipher_pss(rsa_key_custom).sign(h)

	def data_to_sign(self):
		data = bytearray()
		data += bytearray(0x10)
		data += bytearray(0x10)
		data += struct.pack("<I", self.boot_data_version)
		data += struct.pack("<I", self.block_size_log2)
		data += struct.pack("<I", self.page_size_log2)
		data += struct.pack("<I", self.partition_size)
		data += struct.pack("<I", self.num_param_sets)
		data += struct.pack("<I", self.type)
		data += struct.pack("<I", self.clock_divider)
		data += struct.pack("<I", self.data_width)
		data += bytearray(0x40 - 0x8)
		data += bytearray(0x4)
		data += bytearray(0x768)
		data += bytearray(0x768)
		data += bytearray(0x768)
		data += bytearray(0x768)
		data += struct.pack("<I", self.loaders_used)
		data += self.bl_entry.to_bytes()
		data += bytearray(0x12c)
		data += bytearray(0x12c)
		data += bytearray(0x12c)
		data += bytearray(1)
		data += bytearray(4)
		data += bytearray(4)
		data += struct.pack("<I", 0x80000000)
		data += bytearray(0x13)

		return data

	def to_bytes(self):
		data = bytearray()
		data += struct.pack("<I", self.entries_used)
		data += struct.pack("<B", self.virtual_block_size_log2)
		data += struct.pack("<B", self.block_size_log2)
		data += bytearray(0x200)
		data += bytearray(0xa)
		data += rsa_n_custom.to_bytes(length = 0x100, byteorder = "little", signed = False)
		data += bytearray(0x10)
		h = get_sha256(self.data_to_sign())
		data += get_cipher_pss(rsa_key_custom).sign(h)[::-1]

		data += bytearray(0x4)
		data += bytearray(0x20)
		data += bytearray(0xc4)
		data += bytearray(0x4)
		data += bytearray(0x4)

		data += self.data_to_sign()

		return data

	def write_to_file(self, path):
		with open(path, "wb") as f:
			f.write(self.to_bytes())


	def write_sig_to_file(self, path):
		with open(path, "wb") as f:
			data = self.to_bytes()
			f.write(data[0x320:0x420])

	def write_bl_entry_to_file(self, path):
		with open(path, "wb") as f:
			data = self.to_bytes()
			f.write(data[0x2330:0x245c])


	def is_signature_valid(self):
		h = get_sha256(self.data_to_sign())
		try:
			get_cipher_pss(rsa_key_custom).verify(h, self.signature)
		except ValueError as e:
			return False
		return True

	def verify(self):
		errs = []
		if not self.is_signature_valid():
			errs += [ValueError("Invalid signature")]
		if errs:
			raise ExceptionGroup("Package invalid", errs)
		else:
			return True

	def __str__(self):
		ret = ""
		prefix = "Signature:             "
		width = 64
		for i in range(0, len(self.signature.hex()), width):
			ret += f"{prefix if i == 0 else ' ' * len(prefix)}{self.signature.hex()[i:i+width]}\n"
		prefix = "Is valid:              "
		try:
			self.verify()
		except ExceptionGroup as eg:
			i = 0
			for e in eg.exceptions:
				ret += f"{prefix if i == 0 else ' ' * len(prefix)}{e}\n"
				i += 1
		else:
			ret += f"{prefix}{True}"

		ret += "\nBootloader:\n"
		ret += str(self.bl_entry)
		return ret

def make_package(args):
	pkg1 = Pkg1.from_file(args.payload, args.loader, args.is_encrypted)
	p = Payload.from_components(pkg1, entry_point = args.entry_point, load_address = args.load_addr, version = args.version)
	print(p)
	p.write_to_file(args.out_file)

def check_package(args):
	key = rsa_key_custom
	if(args.use_nintendo_pk):
		key = rsa_key_nintendo
	p = Payload.from_file(args.package, key = key)
	# p.verify()
	print(p)

def decrypt(args):
	with open(args.file_in, "rb") as fin:
		data = decrypt_bytes(bytearray(fin.read()))
		with open(args.file_out, "wb") as fout:
			fout.write(data)

def make_erista_bct(args):
	bl_entry = EristaBlEntry.from_file(args.payload_enc, args.loader_enc, args.version, args.start_block, args.load_addr)
	bct = EristaBct(bl_entry)
	print(bct)
	if args.out_file is not None:
		bct.write_to_file(args.out_file)
	if args.bl_entry_out_path is not None:
		bct.write_bl_entry_to_file(args.bl_entry_out_path)
	if args.sig_out_path is not None:
		bct.write_sig_to_file(args.sig_out_path)


def auto_int(x):
	return int(x, 0)

parser = argparse.ArgumentParser()
subparsers = parser.add_subparsers(required = True)
parser_make = subparsers.add_parser("make")

parser_make.add_argument("payload", type = str)
parser_make.add_argument("out_file", type = str)
parser_make.add_argument("--loader", type = str)
parser_make.add_argument("--is_encrypted", action = "store_true")
parser_make.add_argument("--load_addr", type = auto_int)
parser_make.add_argument("--entry_point", type = auto_int)
parser_make.add_argument("--version", type = auto_int)
parser_make.set_defaults(func = make_package)

parser_check = subparsers.add_parser("check")
parser_check.add_argument("package", type = str)
parser_check.add_argument("--use_nintendo_pk", action = "store_true")
parser_check.set_defaults(func = check_package)

parser_dec = subparsers.add_parser("decrypt")
parser_dec.add_argument("file_in", type = str)
parser_dec.add_argument("file_out", type = str)
parser_dec.set_defaults(func = decrypt)

parser_make_erista = subparsers.add_parser("make_erista_bct")
parser_make_erista.add_argument("payload_enc", type = str)
parser_make_erista.add_argument("loader_enc", type = str)
parser_make_erista.add_argument("--out_file", type = str)
parser_make_erista.add_argument("--load_addr", type = auto_int)
parser_make_erista.add_argument("--entry_point", type = auto_int)
parser_make_erista.add_argument("--version", type = auto_int)
parser_make_erista.add_argument("--start_block", type = auto_int)
parser_make_erista.add_argument("--sig_out_path", type = str)
parser_make_erista.add_argument("--bl_entry_out_path", type = str)
parser_make_erista.set_defaults(func = make_erista_bct)


args = parser.parse_args()
args.func(args)
