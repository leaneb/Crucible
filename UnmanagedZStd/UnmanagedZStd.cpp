#include "stdafx.h"

#include "UnmanagedZStd.h"

#include <string.h>
#include <zstd.h>

#define __min(a,b) (a < b ? a : b)

array<Byte>^ UnmanagedZStd::ZStd::StreamingUncompress(array<Byte>^ managed_data, size_t uncompressed_size)
{
	pin_ptr<Byte> data_pinned_ptr = &managed_data[0];
	Byte* data = data_pinned_ptr;

	array<Byte>^ result = gcnew array<Byte>((int)uncompressed_size);
	pin_ptr<Byte> result_pinned_ptr = &result[0];
	Byte* output_buffer = result_pinned_ptr;

	size_t const buffInSize = ZSTD_DStreamInSize();
	size_t const buffOutSize = ZSTD_DStreamOutSize();  /* Guarantee to successfully flush at least one complete compressed block in all circumstances. */

	ZSTD_DStream* const dstream = ZSTD_createDStream();
	if (dstream == NULL) {
		throw gcnew Exception("ZSTD_createDStream() error");
	}

	/* In more complex scenarios, a file may consist of multiple appended frames (ex : pzstd).
	*  The following example decompresses only the first frame.
	*  It is compatible with other provided streaming examples */
	size_t const initResult = ZSTD_initDStream(dstream);
	if (ZSTD_isError(initResult)) {
		throw gcnew Exception(String::Format("ZSTD_initDStream() error : {0}", gcnew String(ZSTD_getErrorName(initResult))));
	}

	size_t current_input_position = 0;

	ZSTD_outBuffer zstd_output = { 0 };
	zstd_output.dst = output_buffer;
	zstd_output.size = (size_t)result->LongLength;

	auto max_packet_size = initResult;
	auto packet_buffer = new char[initResult];

	while (current_input_position < (size_t)managed_data->LongLength)
	{
		size_t packet_size = __min(managed_data->LongLength - current_input_position, max_packet_size);
		memset(packet_buffer, 0, max_packet_size);
		memcpy(packet_buffer, data + current_input_position, packet_size);

		ZSTD_inBuffer zstd_input = { packet_buffer, packet_size, 0 };
		auto toRead = ZSTD_decompressStream(dstream, &zstd_output, &zstd_input);  /* toRead : size of next compressed block */

		if (toRead == 0 || zstd_output.pos >= uncompressed_size)
		{
			break;
		}

		if (ZSTD_isError(toRead))
		{
			throw gcnew Exception(String::Format("ZSTD_decompressStream() error : {0}", gcnew String(ZSTD_getErrorName(toRead))));
		}

		current_input_position += max_packet_size;
	}

	ZSTD_freeDStream(dstream);
	delete[] packet_buffer;

	return result;
}
