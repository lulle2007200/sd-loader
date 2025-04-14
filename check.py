from Crypto.Cipher import AES
from Crypto.Signature import pss
from Crypto.Signature import pkcs1_15
from Crypto.Hash import SHA256
from Crypto.PublicKey import RSA
import struct
import argparse



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


args = parser.parse_args()
args.func(args)