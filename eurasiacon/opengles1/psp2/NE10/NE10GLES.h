#ifndef _PSP2_NE10GLES_
#define _PSP2_NE10GLES_

#include "../../context.h"
#include "NE10_types.h"

int ne10_transmat_4x4f_neon(GLESMatrix *dst,
	GLESMatrix *src,
	unsigned int count
);


int ne10_invmat_4x4f_neon(GLESMatrix *dst,
	GLESMatrix *src,
	unsigned int 	count
);

int ne10_mulmat_4x4f_neon(GLESMatrix *dst,
	GLESMatrix *src1,
	GLESMatrix *src2,
	unsigned int 	count
);


int ne10_identitymat_4x4f_neon(GLESMatrix *dst,
	unsigned int 	count
);

int ne10_mulc_vec4f_neon(ne10_vec4f_t *dst,
	ne10_vec4f_t *src,
	const ne10_vec4f_t *cst,
	unsigned int 	count
);


int ne10_mulcmatvec_cm4x4f_v4f_neon(ne10_vec4f_t *dst,
	const GLESMatrix *cst,
	ne10_vec4f_t *src,
	unsigned int 	count
);

int ne10_setc_vec4f_neon(ne10_vec4f_t * dst, const ne10_vec4f_t * cst, ne10_uint32_t count);

#endif /* _PSP2_NE10GLES_ */
