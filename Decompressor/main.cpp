// © Maxim "Kailo" Telezhenko, 2018

#include <iostream>
#include <fstream>
#include <smx/smx-headers.h>
#include <zlib.h>

using namespace std;
using namespace sp;

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		cerr << "You must to pass file for (de)compress.\nUsing: Decompressor.exe <smx file>" << endl;
		return 0;
	}

	fstream ifs(argv[1], fstream::in | fstream::binary);
	if (ifs.fail())
	{
		cerr << "Can't open file (\"" << argv[1] << "\") to read." << endl;
		return 0;
	}

	ifs.seekg(0, fstream::end);
	size_t length = (long int)ifs.tellg();
	ifs.seekg(0, fstream::beg);
	char *buffer = new char[length];
	ifs.read(buffer, length);
	ifs.close();
	
	if (length < sizeof(sp_file_hdr_t))
	{
		cerr << "Not smx file: bad header" << endl;
		delete[] buffer;
		return 0;
	}

	sp_file_hdr_t *header = (sp_file_hdr_t *)buffer;
	if (header->magic != SmxConsts::FILE_MAGIC)
	{
		cerr << "Not smx file: wrong magic" << endl;
		delete[] buffer;
		return 0;
	}

	if (header->disksize != length)
	{
		cerr << "Illegal disk size" << endl;
		delete[] buffer;
		return 0;
	}

	if (header->dataoffs > length)
	{
		cerr << "Illegal data region: bigger than length" << endl;
		delete[] buffer;
		return 0;
	}

	if (header->dataoffs < sizeof(sp_file_hdr_t))
	{
		cerr << "Illegal data region: smaller than file header" << endl;
		delete[] buffer;
		return 0;
	}

	if (header->imagesize < header->dataoffs)
	{
		cerr << "Illegal image size" << endl;
		delete[] buffer;
		return 0;
	}

	switch (header->compression)
	{
		case SmxConsts::FILE_COMPRESSION_NONE:
		{
			if (header->disksize != header->imagesize)
			{
				cerr << "Illegal image size: no compression but not equal disksize" << endl;
				delete[] buffer;
				return 0;
			}

			size_t uncompressedSize = header->disksize - header->dataoffs;
			const uint8_t *src = (uint8_t *)buffer + header->dataoffs;
			uLong destlen = compressBound((uLong)uncompressedSize);
			char *compressed = new char[destlen];
			uint8_t *dest = (uint8_t *)compressed;
			

			int r = compress2(dest, &destlen, src, (uLong)uncompressedSize, Z_BEST_COMPRESSION);
			if (r != Z_OK)
			{
				cerr << "Unable to compress, error " << r << endl;
				delete[] compressed;
				delete[] buffer;
				return 0;
			}

			header->disksize = header->dataoffs + destlen;
			header->compression = SmxConsts::FILE_COMPRESSION_GZ;
			fstream ofs("compressed.smx", fstream::out | fstream::binary);
			ofs.write(buffer, header->dataoffs);
			ofs.write(compressed, destlen);
			ofs.close();

			delete[] compressed;
			break;
		}
		case SmxConsts::FILE_COMPRESSION_GZ:
		{
			size_t compressedSize = header->disksize - header->dataoffs;
			const uint8_t *src = (uint8_t *)buffer + header->dataoffs;
			uLongf destlen = header->imagesize - header->dataoffs;
			char *uncompressed = new char[destlen];
			uint8_t *dest = (uint8_t *)uncompressed;

			int r = uncompress(dest, &destlen, src, (uLong)compressedSize);
			if (r != Z_OK)
			{
				cerr << "Could not decode compressed region: " << r << endl;
				delete[] uncompressed;
				delete[] buffer;
				return 0;
			}

			header->disksize = header->dataoffs + destlen;
			if (header->disksize != header->imagesize)
				clog << "Imagesize not equal disksize after uncopression" << endl;
			header->compression = SmxConsts::FILE_COMPRESSION_NONE;
			fstream ofs("decompressed.smx", fstream::out | fstream::binary);
			ofs.write(buffer, header->dataoffs);
			ofs.write(uncompressed, destlen);
			ofs.close();

			delete[] uncompressed;
			break;
		}
		default:
			cerr << "Unknown compression type" << endl;
	}

	delete[] buffer;
	return 0;
}