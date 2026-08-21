// ngmark.c -- <INVENTED FILENAME>. The ngWorldStats verification mark: the
// running MD5 over every log line, the per-player identifier and the
// transmitted mark.

#include "g_local.h"


// gamex86.dll: 1004DCE0..1004DCF2
// gamei386.so: 0006EF58..0006EF78
void ngLog_initMark (void)
{
	MD5Init (&context);
}

/*
==============
ngLog_inputLine

Fold one line into the running checksum, then scramble it in place.
==============
*/
// gamex86.dll: 1004DCF2..1004DD52
// gamei386.so: 0006EF78..0006EFC9
void ngLog_inputLine (char *line)
{
	int		len;
	int		i;

	len = strlen (line);
	MD5Update (&context, line, len);
	for (i = 0; i < len; i++)
		line[i] = line[i] ^ 0xa7;
}

/*
==============
ngLog_giveMark

Close out the running checksum and write it as 32 hex digits.
==============
*/
// gamex86.dll: 1004DD52..1004DE1E
// gamei386.so: 0006EFCC..0006F086
void ngLog_giveMark (char *out)
{
	unsigned char	mark[16];
	char			salt[2048];
	char			hexchars[1024];
	unsigned int	i;

	ngLog_transMark (salt, (int *)&i);
	MD5Update (&context, salt, i);
	MD5Final (mark, &context);

	salt[0] = '\0';
	for (i = 0; i < 16; i++)
	{
		sprintf (hexchars, "%02x", mark[i]);
		strcat (salt, hexchars);
	}
	strcpy (out, salt);
}

/*
==============
ngLog_playerIdentifier

An MD5 of two strings plus the salt, as hex. Its own context, not the running
one, so it does not disturb the log mark.
==============
*/
// gamex86.dll: 1004DE1E..1004DEFF
// gamei386.so: 0006F088..0006F1AB
char *ngLog_playerIdentifier (char *a, char *b)
{
	// 2048, not 256: real's .bss puts the next TU's first static exactly 0x800
	// bytes further on.  Nothing in either audit can see a static buffer's
	// SIZE -- the ELF masks the [ebx+-disp] that reaches it.
	static char		ident[2048];
	MD5_CTX			md5ctx;
	unsigned char	digest[16];
	char			hexchars[1024];
	int				i;

	MD5Init (&md5ctx);
	MD5Update (&md5ctx, a, strlen (a));
	MD5Update (&md5ctx, b, strlen (b));
	ngLog_transMark (ident, &i);
	MD5Update (&md5ctx, ident, i);
	MD5Final (digest, &md5ctx);

	ident[0] = '\0';
	for (i = 0; i < 16; i++)
	{
		sprintf (hexchars, "%02x", digest[i]);
		strcat (ident, hexchars);
	}
	return ident;
}

/*
==============
ngLog_transMark

Decode the 0x21-int salt table out of .rodata, one byte at a time, and wipe the
plaintext copy afterwards so it is never resident.
==============
*/
// gamex86.dll: 1004DEFF..1004E113
// gamei386.so: 0006F1AC..0006F257
void ngLog_transMark (char *out, int *count)
{
	char		scratch[16];
	char		buf[128];
	// "ngUS@ ngL0G Kw@ke2 1mplem3ntati0n" ^ 0xa9, 33 ints.
	int			salttab[33] = {
		0xc7, 0xce, 0xfc, 0xfa, 0xe9, 0x89, 0xc7, 0xce, 0xe5, 0x99, 0xee,
		0x89, 0xe2, 0xde, 0xe9, 0xc2, 0xcc, 0x9b, 0x89, 0x98, 0xc4, 0xd9,
		0xc5, 0xcc, 0xc4, 0x9a, 0xc7, 0xdd, 0xc8, 0xdd, 0xc0, 0x99, 0xc7 };
	int			i;

	buf[0] = '\0';
	*count = 33;

	for (i = 0; i < *count; i++)
	{
		sprintf (scratch, "%c", salttab[i] ^ 0xa9);
		strcat (buf, scratch);
	}
	strcpy (out, buf);

	for (i = 0; i < *count; i++)
		buf[i] = '\0';
}
