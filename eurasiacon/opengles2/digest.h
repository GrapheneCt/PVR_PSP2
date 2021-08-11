/*************************************************************************/ /*!
@Title          Crypto functions for blob cache key generation
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _DIGEST_H_
#define _DIGEST_H_

#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH 32 /* 256bit / 8 */
#endif

#define DIGEST_STRING_LENGTH ((SHA256_DIGEST_LENGTH) * 2 + 1)

void DigestTextToHashString(const IMG_CHAR *szString,
							IMG_CHAR szHashStr[DIGEST_STRING_LENGTH]);

#endif /* _DIGEST_H_ */
