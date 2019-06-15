#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define SWAP(x) ((x >> 24) & 0xff     | (x >> 8)  & 0xff00 | \
				 (x << 8)  & 0xff0000 | (x << 24) & 0xff000000)

void decompressType0x50(uint8_t *out, uint8_t *in, uint32_t len)
{
	uint32_t pos = 8;

	uint8_t *end  = in + len;
	uint8_t *data = in + pos;

	if (pos < len)
	{
		uint32_t flag;

		while (1)
		{
			flag = 0xFF000000 * data[0];

			if (flag)
				break;

			data++;

			for (int i = 0; i < 8; i++)
				*out++ = *data++;

CheckFinished:
			if (data >= end)
				goto EndOperation;
		}

		flag |= 0x800000;

		data++;

		while (!(flag & 0x80000000))
		{
IterateFlag:
			flag <<= 1;
			*out++ = *data++;
		}

		while (1)
		{
			flag <<= 1;

			if (!flag)
				goto CheckFinished;

			int32_t op_ofs = (data[0] >> 4) | (data[1] << 4);
			int32_t op_len = data[0] & 0xF;

			if (!op_ofs)
				return;

			uint8_t *chunk = out - op_ofs;

			if (op_len > 1)
				data += 2;
			else
			{
				int32_t op_len_ext = data[2] + (op_len | 0x10);

				if (op_len == 1)
				{
					int32_t add_len = (data[3] << 8) | 0xFF;

					data += 4;

					op_len = op_len_ext + add_len;

					if (op_ofs >= 2)
						goto Loop1;
				}
				else
				{
					data += 3;

					op_len = op_len_ext;

					if (op_ofs >= 2)
					{
Loop1:
						if (!(((uint8_t)chunk ^ (uint8_t)out) & 1))
						{
							if ((uint8_t)chunk & 1)
							{
								*out++ = *chunk++;
								op_len--;
							}

							uint32_t op_len_sub = op_len - 2;

							if (op_len >= 2)
							{
								int32_t masked_len = ((op_len_sub >> 1) + 1) & 7;

								uint8_t *out_ptr = out;
								uint8_t *chunk_ptr = chunk;

								if (masked_len)
								{
									while (masked_len--)
									{
										*out_ptr++ = *chunk_ptr++;
										*out_ptr++ = *chunk_ptr++;
										op_len -= 2;
									}
								}

								uint32_t masked_ext_len = op_len_sub & 0xFFFFFFFE;

								if (op_len_sub >= 0xE)
								{
									do
									{
										for (int i = 0; i < 0x10; i++)
											*out_ptr++ = *chunk_ptr++;

										op_len -= 0x10;
									}
									while (op_len > 1);
								}

								out += masked_ext_len + 2;
								op_len = op_len_sub - masked_ext_len;
								chunk += masked_ext_len + 2;
							}

							if (!op_len)
								goto CheckFlag;
						}

						goto Loop2;
					}
				}
			}
Loop2:;
			int32_t masked_len = op_len & 7;

			uint8_t *out_ptr = out;
			uint8_t *chunk_ptr = chunk;

			if (masked_len)
			{
				while (masked_len--)
					*out_ptr++ = *chunk_ptr++;
			}

			if (op_len - 1 >= 7)
			{
				do
				{
					for (int i = 0; i < 8; i++)
						*out_ptr++ = *chunk_ptr++;
				}
				while (chunk_ptr != chunk + op_len);
			}

			out += op_len;

CheckFlag:
			if (!(flag & 0x80000000))
				goto IterateFlag;
		}
	}

EndOperation:;
	uint8_t *ext = end + 0x20;

	if (data < ext)
		do
			*end-- = *--ext;
		while (data < ext);
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("Usage: %s infile outfile\n", argv[0]);
		return 1;
	}

	FILE *in = fopen(argv[1], "rb");
	FILE *out = fopen(argv[2], "wb");

	fseek(in, 0, SEEK_END);
	size_t isz = ftell(in);
	rewind(in);

	char *inbuf = malloc(isz);

	fread(inbuf, 1, isz, in);

	int32_t osz = SWAP(*(int32_t *)&inbuf[4]);

	char *outbuf = malloc(osz);

	decompressType0x50(outbuf, inbuf, osz);

	fwrite(outbuf, 1, osz, out);

	free(inbuf);
	free(outbuf);

	fclose(in);
	fclose(out);

	return 0;
}