/******************************************************************************
 Name           : HWINST.C

 Title          : HW instruction encode/decode

 C Author       : Joe Molleson

 Created        : 02/01/2002

 Copyright      : 2002-2009 by Imagination Technologies Limited.
                : All rights reserved. No part of this software, either
                : material or conceptual may be copied or distributed,
                : transmitted, transcribed, stored in a retrieval system or
                : translated into any human or computer language in any form
                : by any means, electronic, mechanical, manual or otherwise,
                : or disclosed to third parties without the express written
                : permission of Imagination Technologies Limited,
                : Home Park Estate, Kings Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description    : Utilities to decode and encode USSE HW instructions

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.99 $

 Modifications  :

 $Log: hwinst.c $
******************************************************************************/
#include "img_types.h"
#include "sgxdefs.h"
#include "usp_typedefs.h"
#include "usp.h"
#include "hwinst.h"
#include "uspshrd.h"

#include <stdio.h>

/*****************************************************************************
 Name:		HWInstGetOpcode

 Purpose:	Decode the opcode for a HW instruction

 Inputs:	psHWInst - The HW instruction to examine

 Outputs:	peOpcode - An enumerated type representing the HW operation type

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstGetOpcode(PHW_INST psHWInst, PUSP_OPCODE peOpcode)
{
	IMG_UINT32	uInstClass;

	uInstClass = (psHWInst->uWord1 & ~EURASIA_USE1_OP_CLRMSK) >>
				 EURASIA_USE1_OP_SHIFT;

	switch (uInstClass)
	{
		
		#if defined(SGX545)
		case SGX545_USE1_OP_FDUAL:
		{
			*peOpcode = (psHWInst->uWord1 & ~SGX545_USE1_FDUAL_OP1_CLRMSK) >> SGX545_USE1_FDUAL_OP1_SHIFT;
			*peOpcode = USP_OPCODE_DUAL;
			return IMG_TRUE;
			break;
		}
		#endif /* defined(SGX545) */

		#if defined(SGX_FEATURE_USE_VEC34)
		case SGXVEC_USE1_OP_VECMAD:
		{
			*peOpcode = USP_OPCODE_VMAD;
			return IMG_TRUE;
			break;
		}

		case SGXVEC_USE1_OP_VECNONMADF32:
		case SGXVEC_USE1_OP_VECNONMADF16:
		{
			IMG_UINT32	uOpcode = 
				(psHWInst->uWord0 & ~SGXVEC_USE0_VECNONMAD_OP2_CLRMSK) >> SGXVEC_USE0_VECNONMAD_OP2_SHIFT;

			switch(uOpcode)
			{
				case SGXVEC_USE0_VECNONMAD_OP2_VMUL:
				{
					*peOpcode = USP_OPCODE_VMUL;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE0_VECNONMAD_OP2_VADD:
				{
					*peOpcode = USP_OPCODE_VADD;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE0_VECNONMAD_OP2_VFRC:
				{
					*peOpcode = USP_OPCODE_VFRC;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE0_VECNONMAD_OP2_VDSX:
				{
					*peOpcode = USP_OPCODE_VDSX;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE0_VECNONMAD_OP2_VDSY:
				{
					*peOpcode = USP_OPCODE_VDSY;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE0_VECNONMAD_OP2_VMIN:
				{
					*peOpcode = USP_OPCODE_VMIN;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE0_VECNONMAD_OP2_VMAX:
				{
					*peOpcode = USP_OPCODE_VMAX;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE0_VECNONMAD_OP2_VDP:
				{
					*peOpcode = USP_OPCODE_VDP;
					return IMG_TRUE;
					break;
				}
				
				default :
				{
					return IMG_FALSE;
				}
			}

			break;
		}
		
		case SGXVEC_USE1_OP_SVEC:
		{
			IMG_UINT32	uOpcode = 
				(psHWInst->uWord0 & ~SGXVEC_USE1_SVEC_OP2_CLRMSK) >> SGXVEC_USE1_SVEC_OP2_SHIFT;
			
			switch(uOpcode)
			{
				case SGXVEC_USE1_SVEC_OP2_DP3:
				{
					*peOpcode = USP_OPCODE_VDP3;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE1_SVEC_OP2_DP4:
				{
					*peOpcode = USP_OPCODE_VDP4;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE1_SVEC_OP2_VMAD3:
				{
					*peOpcode = USP_OPCODE_VMAD3;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE1_SVEC_OP2_VMAD4:
				{
					*peOpcode = USP_OPCODE_VMAD4;
					return IMG_TRUE;
					break;
				}

				default:
				{
					return IMG_FALSE;
					break;
				}
			}

			break;
		}

		case SGXVEC_USE1_OP_DVEC3:
		case SGXVEC_USE1_OP_DVEC4:
		{
			
			*peOpcode = USP_OPCODE_VDUAL;
			return IMG_TRUE;
			break;
		}

		case SGXVEC_USE1_OP_VECCOMPLEX:
		{
			IMG_UINT32	uOpcode = 
				(psHWInst->uWord0 & ~SGXVEC_USE1_VECCOMPLEXOP_OP2_CLRMSK) >> SGXVEC_USE1_VECCOMPLEXOP_OP2_SHIFT;

			switch(uOpcode)
			{
				case SGXVEC_USE1_VECCOMPLEXOP_OP2_RCP:
				{
					*peOpcode = USP_OPCODE_VRCP;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE1_VECCOMPLEXOP_OP2_RSQ:
				{
					*peOpcode = USP_OPCODE_VRSQ;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE1_VECCOMPLEXOP_OP2_LOG:
				{
					*peOpcode = USP_OPCODE_VLOG;
					return IMG_TRUE;
					break;
				}

				case SGXVEC_USE1_VECCOMPLEXOP_OP2_EXP:
				{
					*peOpcode = USP_OPCODE_VEXP;
					return IMG_TRUE;
					break;
				}

				default:
				{
					return IMG_FALSE;
					break;
				}

			}

			break;
		}
		
		case SGXVEC_USE1_OP_VECMOV:
		{
			*peOpcode = USP_OPCODE_VMOV;
			return IMG_TRUE;
			break;
		}

		#else

		case EURASIA_USE1_OP_FARITH:
		{
			IMG_UINT32	uOpcode;

			uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_FLOAT_OP2_CLRMSK) >>
					  EURASIA_USE1_FLOAT_OP2_SHIFT;

			switch (uOpcode)
			{
				case EURASIA_USE1_FLOAT_OP2_MAD:
				{
					*peOpcode = USP_OPCODE_MAD;
					return IMG_TRUE;
				}
				case EURASIA_USE1_FLOAT_OP2_ADM:
				{
					*peOpcode = USP_OPCODE_ADM;
					return IMG_TRUE;
				}
				case EURASIA_USE1_FLOAT_OP2_MSA:
				{
					*peOpcode = USP_OPCODE_MSA;
					return IMG_TRUE;
				}
				case EURASIA_USE1_FLOAT_OP2_FRC:
				{
					*peOpcode =  USP_OPCODE_FRC;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			break;
		}

		case EURASIA_USE1_OP_FSCALAR:
		{
			IMG_UINT32	uOpcode;

			uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_FLOAT_OP2_CLRMSK) >>
					  EURASIA_USE1_FLOAT_OP2_SHIFT;

			switch (uOpcode)
			{
				case EURASIA_USE1_FLOAT_OP2_RCP:
				{
					*peOpcode =  USP_OPCODE_RCP;
					return IMG_TRUE;
				}
				case EURASIA_USE1_FLOAT_OP2_RSQ:
				{
					*peOpcode =  USP_OPCODE_RSQ;
					return IMG_TRUE;
				}
				case EURASIA_USE1_FLOAT_OP2_LOG:
				{
					*peOpcode =  USP_OPCODE_LOG;
					return IMG_TRUE;
				}
				case EURASIA_USE1_FLOAT_OP2_EXP:
				{
					*peOpcode =  USP_OPCODE_EXP;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			break;
		}

		case EURASIA_USE1_OP_FDOTPRODUCT:
		{
			IMG_UINT32	uOpcode;

			uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_FLOAT_OP2_CLRMSK) >>
					  EURASIA_USE1_FLOAT_OP2_SHIFT;

			switch (uOpcode)
			{
				case EURASIA_USE1_FLOAT_OP2_DP:
				{
					*peOpcode =  USP_OPCODE_DP;
					return IMG_TRUE;
				}
				case EURASIA_USE1_FLOAT_OP2_DDP:
				{
					*peOpcode =  USP_OPCODE_DDP;
					return IMG_TRUE;
				}
				case EURASIA_USE1_FLOAT_OP2_DDPC:
				{
					*peOpcode =  USP_OPCODE_DDPC;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			break;
		}

		case EURASIA_USE1_OP_FMINMAX:
		{
			IMG_UINT32	uOpcode;

			uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_FLOAT_OP2_CLRMSK) >>
					  EURASIA_USE1_FLOAT_OP2_SHIFT;

			switch (uOpcode)
			{
				case EURASIA_USE1_FLOAT_OP2_MIN:
				{
					*peOpcode =  USP_OPCODE_MIN;
					return IMG_TRUE;
				}
				case EURASIA_USE1_FLOAT_OP2_MAX:
				{
					*peOpcode =  USP_OPCODE_MAX;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			break;
		}

		case EURASIA_USE1_OP_FGRADIENT:
		{
			IMG_UINT32	uOpcode;

			uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_FLOAT_OP2_CLRMSK) >>
					  EURASIA_USE1_FLOAT_OP2_SHIFT;

			switch (uOpcode)
			{
				case EURASIA_USE1_FLOAT_OP2_DSX:
				{
					*peOpcode =  USP_OPCODE_DSX;
					return IMG_TRUE;
				}
				case EURASIA_USE1_FLOAT_OP2_DSY:
				{
					*peOpcode =  USP_OPCODE_DSY;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			break;
		}

		case EURASIA_USE1_OP_MOVC:
		{
			*peOpcode =  USP_OPCODE_MOVC;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_FARITH16:
		{
			#if defined(SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES)
			{
				/* on SGX545 this is always FMAD16 and OP2 is for swizzles */
				*peOpcode =  USP_OPCODE_FMAD16;
				return IMG_TRUE;
			}
			#else
			{
				IMG_UINT32	uOpcode;

				uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_FLOAT_OP2_CLRMSK) >>
						  EURASIA_USE1_FLOAT_OP2_SHIFT;

				switch (uOpcode)
				{
					case EURASIA_USE1_FLOAT_OP2_FMAD16:
					{
						*peOpcode =  USP_OPCODE_FMAD16;
						return IMG_TRUE;
					}
					default:
					{
						break;
					}
				}

				break;
			}
			#endif /* defined(SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES) */
		}

		case EURASIA_USE1_OP_EFO:
		{
			*peOpcode =  USP_OPCODE_EFO;
			return IMG_TRUE;
		}
		#endif /* defined(SGX_FEATURE_USE_VEC34) */

		case EURASIA_USE1_OP_PCKUNPCK:
		{
			*peOpcode =  USP_OPCODE_PCKUNPCK;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_TEST:
		{
			*peOpcode =  USP_OPCODE_TEST;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_ANDOR:
		{
			IMG_UINT32	uOpcode;

			uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_BITWISE_OP2_CLRMSK) >>
					  EURASIA_USE1_BITWISE_OP2_SHIFT;

			switch (uOpcode)
			{
				case EURASIA_USE1_BITWISE_OP2_AND:
				{
					*peOpcode =  USP_OPCODE_AND;
					return IMG_TRUE;
				}
				case EURASIA_USE1_BITWISE_OP2_OR:
				{
					*peOpcode =  USP_OPCODE_AND;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			break;
		}

		case EURASIA_USE1_OP_XOR:
		{
			*peOpcode =  USP_OPCODE_XOR;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_SHLROL:
		{
			IMG_UINT32	uOpcode;

			uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_BITWISE_OP2_CLRMSK) >>
					  EURASIA_USE1_BITWISE_OP2_SHIFT;

			switch (uOpcode)
			{
				case EURASIA_USE1_BITWISE_OP2_SHL:
				{
					*peOpcode =  USP_OPCODE_SHL;
					return IMG_TRUE;
				}
				case EURASIA_USE1_BITWISE_OP2_ROL:
				{
					*peOpcode =  USP_OPCODE_ROL;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			break;
		}

		case EURASIA_USE1_OP_SHRASR:
		{
			IMG_UINT32	uOpcode;

			uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_BITWISE_OP2_CLRMSK) >>
					  EURASIA_USE1_BITWISE_OP2_SHIFT;

			switch (uOpcode)
			{
				case EURASIA_USE1_BITWISE_OP2_SHR:
				{
					*peOpcode =  USP_OPCODE_SHR;
					return IMG_TRUE;
				}
				case EURASIA_USE1_BITWISE_OP2_ASR:
				{
					*peOpcode =  USP_OPCODE_ASR;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			break;
		}

		case EURASIA_USE1_OP_RLP:
		{
			*peOpcode =  USP_OPCODE_RLP;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_TESTMASK:
		{
			*peOpcode =  USP_OPCODE_TESTMASK;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_SOP2:
		{
			*peOpcode =  USP_OPCODE_SOP2;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_SOP3:
		{
			*peOpcode =  USP_OPCODE_SOP3;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_SOPWM:
		{
			*peOpcode =  USP_OPCODE_SOPWM;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_IMA8:
		{
			*peOpcode =  USP_OPCODE_IMA8;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_IMA16:
		{
			*peOpcode =  USP_OPCODE_IMA16;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_IMAE:
		{
			*peOpcode =  USP_OPCODE_IMAE;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_ADIFFIRVBILIN:
		{
			IMG_UINT32	uOpcode;

			uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_ADIFFIRVBILIN_OPSEL_CLRMSK) >>
					  EURASIA_USE1_ADIFFIRVBILIN_OPSEL_SHIFT;

			switch (uOpcode)
			{
				case EURASIA_USE1_ADIFFIRVBILIN_OPSEL_ADIF:
				{
					*peOpcode =  USP_OPCODE_ADIF;
					return IMG_TRUE;
				}
				case EURASIA_USE1_ADIFFIRVBILIN_OPSEL_BILIN:
				{
					*peOpcode =  USP_OPCODE_BILIN;
					return IMG_TRUE;
				}
				case EURASIA_USE1_ADIFFIRVBILIN_OPSEL_FIRV:
				{
					*peOpcode =  USP_OPCODE_FIRV;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			break;
		}

		case EURASIA_USE1_OP_FIRH:
		{
			*peOpcode =  USP_OPCODE_FIRH;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_DOT3DOT4:
		{
			IMG_UINT32	uOpcode;

			uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_DOT34_OPSEL_CLRMSK) >>
					  EURASIA_USE1_DOT34_OPSEL_SHIFT;

			switch (uOpcode)
			{
				case EURASIA_USE1_DOT34_OPSEL_DOT3:
				{
					*peOpcode =  USP_OPCODE_DOT3;
					return IMG_TRUE;
				}
				case EURASIA_USE1_DOT34_OPSEL_DOT4:
				{
					*peOpcode =  USP_OPCODE_DOT4;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			break;
		}

		case EURASIA_USE1_OP_FPMA:
		{
			*peOpcode =  USP_OPCODE_FPMA;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_SMP:
		{
			IMG_UINT32	uVariant;

			uVariant = (psHWInst->uWord1 & ~EURASIA_USE1_SMP_LODM_CLRMSK) >>
					   EURASIA_USE1_SMP_LODM_SHIFT;

			switch (uVariant)
			{
				case EURASIA_USE1_SMP_LODM_NONE:
				{
					*peOpcode =  USP_OPCODE_SMP;
					return IMG_TRUE;
				}
				case EURASIA_USE1_SMP_LODM_BIAS:
				{
					*peOpcode =  USP_OPCODE_SMPBIAS;
					return IMG_TRUE;
				}
				case EURASIA_USE1_SMP_LODM_REPLACE:
				{
					*peOpcode =  USP_OPCODE_SMPREPLACE;
					return IMG_TRUE;
				}
				case EURASIA_USE1_SMP_LODM_GRADIENTS:
				{
					*peOpcode =  USP_OPCODE_SMPGRAD;
					return IMG_TRUE;
				}
				default:
				{
					break;
				}
			}

			*peOpcode =  USP_OPCODE_SMP;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_LD:
		{
			*peOpcode =  USP_OPCODE_LD;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_ST:
		{
			*peOpcode =  USP_OPCODE_ST;
			return IMG_TRUE;
		}

		case EURASIA_USE1_OP_SPECIAL:
		{
			IMG_UINT32	uOpCat;

			uOpCat = (psHWInst->uWord1 & ~EURASIA_USE1_SPECIAL_OPCAT_CLRMSK) >>
					 EURASIA_USE1_SPECIAL_OPCAT_SHIFT;

			switch (uOpCat)
			{
				case EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL:
				{
					if (psHWInst->uWord1 & EURASIA_USE1_SPECIAL_OPCAT_EXTRA)
					{
						IMG_UINT32	uOp2;

						uOp2 = (psHWInst->uWord1 & ~EURASIA_USE1_OTHER2_OP2_CLRMSK) >> EURASIA_USE1_OTHER2_OP2_SHIFT;

						switch (uOp2)
						{
							#if defined(SGX_FEATURE_USE_IDXSC)
							case SGXVEC_USE1_OTHER2_OP2_IDXSCR:
							{
								*peOpcode =  USP_OPCODE_IDXSCR;
								return IMG_TRUE;
							}
							case SGXVEC_USE1_OTHER2_OP2_IDXSCW:
							{
								*peOpcode =  USP_OPCODE_IDXSCW;
								return IMG_TRUE;
							}
							#endif /* defined(SGX_FEATURE_USE_IDXSC) */
							default:
							{
								break;
							}
						}
					}
					else
					{
						IMG_UINT32	uOperand;

						uOperand = (psHWInst->uWord1 & ~EURASIA_USE1_FLOWCTRL_OP2_CLRMSK) >>
								   EURASIA_USE1_FLOWCTRL_OP2_SHIFT;

						switch (uOperand)
						{
							case EURASIA_USE1_FLOWCTRL_OP2_BA:
							{
								*peOpcode =  USP_OPCODE_BA;
								return IMG_TRUE;
							}
							case EURASIA_USE1_FLOWCTRL_OP2_BR:
							{
								*peOpcode =  USP_OPCODE_BR;
								return IMG_TRUE;
							}
							case EURASIA_USE1_FLOWCTRL_OP2_LAPC:
							{
								*peOpcode =  USP_OPCODE_LAPC;
								return IMG_TRUE;
							}
							case EURASIA_USE1_FLOWCTRL_OP2_SETL:
							{
								*peOpcode =  USP_OPCODE_SETL;
								return IMG_TRUE;
							}
							case EURASIA_USE1_FLOWCTRL_OP2_SAVL:
							{
								*peOpcode =  USP_OPCODE_SAVL;
								return IMG_TRUE;
							}
							case EURASIA_USE1_FLOWCTRL_OP2_NOP:
							{
								*peOpcode =  USP_OPCODE_NOP;
								return IMG_TRUE;
							}
							default:
							{	
								break;
							}
						}
					}

					break;
				}

				case EURASIA_USE1_SPECIAL_OPCAT_MOECTRL:
				{
					IMG_UINT32	uOpcode;

					uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_MOECTRL_OP2_CLRMSK) >>
							  EURASIA_USE1_MOECTRL_OP2_SHIFT;

					switch (uOpcode)
					{
						case EURASIA_USE1_MOECTRL_OP2_SMOA:
						{
							*peOpcode =  USP_OPCODE_SMOA;
							return IMG_TRUE;
						}
						case EURASIA_USE1_MOECTRL_OP2_SMR:
						{
							*peOpcode =  USP_OPCODE_SMR;
							return IMG_TRUE;
						}
						case EURASIA_USE1_MOECTRL_OP2_SMLSI:
						{
							*peOpcode =  USP_OPCODE_SMLSI;
							return IMG_TRUE;
						}
						case EURASIA_USE1_MOECTRL_OP2_SMBO:
						{
							*peOpcode =  USP_OPCODE_SMBO;
							return IMG_TRUE;
						}
						case EURASIA_USE1_MOECTRL_OP2_IMO:
						{
							*peOpcode =  USP_OPCODE_IMO;
							return IMG_TRUE;
						}
						case EURASIA_USE1_MOECTRL_OP2_SETFC:
						{
							*peOpcode =  USP_OPCODE_SETFC;
							return IMG_TRUE;
						}
						default:
						{
							break;
						}
					}

					break;
				}

				case EURASIA_USE1_SPECIAL_OPCAT_OTHER:
				{
					IMG_UINT32	uOpcode;

					uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_OTHER_OP2_CLRMSK) >>
							  EURASIA_USE1_OTHER_OP2_SHIFT;

					switch (uOpcode)
					{
						case EURASIA_USE1_OTHER_OP2_IDF:
						{
							*peOpcode =  USP_OPCODE_IDF;
							return IMG_TRUE;
						}
						case EURASIA_USE1_OTHER_OP2_WDF:
						{
							*peOpcode =  USP_OPCODE_WDF;
							return IMG_TRUE;
						}
						case SGX545_USE1_OTHER_OP2_SETM:
						{
							*peOpcode =  USP_OPCODE_SETM;
							return IMG_TRUE;
						}
						case EURASIA_USE1_OTHER_OP2_EMIT:
						{
							*peOpcode =  USP_OPCODE_EMIT;
							return IMG_TRUE;
						}
						case EURASIA_USE1_OTHER_OP2_LIMM:
						{
							*peOpcode =  USP_OPCODE_LIMM;
							return IMG_TRUE;
						}
						case EURASIA_USE1_OTHER_OP2_LOCKRELEASE:
						{
							if	(psHWInst->uWord0 & EURASIA_USE0_LOCKRELEASE_ACTION_RELEASE)
							{
								*peOpcode =  USP_OPCODE_RELEASE;
								return IMG_TRUE;
							}
							else
							{
								*peOpcode =  USP_OPCODE_LOCK;
								return IMG_TRUE;
							}
						}
						case EURASIA_USE1_OTHER_OP2_LDRSTR:
						{
							if	(psHWInst->uWord1 & EURASIA_USE1_LDRSTR_DSEL_STORE)
							{
								*peOpcode =  USP_OPCODE_STR;
								return IMG_TRUE;
							}
							else
							{
								*peOpcode =  USP_OPCODE_LDR;
								return IMG_TRUE;
							}
						}
						case EURASIA_USE1_OTHER_OP2_WOP:
						{
							*peOpcode =  USP_OPCODE_WOP;
							return IMG_TRUE;
						}
						default:
						{
							break;
						}
					}

					break;
				}

				case EURASIA_USE1_SPECIAL_OPCAT_VISTEST:
				{
					IMG_UINT32	uOpcode;

					uOpcode = (psHWInst->uWord1 & ~EURASIA_USE1_OTHER_OP2_CLRMSK) >>
							  EURASIA_USE1_OTHER_OP2_SHIFT;

					switch (uOpcode)
					{
						case EURASIA_USE1_VISTEST_OP2_PTCTRL:
						{
							IMG_UINT32	uPTCtrlType;

							uPTCtrlType	= (psHWInst->uWord1 & ~EURASIA_USE1_VISTEST_PTCTRL_TYPE_CLRMSK) >>
										  EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT;

							switch (uPTCtrlType)
							{
								case EURASIA_USE1_VISTEST_PTCTRL_TYPE_PLANE:
								{
									*peOpcode =  USP_OPCODE_PCOEFF;
									return IMG_TRUE;
								}
								case EURASIA_USE1_VISTEST_PTCTRL_TYPE_PTOFF:
								{
									*peOpcode =  USP_OPCODE_PTOFF;
									return IMG_TRUE;
								}
								default:
								{
									break;
								}
							}
						}
						case EURASIA_USE1_VISTEST_OP2_ATST8:
						{
							*peOpcode =  USP_OPCODE_ATST8;
							return IMG_TRUE;
						}
						case EURASIA_USE1_VISTEST_OP2_DEPTHF:
						{
							*peOpcode =  USP_OPCODE_DEPTHF;
							return IMG_TRUE;
						}
						default:
						{
							break;
						}
					}

					break;
				}

				default:
				{
					break;
				}
			}

			break;
		}

		#if defined(SGX_FEATURE_USE_NORMALISE)
		case SGX540_USE1_OP_FNRM:
		{
			*peOpcode = USP_OPCODE_FNRM;
			return IMG_TRUE;
		}
		#endif /* #ifdef SGX_FEATURE_USE_NORMALISE */

		#if defined(SGX_FEATURE_USE_32BIT_INT_MAD)
		case EURASIA_USE1_OP_32BITINT:
		{
			*peOpcode = USP_OPCODE_IMA32;
			return IMG_TRUE;
		}
		#endif /* defined(SGX_FEATURE_USE_32BIT_INT_MAD) */

		default:
		{
			break;
		}
	}

	return IMG_FALSE;
}

#if !defined(SGX_FEATURE_USE_VEC34)
/*****************************************************************************
 Name:		HWInstGetOperandsUsedEFO

 Purpose:	Determine which operands will be used by an EFO HW instruction

 Inputs:	psHWInst		- The HW EFO instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
static IMG_BOOL HWInstGetOperandsUsedEFO(PHW_INST		psHWInst,
										 IMG_PUINT32	puOperandsUsed)
{
	IMG_UINT32	uDstSrc;
	IMG_UINT32	uISrc;
	IMG_UINT32	uASrc;
	IMG_UINT32	uMSrc;
	IMG_UINT32	uM0OperandsUsed;
	IMG_UINT32	uM1OperandsUsed;
	IMG_UINT32	uA0OperandsUsed;
	IMG_UINT32	uA1OperandsUsed;
	IMG_UINT32	uOperandsUsed;

	/*
		Extract fields that control which data will be used by the
		instruction
	*/
	uDstSrc = (psHWInst->uWord1 & ~EURASIA_USE1_EFO_DSRC_CLRMSK) >>
			  EURASIA_USE1_EFO_DSRC_SHIFT;
	uISrc	= (psHWInst->uWord1 & ~EURASIA_USE1_EFO_ISRC_CLRMSK) >>
			  EURASIA_USE1_EFO_ISRC_SHIFT;
	uASrc	= (psHWInst->uWord1 & ~EURASIA_USE1_EFO_ASRC_CLRMSK) >>
			  EURASIA_USE1_EFO_ASRC_SHIFT;
	uMSrc	= (psHWInst->uWord1 & ~EURASIA_USE1_EFO_MSRC_CLRMSK) >>
			  EURASIA_USE1_EFO_MSRC_SHIFT;

	/*
		Determine which operands will be used by the multipliers
	*/
	switch (uMSrc)
	{
		case EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC2:
		{
			uM0OperandsUsed = USP_OPERANDFLAG_SRC0 |
							  USP_OPERANDFLAG_SRC1;
			uM1OperandsUsed = USP_OPERANDFLAG_SRC0 |
							  USP_OPERANDFLAG_SRC2;
			break;
		}
		case EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC0:
		{
			uM0OperandsUsed = USP_OPERANDFLAG_SRC0 |
							  USP_OPERANDFLAG_SRC1;
			uM1OperandsUsed = USP_OPERANDFLAG_SRC0;
			break;
		}
		case EURASIA_USE1_EFO_MSRC_SRC1SRC2_SRC0SRC0:
		{
			uM0OperandsUsed = USP_OPERANDFLAG_SRC1 |
							  USP_OPERANDFLAG_SRC2;
			uM1OperandsUsed = USP_OPERANDFLAG_SRC0;
			break;
		}
		case EURASIA_USE1_EFO_MSRC_SRC1I0_SRC0I1:
		{
			uM0OperandsUsed = 0;
			uM1OperandsUsed = 0;
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}

	/*
		Determine which operands will be used by the adders
	*/
	switch (uASrc)
	{
		case EURASIA_USE1_EFO_ASRC_M0M1_I1I0:
		{
			uA0OperandsUsed = uM0OperandsUsed | uM1OperandsUsed;
			uA1OperandsUsed = 0;
			break;
		}
		case EURASIA_USE1_EFO_ASRC_M0SRC2_I1I0:
		{
			uA0OperandsUsed = uM0OperandsUsed | 
							  USP_OPERANDFLAG_SRC2;
			uA1OperandsUsed = 0;
			break;
		}
		case EURASIA_USE1_EFO_ASRC_M0I0_I1M1:
		{
			uA0OperandsUsed = uM0OperandsUsed;
			uA1OperandsUsed = uM1OperandsUsed;
			break;
		}
		case EURASIA_USE1_EFO_ASRC_SRC0SRC1_SRC2SRC0:
		{
			uA0OperandsUsed = USP_OPERANDFLAG_SRC0 | USP_OPERANDFLAG_SRC1;
			uA1OperandsUsed = USP_OPERANDFLAG_SRC2 | USP_OPERANDFLAG_SRC0;
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}

	/*
		Determine which operands will be used overall
	*/
	uOperandsUsed = 0;

	switch (uDstSrc)
	{
		case EURASIA_USE1_EFO_DSRC_I0:
		case EURASIA_USE1_EFO_DSRC_I1:
		{
			break;
		}

		case EURASIA_USE1_EFO_DSRC_A0:
		{
			uOperandsUsed |= uA0OperandsUsed;
			break;
		}

		case EURASIA_USE1_EFO_DSRC_A1:
		{
			uOperandsUsed |= uA1OperandsUsed;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	if	(psHWInst->uWord1 & EURASIA_USE1_EFO_WI0EN)
	{
		switch (uISrc)
		{
			case EURASIA_USE1_EFO_ISRC_I0A0_I1A1:
			{
				uOperandsUsed |= uA0OperandsUsed;
				break;
			}
			case EURASIA_USE1_EFO_ISRC_I0A1_I1A0:
			{
				uOperandsUsed |= uA1OperandsUsed;
				break;
			}
			case EURASIA_USE1_EFO_ISRC_I0M0_I1M1:
			{
				uOperandsUsed |= uM0OperandsUsed;
				break;
			}
			case EURASIA_USE1_EFO_ISRC_I0A0_I1M1:
			{
				uOperandsUsed |= uA0OperandsUsed;
				break;
			}
			default:
			{
				return IMG_FALSE;
			}
		}
	}

	if	(psHWInst->uWord1 & EURASIA_USE1_EFO_WI1EN)
	{
		switch (uISrc)
		{
			case EURASIA_USE1_EFO_ISRC_I0A0_I1A1:
			{
				uOperandsUsed |= uA1OperandsUsed;
				break;
			}
			case EURASIA_USE1_EFO_ISRC_I0A1_I1A0:
			{
				uOperandsUsed |= uA0OperandsUsed;
				break;
			}
			case EURASIA_USE1_EFO_ISRC_I0M0_I1M1:
			{
				uOperandsUsed |= uM1OperandsUsed;
				break;
			}
			case EURASIA_USE1_EFO_ISRC_I0A0_I1M1:
			{
				uOperandsUsed |= uM1OperandsUsed;
				break;
			}
			default:
			{
				return IMG_FALSE;
			}
		}
	}

	*puOperandsUsed = uOperandsUsed;
	return IMG_TRUE;
}
#endif /* !defined(SGX_FEATURE_USE_VEC34) */

/*****************************************************************************
 Name:		HWInstGetOperandsUsedPCKUNPCK

 Purpose:	Determine which operands will be used by a PCKUNPCK HW instruction

 Inputs:	psHWInst		- The HW PCKUNPCK instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
static IMG_BOOL HWInstGetOperandsUsedPCKUNPCK(PHW_INST		psHWInst,
											  IMG_PUINT32	puOperandsUsed)
{
	IMG_UINT32	uDstFmt;
	IMG_UINT32	uDstMask;
	IMG_UINT32	uOperandsUsed;

	/*
		Extract fields controlling the instruction behaviour
	*/
	uDstFmt		= (psHWInst->uWord1 & ~EURASIA_USE1_PCK_DSTF_CLRMSK) >>
				  EURASIA_USE1_PCK_DSTF_SHIFT;
	uDstMask	= (psHWInst->uWord1 & ~EURASIA_USE1_PCK_DMASK_CLRMSK) >>
				  EURASIA_USE1_PCK_DMASK_SHIFT;

	/*
		Determine which operands will be used
	*/
	uOperandsUsed = 0;

	if	(uDstMask != 0)
	{
		uOperandsUsed |= USP_OPERANDFLAG_DST;

		switch (uDstFmt)
		{
			case EURASIA_USE1_PCK_FMT_U8:
			case EURASIA_USE1_PCK_FMT_S8:
			case EURASIA_USE1_PCK_FMT_O8:
			case EURASIA_USE1_PCK_FMT_C10:
			{
				IMG_UINT32	uOperandIdx;
				IMG_UINT32	uDstComp;

				/*
					When packing to 10/8-bits, each channel written
					will alternate between sources 1 and 2
				*/
				uOperandIdx = USP_OPERANDIDX_SRC1;
				for	(uDstComp = 0; uDstComp < 4; uDstComp++)
				{
					if	(uDstMask & (0x1 << uDstComp))
					{
						uOperandsUsed |= 1 << uOperandIdx;

						if	(uOperandIdx == USP_OPERANDFLAG_SRC1)
						{
							uOperandIdx = USP_OPERANDFLAG_SRC2;
						}
						else
						{
							uOperandIdx = USP_OPERANDFLAG_SRC1;
						}
					}
				}

				break;
			}

			case EURASIA_USE1_PCK_FMT_U16:
			case EURASIA_USE1_PCK_FMT_S16:
			case EURASIA_USE1_PCK_FMT_F16:
			{
				IMG_UINT32	uOperandIdx;
				IMG_UINT32	uDstComp;

				/*
					When packing to 16-bits, each channel written
					will alternate between sources 1 and 2
				*/
				uOperandIdx = USP_OPERANDIDX_SRC1;
				for	(uDstComp = 0; uDstComp < 4; uDstComp += 2)
				{
					if	(uDstMask & (0x3 << uDstComp))
					{
						uOperandsUsed |= 1 << uOperandIdx;

						if	(uOperandIdx == USP_OPERANDFLAG_SRC1)
						{
							uOperandIdx = USP_OPERANDFLAG_SRC2;
						}
						else
						{
							uOperandIdx = USP_OPERANDFLAG_SRC1;
						}
					}
				}

				break;
			}

			case EURASIA_USE1_PCK_FMT_F32:
			{
				/*
					Only source 1 used when unpacking to 32-bit data
				*/
				uOperandsUsed |= USP_OPERANDFLAG_SRC1;
			}

			default:
			{
				return IMG_FALSE;
			}
		}
	}

	*puOperandsUsed = uOperandsUsed;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstGetOperandsUsedTEST

 Purpose:	Determine which operands will be used by a TEST/TESTMASK HW
			instruction

 Inputs:	psHWInst		- The HW TEST/TESTMASK instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
static IMG_BOOL HWInstGetOperandsUsedTEST(PHW_INST		psHWInst,
										  IMG_PUINT32	puOperandsUsed)
{
	IMG_UINT32	uOperandsUsed;
	IMG_UINT32	uALUSel;
	IMG_UINT32	uALUOp;

	/*
		Determine the sources used according to the ALU selected ALU
		operation
	*/
	uALUSel	= (psHWInst->uWord0 & ~EURASIA_USE0_TEST_ALUSEL_CLRMSK) >>
			  EURASIA_USE0_TEST_ALUSEL_SHIFT;
	uALUOp	= (psHWInst->uWord0 & ~EURASIA_USE0_TEST_ALUOP_CLRMSK) >>
			  EURASIA_USE0_TEST_ALUOP_SHIFT;

	uOperandsUsed = 0;

	switch (uALUSel)
	{
		case EURASIA_USE0_TEST_ALUSEL_F32:
		{
			switch (uALUOp)
			{
				case EURASIA_USE0_TEST_ALUOP_F32_ADD:
				case EURASIA_USE0_TEST_ALUOP_F32_FRC:
				case EURASIA_USE0_TEST_ALUOP_F32_DP:
				case EURASIA_USE0_TEST_ALUOP_F32_MIN:
				case EURASIA_USE0_TEST_ALUOP_F32_MAX:
				case EURASIA_USE0_TEST_ALUOP_F32_DSX:
				case EURASIA_USE0_TEST_ALUOP_F32_DSY:
				case EURASIA_USE0_TEST_ALUOP_F32_MUL:
				case EURASIA_USE0_TEST_ALUOP_F32_SUB:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC1 |
									 USP_OPERANDFLAG_SRC2;
					break;
				}

				case EURASIA_USE0_TEST_ALUOP_F32_RCP:
				case EURASIA_USE0_TEST_ALUOP_F32_RSQ:
				case EURASIA_USE0_TEST_ALUOP_F32_LOG:
				case EURASIA_USE0_TEST_ALUOP_F32_EXP:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC1;
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			break;
		}

		case EURASIA_USE0_TEST_ALUSEL_I16:
		{
			switch (uALUOp)
			{
				case EURASIA_USE0_TEST_ALUOP_I16_IADD:
				case EURASIA_USE0_TEST_ALUOP_I16_ISUB:
				case EURASIA_USE0_TEST_ALUOP_I16_IMUL:
				case EURASIA_USE0_TEST_ALUOP_I16_IADDU:
				case EURASIA_USE0_TEST_ALUOP_I16_ISUBU:
				case EURASIA_USE0_TEST_ALUOP_I16_IMULU:
				case EURASIA_USE0_TEST_ALUOP_I16_IADD32:
				case EURASIA_USE0_TEST_ALUOP_I16_IADDU32:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC1 |
									 USP_OPERANDFLAG_SRC2;
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			break;
		}

		case EURASIA_USE0_TEST_ALUSEL_I8:
		{
			switch (uALUOp)
			{
				case EURASIA_USE0_TEST_ALUOP_I8_ADD:
				case EURASIA_USE0_TEST_ALUOP_I8_SUB:
				case EURASIA_USE0_TEST_ALUOP_I8_ADDU:
				case EURASIA_USE0_TEST_ALUOP_I8_SUBU:
				case EURASIA_USE0_TEST_ALUOP_I8_MUL:
				case EURASIA_USE0_TEST_ALUOP_I8_FPMUL:
				case EURASIA_USE0_TEST_ALUOP_I8_MULU:
				case EURASIA_USE0_TEST_ALUOP_I8_FPADD:
				case EURASIA_USE0_TEST_ALUOP_I8_FPSUB:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC1 |
									 USP_OPERANDFLAG_SRC2;
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			break;
		}

		case EURASIA_USE0_TEST_ALUSEL_BITWISE:
		{
			switch (uALUOp)
			{
				case EURASIA_USE0_TEST_ALUOP_BITWISE_AND:
				case EURASIA_USE0_TEST_ALUOP_BITWISE_OR:
				case EURASIA_USE0_TEST_ALUOP_BITWISE_XOR:
				case EURASIA_USE0_TEST_ALUOP_BITWISE_SHL:
				case EURASIA_USE0_TEST_ALUOP_BITWISE_SHR:
				case EURASIA_USE0_TEST_ALUOP_BITWISE_ROL:
				case EURASIA_USE0_TEST_ALUOP_BITWISE_ASR:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC1 |
									 USP_OPERANDFLAG_SRC2;
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	/*
		Destination is used if the test instruction writes-back the
		result of the specified ALU op
	*/
	if	(psHWInst->uWord0 & EURASIA_USE0_TEST_WBEN)
	{
		uOperandsUsed |= USP_OPERANDFLAG_DST;
	}

	*puOperandsUsed = uOperandsUsed;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstGetOperandsUsedSOP2

 Purpose:	Determine which operands will be used by a SOP2 HW instruction

 Inputs:	psHWInst		- The HW SOP2 instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
static IMG_BOOL HWInstGetOperandsUsedSOP2(PHW_INST		psHWInst,
										  IMG_PUINT32	puOperandsUsed)
{
	IMG_UINT32	uCSel1;
	IMG_UINT32	uCSel2;
	IMG_UINT32	uASel1;
	IMG_UINT32	uASel2;
	IMG_UINT32	uOperandsUsed;

	/*
		Extract fields defining the instruction's behaviour
	*/
	uCSel1	= (psHWInst->uWord1 & ~EURASIA_USE1_SOP2_CSEL1_CLRMSK) >>
			  EURASIA_USE1_SOP2_CSEL1_SHIFT;
	uCSel2	= (psHWInst->uWord1 & ~EURASIA_USE1_SOP2_CSEL2_CLRMSK) >>
			  EURASIA_USE1_SOP2_CSEL2_SHIFT;
	uASel1	= (psHWInst->uWord1 & ~EURASIA_USE1_SOP2_ASEL1_CLRMSK) >>
			  EURASIA_USE1_SOP2_ASEL1_SHIFT;
	uASel2	= (psHWInst->uWord1 & ~EURASIA_USE1_SOP2_ASEL2_CLRMSK) >>
			  EURASIA_USE1_SOP2_ASEL2_SHIFT;

	/*
		Destination always used. Determine which sources are used
		according to the chosen colour and alpha operation.
	*/
	uOperandsUsed = USP_OPERANDFLAG_DST;

	switch (uCSel1)
	{
		case EURASIA_USE1_SOP2_CSEL1_ZERO:
		{
			IMG_UINT32	uC1Mod;

			uC1Mod = (psHWInst->uWord1 & ~EURASIA_USE1_SOP2_CMOD1_CLRMSK) >>
					 EURASIA_USE1_SOP2_CMOD1_SHIFT;
			
			switch	(uC1Mod)
			{
				case EURASIA_USE1_SOP2_CMOD1_NONE:
				{
					break;
				}

				case EURASIA_USE1_SOP2_CMOD1_COMPLEMENT:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC1;
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			break;
		}

		case EURASIA_USE1_SOP2_CSEL1_SRC1:
		case EURASIA_USE1_SOP2_CSEL1_SRC1ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC1;
			break;
		}

		case EURASIA_USE1_SOP2_CSEL1_SRC2:
		case EURASIA_USE1_SOP2_CSEL1_SRC2ALPHA:
		case EURASIA_USE1_SOP2_CSEL1_SRC2DESTX2:
		case EURASIA_USE1_SOP2_CSEL1_MINSRC1A1MSRC2A:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC1 |
							 USP_OPERANDFLAG_SRC2;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	switch (uCSel2)
	{
		case EURASIA_USE1_SOP2_CSEL2_ZERO:
		case EURASIA_USE1_SOP2_CSEL2_ZEROSRC2MINUSHALF:
		{
			IMG_UINT32	uC2Mod;

			uC2Mod = (psHWInst->uWord1 & ~EURASIA_USE1_SOP2_CMOD2_CLRMSK) >>
					 EURASIA_USE1_SOP2_CMOD2_SHIFT;
			
			switch	(uC2Mod)
			{
				case EURASIA_USE1_SOP2_CMOD2_NONE:
				{
					break;
				}

				case EURASIA_USE1_SOP2_CMOD2_COMPLEMENT:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC2;
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			break;
		}

		case EURASIA_USE1_SOP2_CSEL2_SRC1:
		case EURASIA_USE1_SOP2_CSEL2_SRC1ALPHA:
		case EURASIA_USE1_SOP2_CSEL2_MINSRC1A1MSRC2A:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC1 |
							 USP_OPERANDFLAG_SRC2;
			break;
		}

		case EURASIA_USE1_SOP2_CSEL2_SRC2:
		case EURASIA_USE1_SOP2_CSEL2_SRC2ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC2;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	switch (uASel1)
	{
		case EURASIA_USE1_SOP2_ASEL1_ZERO:
		{
			IMG_UINT32	uAMod1;

			uAMod1 = (psHWInst->uWord1 & ~EURASIA_USE1_SOP2_AMOD1_CLRMSK) >>
					 EURASIA_USE1_SOP2_AMOD1_SHIFT;

			switch (uAMod1)
			{
				case EURASIA_USE1_SOP2_AMOD1_NONE:
				{
					break;
				}

				case EURASIA_USE1_SOP2_AMOD1_COMPLEMENT:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC1;
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			break;
		}

		case EURASIA_USE1_SOP2_ASEL1_SRC1ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC1;
			break;
		}

		case EURASIA_USE1_SOP2_ASEL1_SRC2ALPHA:
		case EURASIA_USE1_SOP2_ASEL1_SRC2ALPHAX2:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC1 |
							 USP_OPERANDFLAG_SRC2;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	switch (uASel2)
	{
		case EURASIA_USE1_SOP2_ASEL2_ZERO:
		case EURASIA_USE1_SOP2_ASEL2_ZEROSRC2MINUSHALF:
		{
			IMG_UINT32	uAMod2;

			uAMod2 = (psHWInst->uWord1 & ~EURASIA_USE1_SOP2_AMOD2_CLRMSK) >>
					 EURASIA_USE1_SOP2_AMOD2_SHIFT;

			switch (uAMod2)
			{
				case EURASIA_USE1_SOP2_AMOD2_NONE:
				{
					break;
				}

				case EURASIA_USE1_SOP2_AMOD2_COMPLEMENT:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC2;
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			break;
		}

		case EURASIA_USE1_SOP2_ASEL2_SRC1ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC1 |
							 USP_OPERANDFLAG_SRC2;
			break;
		}

		case EURASIA_USE1_SOP2_ASEL2_SRC2ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC2;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	*puOperandsUsed = uOperandsUsed;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstGetOperandsUsedSOPWM

 Purpose:	Determine which operands will be used by a SOPWM HW instruction

 Inputs:	psHWInst		- The HW SOPWM instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
static IMG_BOOL HWInstGetOperandsUsedSOPWM(PHW_INST		psHWInst,
										   IMG_PUINT32	puOperandsUsed)
{
	IMG_UINT32	uSel1;
	IMG_UINT32	uSel2;
	IMG_UINT32	uDstMask;
	IMG_UINT32	uOperandsUsed;

	/*
		Extract fields effecting which operands may be used
	*/
	uSel1		= (psHWInst->uWord1 & ~EURASIA_USE1_SOP2WM_SEL1_CLRMSK) >>
				  EURASIA_USE1_SOP2WM_SEL1_SHIFT;
	uSel2		= (psHWInst->uWord1 & ~EURASIA_USE1_SOP2WM_SEL2_CLRMSK) >>
				  EURASIA_USE1_SOP2WM_SEL2_SHIFT;
	uDstMask	= (psHWInst->uWord1 & ~EURASIA_USE1_SOP2WM_WRITEMASK_CLRMSK) >>
				  EURASIA_USE1_SOP2WM_WRITEMASK_SHIFT;

	/*
		Determine the operands used, based upon the selected instruction
		behaviour
	*/
	uOperandsUsed = 0;

	if	(uDstMask != 0)
	{
		uOperandsUsed |= USP_OPERANDFLAG_DST;
	}

	switch (uSel1)
	{
		case EURASIA_USE1_SOP2WM_SEL1_ZERO:
		{
			IMG_UINT32	uMod1;

			uMod1 = (psHWInst->uWord1 & ~EURASIA_USE1_SOP2WM_MOD1_CLRMSK) >>
					EURASIA_USE1_SOP2WM_MOD1_SHIFT;

			switch (uMod1)
			{
				case EURASIA_USE1_SOP2WM_MOD1_NONE:
				{
					break;
				}

				case EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC1;
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}
					
			break;
		}

		case EURASIA_USE1_SOP2WM_SEL1_SRC1:
		case EURASIA_USE1_SOP2WM_SEL1_SRC1ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC1;
			break;
		}

		case EURASIA_USE1_SOP2WM_SEL1_MINSRC1A1MSRC2A:
		case EURASIA_USE1_SOP2WM_SEL1_SRC2:
		case EURASIA_USE1_SOP2WM_SEL1_SRC2ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC1 |
							 USP_OPERANDFLAG_SRC2;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	switch (uSel2)
	{
		case EURASIA_USE1_SOP2WM_SEL2_ZERO:
		{
			IMG_UINT32	uMod2;

			uMod2 = (psHWInst->uWord1 & ~EURASIA_USE1_SOP2WM_MOD2_CLRMSK) >>
					EURASIA_USE1_SOP2WM_MOD2_SHIFT;

			switch (uMod2)
			{
				case EURASIA_USE1_SOP2WM_MOD2_NONE:
				{
					break;
				}

				case EURASIA_USE1_SOP2WM_MOD2_COMPLEMENT:
				{
					uOperandsUsed |= USP_OPERANDFLAG_SRC2;
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}
					
			break;
		}

		case EURASIA_USE1_SOP2WM_SEL2_SRC2:
		case EURASIA_USE1_SOP2WM_SEL2_SRC2ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC2;
			break;
		}

		case EURASIA_USE1_SOP2WM_SEL2_MINSRC1A1MSRC2A:
		case EURASIA_USE1_SOP2WM_SEL2_SRC1:
		case EURASIA_USE1_SOP2WM_SEL2_SRC1ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC1 |
							 USP_OPERANDFLAG_SRC2;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	*puOperandsUsed = uOperandsUsed;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstGetOperandsUsedIMA8

 Purpose:	Determine which operands will be used by a IMA8 HW instruction

 Inputs:	psHWInst		- The HW IMA8 instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
static IMG_BOOL HWInstGetOperandsUsedIMA8(PHW_INST		psHWInst,
										  IMG_PUINT32	puOperandsUsed)
{
	IMG_UINT32	uCSel0;
	IMG_UINT32	uASel0;
	IMG_UINT32	uOperandsUsed;

	/*
		Extract fields that may effect the operands used
	*/
	uCSel0	= (psHWInst->uWord1 & ~EURASIA_USE1_IMA8_CSEL0_CLRMSK) >>
			  EURASIA_USE1_IMA8_CSEL0_SHIFT;
	uASel0	= (psHWInst->uWord1 & ~EURASIA_USE1_IMA8_ASEL0_CLRMSK) >>
			  EURASIA_USE1_IMA8_ASEL0_SHIFT;

	/*
		Determine the operands used, according to the specified instruction
		behaviour.

		NB: Only source0 may be optionally used, based upon CSel0 and ASel0
	*/
	uOperandsUsed = USP_OPERANDFLAG_DST |
					USP_OPERANDFLAG_SRC1 |
					USP_OPERANDFLAG_SRC2;

	switch (uCSel0)
	{
		case EURASIA_USE1_IMA8_CSEL0_SRC0:
		case EURASIA_USE1_IMA8_CSEL0_SRC0ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC0;
			break;
		}

		case EURASIA_USE1_IMA8_CSEL0_SRC1:
		case EURASIA_USE1_IMA8_CSEL0_SRC1ALPHA:
		{
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	switch (uASel0)
	{
		case EURASIA_USE1_IMA8_ASEL0_SRC0ALPHA:
		{
			uOperandsUsed |= USP_OPERANDFLAG_SRC0;
			break;
		}

		case EURASIA_USE1_IMA8_ASEL0_SRC1ALPHA:
		{
			break;
		}
		
		default:
		{
			return IMG_FALSE;
		}
	}

	*puOperandsUsed = uOperandsUsed;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstGetOperandsUsedADIF

 Purpose:	Determine which operands will be used by a ADIF HW instruction

 Inputs:	psHWInst		- The HW ADIF instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
static IMG_BOOL HWInstGetOperandsUsedADIF(PHW_INST		psHWInst,
										  IMG_PUINT32	puOperandsUsed)
{
	IMG_UINT32	uOperandsUsed;

	/*
		Destination and sources 1 and 2 are always used. Source 0 is only
		used if a summation is required.
	*/
	uOperandsUsed = USP_OPERANDFLAG_DST |
					USP_OPERANDFLAG_SRC1 |
					USP_OPERANDFLAG_SRC2;

	if	(psHWInst->uWord1 & EURASIA_USE1_ADIF_SUM)
	{
		uOperandsUsed |= USP_OPERANDFLAG_SRC0;
	}

	*puOperandsUsed = uOperandsUsed;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstGetOperandsUsedDOT3

 Purpose:	Determine which operands will be used by a DOT3 HW instruction

 Inputs:	psHWInst		- The HW DOT3 instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
static IMG_BOOL HWInstGetOperandsUsedDOT3(PHW_INST		psHWInst,
										  IMG_PUINT32	puOperandsUsed)
{
	IMG_UINT32	uASel1;
	IMG_UINT32	uOperandsUsed;

	/*
		Destination, source4 1 and Source 2 are always used (to calculate
		the colour channel result)
	*/
	uOperandsUsed = USP_OPERANDFLAG_DST | 
					USP_OPERANDFLAG_SRC1 |
					USP_OPERANDFLAG_SRC2;

	/*
		Determine whether source 0 will be used to calculate the alpha result
	*/
	uASel1	= (psHWInst->uWord1 & ~EURASIA_USE1_DOT34_ASEL1_CLRMSK) >>
			  EURASIA_USE1_DOT34_ASEL1_SHIFT;

	switch (uASel1)
	{
		case EURASIA_USE1_DOT34_ASEL1_ZERO:
		case EURASIA_USE1_DOT34_ASEL1_SRC1ALPHA:
		case EURASIA_USE1_DOT34_ASEL1_SRC2ALPHA:
		{
			IMG_UINT32	uASel2;

			uASel2	= (psHWInst->uWord1 & ~EURASIA_USE1_DOT34_ASEL2_CLRMSK) >>
					  EURASIA_USE1_DOT34_ASEL2_SHIFT;

			switch (uASel2)
			{
				case EURASIA_USE1_DOT34_ASEL2_ZERO:
				case EURASIA_USE1_DOT34_ASEL2_SRC1ALPHA:
				case EURASIA_USE1_DOT34_ASEL2_SRC2ALPHA:
				{
					IMG_UINT32	uAOp;

					uAOp	= (psHWInst->uWord1 & ~EURASIA_USE1_DOT34_AOP_CLRMSK) >>
							  EURASIA_USE1_DOT34_AOP_SHIFT;

					switch (uAOp)
					{
						case EURASIA_USE1_DOT34_AOP_ADD:
						case EURASIA_USE1_DOT34_AOP_SUBTRACT:
						{
							/*
								Source 0 not selectably by ASel1 or ASel2
							*/
							break;
						}

						case EURASIA_USE1_DOT34_AOP_INTERPOLATE1:
						case EURASIA_USE1_DOT34_AOP_INTERPOLATE2:
						{
							/*
								Source0 always used for interpolate modes
							*/
							uOperandsUsed |= USP_OPERANDFLAG_SRC0;
							break;
						}
						
						default:
						{
							return IMG_FALSE;
						}
					}

					break;
				}

				case EURASIA_USE1_DOT34_ASEL2_FOURWAY:
				{
					/*
						8-bit dot-product result written into all 4 channels
						of the destination. So only source 1 and 2 used.
					*/
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			break;
		}

		case EURASIA_USE1_DOT34_ASEL1_ONEWAY:
		{
			/*
				16-bit Dot-product result written into low and high halves
				of the destination register. So only Source 1 and 2 used.
			*/
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	*puOperandsUsed = uOperandsUsed;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstGetOperandsUsedLD

 Purpose:	Determine which operands will be used by a LD HW instruction

 Inputs:	psHWInst		- The HW LD instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
static IMG_BOOL HWInstGetOperandsUsedLD(PHW_INST	psHWInst,
										IMG_PUINT32	puOperandsUsed)
{
	IMG_UINT32	uOperandsUsed;

	/*
		Destination and sources 0 and 1 always used. Source 2 is only used
		if range checking is enabled.
	*/
	uOperandsUsed = USP_OPERANDFLAG_DST |
					USP_OPERANDFLAG_SRC0 |
					USP_OPERANDFLAG_SRC1;

	if	(psHWInst->uWord1 & EURASIA_USE1_LDST_RANGEENABLE)
	{
		uOperandsUsed |= USP_OPERANDFLAG_SRC2;	
	}

	*puOperandsUsed = uOperandsUsed;
	return IMG_TRUE;
}

#if defined(SGX_FEATURE_USE_NORMALISE)
/*****************************************************************************
 Name:		HWInstGetOperandsUsedFNRM

 Purpose:	Determine which operands will be used by a FNRM HW instruction

 Inputs:	psHWInst		- The HW LD instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
static IMG_BOOL HWInstGetOperandsUsedFNRM(PHW_INST		psHWInst,
										  IMG_PUINT32	puOperandsUsed)
{
	IMG_UINT32	uOperandsUsed;

	/*
		F32 normalise uses all 3 sources.
		F16 normalise uses sources 1 and 2. 
	*/
	if	(psHWInst->uWord1 & SGX540_USE1_FNRM_F16DP)
	{
		uOperandsUsed = USP_OPERANDFLAG_DST |
						USP_OPERANDFLAG_SRC1 |
						USP_OPERANDFLAG_SRC2;
	}
	else
	{
		uOperandsUsed = USP_OPERANDFLAG_DST |
						USP_OPERANDFLAG_SRC0 |
						USP_OPERANDFLAG_SRC1 |
						USP_OPERANDFLAG_SRC2;
	}

	*puOperandsUsed = uOperandsUsed;
	return IMG_TRUE;
}
#endif /* #ifdef SGX_FEATURE_USE_NORMALISE */

/*****************************************************************************
 Name:		HWInstGetOperandsUsed

 Purpose:	Determine which operands will be used by a HW instruction

 Inputs:	eOpcode			- The decoded opcode for the HW instruction
			psHWInst		- The HW instruction

 Outputs:	puOperandsUsed	- Bitmask insidcating which operands are used by
							  the instrucion. (see USP_OPERANDFLAG_xxx)

 Returns:	IMG_FALSE if the HW instruction is invalid
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstGetOperandsUsed(USP_OPCODE	eOpcode,
											PHW_INST	psHWInst,
											IMG_PUINT32	puOperandsUsed)
{
	switch (eOpcode)
	{
		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_MAD:
		case USP_OPCODE_ADM:
		case USP_OPCODE_MSA:
		case USP_OPCODE_DDP:
		case USP_OPCODE_DDPC:
		case USP_OPCODE_MOVC:
		case USP_OPCODE_FMAD16:
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */
		case USP_OPCODE_IMA16:
		case USP_OPCODE_IMAE:
		case USP_OPCODE_BILIN:
		case USP_OPCODE_FIRV:
		case USP_OPCODE_FIRH:
		case USP_OPCODE_SMPBIAS:
		case USP_OPCODE_SMPREPLACE:
		case USP_OPCODE_SMPGRAD:
		case USP_OPCODE_ATST8:
		#if defined(SGX_FEATURE_USE_32BIT_INT_MAD)
		case USP_OPCODE_IMA32:
		#endif /* defined(SGX_FEATURE_USE_32BIT_INT_MAD) */
		{
			/*
				Dest and all 3 sources used
			*/
			*puOperandsUsed = USP_OPERANDFLAG_DST |
							  USP_OPERANDFLAG_SRC0 |
							  USP_OPERANDFLAG_SRC1 |
							  USP_OPERANDFLAG_SRC2;
			return IMG_TRUE;
		}

		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_FRC:
		case USP_OPCODE_DP:
		case USP_OPCODE_MIN:
		case USP_OPCODE_MAX:
		case USP_OPCODE_DSX:
		case USP_OPCODE_DSY:
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */
		case USP_OPCODE_AND:
		case USP_OPCODE_OR:
		case USP_OPCODE_XOR:
		case USP_OPCODE_SHL:
		case USP_OPCODE_ROL:
		case USP_OPCODE_SHR:
		case USP_OPCODE_ASR:
		case USP_OPCODE_DOT4:
		{
			/*
				Dest and source 1 and 2 used
			*/
			*puOperandsUsed = USP_OPERANDFLAG_DST |
							  USP_OPERANDFLAG_SRC1 |
							  USP_OPERANDFLAG_SRC2;
			return IMG_TRUE;
		}

		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_RCP:
		case USP_OPCODE_RSQ:
		case USP_OPCODE_LOG:
		case USP_OPCODE_EXP:
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */
 		case USP_OPCODE_RLP:
		{
			/*
				Dest and source 1
			*/
			*puOperandsUsed = USP_OPERANDFLAG_DST | USP_OPERANDFLAG_SRC1;
			return IMG_TRUE;
		}

		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_EFO:
		{
			return HWInstGetOperandsUsedEFO(psHWInst, puOperandsUsed);
		}
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */

		case USP_OPCODE_PCKUNPCK:
		{
			return HWInstGetOperandsUsedPCKUNPCK(psHWInst, puOperandsUsed);
		}

		case USP_OPCODE_TEST:
		case USP_OPCODE_TESTMASK:
		{
			return HWInstGetOperandsUsedTEST(psHWInst, puOperandsUsed);
		}

		case USP_OPCODE_SOP2:
		{
			return HWInstGetOperandsUsedSOP2(psHWInst, puOperandsUsed);
		}

		case USP_OPCODE_SOP3:
		{
			/*
				Currently not generated by USC
			*/
			break;
		}

		case USP_OPCODE_SOPWM:
		{
			return HWInstGetOperandsUsedSOPWM(psHWInst, puOperandsUsed);
		}

		case USP_OPCODE_IMA8:
		case USP_OPCODE_FPMA:
		{
			return HWInstGetOperandsUsedIMA8(psHWInst, puOperandsUsed);
		}

		case USP_OPCODE_ADIF:
		{
			return HWInstGetOperandsUsedADIF(psHWInst, puOperandsUsed);
		}

		case USP_OPCODE_DOT3:
		{
			return HWInstGetOperandsUsedDOT3(psHWInst, puOperandsUsed);
		}

		case USP_OPCODE_SMP:
		{
			/*
				Dest, source 0 and 1 used (source 2 is used by SMPBIAS,
				SMPREPLACE or SMPGRAD).
			*/
			*puOperandsUsed = USP_OPERANDFLAG_DST |
							  USP_OPERANDFLAG_SRC0 |
							  USP_OPERANDFLAG_SRC1;
			return IMG_TRUE;
		}

		case USP_OPCODE_ST:
		case USP_OPCODE_EMIT:
		case USP_OPCODE_PCOEFF:
		case USP_OPCODE_DEPTHF:
		{
			/*
				Destination unused.
			*/
			*puOperandsUsed = USP_OPERANDFLAG_SRC0 |
							  USP_OPERANDFLAG_SRC1 |
							  USP_OPERANDFLAG_SRC2;
			return IMG_TRUE;
		}

		case USP_OPCODE_LD:
		{
			return HWInstGetOperandsUsedLD(psHWInst, puOperandsUsed);
		}

		case USP_OPCODE_SETL:
		{
			/*
				Only Source 1 used.
			*/
			*puOperandsUsed = USP_OPERANDFLAG_SRC1;
			return IMG_TRUE;
		}

		case USP_OPCODE_SAVL:
		case USP_OPCODE_LIMM:
		{
			/*
				Only destination used.
			*/
			*puOperandsUsed = USP_OPERANDFLAG_DST;
			return IMG_TRUE;
		}

		case USP_OPCODE_LDR:
		{
			/*
				Only destination and source 2 used.
			*/
			*puOperandsUsed = USP_OPERANDFLAG_DST | USP_OPERANDFLAG_SRC2;
			return IMG_TRUE;
		}

		case USP_OPCODE_STR:
		{
			/*
				Only sources 1 and 2 used.
			*/
			*puOperandsUsed = USP_OPERANDFLAG_SRC1 | USP_OPERANDFLAG_SRC2;
			return IMG_TRUE;
		}

		#if defined(SGX_FEATURE_USE_IDXSC)
		case USP_OPCODE_IDXSCR:
		case USP_OPCODE_IDXSCW:
		{
			/*
				Only source 1 used.
			*/
			*puOperandsUsed = USP_OPERANDFLAG_SRC1;
			return IMG_TRUE;
		}
		#endif /* defined(SGX_FEATURE_USE_IDXSC) */

		case USP_OPCODE_BA:
		case USP_OPCODE_BR:
		case USP_OPCODE_NOP:
		case USP_OPCODE_LAPC:
		case USP_OPCODE_SMOA:
		case USP_OPCODE_SMR:
		case USP_OPCODE_SMLSI:
		case USP_OPCODE_SMBO:
		case USP_OPCODE_IMO:
		case USP_OPCODE_SETFC:
		case USP_OPCODE_IDF:
		case USP_OPCODE_WDF:
		case USP_OPCODE_SETM:
		case USP_OPCODE_LOCK:
		case USP_OPCODE_RELEASE:
		case USP_OPCODE_WOP:
		case USP_OPCODE_PTOFF:
		case USP_OPCODE_PREAMBLE:
		{
			/*
				No operands used
			*/
			*puOperandsUsed = 0;
			return IMG_TRUE;
		}

		#if defined(SGX_FEATURE_USE_NORMALISE)
		case USP_OPCODE_FNRM:
		{
			return HWInstGetOperandsUsedFNRM(psHWInst, puOperandsUsed);
		}
		#endif /* #if defined(SGX_FEATURE_USE_NORMALISE) */

		default:
		{
			break;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 Name:		HWInstCanUseExtSrc0Banks

 Purpose:	Indicates whether a HW instruction supports extended source banks
			for HW source 0

 Inputs:	eOpcode		- The decoded opcode for the HW instruction

 Outputs:	none

 Returns:	TRUE/FALSE to indicate whether source0 can use extended banks
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstCanUseExtSrc0Banks(USP_OPCODE eOpcode)
{
	switch (eOpcode)
	{
		case USP_OPCODE_SMP:
		case USP_OPCODE_SMPBIAS:
		case USP_OPCODE_SMPREPLACE:
		case USP_OPCODE_SMPGRAD:
		case USP_OPCODE_LD:
		case USP_OPCODE_ST:
		case USP_OPCODE_EMIT:
		case USP_OPCODE_PCOEFF:
		case USP_OPCODE_ATST8:
		case USP_OPCODE_DEPTHF:
		#if defined(SGX_FEATURE_USE_NORMALISE)
		case USP_OPCODE_FNRM:
		#endif /* #if defined(SGX_FEATURE_USE_NORMALISE) */
		{
			return IMG_TRUE;
		}

		default:
		{
			break;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 Name:		HWInstIsMOEControlInst

 Purpose:	Indicates whether an instruction effects the MOE state

 Inputs:	eOpcode		- The decoded opcode for the HW instruction

 Outputs:	none

 Returns:	TRUE/FALSE to indicate whether source0 can use extended banks
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstIsMOEControlInst(USP_OPCODE eOpcode)
{
	switch (eOpcode)
	{
		case USP_OPCODE_SMOA:
		case USP_OPCODE_SMR:
		case USP_OPCODE_SMLSI:
		case USP_OPCODE_SMBO:
		case USP_OPCODE_IMO:
		case USP_OPCODE_SETFC:
		{
			return IMG_TRUE;
		}

		default:
		{
			break;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 Name:		HWInstUpdateMOEState

 Purpose:	Update MOE state to reflect a MOE-control instruction

 Inputs:	eOpcode		- The decoded opcode for the HW instruction
			psHWInst	- The HW instruction to examine
			psMOEState	- The MOE state to update

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstUpdateMOEState(USP_OPCODE		eOpcode,
										   PHW_INST			psHWInst,
										   PUSP_MOESTATE	psMOEState)
{
	switch (eOpcode)
	{
		case USP_OPCODE_SMOA:
		case USP_OPCODE_SMR:
		case USP_OPCODE_IMO:
		{
			/*
				Currently not supported
			*/
			break;
		}

		case USP_OPCODE_SMBO:
		{
			IMG_UINT32	uWord0;
			IMG_UINT32	uWord1;
			IMG_PUINT32	puBaseOff;

			/*
				Record the new MOE base-offset for each operand

				NB: The base-offset for source 0 straddles the two words,
					hence the awkward decoding.
			*/
			uWord0		= psHWInst->uWord0;
			uWord1		= psHWInst->uWord1;
			puBaseOff	= psMOEState->auBaseOffset;

			puBaseOff[0] = (uWord1 & ~EURASIA_USE1_MOECTRL_SMR_DRANGE_CLRMSK) >>
							EURASIA_USE1_MOECTRL_SMR_DRANGE_SHIFT;

			puBaseOff[1] = ((uWord0 & ~EURASIA_USE0_MOECTRL_SMR_S0RANGE_CLRMSK) >>
							EURASIA_USE0_MOECTRL_SMR_S0RANGE_SHIFT) |
						   (((uWord1 & ~EURASIA_USE1_MOECTRL_SMR_S0RANGE_CLRMSK) >>
							EURASIA_USE1_MOECTRL_SMR_S0RANGE_SHIFT) << 16);

			puBaseOff[2] = (uWord0 & ~EURASIA_USE0_MOECTRL_SMR_S1RANGE_CLRMSK) >>
							EURASIA_USE0_MOECTRL_SMR_S1RANGE_SHIFT;

			puBaseOff[3] = (uWord0 & ~EURASIA_USE0_MOECTRL_SMR_S2RANGE_CLRMSK) >>
							EURASIA_USE0_MOECTRL_SMR_S2RANGE_SHIFT;

			break;
		}

		case USP_OPCODE_SMLSI:
		{
			IMG_UINT32	i;

			/*
				Update the Swizzle/Increment state for each operand
			*/
			for	(i = 0; i <= USP_OPERANDIDX_MAX; i++)
			{
				static const IMG_UINT32 auOperandIncClrMask[] =
				{
					EURASIA_USE0_MOECTRL_SMLSI_DINC_CLRMSK,
					EURASIA_USE0_MOECTRL_SMLSI_S0INC_CLRMSK,
					EURASIA_USE0_MOECTRL_SMLSI_S1INC_CLRMSK,
					EURASIA_USE0_MOECTRL_SMLSI_S2INC_CLRMSK
				};
				static const IMG_UINT32 auOperandIncShift[] =
				{
					EURASIA_USE0_MOECTRL_SMLSI_DINC_SHIFT,
					EURASIA_USE0_MOECTRL_SMLSI_S0INC_SHIFT,
					EURASIA_USE0_MOECTRL_SMLSI_S1INC_SHIFT,
					EURASIA_USE0_MOECTRL_SMLSI_S2INC_SHIFT
				};
				static const IMG_UINT32 auOperandUseSwiz[] =
				{
					EURASIA_USE1_MOECTRL_SMLSI_DUSESWIZ,
					EURASIA_USE1_MOECTRL_SMLSI_S0USESWIZ,
					EURASIA_USE1_MOECTRL_SMLSI_S1USESWIZ,
					EURASIA_USE1_MOECTRL_SMLSI_S2USESWIZ
				};

				IMG_UINT32	uOperandSwizOrInc;

				uOperandSwizOrInc	= (psHWInst->uWord0 & ~auOperandIncClrMask[i]) >>
									  auOperandIncShift[i];

				if	(psHWInst->uWord1 & auOperandUseSwiz[i])
				{
					psMOEState->abUseSwiz[i]	= IMG_TRUE;
					psMOEState->auSwiz[i]		= (IMG_UINT16)uOperandSwizOrInc;
				}
				else
				{
					psMOEState->abUseSwiz[i]	= IMG_FALSE;
					psMOEState->aiInc[i]		= (IMG_INT8)uOperandSwizOrInc;
				}
			}

			break;
		}

		case USP_OPCODE_SETFC:
		{
			/*
				Update the per-operand format-control state for
				EFO and colour instructions
			*/
			if	(psHWInst->uWord0 & EURASIA_USE0_MOECTRL_SETFC_EFO_SELFMTCTL)
			{
				psMOEState->bEFOFmtCtl = IMG_TRUE;
			}
			else
			{
				psMOEState->bEFOFmtCtl = IMG_FALSE;
			}

			if	(psHWInst->uWord0 & EURASIA_USE0_MOECTRL_SETFC_COL_SETFMTCTL)
			{
				psMOEState->bColFmtCtl = IMG_TRUE;
			}
			else
			{
				psMOEState->bColFmtCtl = IMG_FALSE;
			}
			
			break;
		}

		default:
		{
			break;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstGetPerOperandFmtCtl

 Purpose:	Determine whether per-operand control over data-format is enabled
			for an instruction

 Inputs:	psMOEState	- The decoded MOE state for the instruction
			eOpcode		- The decoded opcode for the HW instruction
			psHWInst	- The HW instruction to examine

 Outputs:	peFmtCtl	- The available per-operand format control

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstGetPerOperandFmtCtl(PUSP_MOESTATE	psMOEState,
												USP_OPCODE		eOpcode,
												PHW_INST		psHWInst,
												PUSP_FMTCTL		peFmtCtl)
{
	USP_FMTCTL	eFmtCtl;

	/*
		Determine how this instruction allows the data-format
		to be specified per-operand.
	*/
	eFmtCtl = USP_FMTCTL_NONE;

#if !defined(SGX_FEATURE_EXTENDED_USE_ALU)

	USP_UNREFERENCED_PARAMETER(psMOEState);
	USP_UNREFERENCED_PARAMETER(eOpcode);
	USP_UNREFERENCED_PARAMETER(psHWInst);

#else	/* #if defined(SGX_FEATURE_EXTENDED_USE_ALU) */

	switch (eOpcode)
	{
		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_MAD:
		case USP_OPCODE_ADM:
		case USP_OPCODE_MSA:
		case USP_OPCODE_FRC:
		case USP_OPCODE_DP:
		case USP_OPCODE_DDP:
		case USP_OPCODE_DDPC:
		case USP_OPCODE_MIN:
		case USP_OPCODE_MAX:
		case USP_OPCODE_DSX:
		case USP_OPCODE_DSY:
		#if defined(SGX545)
		case USP_OPCODE_DUAL:
		#endif /* defined(SGX545) */
		{
			/*
				On an extended ALU, these Instruction include a
				dedicated format control, which determines whether
				per-operand format selection (between F32 and F16)
				is available. If this isn't set, the operands assume
				F32 data only.
			*/
			if	(psHWInst->uWord1 & EURASIA_USE1_FLOAT_SFASEL)
			{
				eFmtCtl = USP_FMTCTL_F32_OR_F16;
			}

			break;
		}
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */

		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_RCP:
		case USP_OPCODE_RSQ:
		case USP_OPCODE_LOG:
		case USP_OPCODE_EXP:
		case USP_OPCODE_MOVC:
		case USP_OPCODE_FMAD16:
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */
		case USP_OPCODE_SMP:
		case USP_OPCODE_SMPBIAS:
		case USP_OPCODE_SMPREPLACE:
		case USP_OPCODE_SMPGRAD:
		case USP_OPCODE_PCKUNPCK:
		case USP_OPCODE_AND:
		case USP_OPCODE_OR:
		case USP_OPCODE_XOR:
		case USP_OPCODE_SHL:
		case USP_OPCODE_ROL:
		case USP_OPCODE_SHR:
		case USP_OPCODE_ASR:
		case USP_OPCODE_RLP:
		case USP_OPCODE_LD:
		case USP_OPCODE_ST:
		case USP_OPCODE_BA:
		case USP_OPCODE_BR:
		case USP_OPCODE_LAPC:
		case USP_OPCODE_SETL:
		case USP_OPCODE_SAVL:
		case USP_OPCODE_NOP:
		case USP_OPCODE_PREAMBLE:
		case USP_OPCODE_SMOA:
		case USP_OPCODE_SMR:
		case USP_OPCODE_SMLSI:
		case USP_OPCODE_SMBO:
		case USP_OPCODE_IMO:
		case USP_OPCODE_SETFC:
		case USP_OPCODE_IDF:
		case USP_OPCODE_WDF:
		case USP_OPCODE_SETM:
		case USP_OPCODE_EMIT:
		case USP_OPCODE_LIMM:
		case USP_OPCODE_LOCK:
		case USP_OPCODE_RELEASE:
		case USP_OPCODE_LDR:
		case USP_OPCODE_STR:
		case USP_OPCODE_WOP:
		case USP_OPCODE_PCOEFF:
		case USP_OPCODE_PTOFF:
		case USP_OPCODE_ATST8:
		case USP_OPCODE_DEPTHF:
		#if defined(SGX_FEATURE_USE_NORMALISE)
		case USP_OPCODE_FNRM:
		#endif /* #if defined(SGX_FEATURE_USE_NORMALISE) */
		#if defined(SGX_FEATURE_USE_VEC34)
		
		case USP_OPCODE_VMAD:

		case USP_OPCODE_VMUL:
		case USP_OPCODE_VADD:
		case USP_OPCODE_VFRC:
		case USP_OPCODE_VDSX:
		case USP_OPCODE_VDSY:
		case USP_OPCODE_VMIN:
		case USP_OPCODE_VMAX:
		case USP_OPCODE_VDP:

		case USP_OPCODE_VDP3:
		case USP_OPCODE_VDP4:
		case USP_OPCODE_VMAD3:
		case USP_OPCODE_VMAD4:

		case USP_OPCODE_VDUAL:

		case USP_OPCODE_VRCP:
		case USP_OPCODE_VRSQ:
		case USP_OPCODE_VLOG:
		case USP_OPCODE_VEXP:

		case USP_OPCODE_VMOV:
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
		#if defined(SGX_FEATURE_USE_IDXSC)
		case USP_OPCODE_IDXSCR:
		case USP_OPCODE_IDXSCW:
		#endif /* defined(SGX_FEATURE_USE_IDXSC) */
		{
			/*
				Per-operand control of data-type not available for these
				instructions. Some do have additional fields to indicate
				how operand data is interpreted.
			*/
			break;
		}

		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_EFO:
		{
			/*
				With an extended ALU, per-operand data-format control is
				available if selected in the MOE state. If used, the
				per-operand control chooses between F32 and F16 data.
			*/
			if	(psMOEState->bEFOFmtCtl)
			{
				eFmtCtl = USP_FMTCTL_F32_OR_F16;
			}

			break;
		}
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */

		case USP_OPCODE_TEST:
		case USP_OPCODE_TESTMASK:
		{
			IMG_UINT32	uALUSel;

			/*
				On an extended ALU, these instructions override the 
				PARTIAL field to control whether a per-operand format-
				control is used (for non-bitwise operations only).
			*/
			uALUSel = (psHWInst->uWord1 & ~EURASIA_USE0_TEST_ALUSEL_CLRMSK) >>
					  EURASIA_USE0_TEST_ALUSEL_SHIFT;

			switch (uALUSel)
			{
				case EURASIA_USE0_TEST_ALUSEL_F32:
				{
					if	(psHWInst->uWord1 & EURASIA_USE1_TEST_PARTIAL)
					{
						eFmtCtl = USP_FMTCTL_F32_OR_F16;
					}
					break;
				}

				case EURASIA_USE0_TEST_ALUSEL_I8:
				{
					if	(psHWInst->uWord1 & EURASIA_USE1_TEST_PARTIAL)
					{
						eFmtCtl = USP_FMTCTL_U8_OR_C10;
					}
					break;
				}

				case EURASIA_USE0_TEST_ALUSEL_I16:
				case EURASIA_USE0_TEST_ALUSEL_BITWISE:
				{
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			break;
		}

		case USP_OPCODE_SOP2:
		case USP_OPCODE_SOP3:
		case USP_OPCODE_SOPWM:
		case USP_OPCODE_IMA8:
		case USP_OPCODE_IMA16:
		case USP_OPCODE_IMAE:
		case USP_OPCODE_ADIF:
		case USP_OPCODE_BILIN:
		case USP_OPCODE_FIRV:
		case USP_OPCODE_FIRH:
		case USP_OPCODE_DOT3:
		case USP_OPCODE_DOT4:
		case USP_OPCODE_FPMA:
		#if defined(SGX_FEATURE_USE_32BIT_INT_MAD)
		case USP_OPCODE_IMA32:
		#endif /* defined(SGX_FEATURE_USE_32BIT_INT_MAD) */
		{
			/*
				With an extended ALU, per-operand data-format control 
				(choosing between U8 and C10 data) is available if
				selected in the MOE state.
			*/
			if	(psMOEState->bColFmtCtl)
			{
				eFmtCtl = USP_FMTCTL_U8_OR_C10;
			}
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

#endif	/* #if defined(SGX_FEATURE_EXTENDED_USE_ALU) */

	*peFmtCtl = eFmtCtl;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstDecodeIndexedOperandNum

 Purpose:	Extract the destination register type and number used by a
			indexed HW instruction operand.

 Inputs:	eFmtCtl		- The per-operand format-control available
			uHWRegNum	- The extracted HW operand number to be decoded

 Outputs:	peRegType	- The decoded register type
			puRegOffset	- The decoded register offset
			peDynIdx	- The decoded index register used

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL HWInstDecodeIndexedOperandNum(USP_FMTCTL	eFmtCtl,
											  IMG_UINT32	uHWRegNum,
											  PUSP_REGTYPE	peRegType,
											  IMG_PUINT32	puRegOffset,
											  PUSP_DYNIDX	peDynIdx)
{
	IMG_UINT32	uHWIndexReg;
	IMG_UINT32	uHWRegBank;

	if	(eFmtCtl != USP_FMTCTL_NONE)
	{
		*puRegOffset	= (uHWRegNum & ~EURASIA_USE_FCINDEX_OFFSET_CLRMSK) >>
						  EURASIA_USE_FCINDEX_OFFSET_SHIFT;

		uHWIndexReg		= uHWRegNum & EURASIA_USE_FCINDEX_IDXSEL;
		uHWRegBank		= (uHWRegNum & ~EURASIA_USE_FCINDEX_BANK_CLRMSK) >>
						  EURASIA_USE_FCINDEX_BANK_SHIFT;
	}
	else
	{
		*puRegOffset	= (uHWRegNum & ~EURASIA_USE_INDEX_OFFSET_CLRMSK) >>
						  EURASIA_USE_INDEX_OFFSET_SHIFT;

		uHWIndexReg		= uHWRegNum & EURASIA_USE_INDEX_IDXSEL;
		uHWRegBank		= (uHWRegNum & ~EURASIA_USE_INDEX_BANK_CLRMSK) >>
						  EURASIA_USE_INDEX_BANK_SHIFT;
	}

	switch (uHWRegBank)
	{
		case EURASIA_USE_INDEX_BANK_TEMP:
		{
			*peRegType = USP_REGTYPE_TEMP;
			break;
		}

		case EURASIA_USE_INDEX_BANK_OUTPUT:
		{
			*peRegType = USP_REGTYPE_OUTPUT;
			break;
		}

		case EURASIA_USE_INDEX_BANK_PRIMATTR:
		{
			*peRegType = USP_REGTYPE_PA;
			break;
		}

		case EURASIA_USE_INDEX_BANK_SECATTR:
		{
			*peRegType = USP_REGTYPE_SA;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	if	(uHWIndexReg)
	{
		*peDynIdx = USP_DYNIDX_IH;
	}
	else
	{
		*peDynIdx = USP_DYNIDX_IL;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstEncodeIndexedOperandNum

 Purpose:	Pack the destination register type and number used by a
			indexed HW instruction operand.

 Inputs:	eFmtCtl		- The per-operand format-control available
			eRegType	- The required register type
			uRegOffset	- The required register offset
			eDynIdx		- The required index register
			
 Outputs:	puHWRegNum	- The encoded HW operand number to be decoded

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL HWInstEncodeIndexedOperandNum(USP_FMTCTL	eFmtCtl,
											  USP_REGTYPE	eRegType,
											  IMG_UINT32	uRegOffset,
											  USP_DYNIDX	eDynIdx,
											  IMG_PUINT32	puHWRegNum)
{
	IMG_UINT32	uIndexReg;
	IMG_UINT32	uHWRegBank;

	switch (eDynIdx)
	{
		case USP_DYNIDX_IH:
		{
			uIndexReg = 1;
			break;
		}

		case USP_DYNIDX_IL:
		{
			uIndexReg = 0;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	switch (eRegType)
	{
		case USP_REGTYPE_TEMP:
		{
			uHWRegBank = EURASIA_USE_INDEX_BANK_TEMP;
			break;
		}

		case USP_REGTYPE_OUTPUT:
		{
			uHWRegBank = EURASIA_USE_INDEX_BANK_OUTPUT;
			break;
		}

		case USP_REGTYPE_PA:
		{
			uHWRegBank = EURASIA_USE_INDEX_BANK_PRIMATTR;
			break;
		}

		case USP_REGTYPE_SA:
		{
			uHWRegBank = EURASIA_USE_INDEX_BANK_SECATTR;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	*puHWRegNum = 0;

	if	(eFmtCtl != USP_FMTCTL_NONE)
	{
		if	(uRegOffset > EURASIA_USE_FCINDEX_MAXIMUM_OFFSET)
		{
			return IMG_FALSE;
		}

		*puHWRegNum |= (uHWRegBank << EURASIA_USE_FCINDEX_BANK_SHIFT) &
					   (~EURASIA_USE_FCINDEX_BANK_CLRMSK);
		*puHWRegNum |= uIndexReg * EURASIA_USE_FCINDEX_IDXSEL;
		*puHWRegNum |= (uRegOffset << EURASIA_USE_FCINDEX_OFFSET_SHIFT) &
					   (~EURASIA_USE_FCINDEX_OFFSET_CLRMSK);
	}
	else
	{
		if	(uRegOffset > EURASIA_USE_INDEX_MAXIMUM_OFFSET)
		{
			return IMG_FALSE;
		}

		*puHWRegNum |= (uHWRegBank << EURASIA_USE_INDEX_BANK_SHIFT) &
					   (~EURASIA_USE_INDEX_BANK_CLRMSK);
		*puHWRegNum |= uIndexReg * EURASIA_USE_INDEX_IDXSEL;
		*puHWRegNum |= (uRegOffset << EURASIA_USE_INDEX_OFFSET_SHIFT) &
					   (~EURASIA_USE_INDEX_OFFSET_CLRMSK);
	}

	return IMG_TRUE;
}

typedef enum _USP_SRCARG_
{
	USP_SRCARG_SRC0	= 0,
	USP_SRCARG_SRC1 = 1,
	USP_SRCARG_SRC2 = 2,
} USP_SRCARG;

typedef struct _USP_SHIFT_AND_MASK_
{
	IMG_UINT32	uShift;
	IMG_UINT32	uMask;
} USP_SHIFT_AND_MASK, *PUSP_SHIFT_AND_MASK;

/*****************************************************************************
 Name:		HWInstEncodeSrcNumber

 Purpose:	Encode the source register number used by a
			HW instruction.

 Inputs:	eOpcode		- the decoded opcode of the instruction
			psHWInst	- The HW instruction to change
			eRegType	- Type of the register to encode
			eRegFmt		- Format of the register to encode
			uHWRegNum	- The register number to encode.
			eSrcArg		- The instruction source argument to encode.

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID HWInstEncodeSrcNumber(	
										USP_OPCODE	eOpcode,
										PHW_INST	psHWInst,
										USP_REGTYPE	eRegType,
										USP_REGFMT	eRegFmt,
										IMG_UINT32	uHWRegNum,
										USP_SRCARG	eSrcArg)
{
	static const USP_SHIFT_AND_MASK asDefaultShiftsMasks[] =
	{
		{EURASIA_USE0_SRC0_SHIFT, EURASIA_USE0_SRC0_CLRMSK},	/* USP_SRCARG_SRC0 */
		{EURASIA_USE0_SRC1_SHIFT, EURASIA_USE0_SRC1_CLRMSK},	/* USP_SRCARG_SRC1 */
		{EURASIA_USE0_SRC2_SHIFT, EURASIA_USE0_SRC2_CLRMSK},	/* USP_SRCARG_SRC2 */
	};
	#if defined(SGX_FEATURE_USE_VEC34)
	static const USP_SHIFT_AND_MASK asPCKShiftsMasks[] =
	{
		{(IMG_UINT32)-1, (IMG_UINT32)-1},										/* USP_SRCARG_SRC0 */
		{SGXVEC_USE0_PCK_64BITSRC1_SHIFT, SGXVEC_USE0_PCK_64BITSRC1_CLRMSK},	/* USP_SRCARG_SRC1 */
		{SGXVEC_USE0_PCK_64BITSRC2_SHIFT, SGXVEC_USE0_PCK_64BITSRC2_CLRMSK},	/* USP_SRCARG_SRC1 */
	};
	static const USP_SHIFT_AND_MASK asVMADShiftsMasks[] =
	{
		{SGXVEC_USE0_VECMAD_SRC0_SHIFT, SGXVEC_USE0_VECMAD_SRC0_CLRMSK},		/* USP_SRCARG_SRC0 */
		{SGXVEC_USE0_VECMAD_SRC1_SHIFT, SGXVEC_USE0_VECMAD_SRC1_CLRMSK},		/* USP_SRCARG_SRC1 */
		{SGXVEC_USE0_VECMAD_SRC2_SHIFT, SGXVEC_USE0_VECMAD_SRC2_CLRMSK},		/* USP_SRCARG_SRC2 */
	};
	static const USP_SHIFT_AND_MASK asVMOVShiftsMasks[] =
	{
		{SGXVEC_USE0_VMOVC_SRC0_SHIFT, SGXVEC_USE0_VMOVC_SRC0_CLRMSK},		/* USP_SRCARG_SRC0 */
		{SGXVEC_USE0_VMOVC_SRC1_SHIFT, SGXVEC_USE0_VMOVC_SRC1_CLRMSK},		/* USP_SRCARG_SRC1 */
		{SGXVEC_USE0_VMOVC_SRC2_SHIFT, SGXVEC_USE0_VMOVC_SRC2_CLRMSK},		/* USP_SRCARG_SRC2 */
	};
	IMG_BOOL					bDoubleRegisters;
	#endif /* defined(SGX_FEATURE_USE_VEC34) */
	USP_SHIFT_AND_MASK const*	psShiftMaskTable;

	#if !defined(SGX_FEATURE_USE_VEC34)
	PVR_UNREFERENCED_PARAMETER(eRegType);
	PVR_UNREFERENCED_PARAMETER(eRegFmt);
	#endif /* !defined(SGX_FEATURE_USE_VEC34) */

	switch (eOpcode)
	{
		#if defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_PCKUNPCK:
		{
			if (eRegFmt == USP_REGFMT_U8 && (eSrcArg == USP_SRCARG_SRC1 || eSrcArg == USP_SRCARG_SRC2))
			{
				bDoubleRegisters = IMG_FALSE;
				psShiftMaskTable = asDefaultShiftsMasks;
			}
			else
			{
				bDoubleRegisters = IMG_TRUE;
				psShiftMaskTable = asPCKShiftsMasks;
			}
			break;
		}
		case USP_OPCODE_SMP:
		case USP_OPCODE_SMPBIAS:
		case USP_OPCODE_SMPREPLACE:
		case USP_OPCODE_SMPGRAD:
		{
			bDoubleRegisters = IMG_TRUE;
			psShiftMaskTable = asDefaultShiftsMasks;
			break;
		}
		case USP_OPCODE_VMAD:
		{
			bDoubleRegisters = IMG_TRUE;
			psShiftMaskTable = asVMADShiftsMasks;
			break;
		}
		case USP_OPCODE_VMOV:
		{
			bDoubleRegisters = IMG_FALSE;
			psShiftMaskTable = asVMOVShiftsMasks;
			break;
		}
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
		default:
		{
			#if defined(SGX_FEATURE_USE_VEC34)
			bDoubleRegisters = IMG_FALSE;
			#endif /* defined(SGX_FEATURE_USE_VEC34) */
			psShiftMaskTable = asDefaultShiftsMasks;
			break;
		}
	}

	#if defined(SGX_FEATURE_USE_VEC34)
	if (bDoubleRegisters)
	{
		if (eRegType != USP_REGTYPE_SPECIAL)
		{
			uHWRegNum >>= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
		}
	}
	#endif /* defined(SGX_FEATURE_USE_VEC34) */

	psHWInst->uWord0 &= psShiftMaskTable[eSrcArg].uMask;
	psHWInst->uWord0 |= uHWRegNum << psShiftMaskTable[eSrcArg].uShift;
}

#if defined(SGX_FEATURE_USE_VEC34)
/*****************************************************************************
 Name:		HWInstHasVectorResultPCKUNPCKInst

 Purpose:	Check for a combination of PCK source and destination formats
			where the hardware will write a 64-bit result.

 Inputs:	psHWInst	- The HW PCK instruction to check.

 Outputs:	none

 Returns:	IMG_TRUE if the PCK instruction has a vector result.
*****************************************************************************/
static
IMG_BOOL HWInstHasVectorResultPCKUNPCKInst(PHW_INST	psHWInst)
{
	IMG_UINT32	uHWDstPckFmt;
	IMG_UINT32	uHWSrcPckFmt;

	uHWDstPckFmt = (psHWInst->uWord1 & ~EURASIA_USE1_PCK_DSTF_CLRMSK) >>
					EURASIA_USE1_PCK_DSTF_SHIFT;
	uHWSrcPckFmt = (psHWInst->uWord1 & ~EURASIA_USE1_PCK_SRCF_CLRMSK) >>
					EURASIA_USE1_PCK_SRCF_SHIFT;

	if (
			uHWDstPckFmt == EURASIA_USE1_PCK_FMT_F32 ||
			uHWDstPckFmt == EURASIA_USE1_PCK_FMT_F16 ||
			uHWDstPckFmt == EURASIA_USE1_PCK_FMT_C10
	   )
	{
		if (
				(uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_U8 && (psHWInst->uWord0 & EURASIA_USE0_PCK_SCALE) != 0) ||
				uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_F16 ||
				uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_F32 ||
				uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_C10
		   )	
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}
#endif /* defined(SGX_FEATURE_USE_VEC34) */

/*****************************************************************************
 Name:		HWInstEncodeDestBankAndNum

 Purpose:	Encode the destination register bank and number used by a
			HW instruction.

 Inputs:	eFmtCtl		- The per-operand format-control available
			eOpcode		- the decoded opcode of the instruction
			psHWInst	- The HW instruction to change
			psReg		- The register to encode

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeDestBankAndNum(USP_FMTCTL	eFmtCtl,
												 USP_OPCODE	eOpcode,
							  					 PHW_INST	psHWInst,
												 PUSP_REG	psReg)
{
	IMG_UINT32	uHWRegNum;
	IMG_UINT32	uHWRegBank;
	IMG_UINT32	uHWBankExt;
	USP_REGTYPE	eRegType;
	IMG_UINT32	uRegNum;

	/*
		Disable per-operand format control options for inappropriate 
		register types

		NB: No format-control on destination with internal registers
	*/
	if	(
			(psReg->eType == USP_REGTYPE_INDEX) ||
			(psReg->eType == USP_REGTYPE_SPECIAL) ||
			(psReg->eType == USP_REGTYPE_IMM) ||
			(psReg->eType == USP_REGTYPE_INTERNAL)
		)
	{
		eFmtCtl = USP_FMTCTL_NONE;
	}

	/*
		For source 0, the internal registers are mapped to the
		last 4 temporaires.
	*/
	eRegType = psReg->eType;
	uRegNum	 = psReg->uNum;

	#if defined(SGX_FEATURE_USE_VEC34)
	if	(eRegType == USP_REGTYPE_INTERNAL)
	{
		IMG_UINT32	uMaxTempRegNum;

		/*
			The top 4 temporaries map to the internal registers
			for dest for sgx543. 
		*/
		if	(eFmtCtl != USP_FMTCTL_NONE)
		{
			uMaxTempRegNum = EURASIA_USE_FCREGISTER_NUMBER_MAX;
			if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
			{
				uMaxTempRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
			}
		}
		else
		{
			uMaxTempRegNum = EURASIA_USE_REGISTER_NUMBER_MAX;
		}

		uMaxTempRegNum -= EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;

		uRegNum += uMaxTempRegNum;
		eRegType = USP_REGTYPE_TEMP;
	}
	#endif /* defined(SGX_FEATURE_USE_VEC34) */

	/*
		Encode the HW register number
	*/
	if	(psReg->eDynIdx != USP_DYNIDX_NONE)
	{
		if	(!HWInstEncodeIndexedOperandNum(eFmtCtl,
											eRegType,
											uRegNum,
											psReg->eDynIdx,
											&uHWRegNum))
		{
			return IMG_FALSE;
		}
	}
	else
	{
		uHWRegNum = uRegNum;

		if	(eFmtCtl == USP_FMTCTL_NONE)
		{
			if	(uHWRegNum >= EURASIA_USE_REGISTER_NUMBER_MAX)
			{
				return IMG_FALSE;
			}
		}
		else
		{
			if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
			{
				if	(psReg->eFmt == USP_REGFMT_F16)
				{
					uHWRegNum <<= EURASIA_USE_FMTF16_REGNUM_SHIFT;

					switch (psReg->uComp)
					{
						case 0:
						{
							uHWRegNum |= EURASIA_USE_FMTF16_SELECTLOW;
							break;
						}
						case 2:
						{
							uHWRegNum |= EURASIA_USE_FMTF16_SELECTHIGH;
						}
						default:
						{
							return IMG_FALSE;
						}
					}
				}
			}

			if	(uHWRegNum >= EURASIA_USE_FCREGISTER_NUMBER_MAX)
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Encode the format-select bit if this instruction allows per-operand
		format control.
	*/
	switch (eFmtCtl)
	{
		case USP_FMTCTL_F32_OR_F16:
		{
			switch (psReg->eFmt)
			{
				case USP_REGFMT_F32:
				{
					uHWRegNum &= ~EURASIA_USE_FMTSELECT;
					break;
				}
				case USP_REGFMT_F16:
				{
					uHWRegNum |= EURASIA_USE_FMTSELECT;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		case USP_FMTCTL_U8_OR_C10:
		{
			switch (psReg->eFmt)
			{
				case USP_REGFMT_U8:
				{
					uHWRegNum &= ~EURASIA_USE_FMTSELECT;
					break;
				}
				case USP_REGFMT_C10:
				{
					uHWRegNum |= EURASIA_USE_FMTSELECT;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		case USP_FMTCTL_NONE:
		default:
		{
			break;
		}
	}

	/*
		Encode the register-bank
	*/
	if	(psReg->eDynIdx != USP_DYNIDX_NONE)
	{
		#if defined(SGX_FEATURE_USE_VEC34)
		uHWRegBank = SGXVEC_USE1_D1STDBANK_INDEXED_IL;
		uHWBankExt = 0;
		#else
		uHWRegBank = EURASIA_USE1_D1STDBANK_INDEXED;
		uHWBankExt = 0;
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
	}
	else
	{
		switch (eRegType)
		{
			case USP_REGTYPE_TEMP:
			{
				uHWRegBank = EURASIA_USE1_D1STDBANK_TEMP;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_PA:
			{
				uHWRegBank = EURASIA_USE1_D1STDBANK_PRIMATTR;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_SA:
			{
				uHWRegBank = EURASIA_USE1_D1EXTBANK_SECATTR;
				uHWBankExt = EURASIA_USE1_DBEXT;
				break;
			}

			case USP_REGTYPE_OUTPUT:
			{
				uHWRegBank = EURASIA_USE1_D1STDBANK_OUTPUT;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_INTERNAL:
			{
				#if defined(SGX_FEATURE_USE_VEC34)
				return IMG_FALSE;
				#else
				uHWRegBank = EURASIA_USE1_D1EXTBANK_FPINTERNAL;				
				uHWBankExt = EURASIA_USE1_DBEXT;
				#endif /* defined(SGX_FEATURE_USE_VEC34) */
				break;
			}

			case USP_REGTYPE_INDEX:
			{
				uHWRegBank = EURASIA_USE1_D1EXTBANK_INDEX;
				uHWBankExt = EURASIA_USE1_DBEXT;
				break;
			}

			case USP_REGTYPE_SPECIAL:
			{
				uHWRegBank = EURASIA_USE1_D1EXTBANK_SPECIAL;
				uHWBankExt = EURASIA_USE1_DBEXT;
				break;
			}

			default:
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Write the encoded HW bank into the instruction
	*/
	switch (eOpcode)
	{
		case USP_OPCODE_SMP:
		case USP_OPCODE_SMPBIAS:
		case USP_OPCODE_SMPREPLACE:
		case USP_OPCODE_SMPGRAD:
		{
			IMG_UINT32	uSMPDestBank;

			/*
				Map the usual dest register bank to the special TEMP and PA
				options used by SMP instructions
			*/
			switch (uHWRegBank)
			{
				case EURASIA_USE1_D1STDBANK_TEMP:
				{
					uSMPDestBank = EURASIA_USE1_SMP_DBANK_TEMP;
					break;
				}
				case EURASIA_USE1_D1STDBANK_PRIMATTR:
				{
					uSMPDestBank = EURASIA_USE1_SMP_DBANK_PRIMATTR;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}

			psHWInst->uWord1 &= EURASIA_USE1_SMP_DBANK_CLRMSK;
			psHWInst->uWord1 |= uSMPDestBank;
			break;
		}

		case USP_OPCODE_LD:
		{
			IMG_UINT32	uLDDestBank;

			/*
				Map the usual dest register bank to the special TEMP and PA
				options used by LD instructions
			*/
			switch (uHWRegBank)
			{
				case EURASIA_USE1_D1STDBANK_TEMP:
				{
					uLDDestBank = EURASIA_USE1_LDST_DBANK_TEMP;
					break;
				}
				case EURASIA_USE1_D1STDBANK_PRIMATTR:
				{
					uLDDestBank = EURASIA_USE1_LDST_DBANK_PRIMATTR;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}

			psHWInst->uWord1 &= EURASIA_USE1_LDST_DBANK_CLRMSK;
			psHWInst->uWord1 |= uLDDestBank;
			break;
		}

		case USP_OPCODE_LDR:
		{
			IMG_UINT32	uLDRDestBank;

			/*
				Map the usual dest register bank to the special TEMP and PA
				options used by LDR instructions
			*/
			switch (uHWRegBank)
			{
				case EURASIA_USE1_D1STDBANK_TEMP:
				{
					uLDRDestBank = EURASIA_USE1_LDRSTR_DBANK_TEMP;
					break;
				}
				case EURASIA_USE1_D1STDBANK_PRIMATTR:
				{
					uLDRDestBank = EURASIA_USE1_LDRSTR_DBANK_PRIMATTR;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}

			psHWInst->uWord1 &= EURASIA_USE1_LDRSTR_DBANK_CLRMSK;
			psHWInst->uWord1 |= uLDRDestBank;
			break;
		}

		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_EFO:
		case USP_OPCODE_DDP:
		case USP_OPCODE_DDPC:
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */
		case USP_OPCODE_ATST8:
		{
			/*
				No extended destination banks on these instructions.
			*/
			if (uHWBankExt != 0)
			{
				return IMG_FALSE;
			}

			psHWInst->uWord1 &= EURASIA_USE1_D1BANK_CLRMSK;
			psHWInst->uWord1 |= (uHWRegBank << EURASIA_USE1_D1BANK_SHIFT) &
								(~EURASIA_USE1_D1BANK_CLRMSK);
			break;
		}

		#if defined(SGX_FEATURE_USE_NORMALISE)
		case USP_OPCODE_FNRM:
		{
			/*
				Only TEMP and PA destination banks allowed for FNRM.
			*/
			if	(
					(uHWRegBank != EURASIA_USE1_D1STDBANK_TEMP) ||
					(uHWRegBank != EURASIA_USE1_D1STDBANK_PRIMATTR)
				)
			{
				return IMG_FALSE;
			}

			psHWInst->uWord1 &= EURASIA_USE1_D1BANK_CLRMSK;
			psHWInst->uWord1 |= (uHWRegBank << EURASIA_USE1_D1BANK_SHIFT) &
								(~EURASIA_USE1_D1BANK_CLRMSK);

			break;
		}
		#endif /* #if defined(SGX_FEATURE_USE_NORMALISE)*/

		default:
		{
			psHWInst->uWord1 &= EURASIA_USE1_D1BANK_CLRMSK &
								(~EURASIA_USE1_DBEXT);
			psHWInst->uWord1 |= (uHWRegBank << EURASIA_USE1_D1BANK_SHIFT) &
								(~EURASIA_USE1_D1BANK_CLRMSK);
			psHWInst->uWord1 |= uHWBankExt;
		}
	}

	/*
		Write the encoded register number into the instruction	
	*/
	psHWInst->uWord0 &= EURASIA_USE0_DST_CLRMSK;

	#if defined(SGX_FEATURE_USE_VEC34)
	if(eOpcode == USP_OPCODE_PCKUNPCK)
	{
		if(!HWInstHasVectorResultPCKUNPCKInst(psHWInst))
		{
			psHWInst->uWord0 |= (uHWRegNum << EURASIA_USE0_DST_SHIFT) &
						(~EURASIA_USE0_DST_CLRMSK);
		}
		else
		{
			/* Clear the least significant bit */
			uHWRegNum >>= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
			uHWRegNum <<= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
			psHWInst->uWord0 |= (uHWRegNum << EURASIA_USE0_DST_SHIFT) & 
						(~EURASIA_USE0_DST_CLRMSK);
		}
	}
	else
	{
		if(eOpcode == USP_OPCODE_VMOV)
		{
			psHWInst->uWord0 |= (uHWRegNum << SGXVEC_USE0_VMOVC_DST_SHIFT) &
							(~SGXVEC_USE0_VMOVC_DST_CLRMSK);
		}
		else
		{
			psHWInst->uWord0 |= (uHWRegNum << EURASIA_USE0_DST_SHIFT) &
							(~EURASIA_USE0_DST_CLRMSK);
		}
	}
	#else
	psHWInst->uWord0 |= (uHWRegNum << EURASIA_USE0_DST_SHIFT) &
						(~EURASIA_USE0_DST_CLRMSK);
	#endif /* defined(SGX_FEATURE_USE_VEC34) */
	
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstDecodeDestBankAndNum

 Purpose:	Decode the destination register bank and number of a HW
			instruction

 Inputs:	eFmtCtl		- The per-operand format-control available
			eOpcode		- The decoded opcode of the instruction
			psHWInst	- The HW instruction to examine

 Outputs:	psReg		- The decoded register details

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstDecodeDestBankAndNum(USP_FMTCTL	eFmtCtl,
												 USP_OPCODE	eOpcode,
												 PHW_INST	psHWInst,
												 PUSP_REG	psReg)
{
	IMG_UINT32	uHWRegBank;
	IMG_UINT32	uHWRegNum;
	IMG_UINT32	uHWFmtCtl;

	/*
		Extract the common destination register index (bank-encoding is
		opcode specific)
	*/
	uHWRegNum = (psHWInst->uWord0 & ~EURASIA_USE0_DST_CLRMSK) >>
				EURASIA_USE0_DST_SHIFT;

	/*
		Decode the HW register bank
	*/
	psReg->eDynIdx = USP_DYNIDX_NONE;

	switch (eOpcode)
	{
		case USP_OPCODE_SMP:
		case USP_OPCODE_SMPBIAS:
		case USP_OPCODE_SMPREPLACE:
		case USP_OPCODE_SMPGRAD:
		{
			IMG_UINT32	uSMPDestBank;

			/*
				Only TEMP and PA banks available for SMP instructions
			*/
			uSMPDestBank = psHWInst->uWord1 & ~EURASIA_USE1_SMP_DBANK_CLRMSK;
			switch (uSMPDestBank)
			{
				case EURASIA_USE1_SMP_DBANK_TEMP:
				{
					psReg->eType = USP_REGTYPE_TEMP;
					break;
				}
				case EURASIA_USE1_SMP_DBANK_PRIMATTR:
				{
					psReg->eType = USP_REGTYPE_PA;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		case USP_OPCODE_LD:
		{
			IMG_UINT32	uLDDestBank;

			/*
				Only TEMP and PA banks available for LDST instructions
			*/
			uLDDestBank = psHWInst->uWord1 & ~EURASIA_USE1_LDST_DBANK_CLRMSK;
			switch (uLDDestBank)
			{
				case EURASIA_USE1_LDST_DBANK_TEMP:
				{
					psReg->eType = USP_REGTYPE_TEMP;
					break;
				}
				case EURASIA_USE1_LDST_DBANK_PRIMATTR:
				{
					psReg->eType = USP_REGTYPE_PA;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		case USP_OPCODE_LDR:
		{
			IMG_UINT32	uLDRDestBank;

			/*
				Only TEMP and PA banks available for LDRSTR instructions
			*/
			uLDRDestBank = psHWInst->uWord1 & ~EURASIA_USE1_LDRSTR_DBANK_CLRMSK;
			switch (uLDRDestBank)
			{
				case EURASIA_USE1_LDRSTR_DBANK_TEMP:
				{
					psReg->eType = USP_REGTYPE_TEMP;
					break;
				}
				case EURASIA_USE1_LDRSTR_DBANK_PRIMATTR:
				{
					psReg->eType = USP_REGTYPE_PA;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		default:
		{
			IMG_UINT32	uHWBankExt;

			/*
				All other opcodes use the usual destination bank encoding
			*/
			uHWRegBank = (psHWInst->uWord1 & ~EURASIA_USE1_D1BANK_CLRMSK) >> 
						 EURASIA_USE1_D1BANK_SHIFT;

			/*
				Ignore extended destination banks on instructions that do not
				support them
			*/
			if	(
					#if defined(SGX_FEATURE_USE_NORMALISE)
					(eOpcode == USP_OPCODE_FNRM) ||
					#endif /* #if defined(SGX_FEATURE_USE_NORMALISE) */
					#if !defined(SGX_FEATURE_USE_VEC34)
					(eOpcode == USP_OPCODE_EFO) ||
					(eOpcode == USP_OPCODE_DDP) ||
					(eOpcode == USP_OPCODE_DDPC) ||
					#endif /* !defined(SGX_FEATURE_USE_VEC34) */
					(eOpcode == USP_OPCODE_ATST8)
				)
			{
				uHWBankExt = 0;
			}
			else
			{
				uHWBankExt = psHWInst->uWord1 & EURASIA_USE1_DBEXT;
			}

			if	(uHWBankExt)
			{
				switch (uHWRegBank)
				{
					case EURASIA_USE1_D1EXTBANK_SECATTR:
					{
						psReg->eType = USP_REGTYPE_SA;
						break;
					}

					case EURASIA_USE1_D1EXTBANK_SPECIAL:
					{
						psReg->eType = USP_REGTYPE_SPECIAL;
						break;
					}

					case EURASIA_USE1_D1EXTBANK_INDEX:
					{
						psReg->eType = USP_REGTYPE_INDEX;
						break;
					}

					#if defined(SGX_FEATURE_USE_VEC34)
					case SGXVEC_USE1_D1EXTBANK_INDEXED_IH:
					#else
					case EURASIA_USE1_D1EXTBANK_FPINTERNAL:
					#endif /* defined(SGX_FEATURE_USE_VEC34) */
					{
						psReg->eType = USP_REGTYPE_INTERNAL;
						break;
					}

					default:
					{
						return IMG_FALSE;
					}
				}
			}
			else
			{
				switch (uHWRegBank)
				{
					#if !defined(SGX_FEATURE_USE_VEC34)
					case EURASIA_USE1_D1STDBANK_TEMP:
					{
						psReg->eType = USP_REGTYPE_TEMP;
						break;
					}
					
					#else
					case EURASIA_USE1_D1STDBANK_TEMP:
					{
						IMG_UINT32	uMaxTempRegNum;

						/*
							The top 4 temporaries map to the internal registers
							for dest for sgx543. Check whether we are addressing one of
							them.
						*/
						if	(eFmtCtl != USP_FMTCTL_NONE)
						{
							uMaxTempRegNum = EURASIA_USE_FCREGISTER_NUMBER_MAX;
							if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
							{
								uMaxTempRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
							}
						}
						else
						{
							uMaxTempRegNum = EURASIA_USE_REGISTER_NUMBER_MAX;
						}

						uMaxTempRegNum -= EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;

						if	(uHWRegNum < uMaxTempRegNum)
						{
							psReg->eType = USP_REGTYPE_TEMP;
						}
						else
						{
							psReg->eType = USP_REGTYPE_INTERNAL;
							uHWRegNum -= uMaxTempRegNum;
						}

						break;
					}
					#endif /* !defined(SGX_FEATURE_USE_VEC34) */

					case EURASIA_USE1_D1STDBANK_OUTPUT:
					{
						psReg->eType = USP_REGTYPE_OUTPUT;
						break;
					}

					case EURASIA_USE1_D1STDBANK_PRIMATTR:
					{
						psReg->eType = USP_REGTYPE_PA;
						break;
					}

					#if defined(SGX_FEATURE_USE_VEC34)
					case SGXVEC_USE1_D1STDBANK_INDEXED_IL:
					#else
					case EURASIA_USE1_D1STDBANK_INDEXED:
					#endif /* defined(SGX_FEATURE_USE_VEC34) */
					{
						HWInstDecodeIndexedOperandNum(eFmtCtl,
													  uHWRegNum,
													  &psReg->eType,
													  &psReg->uNum,
													  &psReg->eDynIdx);
						break;
					}

					default:
					{
						return IMG_FALSE;
					}
				}
			}

			break;
		}
	}

	/*
		Disable per-operand format control options for inappropriate 
		register types

		NB: For format control on destination with internal registers
	*/
	if	(
			(psReg->eType == USP_REGTYPE_INDEX) ||
			(psReg->eType == USP_REGTYPE_SPECIAL) ||
			(psReg->eType == USP_REGTYPE_IMM) ||
			(psReg->eType == USP_REGTYPE_INTERNAL)
		)
	{
		eFmtCtl = USP_FMTCTL_NONE;
	}

	/*
		Decode the register format if appropriate
	*/
	switch (eFmtCtl)
	{
		case USP_FMTCTL_F32_OR_F16:
		{
			uHWFmtCtl = uHWRegNum & EURASIA_USE_FMTSELECT;

			if	(uHWFmtCtl)
			{
				psReg->eFmt = USP_REGFMT_F16;
			}
			else
			{
				psReg->eFmt = USP_REGFMT_F32;
			}

			uHWRegNum &= ~EURASIA_USE_FMTSELECT;
			break;
		}

		case USP_FMTCTL_U8_OR_C10:
		{
			uHWFmtCtl = uHWRegNum & EURASIA_USE_FMTSELECT;

			if	(uHWFmtCtl)
			{
				psReg->eFmt = USP_REGFMT_C10;
			}
			else
			{
				psReg->eFmt = USP_REGFMT_U8;
			}

			uHWRegNum &= ~EURASIA_USE_FMTSELECT;
			break;
		}

		case USP_FMTCTL_NONE:
		default:
		{
			psReg->eFmt = USP_REGFMT_UNKNOWN;
			uHWFmtCtl = 0;

			break;
		}
	}

	/*
		If the LSB of the register number indicates which half the instruction
		is to use for F16 data, then remove this bit.
	*/
	psReg->uComp = 0;

	if	(
			(psReg->eDynIdx == USP_DYNIDX_NONE) &&
			(eFmtCtl == USP_FMTCTL_F32_OR_F16) &&
			(uHWFmtCtl != 0)
		)
	{
		if	(uHWRegNum & EURASIA_USE_FMTF16_SELECTHIGH)
		{
			psReg->uComp = 2;
		}
		else
		{
			psReg->uComp = 0;
		}

		uHWRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
	}

	psReg->uNum = uHWRegNum;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstEncodeSrc0BankAndNum

 Purpose:	Encode the source 0 register bank and number used by a HW inst
			instruction.

 Inputs:	eFmtCtl			- The per-operand format-control available
			eOpcode			- Opcode of the HW instruction being encoded
			bCanUseExtBanks	- Indicates whether extended banks are available
			psHWInst		- The HW instruction to change
			psReg			- The register to encode

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeSrc0BankAndNum(USP_FMTCTL	eFmtCtl,
												 USP_OPCODE	eOpcode,
												 IMG_BOOL	bCanUseExtBanks,
												 PHW_INST	psHWInst,
												 PUSP_REG	psReg)
{
	USP_REGTYPE	eRegType;
	IMG_UINT32	uRegNum;
	IMG_UINT32	uHWRegNum;
	IMG_UINT32	uHWRegBank;
	IMG_UINT32	uHWBankExt;

	/*
		Dynamic indexing not supported for HW source 0
	*/
	if	(psReg->eDynIdx != USP_DYNIDX_NONE)
	{
		return IMG_FALSE;
	}

	/*
		Disable per-operand format control options for inappropriate 
		register types
	*/
	if	(
			(psReg->eType == USP_REGTYPE_INDEX) ||
			(psReg->eType == USP_REGTYPE_SPECIAL) ||
			(psReg->eType == USP_REGTYPE_IMM) ||
			(psReg->eType == USP_REGTYPE_INTERNAL)
		)
	{
		eFmtCtl = USP_FMTCTL_NONE;
	}

	/*
		For source 0, the internal registers are mapped to the
		last 4 temporaires.
	*/
	eRegType = psReg->eType;
	uRegNum	 = psReg->uNum;

	if	(eRegType == USP_REGTYPE_INTERNAL)
	{
		IMG_UINT32	uMaxTempRegNum;

		/*
			The top 4 temporaries map to the internal registers
			for source 0. 
		*/
		if	(eFmtCtl != USP_FMTCTL_NONE)
		{
			uMaxTempRegNum = EURASIA_USE_FCREGISTER_NUMBER_MAX;
			if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
			{
				uMaxTempRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
			}
		}
		else
		{
			uMaxTempRegNum = EURASIA_USE_REGISTER_NUMBER_MAX;
		}

		uMaxTempRegNum -= EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;

		uRegNum += uMaxTempRegNum;
		eRegType = USP_REGTYPE_TEMP;
	}

	/*
		Encode the HW register number
	*/
	uHWRegNum = uRegNum;

	if	(eFmtCtl == USP_FMTCTL_NONE)
	{
		if	(uHWRegNum >= EURASIA_USE_REGISTER_NUMBER_MAX)
		{
			return IMG_FALSE;
		}
	}
	else
	{
		if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
		{
			if	(psReg->eFmt == USP_REGFMT_F16)
			{
				uHWRegNum <<= EURASIA_USE_FMTF16_REGNUM_SHIFT;
				switch (psReg->uComp)
				{
					case 0:
					{
						uHWRegNum |= EURASIA_USE_FMTF16_SELECTLOW;
						break;
					}
					case 2:
					{
						uHWRegNum |= EURASIA_USE_FMTF16_SELECTHIGH;
					}
					default:
					{
						return IMG_FALSE;
					}
				}
			}
		}

		if	(uHWRegNum >= EURASIA_USE_FCREGISTER_NUMBER_MAX)
		{
			return IMG_FALSE;
		}
	}

	/*
		Encode the format-select bit if this instruction allows per-operand
		format control.
	*/
	switch (eFmtCtl)
	{
		case USP_FMTCTL_F32_OR_F16:
		{
			switch (psReg->eFmt)
			{
				case USP_REGFMT_F32:
				{
					uHWRegNum &= ~EURASIA_USE_FMTSELECT;
					break;
				}
				case USP_REGFMT_F16:
				{
					uHWRegNum |= EURASIA_USE_FMTSELECT;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		case USP_FMTCTL_U8_OR_C10:
		{
			switch (psReg->eFmt)
			{
				case USP_REGFMT_U8:
				{
					uHWRegNum &= ~EURASIA_USE_FMTSELECT;
					break;
				}
				case USP_REGFMT_C10:
				{
					uHWRegNum |= EURASIA_USE_FMTSELECT;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		case USP_FMTCTL_NONE:
		default:
		{
			break;
		}
	}

	/*
		Encode the register-bank
	*/
	switch (eRegType)
	{
		case USP_REGTYPE_TEMP:
		{
			uHWRegBank = EURASIA_USE1_S0STDBANK_TEMP;
			uHWBankExt = 0;
			break;
		}

		case USP_REGTYPE_PA:
		{
			uHWRegBank = EURASIA_USE1_S0STDBANK_PRIMATTR;
			uHWBankExt = 0;
			break;
		}

		case USP_REGTYPE_SA:
		{
			uHWRegBank = EURASIA_USE1_S0EXTBANK_SECATTR;
			uHWBankExt = EURASIA_USE1_S0BEXT;
			break;
		}

		case USP_REGTYPE_OUTPUT:
		{
			uHWRegBank = EURASIA_USE1_S0EXTBANK_OUTPUT;
			uHWBankExt = EURASIA_USE1_S0BEXT;
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	if	(uHWBankExt && !bCanUseExtBanks)
	{
		return IMG_FALSE;
	}

	/*
		Write the encoded HW bank.
	*/
	switch (eOpcode)
	{
		case USP_OPCODE_FIRH:
		case USP_OPCODE_FIRV:
		{
			/*
				FIRV and FIRH encode the offset for Src0, but no bank. The bank for
				Src0 is taken from Src1 (i.e. Src0 and Src1 must use the same bank).
			*/
			break;
		}

		default:
		{
			/*
				All other opcodes encode Src0 with an option extended bank selection

				NB: We assume that the USP never tries to encode Src0 for an instruction
					that doesn't encode Src0 at all.
			*/
			if	(bCanUseExtBanks)
			{
				psHWInst->uWord1 &= (~EURASIA_USE1_S0BEXT);
				psHWInst->uWord1 |= uHWBankExt;
			}

			psHWInst->uWord1 &= EURASIA_USE1_S0BANK_CLRMSK;
			psHWInst->uWord1 |= (uHWRegBank << EURASIA_USE1_S0BANK_SHIFT) &
								(~EURASIA_USE1_S0BANK_CLRMSK);
			break;
		}
	}

	/*
		Write the encoded HW number into the instruction
	*/
	HWInstEncodeSrcNumber(eOpcode,
						  psHWInst,
						  eRegType,
						  psReg->eFmt,
						  uHWRegNum,
						  USP_SRCARG_SRC0);

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstDecodeSrc0BankAndNum

 Purpose:	Decode the source 0 register bank and number used by a
			HW instruction.

 Inputs:	eFmtCtl			- The per-operand format-control available
			bCanUseExtBanks	- Indicates whether extended banks are available
			psHWInst		- The HW instruction to examine

 Outputs:	psReg			- The decoded register

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstDecodeSrc0BankAndNum(USP_FMTCTL	eFmtCtl,
												 IMG_BOOL	bCanUseExtBanks,
												 PHW_INST	psHWInst,
												 PUSP_REG	psReg)
{
	IMG_UINT32	uHWRegBank;
	IMG_UINT32	uHWRegNum;
	IMG_UINT32	uHWFmtCtl;

	/*
		Extract the source 0 register-bank and number
	*/
	uHWRegBank = (psHWInst->uWord1 & ~EURASIA_USE1_S0BANK_CLRMSK) >> 
				 EURASIA_USE1_S0BANK_SHIFT;

	uHWRegNum = (psHWInst->uWord0 & ~EURASIA_USE0_SRC0_CLRMSK) >>
				EURASIA_USE0_SRC0_SHIFT;

	/*
		Decode the HW register bank
	*/
	psReg->eDynIdx = USP_DYNIDX_NONE;

	if	(bCanUseExtBanks && (psHWInst->uWord1 & EURASIA_USE1_S0BEXT))
	{
		switch (uHWRegBank)
		{
			case EURASIA_USE1_S0EXTBANK_OUTPUT:
			{
				psReg->eType = USP_REGTYPE_OUTPUT;
				break;
			}
			case EURASIA_USE1_S0EXTBANK_SECATTR:
			{
				psReg->eType = USP_REGTYPE_SA;
				break;
			}
			default:
			{
				return IMG_FALSE;
			}
		}
	}
	else
	{
		switch (uHWRegBank)
		{
			case EURASIA_USE1_S0STDBANK_TEMP:
			{
				IMG_UINT32	uMaxTempRegNum;

				/*
					The top 4 temporaries map to the internal registers
					for source 0. Check whether we are addressing one of
					them.
				*/
				if	(eFmtCtl != USP_FMTCTL_NONE)
				{
					uMaxTempRegNum = EURASIA_USE_FCREGISTER_NUMBER_MAX;
					if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
					{
						uMaxTempRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
					}
				}
				else
				{
					uMaxTempRegNum = EURASIA_USE_REGISTER_NUMBER_MAX;
				}

				uMaxTempRegNum -= EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;

				if	(uHWRegNum < uMaxTempRegNum)
				{
					psReg->eType = USP_REGTYPE_TEMP;
				}
				else
				{
					psReg->eType = USP_REGTYPE_INTERNAL;
					uHWRegNum -= uMaxTempRegNum;
				}

				break;
			}

			case EURASIA_USE1_S0STDBANK_PRIMATTR:
			{
				psReg->eType = USP_REGTYPE_PA;
				break;
			}

			default:
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Disable per-operand format control options for inappropriate 
		register types
	*/
	if	(
			(psReg->eType == USP_REGTYPE_INDEX) ||
			(psReg->eType == USP_REGTYPE_SPECIAL) ||
			(psReg->eType == USP_REGTYPE_IMM) ||
			(psReg->eType == USP_REGTYPE_INTERNAL)
		)
	{
		eFmtCtl = USP_FMTCTL_NONE;
	}

	/*
		Decode the register format if appropriate
	*/
	switch (eFmtCtl)
	{
		case USP_FMTCTL_F32_OR_F16:
		{
			uHWFmtCtl = uHWRegNum & EURASIA_USE_FMTSELECT;

			if	(uHWFmtCtl)
			{
				psReg->eFmt = USP_REGFMT_F16;
			}
			else
			{
				psReg->eFmt = USP_REGFMT_F32;
			}

			uHWRegNum &= ~EURASIA_USE_FMTSELECT;
			break;
		}

		case USP_FMTCTL_U8_OR_C10:
		{
			uHWFmtCtl = uHWRegNum & EURASIA_USE_FMTSELECT;

			if	(uHWFmtCtl)
			{
				psReg->eFmt = USP_REGFMT_C10;
			}
			else
			{
				psReg->eFmt = USP_REGFMT_U8;
			}

			uHWRegNum &= ~EURASIA_USE_FMTSELECT;
			break;
		}

		case USP_FMTCTL_NONE:
		default:
		{
			psReg->eFmt = USP_REGFMT_UNKNOWN;
			uHWFmtCtl = 0;

			break;
		}
	}

	/*
		If the LSB of the register number indicates which half the instruction
		is to use for F16 data, then remove this bit.
	*/
	psReg->uComp = 0;

	if	(
			(eFmtCtl == USP_FMTCTL_F32_OR_F16) &&
			(uHWFmtCtl != 0)
		)
	{
		if	(uHWRegNum & EURASIA_USE_FMTF16_SELECTHIGH)
		{
			psReg->uComp = 2;
		}
		else
		{
			psReg->uComp = 0;
		}

		uHWRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
	}

	psReg->uNum = uHWRegNum;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstEncodeSrc1BankAndNum

 Purpose:	Encode the source 1 register bank and number used by a
			HW instruction.

 Inputs:	eFmtCtl		- The per-operand format-control available
			eOpcode		- The op code of instruction
			psHWInst	- The HW instruction to change
			psReg		- The register details to encode

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeSrc1BankAndNum(USP_FMTCTL	eFmtCtl,
												 USP_OPCODE	eOpcode,
												 PHW_INST	psHWInst,
												 PUSP_REG	psReg)
{
	IMG_UINT32	uHWRegNum;
	IMG_UINT32	uHWRegBank;
	IMG_UINT32	uHWBankExt;
	USP_REGTYPE	eRegType;
	IMG_UINT32	uRegNum;

	#if !defined(SGX_FEATURE_USE_VEC34)
	PVR_UNREFERENCED_PARAMETER(eOpcode);
	#endif /* !defined(SGX543) */

	/*
		Disable per-operand format control options for inappropriate 
		register types
	*/
	if	(
			(psReg->eType == USP_REGTYPE_INDEX) ||
			(psReg->eType == USP_REGTYPE_SPECIAL) ||
			(psReg->eType == USP_REGTYPE_IMM)
		)
	{
		eFmtCtl = USP_FMTCTL_NONE;
	}

	/*
		For source 1 for sgx543, the internal registers are mapped to the
		top 4 temporaires.
	*/
	eRegType = psReg->eType;
	uRegNum	 = psReg->uNum;

	#if defined(SGX_FEATURE_USE_VEC34)
	if	(eRegType == USP_REGTYPE_INTERNAL)
	{
		IMG_UINT32	uMaxTempRegNum;
		IMG_UINT32	uNumTempsMappedToFPI = EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;

		/*
			The top 4 temporaries map to the internal registers
			for source 0. 
		*/
		if	(eFmtCtl != USP_FMTCTL_NONE)
		{
			uMaxTempRegNum = EURASIA_USE_FCREGISTER_NUMBER_MAX;
			if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
			{
				uMaxTempRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
			}
		}
		else
		{
			uMaxTempRegNum = EURASIA_USE_REGISTER_NUMBER_MAX;
		}

		if ((eOpcode == USP_OPCODE_PCKUNPCK) && (psReg->eFmt != USP_REGFMT_U8))
		{
			uNumTempsMappedToFPI <<= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
			uRegNum <<= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
		}

		uMaxTempRegNum -= uNumTempsMappedToFPI;

		uRegNum += uMaxTempRegNum;
		eRegType = USP_REGTYPE_TEMP;
	}
	#endif /* defined (SGX_FEATURE_USE_VEC34) */

	/*
		Encode the HW register number
	*/
	if	(psReg->eDynIdx != USP_DYNIDX_NONE)
	{
		if	(!HWInstEncodeIndexedOperandNum(eFmtCtl,
											eRegType,
											uRegNum,
											psReg->eDynIdx,
											&uHWRegNum))
		{
			return IMG_FALSE;
		}
	}
	else
	{
		uHWRegNum = uRegNum;

		if	(eFmtCtl == USP_FMTCTL_NONE)
		{
			if	(uHWRegNum >= EURASIA_USE_REGISTER_NUMBER_MAX)
			{
				return IMG_FALSE;
			}
		}
		else
		{
			if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
			{
				if	(psReg->eFmt == USP_REGFMT_F16)
				{
					uHWRegNum <<= EURASIA_USE_FMTF16_REGNUM_SHIFT;

					switch (psReg->uComp)
					{
						case 0:
						{
							uHWRegNum |= EURASIA_USE_FMTF16_SELECTLOW;
							break;
						}
						case 2:
						{
							uHWRegNum |= EURASIA_USE_FMTF16_SELECTHIGH;
						}
						default:
						{
							return IMG_FALSE;
						}
					}
				}
			}

			if	(uHWRegNum >= EURASIA_USE_FCREGISTER_NUMBER_MAX)
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Encode the format-select bit if this instruction allows per-operand
		format control.
	*/
	switch (eFmtCtl)
	{
		case USP_FMTCTL_F32_OR_F16:
		{
			switch (psReg->eFmt)
			{
				case USP_REGFMT_F32:
				{
					uHWRegNum &= ~EURASIA_USE_FMTSELECT;
					break;
				}
				case USP_REGFMT_F16:
				{
					uHWRegNum |= EURASIA_USE_FMTSELECT;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		case USP_FMTCTL_U8_OR_C10:
		{
			switch (psReg->eFmt)
			{
				case USP_REGFMT_U8:
				{
					uHWRegNum &= ~EURASIA_USE_FMTSELECT;
					break;
				}
				case USP_REGFMT_C10:
				{
					uHWRegNum |= EURASIA_USE_FMTSELECT;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		case USP_FMTCTL_NONE:
		default:
		{
			break;
		}
	}

	/*
		Encode the register-bank
	*/
	if	(psReg->eDynIdx != USP_DYNIDX_NONE)
	{
		#if defined(SGX_FEATURE_USE_VEC34)
		uHWRegBank = SGXVEC_USE0_S1EXTBANK_INDEXED_IL;
		uHWBankExt = EURASIA_USE1_S1BEXT;	
		#else
		uHWRegBank = EURASIA_USE0_S1EXTBANK_INDEXED;
		uHWBankExt = EURASIA_USE1_S1BEXT;
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
	}
	else
	{
		switch (eRegType)
		{
			case USP_REGTYPE_TEMP:
			{
				uHWRegBank = EURASIA_USE0_S1STDBANK_TEMP;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_PA:
			{
				uHWRegBank = EURASIA_USE0_S1STDBANK_PRIMATTR;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_SA:
			{
				uHWRegBank = EURASIA_USE0_S1STDBANK_SECATTR;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_OUTPUT:
			{
				uHWRegBank = EURASIA_USE0_S1STDBANK_OUTPUT;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_INTERNAL:
			{
				#if defined(SGX_FEATURE_USE_VEC34)
				return IMG_FALSE;
				#else
				uHWRegBank = EURASIA_USE0_S1EXTBANK_FPINTERNAL;				
				uHWBankExt = EURASIA_USE1_S1BEXT;
				#endif /* defined(SGX_FEATURE_USE_VEC34) */
				break;
			}

			case USP_REGTYPE_IMM:
			{
				uHWRegBank = EURASIA_USE0_S1EXTBANK_IMMEDIATE;
				uHWBankExt = EURASIA_USE1_S1BEXT;
				break;
			}

			case USP_REGTYPE_SPECIAL:
			{
				uHWRegBank = EURASIA_USE0_S1EXTBANK_SPECIAL;
				uHWBankExt = EURASIA_USE1_S1BEXT;
				break;
			}

			default:
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Write the encoded HW bank and number into the instruction
	*/
	psHWInst->uWord1 &= (~EURASIA_USE1_S1BEXT);
	psHWInst->uWord1 |= uHWBankExt;

	psHWInst->uWord0 &= EURASIA_USE0_S1BANK_CLRMSK;
	psHWInst->uWord0 |= (uHWRegBank << EURASIA_USE0_S1BANK_SHIFT) &
						(~EURASIA_USE0_S1BANK_CLRMSK);

	HWInstEncodeSrcNumber(eOpcode,
						  psHWInst,
						  eRegType,
						  psReg->eFmt,
						  uHWRegNum,
						  USP_SRCARG_SRC1);

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstDecodeSrc1BankAndNum

 Purpose:	Decode the source 1 register bank and number from a HW
			instruction

 Inputs:	eFmtCtl		- The per-operand format-control available
			psHWInst	- The HW instruction to examine

 Outputs:	psReg		- The decoded register details

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstDecodeSrc1BankAndNum(USP_FMTCTL	eFmtCtl,
												 PHW_INST	psHWInst,
												 PUSP_REG	psReg)
{
	IMG_UINT32	uHWRegBank;
	IMG_UINT32	uHWRegNum;
	IMG_UINT32	uHWFmtCtl;

	/*
		Extract the source 1 register-bank and number
	*/
	uHWRegBank = (psHWInst->uWord0 & ~EURASIA_USE0_S1BANK_CLRMSK) >> 
				 EURASIA_USE0_S1BANK_SHIFT;

	uHWRegNum = (psHWInst->uWord0 & ~EURASIA_USE0_SRC1_CLRMSK) >>
				EURASIA_USE0_SRC1_SHIFT;

	/*
		Decode the HW register bank
	*/
	psReg->eDynIdx = USP_DYNIDX_NONE;

	if	(psHWInst->uWord1 & EURASIA_USE1_S1BEXT)
	{
		switch (uHWRegBank)
		{
			#if defined(SGX_FEATURE_USE_VEC34)
			case SGXVEC_USE0_S1EXTBANK_INDEXED_IL:
			#else
			case EURASIA_USE0_S1EXTBANK_INDEXED:
			#endif /* defined(SGX_FEATURE_USE_VEC34) */
			{
				HWInstDecodeIndexedOperandNum(eFmtCtl,
											  uHWRegNum,
											  &psReg->eType,
											  &psReg->uNum,
											  &psReg->eDynIdx);
				break;
			}

			case EURASIA_USE0_S1EXTBANK_SPECIAL:
			{
				psReg->eType = USP_REGTYPE_SPECIAL;
				break;
			}

			case EURASIA_USE0_S1EXTBANK_IMMEDIATE:
			{
				psReg->eType = USP_REGTYPE_IMM;
				break;
			}

			#if defined(SGX_FEATURE_USE_VEC34)
			case SGXVEC_USE0_S1EXTBANK_INDEXED_IH:
			#else
			case EURASIA_USE0_S1EXTBANK_FPINTERNAL:
			#endif /* defined(SGX_FEATURE_USE_VEC34)*/
			{
				psReg->eType = USP_REGTYPE_INTERNAL;
				break;
			}

			default:
			{
				return IMG_FALSE;
			}
		}
	}
	else
	{
		switch (uHWRegBank)
		{
			#if defined(SGX_FEATURE_USE_VEC34)
			case EURASIA_USE0_S1STDBANK_TEMP:
			{
				IMG_UINT32	uMaxTempRegNum;

				/*
					The top 4 temporaries map to the internal registers
					for source 1 for sgx543. Check whether we are addressing one of
					them.
				*/
				if	(eFmtCtl != USP_FMTCTL_NONE)
				{
					uMaxTempRegNum = EURASIA_USE_FCREGISTER_NUMBER_MAX;
					if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
					{
						uMaxTempRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
					}
				}
				else
				{
					uMaxTempRegNum = EURASIA_USE_REGISTER_NUMBER_MAX;
				}

				uMaxTempRegNum -= EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;

				if	(uHWRegNum < uMaxTempRegNum)
				{
					psReg->eType = USP_REGTYPE_TEMP;
				}
				else
				{
					psReg->eType = USP_REGTYPE_INTERNAL;
					uHWRegNum -= uMaxTempRegNum;
				}
				break;
			}
			#else
			case EURASIA_USE0_S1STDBANK_TEMP:
			{
				psReg->eType = USP_REGTYPE_TEMP;
				break;
			}
			#endif /* defined(SGX_FEATURE_USE_VEC34) */
			case EURASIA_USE0_S1STDBANK_OUTPUT:
			{
				psReg->eType = USP_REGTYPE_OUTPUT;
				break;
			}

			case EURASIA_USE0_S1STDBANK_PRIMATTR:
			{
				psReg->eType = USP_REGTYPE_PA;
				break;
			}

			case EURASIA_USE0_S1STDBANK_SECATTR:
			{
				psReg->eType = USP_REGTYPE_SA;
				break;
			}

			default:
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Disable per-operand format control options for inappropriate 
		register types
	*/
	if	(
			(psReg->eType == USP_REGTYPE_INDEX) ||
			(psReg->eType == USP_REGTYPE_SPECIAL) ||
			(psReg->eType == USP_REGTYPE_IMM) ||
			(psReg->eType == USP_REGTYPE_INTERNAL)
		)
	{
		eFmtCtl = USP_FMTCTL_NONE;
	}

	/*
		Decode the register format if appropriate
	*/
	switch (eFmtCtl)
	{
		case USP_FMTCTL_F32_OR_F16:
		{
			uHWFmtCtl = uHWRegNum & EURASIA_USE_FMTSELECT;

			if	(uHWFmtCtl)
			{
				psReg->eFmt = USP_REGFMT_F16;
			}
			else
			{
				psReg->eFmt = USP_REGFMT_F32;
			}

			uHWRegNum &= ~EURASIA_USE_FMTSELECT;
			break;
		}

		case USP_FMTCTL_U8_OR_C10:
		{
			uHWFmtCtl = uHWRegNum & EURASIA_USE_FMTSELECT;

			if	(uHWFmtCtl)
			{
				psReg->eFmt = USP_REGFMT_C10;
			}
			else
			{
				psReg->eFmt = USP_REGFMT_U8;
			}

			uHWRegNum &= ~EURASIA_USE_FMTSELECT;
			break;
		}

		case USP_FMTCTL_NONE:
		default:
		{
			psReg->eFmt = USP_REGFMT_UNKNOWN;
			uHWFmtCtl = 0;

			break;
		}
	}

	/*
		If the LSB of the register number indicates which half the instruction
		is to use for F16 data, then remove this bit.
	*/
	psReg->uComp = 0;

	if	(
			(psReg->eDynIdx == USP_DYNIDX_NONE) &&
			(eFmtCtl == USP_FMTCTL_F32_OR_F16) &&
			(uHWFmtCtl != 0)
		)
	{
		if	(uHWRegNum & EURASIA_USE_FMTF16_SELECTHIGH)
		{
			psReg->uComp = 2;
		}
		else
		{
			psReg->uComp = 0;
		}

		uHWRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
	}

	psReg->uNum = uHWRegNum;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstEncodeSrc2BankAndNum

 Purpose:	Encode the source 2 register bank and number used by a
			HW instruction.

 Inputs:	eFmtCtl		- The per-operand format-control available
			eOpcode		- The op code of instruction
			psHWInst	- The HW instruction to change
			psReg		- The register to encode

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeSrc2BankAndNum(USP_FMTCTL	eFmtCtl,
												 USP_OPCODE	eOpcode,
							  					 PHW_INST	psHWInst,
												 PUSP_REG	psReg)
{
	IMG_UINT32	uHWRegNum;
	IMG_UINT32	uHWRegBank;
	IMG_UINT32	uHWBankExt;
	USP_REGTYPE	eRegType;
	IMG_UINT32	uRegNum;

	#if !defined(SGX_FEATURE_USE_VEC34)
	PVR_UNREFERENCED_PARAMETER(eOpcode);
	#endif /* !defined(SGX543) */

	/*
		Disable per-operand format control options for inappropriate 
		register types
	*/
	if	(
			(psReg->eType == USP_REGTYPE_INDEX) ||
			(psReg->eType == USP_REGTYPE_SPECIAL) ||
			(psReg->eType == USP_REGTYPE_IMM) ||
			(psReg->eType == USP_REGTYPE_INTERNAL)
		)
	{
		eFmtCtl = USP_FMTCTL_NONE;
	}

	eRegType = psReg->eType;
	uRegNum	 = psReg->uNum;

	#if defined(SGX_FEATURE_USE_VEC34)
	if	(eRegType == USP_REGTYPE_INTERNAL)
	{
		IMG_UINT32	uMaxTempRegNum;
		IMG_UINT32	uNumTempsMappedToFPI = EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;

		/*
			The top 4 temporaries map to the internal registers
			for source 0. 
		*/
		if	(eFmtCtl != USP_FMTCTL_NONE)
		{
			uMaxTempRegNum = EURASIA_USE_FCREGISTER_NUMBER_MAX;
			if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
			{
				uMaxTempRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
			}
		}
		else
		{
			uMaxTempRegNum = EURASIA_USE_REGISTER_NUMBER_MAX;
		}

		if ((eOpcode == USP_OPCODE_PCKUNPCK) && (psReg->eFmt != USP_REGFMT_U8))
		{
			uNumTempsMappedToFPI <<= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
			uRegNum <<= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
		}

		uMaxTempRegNum -= uNumTempsMappedToFPI;

		uRegNum += uMaxTempRegNum;
		eRegType = USP_REGTYPE_TEMP;
	}
	#endif /* defined (SGX_FEATURE_USE_VEC34) */

	/*
		Encode the HW register number
	*/
	if	(psReg->eDynIdx != USP_DYNIDX_NONE)
	{
		if	(!HWInstEncodeIndexedOperandNum(eFmtCtl,
											eRegType,
											uRegNum,
											psReg->eDynIdx,
											&uHWRegNum))
		{
			return IMG_FALSE;
		}
	}
	else
	{
		uHWRegNum = uRegNum;

		if	(eFmtCtl == USP_FMTCTL_NONE)
		{
			if	(uHWRegNum >= EURASIA_USE_REGISTER_NUMBER_MAX)
			{
				return IMG_FALSE;
			}
		}
		else
		{
			if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
			{
				if	(psReg->eFmt == USP_REGFMT_F16)
				{
					uHWRegNum <<= EURASIA_USE_FMTF16_REGNUM_SHIFT;

					switch (psReg->uComp)
					{
						case 0:
						{
							uHWRegNum |= EURASIA_USE_FMTF16_SELECTLOW;
							break;
						}
						case 2:
						{
							uHWRegNum |= EURASIA_USE_FMTF16_SELECTHIGH;
						}
						default:
						{
							return IMG_FALSE;
						}
					}
				}
			}

			if	(uHWRegNum >= EURASIA_USE_FCREGISTER_NUMBER_MAX)
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Encode the format-select bit if this instruction allows per-operand
		format control.
	*/
	switch (eFmtCtl)
	{
		case USP_FMTCTL_F32_OR_F16:
		{
			switch (psReg->eFmt)
			{
				case USP_REGFMT_F32:
				{
					uHWRegNum &= ~EURASIA_USE_FMTSELECT;
					break;
				}
				case USP_REGFMT_F16:
				{
					uHWRegNum |= EURASIA_USE_FMTSELECT;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		case USP_FMTCTL_U8_OR_C10:
		{
			switch (psReg->eFmt)
			{
				case USP_REGFMT_U8:
				{
					uHWRegNum &= ~EURASIA_USE_FMTSELECT;
					break;
				}
				case USP_REGFMT_C10:
				{
					uHWRegNum |= EURASIA_USE_FMTSELECT;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
			break;
		}

		case USP_FMTCTL_NONE:
		default:
		{
			break;
		}
	}

	/*
		Encode the register-bank
	*/
	if	(psReg->eDynIdx != USP_DYNIDX_NONE)
	{
		#if defined(SGX_FEATURE_USE_VEC34)
		uHWRegBank = SGXVEC_USE0_S2EXTBANK_INDEXED_IL;
		uHWBankExt = EURASIA_USE1_S2BEXT;
		#else
		uHWRegBank = EURASIA_USE0_S2EXTBANK_INDEXED;
		uHWBankExt = EURASIA_USE1_S2BEXT;
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
	}
	else
	{
		switch (eRegType)
		{
			case USP_REGTYPE_TEMP:
			{
				uHWRegBank = EURASIA_USE0_S2STDBANK_TEMP;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_PA:
			{
				uHWRegBank = EURASIA_USE0_S2STDBANK_PRIMATTR;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_SA:
			{
				uHWRegBank = EURASIA_USE0_S2STDBANK_SECATTR;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_OUTPUT:
			{
				uHWRegBank = EURASIA_USE0_S2STDBANK_OUTPUT;
				uHWBankExt = 0;
				break;
			}

			case USP_REGTYPE_INTERNAL:
			{
				#if defined(SGX_FEATURE_USE_VEC34)
				return IMG_FALSE;
				#else
				uHWRegBank = EURASIA_USE0_S2EXTBANK_FPINTERNAL;				
				uHWBankExt = EURASIA_USE1_S2BEXT;
				#endif /* defined(SGX_FEATURE_USE_VEC34) */
				break;
			}

			case USP_REGTYPE_IMM:
			{
				uHWRegBank = EURASIA_USE0_S2EXTBANK_IMMEDIATE;
				uHWBankExt = EURASIA_USE1_S2BEXT;
				break;
			}

			case USP_REGTYPE_SPECIAL:
			{
				uHWRegBank = EURASIA_USE0_S2EXTBANK_SPECIAL;
				uHWBankExt = EURASIA_USE1_S2BEXT;
				break;
			}

			default:
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Write the encoded HW bank and number into the instruction
	*/
	psHWInst->uWord1 &= (~EURASIA_USE1_S2BEXT);
	psHWInst->uWord1 |= uHWBankExt;

	psHWInst->uWord0 &= EURASIA_USE0_S2BANK_CLRMSK;
	psHWInst->uWord0 |= (uHWRegBank << EURASIA_USE0_S2BANK_SHIFT) &
						(~EURASIA_USE0_S2BANK_CLRMSK);

	HWInstEncodeSrcNumber(eOpcode,
						  psHWInst,
						  eRegType,
						  psReg->eFmt,
						  uHWRegNum,
						  USP_SRCARG_SRC2);

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstDecodeSrc2BankAndNum

 Purpose:	Decode the source 2 register bank and number from a HW
			instruction

 Inputs:	eFmtCtl		- The per-operand format-control available
			psHWInst	- The HW instruction to examine

 Outputs:	psReg		- The decoded register details

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstDecodeSrc2BankAndNum(USP_FMTCTL	eFmtCtl,
												 PHW_INST	psHWInst,
												 PUSP_REG	psReg)
{
	IMG_UINT32	uHWRegBank;
	IMG_UINT32	uHWRegNum;
	IMG_UINT32	uHWFmtCtl;

	/*
		Extract the source 1 register-bank and number
	*/
	uHWRegBank = (psHWInst->uWord0 & ~EURASIA_USE0_S2BANK_CLRMSK) >>
				 EURASIA_USE0_S2BANK_SHIFT;

	uHWRegNum = (psHWInst->uWord0 & ~EURASIA_USE0_SRC2_CLRMSK) >>
				EURASIA_USE0_SRC2_SHIFT;

	/*
		Decode the HW register bank
	*/
	psReg->eDynIdx = USP_DYNIDX_NONE;

	if	(psHWInst->uWord1 & EURASIA_USE1_S2BEXT)
	{
		switch (uHWRegBank)
		{
			#if defined(SGX_FEATURE_USE_VEC34)
			case SGXVEC_USE0_S2EXTBANK_INDEXED_IL:
			#else
			case EURASIA_USE0_S2EXTBANK_INDEXED:
			#endif /* defined(SGX_FEATURE_USE_VEC34) */
			{
				HWInstDecodeIndexedOperandNum(eFmtCtl,
											  uHWRegNum,
											  &psReg->eType,
											  &psReg->uNum,
											  &psReg->eDynIdx);
				break;
			}

			case EURASIA_USE0_S2EXTBANK_SPECIAL:
			{
				psReg->eType = USP_REGTYPE_SPECIAL;
				break;
			}

			case EURASIA_USE0_S2EXTBANK_IMMEDIATE:
			{
				psReg->eType = USP_REGTYPE_IMM;
				break;
			}

			#if defined(SGX_FEATURE_USE_VEC34)
			case SGXVEC_USE0_S2EXTBANK_INDEXED_IH:
			#else
			case EURASIA_USE0_S2EXTBANK_FPINTERNAL:
			#endif /* defined(SGX_FEATURE_USE_VEC34) */
			{
				psReg->eType = USP_REGTYPE_INTERNAL;
				break;
			}

			default:
			{
				return IMG_FALSE;
			}
		}
	}
	else
	{
		switch (uHWRegBank)
		{
			#if defined(SGX_FEATURE_USE_VEC34)
			case EURASIA_USE0_S2STDBANK_TEMP:
			{
				IMG_UINT32	uMaxTempRegNum;

				/*
					The top temporaries map to the internal registers
					for source 1 on sgx543. Check whether we are addressing one of
					them.
				*/
				if	(eFmtCtl != USP_FMTCTL_NONE)
				{
					uMaxTempRegNum = EURASIA_USE_FCREGISTER_NUMBER_MAX;
					if	(eFmtCtl == USP_FMTCTL_F32_OR_F16)
					{
						uMaxTempRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
					}
				}
				else
				{
					uMaxTempRegNum = EURASIA_USE_REGISTER_NUMBER_MAX;
				}

				uMaxTempRegNum -= EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;

				if	(uHWRegNum < uMaxTempRegNum)
				{
					psReg->eType = USP_REGTYPE_TEMP;
				}
				else
				{
					psReg->eType = USP_REGTYPE_INTERNAL;
					uHWRegNum -= uMaxTempRegNum;
				}
				break;
			}
			#else
			case EURASIA_USE0_S2STDBANK_TEMP:
			{
				psReg->eType = USP_REGTYPE_TEMP;
				break;
			}
			#endif /* defined(SGX_FEATURE_USE_VEC34) */

			case EURASIA_USE0_S2STDBANK_OUTPUT:
			{
				psReg->eType = USP_REGTYPE_OUTPUT;
				break;
			}

			case EURASIA_USE0_S2STDBANK_PRIMATTR:
			{
				psReg->eType = USP_REGTYPE_PA;
				break;
			}

			case EURASIA_USE0_S2STDBANK_SECATTR:
			{
				psReg->eType = USP_REGTYPE_SA;
				break;
			}

			default:
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Disable per-operand format control options for inappropriate 
		register types
	*/
	if	(
			(psReg->eType == USP_REGTYPE_INDEX) ||
			(psReg->eType == USP_REGTYPE_SPECIAL) ||
			(psReg->eType == USP_REGTYPE_IMM) ||
			(psReg->eType == USP_REGTYPE_INTERNAL)
		)
	{
		eFmtCtl = USP_FMTCTL_NONE;
	}

	/*
		Decode the register format if appropriate
	*/
	switch (eFmtCtl)
	{
		case USP_FMTCTL_F32_OR_F16:
		{
			uHWFmtCtl = uHWRegNum & EURASIA_USE_FMTSELECT;

			if	(uHWFmtCtl)
			{
				psReg->eFmt = USP_REGFMT_F16;
			}
			else
			{
				psReg->eFmt = USP_REGFMT_F32;
			}

			uHWRegNum &= ~EURASIA_USE_FMTSELECT;
			break;
		}

		case USP_FMTCTL_U8_OR_C10:
		{
			uHWFmtCtl = uHWRegNum & EURASIA_USE_FMTSELECT;

			if	(uHWFmtCtl)
			{
				psReg->eFmt = USP_REGFMT_C10;
			}
			else
			{
				psReg->eFmt = USP_REGFMT_U8;
			}

			uHWRegNum &= ~EURASIA_USE_FMTSELECT;
			break;
		}

		case USP_FMTCTL_NONE:
		default:
		{
			psReg->eFmt = USP_REGFMT_UNKNOWN;
			uHWFmtCtl = 0;

			break;
		}
	}

	/*
		If the LSB of the register number indicates which half the instruction
		is to use for F16 data, then remove this bit.
	*/
	psReg->uComp = 0;

	if	(
			(psReg->eDynIdx == USP_DYNIDX_NONE) &&
			(eFmtCtl == USP_FMTCTL_F32_OR_F16) &&
			(uHWFmtCtl != 0)
		)
	{
		if	(uHWRegNum & EURASIA_USE_FMTF16_SELECTHIGH)
		{
			psReg->uComp = 2;
		}
		else
		{
			psReg->uComp = 0;
		}

		uHWRegNum >>= EURASIA_USE_FMTF16_REGNUM_SHIFT;
	}

	psReg->uNum = uHWRegNum;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstDecodeOperandBankAndNum

 Purpose:	Decode the register banck and number for an operand from a HW
			instruction

			NB: This routine assumes that the specified operand index is
				valid for the instruction (use HWInstGetOperandsUsed). It
				does not decode opcode specific features such as format
				controls or component selections.

 Inputs:	psMOEState		- The decoded MOE state for the instruction
			eOpcode			- The decoded opcode for the HW instruction
			psHWInst		- The HW instruction to examine
			uOperandIdx		- Which operand to decode (see USP_OPERANDIDX_xxx)

 Outputs:	psReg			- The decoded register details

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstDecodeOperandBankAndNum(PUSP_MOESTATE	psMOEState,
													USP_OPCODE		eOpcode,
													PHW_INST		psHWInst,
													IMG_UINT32		uOperandIdx,
													PUSP_REG		psReg)
{
	USP_FMTCTL	eFmtCtl;

	/*
		Determine whether this instruction has per-operand format-control
	*/
	if	(!HWInstGetPerOperandFmtCtl(psMOEState,
									eOpcode,
									psHWInst,
									&eFmtCtl))
	{
		return IMG_FALSE;
	}

	/*
		Decode the requested register bank and number
	*/
	switch (uOperandIdx)
	{
		case USP_OPERANDIDX_DST:
		{
			return HWInstDecodeDestBankAndNum(eFmtCtl,
											  eOpcode,
											  psHWInst,
											  psReg);
		}

		case USP_OPERANDIDX_SRC0:
		{
			IMG_BOOL bCanUseExtSrc0Banks;

			bCanUseExtSrc0Banks = HWInstCanUseExtSrc0Banks(eOpcode);

			return HWInstDecodeSrc0BankAndNum(eFmtCtl,
											  bCanUseExtSrc0Banks,
											  psHWInst,
											  psReg);
		}

		case USP_OPERANDIDX_SRC1:
		{
			return HWInstDecodeSrc1BankAndNum(eFmtCtl, psHWInst, psReg);
		}

		case USP_OPERANDIDX_SRC2:
		{
			return HWInstDecodeSrc2BankAndNum(eFmtCtl, psHWInst, psReg);
		}

		default:
		{
			break;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 Name:		HWInstDecodeSMPOperandBankAndNum

 Purpose:	Decode the register banck and number for an operand from a HW
			texture sample instruction

			NB: This routine assumes that the specified operand index is
				valid for the instruction (use HWInstGetOperandsUsed). It
				does not decode opcode specific features such as format
				controls or component selections.

 Inputs:	psMOEState		- The decoded MOE state for the instruction
			eOpcode			- The decoded opcode for the HW instruction
			psHWInst		- The HW instruction to examine
			uOperandIdx		- Which operand to decode (see USP_OPERANDIDX_xxx)

 Outputs:	psReg			- The decoded register details

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstDecodeSMPOperandBankAndNum(PUSP_MOESTATE	psMOEState,
													   USP_OPCODE		eOpcode,
													   PHW_INST			psHWInst,
													   IMG_UINT32		uOperandIdx,
													   PUSP_REG			psReg)
{
	/*
		Use the general decoding routine to extract the register bank/number.
	*/
	if (!HWInstDecodeOperandBankAndNum(psMOEState,
									   eOpcode,
								       psHWInst,
									   uOperandIdx,
									   psReg))
	{
		return IMG_FALSE;
	}

	#if defined(SGX_FEATURE_USE_VEC34)
	/*
		The register number is stored in 64-bit units for unified store register types.
	*/
	if (psReg->eType != USP_REGTYPE_SPECIAL && psReg->eType != USP_REGTYPE_IMM)
	{
		psReg->uNum <<= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
	}
	#endif /* defined(SGX_FEATURE_USE_VEC34) */

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstEncodeOperandBankAndNum

 Purpose:	Encode the register banck and number for an operand from a HW
			instruction

			NB: This routine assumes that the specified operand index is
				valid for the instruction (use HWInstGetOperandsUsed). It
				does not encode opcode specific features such as format
				controls or component selections.

 Inputs:	psMOEState	- The decoded MOE state for the instruction
			eOpcode		- The decoded opcode for the HW instruction
			psHWInst	- The HW instruction to change
			uOperandIdx	- Which operand to encode (see USP_OPERANDIDX_xxx)
			psReg		- The register details to encode

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeOperandBankAndNum(PUSP_MOESTATE	psMOEState,
													USP_OPCODE		eOpcode,
													PHW_INST		psHWInst,
													IMG_UINT32		uOperandIdx,
													PUSP_REG		psReg)
{
	USP_FMTCTL	eFmtCtl;

	/*
		Determine whether this instruction has per-operand format-control
	*/
	if	(!HWInstGetPerOperandFmtCtl(psMOEState,
									eOpcode,
									psHWInst,
									&eFmtCtl))
	{
		return IMG_FALSE;
	}

	/*
		Encode the requested register bank and number
	*/
	switch (uOperandIdx)
	{
		case USP_OPERANDIDX_DST:
		{
			return HWInstEncodeDestBankAndNum(eFmtCtl,
											  eOpcode,
											  psHWInst,
											  psReg);
		}

		case USP_OPERANDIDX_SRC0:
		{
			IMG_BOOL bCanUseExtSrc0Banks;

			bCanUseExtSrc0Banks = HWInstCanUseExtSrc0Banks(eOpcode);

			return HWInstEncodeSrc0BankAndNum(eFmtCtl,
											  eOpcode,
											  bCanUseExtSrc0Banks,
											  psHWInst,
											  psReg);
		}

		case USP_OPERANDIDX_SRC1:
		{
			return HWInstEncodeSrc1BankAndNum(eFmtCtl, eOpcode, psHWInst, psReg);
		}

		case USP_OPERANDIDX_SRC2:
		{
			return HWInstEncodeSrc2BankAndNum(eFmtCtl, eOpcode, psHWInst, psReg);
		}

		default:
		{
			break;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 Name:		HWInstEncodeANDInst

 Purpose:	Encode an basic bitwise AND HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			bSkipInv		- The skip-invalid state for the instruction
			psDestReg		- The register to be used by the dest operand
			psSrc1Reg		- The register to be used by the src1 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeANDInst(PHW_INST	psHWInst,
										    IMG_UINT32	uRepeatCount,
											IMG_BOOL	bUsePred,
											IMG_BOOL	bNegatePred,
											IMG_UINT32	uPredNum,
										    IMG_BOOL	bSkipInv,
										    PUSP_REG	psDestReg,
										    PUSP_REG	psSrc1Reg,
											PUSP_REG	psSrc2Reg)
{
	IMG_UINT32	uSrc2ImdVal = 0;
	IMG_BOOL	bInvertVal 	= IMG_FALSE;
	IMG_BOOL	bIsSrc2Imd 	= IMG_FALSE;
	IMG_UINT32	uRotateVal 	= 0;
	IMG_UINT32	uHWPred 	= EURASIA_USE1_EPRED_ALWAYS;

	if(bUsePred)
	{
		switch(uPredNum)
		{
			case 0 :
			{
				if(bNegatePred)
					uHWPred = EURASIA_USE1_EPRED_NOTP0;
				else
					uHWPred = EURASIA_USE1_EPRED_P0;

				break;
			}

			case 1 :
			{
				if(bNegatePred)
					uHWPred = EURASIA_USE1_EPRED_NOTP1;
				else
					uHWPred = EURASIA_USE1_EPRED_P1;

				break;
			}

			case 2 :
			{
				uHWPred = EURASIA_USE1_EPRED_P2;
				break;
			}

			case 3 :
			{
				uHWPred = EURASIA_USE1_EPRED_P3;
				break;
			}
		}
	}

	if(psSrc2Reg->eType == USP_REGTYPE_IMM)
	{
		bIsSrc2Imd = IMG_TRUE;
	}

	if(bIsSrc2Imd)
	{
		/*
			Adjust immediate value if it is out of range.
		*/
		IMG_BOOL bValidVal = IMG_FALSE;
		IMG_UINT32 uTry;
		for(uTry=0; ((uTry<2)&&(!bValidVal)); uTry++)
		{
			uSrc2ImdVal = psSrc2Reg->uNum;
			uRotateVal = 0;
			
			/*
				Trying second time
			*/
			if(uTry == 1)
			{
				/*
					Now try with invert option	
				*/
				bInvertVal = IMG_TRUE;

				uSrc2ImdVal = ~uSrc2ImdVal;
			}

			while((uRotateVal<32) && 
				  (uSrc2ImdVal & 0xFFFF0000))
			{
				IMG_UINT32 uShiftVal;
				IMG_UINT32 uRotBit;
				uRotateVal++;
				uShiftVal = (uSrc2ImdVal>>1);
				uRotBit = ((uSrc2ImdVal & 0x1) << 31);
				uSrc2ImdVal = uShiftVal | uRotBit;
			}
			
			/*
				we have got a valid value;
			*/
			if(uRotateVal<32)
			{
				bValidVal = IMG_TRUE;
			}
		}
		
		/*
			Check if we have a valid value
		*/
		if(!bValidVal)
		{
			return 0;
		}
	}

	/*
		Encode a base bitwise AND inst (with Src2 set to immediate 0)
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_ANDOR << EURASIA_USE1_OP_SHIFT) | 
					   (EURASIA_USE1_BITWISE_OP2_AND << EURASIA_USE1_BITWISE_OP2_SHIFT) | 
					   (uRotateVal << EURASIA_USE1_BITWISE_SRC2ROT_SHIFT) | 
					   (bInvertVal ? EURASIA_USE1_BITWISE_SRC2INV : 0) |
					   (uHWPred << EURASIA_USE1_EPRED_SHIFT) | 
					   EURASIA_USE1_S2BEXT;

	psHWInst->uWord0 = (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT);

	#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
	if(uRepeatCount>0)
	{
		psHWInst->uWord1 |= ((uRepeatCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
	}
	#else
	/*
		Set repeat count
	*/
	psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
	psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
					    (~EURASIA_USE1_RMSKCNT_CLRMSK);
	#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_AND,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_AND, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	if(bIsSrc2Imd)
	{
		psHWInst->uWord0 |= ((uSrc2ImdVal & 0x007F) >> 0) << EURASIA_USE0_SRC2_SHIFT;
		psHWInst->uWord0 |= ((uSrc2ImdVal & 0x3F80) >> 7) << EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_SHIFT;
		psHWInst->uWord1 |= ((uSrc2ImdVal & 0xC000) >> 14) << EURASIA_USE1_BITWISE_SRC2IEXTH_SHIFT;
	}
	else
	{
		/*
			Encode the src2 register bank and number
		*/
		if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_AND, psHWInst, psSrc2Reg))
		{
			return 0;
		}
	}

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	return 1;
}

#if !defined(SGX_FEATURE_USE_VEC34)
/*****************************************************************************
 Name:		HWInstEncodeFMADInst

 Purpose:	Generates FMAD instruction according to arguments.

 Inputs:	psHwInst		- The hardware instruction to modify
			bSkipInv		- Boolean indicating whether to set the skip inv flag
			uRepeatCount	- The hardware instruction repeat count
			psDestReg		- Destination register
			psSrc0Reg		- Src0 register
			psSrc1Reg		- Src1 register
			psSrc2Reg		- Src2 register

 Outputs:	none

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeFMADInst(PHW_INST   psHWInst,
                                             IMG_BOOL   bSkipInv,
                                             IMG_UINT32 uRepeatCount,
                                             PUSP_REG   psDestReg,
                                             PUSP_REG   psSrc0Reg,
                                             PUSP_REG   psSrc1Reg,
                                             PUSP_REG   psSrc2Reg)
{
	/* Encode dest0 Bank */
	if(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_FMAD16,
	                               psHWInst,
	                               psDestReg))
	{
		return 0;
	}
	
	/* Encode src0 Bank */
	if(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_FMAD16,
	                               IMG_TRUE,
	                               psHWInst,
	                               psSrc0Reg))
	{
		return 0;
	}

	/* Encode src1 Bank */
	if(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_FMAD16,
	                               psHWInst,
	                               psSrc1Reg))
	{
		return 0;
	}

	/* Encode src2 Bank */
	if(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_FMAD16,
	                               psHWInst,
	                               psSrc2Reg))
	{
		return 0;
	}

	/* Set repeat count */
	if( uRepeatCount > 0 )
	{
		#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
		psHWInst->uWord1 |= ((uRepeatCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
		#else
		psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
		psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
			(~EURASIA_USE1_RMSKCNT_CLRMSK);
		#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

	}

	/* Set skipinv flag */
	if( bSkipInv == IMG_TRUE )
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	return 1;
}


/*****************************************************************************
 Name:		HWInstEncodeFRCInst

 Purpose:	Generates FRC instruction according to arguments.

 Inputs:	psHwInst		- The hardware instruction to modify
			bSkipInv		- Boolean indicating whether to set the skip inv flag
			uRepeatCount	- The hardware instruction repeat count
			psDestReg		- Destination register
			psSrc0Reg		- Src0 register
			psSrc1Reg		- Src1 register

 Outputs:	none

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeFRCInst(PHW_INST   psHWInst,
                                          IMG_BOOL   bSkipInv,
                                          IMG_UINT32 uRepeatCount,
                                          PUSP_REG   psDestReg,
                                          PUSP_REG   psSrc1Reg,
                                          PUSP_REG   psSrc2Reg)
{
	/* Encode dest0 Bank */
	if(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_FRC,
	                               psHWInst,
	                               psDestReg))
	{
		return 0;
	}

	/*NB. FRC uses only src 1 and src 2 only */
	
	/* Encode src1 Bank */
	if(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_FRC,
	                               psHWInst,
	                               psSrc1Reg))
	{
		return 0;
	}

	/* Encode src2 Bank */
	if(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_FRC,
	                               psHWInst,
	                               psSrc2Reg))
	{
		return 0;
	}

	/* Set repeat count */
	if( uRepeatCount > 0 )
	{
		#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
		psHWInst->uWord1 |= ((uRepeatCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
		#else
		psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
		psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
			(~EURASIA_USE1_RMSKCNT_CLRMSK);
		#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

	}

	/* Set skipinv flag */
	if( bSkipInv == IMG_TRUE )
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	/* FRC operation */
	psHWInst->uWord1 |= EURASIA_USE1_FLOAT_OP2_FRC << EURASIA_USE1_FLOAT_OP2_SHIFT;

	return 1;
}
#else
/*****************************************************************************
 Name:		HWInstEncodeVMADInst

 Purpose:	Generates FMAD instruction according to arguments.

 Inputs:	psHwInst		- The hardware instruction to modify
			bSkipInv		- Boolean indicating whether to set the skip inv flag
			uRepeatCount	- The hardware instruction repeat count
			psDestReg		- Destination register
			psSrc0Reg		- Src0 register
			psSrc1Reg		- Src1 register
			psSrc2Reg		- Src2 register

 Outputs:	none

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeVMADInst(PHW_INST   psHWInst,
                                             IMG_BOOL   bSkipInv,
                                             IMG_UINT32 uRepeatCount,
                                             PUSP_REG   psDestReg,
											 IMG_UINT32 ui32WMask,
                                             PUSP_REG   psSrc0Reg,
											 IMG_UINT32	ui32Src0Swiz,
                                             PUSP_REG   psSrc1Reg,
											 IMG_UINT32 ui32Src1Swiz,
                                             PUSP_REG   psSrc2Reg,
											 IMG_UINT32 ui32Src2Swiz)
{
	/* Encode dest0 Bank */
	if(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_VMAD,
	                               psHWInst,
	                               psDestReg))
	{
		return 0;
	}
	
	/* Encode src0 Bank */
	if(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_VMAD,
	                               IMG_FALSE,
	                               psHWInst,
	                               psSrc0Reg))
	{
		return 0;
	}

	/* Encode src1 Bank */
	if(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_VMAD,
	                               psHWInst,
	                               psSrc1Reg))
	{
		return 0;
	}

	/* Encode src2 Bank */
	if(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_VMAD,
	                               psHWInst,
	                               psSrc2Reg))
	{
		return 0;
	}

	/* Set repeat count */
	if( uRepeatCount > 0 )
	{
		#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
		psHWInst->uWord1 |= ((uRepeatCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
		#else
		psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
		psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
			(~EURASIA_USE1_RMSKCNT_CLRMSK);
		#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

	}

	psHWInst->uWord0 |= ((ui32Src0Swiz&0x3) << SGXVEC_USE0_VECMAD_SRC0SWIZ_BITS01_SHIFT) |
					 	((ui32Src1Swiz&0x3) << SGXVEC_USE0_VECMAD_SRC1SWIZ_BITS01_SHIFT);


	psHWInst->uWord1 |= (SGXVEC_USE1_VECMAD_F16 << EURASIA_USE1_OP_SHIFT) |
						(((ui32Src0Swiz>>2)&0x1) << SGXVEC_USE1_VECMAD_SRC0SWIZ_BIT2_SHIFT) |
						(((ui32Src1Swiz>>2)&0x1) << SGXVEC_USE1_VECMAD_SRC1SWIZ_BIT2_SHIFT) |
						(ui32Src2Swiz << SGXVEC_USE1_VECMAD_SRC2SWIZ_BITS02_SHIFT) |
						(bSkipInv ? EURASIA_USE1_SKIPINV : 0) |
						SGXVEC_USE1_VECMAD_NOSCHED;

	/* Write to all channels */
	psHWInst->uWord1 |= (ui32WMask << SGXVEC_USE1_VECNONMAD_DMASK_SHIFT);

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeVFRCInst

 Purpose:	Generates VFRC instruction according to arguments.

 Inputs:	psHwInst		- The hardware instruction to modify
			bSkipInv		- Boolean indicating whether to set the skip inv flag
			uRepeatCount	- The hardware instruction repeat count
			psDestReg		- Destination register
			psSrc0Reg		- Src0 register
			psSrc1Reg		- Src1 register

 Outputs:	none

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeVFRCInst(PHW_INST   psHWInst,
                                       		 IMG_BOOL   bSkipInv,
                             				 IMG_UINT32 uRepeatCount,
                                          	 PUSP_REG   psDestReg,
                                          	 PUSP_REG   psSrc1Reg,
											 IMG_UINT32 ui32Src1Swiz,
                                          	 PUSP_REG   psSrc2Reg,
											 IMG_UINT32 ui32Src2Swiz)
{
	/* Encode dest0 Bank */
	if(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_VFRC,
	                               psHWInst,
	                               psDestReg))
	{
		return 0;
	}

	/*NB. FRC uses only src 1 and src 2 only */
	
	/* Encode src1 Bank */
	if(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_VFRC,
	                               psHWInst,
	                               psSrc1Reg))
	{
		return 0;
	}

	/* Encode src2 Bank */
	if(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE,
	                               USP_OPCODE_VFRC,
	                               psHWInst,
	                               psSrc2Reg))
	{
		return 0;
	}

	/* Set repeat count */
	if( uRepeatCount > 0 )
	{
		#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
		psHWInst->uWord1 |= ((uRepeatCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
		#else
		psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
		psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
			(~EURASIA_USE1_RMSKCNT_CLRMSK);
		#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

	}

	/* Set skipinv flag */
	if( bSkipInv == IMG_TRUE )
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	/* Set the opcodes */
	psHWInst->uWord1 |= (SGXVEC_USE1_OP_VECNONMADF32 << EURASIA_USE1_OP_SHIFT);
	psHWInst->uWord0 |= (SGXVEC_USE0_VECNONMAD_OP2_VFRC << SGXVEC_USE0_VECNONMAD_OP2_SHIFT);

	/* Set up the swizzles */
	psHWInst->uWord0 |= ((ui32Src1Swiz&0x7F) << SGXVEC_USE0_VECNONMAD_SRC1SWIZ_BITS06_SHIFT);

	psHWInst->uWord1 |= (((ui32Src1Swiz>>7) & 0x3) << SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BIT78_SHIFT) |
						(ui32Src2Swiz << SGXVEC_USE1_VECNONMAD_SRC2SWIZ_SHIFT) |
						(((ui32Src1Swiz>>9) & 0x1) << SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BIT9_SHIFT) |
						(((ui32Src1Swiz>>10) & 0x3) << SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BITS1011_SHIFT);

	/* Write to all channels */
	psHWInst->uWord1 |= (0xF << SGXVEC_USE1_VECNONMAD_DMASK_SHIFT);

	return 1;
}
#endif /* !defined(SGX_FEATURE_USE_VEC34) */

/*****************************************************************************
 Name:		HWInstEncodeLIMMInst

 Purpose:	Encode an LIMM HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			bSkipInv		- The skip-invalid state for the instruction
			psDestReg		- The register to be used by the dest operand
			uValue			- Value to load into the register

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeLIMMInst(PHW_INST		psHWInst,
											 IMG_UINT32		uRepeatCount,
											 PUSP_REG		psDestReg,
											 IMG_UINT32		uValue)
{
	/* Set up the LIMM instruction */
	psHWInst->uWord0 = (((uValue >> 0) << EURASIA_USE0_LIMM_IMML21_SHIFT) & (~EURASIA_USE0_LIMM_IMML21_CLRMSK));
	
	psHWInst->uWord1 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
						 (EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
						 (EURASIA_USE1_OTHER_OP2_LIMM << EURASIA_USE1_OTHER_OP2_SHIFT) |
						 (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_LIMM_EPRED_SHIFT) |
						 (((uValue >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT) & (~EURASIA_USE1_LIMM_IMM2521_CLRMSK)) |
						 (((uValue >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT) & (~EURASIA_USE1_LIMM_IMM3126_CLRMSK)) |
						 EURASIA_USE1_SKIPINV;

	#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
	if(uRepeatCount>0)
	{
		psHWInst->uWord1 |= ((uRepeatCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
	}
	#else
	/*
		Set repeat count
	*/
	psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
	psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
					    (~EURASIA_USE1_RMSKCNT_CLRMSK);
	#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_LIMM,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeORInst

 Purpose:	Encode an basic bitwise OR HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			bSkipInv		- The skip-invalid state for the instruction
			psDestReg		- The register to be used by the dest operand
			psSrc1Reg		- The register to be used by the src1 operand
			psSrc2Reg		- The register to be used by the src2 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeORInst(PHW_INST		psHWInst,
										   IMG_UINT32	uRepeatCount,
										   IMG_BOOL		bSkipInv,
										   PUSP_REG		psDestReg,
										   PUSP_REG		psSrc1Reg,
										   PUSP_REG		psSrc2Reg)
{
	IMG_UINT32	uSrc2ImdVal = 0;
	IMG_BOOL	bInvertVal = IMG_FALSE;
	IMG_BOOL	bIsSrc2Imd = IMG_FALSE;
	IMG_UINT32	uRotateVal = 0;

	if(psSrc2Reg->eType == USP_REGTYPE_IMM)
	{
		bIsSrc2Imd = IMG_TRUE;
	}

	if(bIsSrc2Imd)
	{
		/*
			Adjust immediate value if it is out of range.
		*/
		IMG_BOOL bValidVal = IMG_FALSE;
		IMG_UINT32 uTry;
		for(uTry=0; ((uTry<2)&&(!bValidVal)); uTry++)
		{
			uSrc2ImdVal = psSrc2Reg->uNum;
			uRotateVal = 0;
			
			/*
				Trying second time
			*/
			if(uTry == 1)
			{
				/*
					Now try with invert option	
				*/
				bInvertVal = IMG_TRUE;

				uSrc2ImdVal = ~uSrc2ImdVal;
			}

			while((uRotateVal<32) && 
				  (uSrc2ImdVal & 0xFFFF0000))
			{
				IMG_UINT32 uShiftVal;
				IMG_UINT32 uRotBit;
				uRotateVal++;
				uShiftVal = (uSrc2ImdVal>>1);
				uRotBit = ((uSrc2ImdVal & 0x1) << 31);
				uSrc2ImdVal = uShiftVal | uRotBit;
			}
			
			/*
				we have got a valid value;
			*/
			if(uRotateVal<32)
			{
				bValidVal = IMG_TRUE;
			}
		}
		
		/*
			Check if we have a valid value
		*/
		if(!bValidVal)
		{
			return 0;
		}
	}

	/*
		Encode a base bitwise OR inst (with Src2 set to immediate 0)
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_ANDOR << EURASIA_USE1_OP_SHIFT) | 
					   (EURASIA_USE1_BITWISE_OP2_OR << EURASIA_USE1_BITWISE_OP2_SHIFT) | 
					   (uRotateVal << EURASIA_USE1_BITWISE_SRC2ROT_SHIFT) | 
					   (bInvertVal ? EURASIA_USE1_BITWISE_SRC2INV : 0) |
					   (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) | 
					   EURASIA_USE1_S2BEXT;

	psHWInst->uWord0 = (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT);

	#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
	if(uRepeatCount>0)
	{
		psHWInst->uWord1 |= ((uRepeatCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
	}
	#else
	/*
		Set repeat count
	*/
	psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
	psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
					    (~EURASIA_USE1_RMSKCNT_CLRMSK);
	#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_OR,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_OR, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	if(bIsSrc2Imd)
	{
		psHWInst->uWord0 |= ((uSrc2ImdVal & 0x007F) >> 0) << EURASIA_USE0_SRC2_SHIFT;
		psHWInst->uWord0 |= ((uSrc2ImdVal & 0x3F80) >> 7) << EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_SHIFT;
		psHWInst->uWord1 |= ((uSrc2ImdVal & 0xC000) >> 14) << EURASIA_USE1_BITWISE_SRC2IEXTH_SHIFT;
	}
	else
	{
		/*
			Encode the src2 register bank and number
		*/
		if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_OR, psHWInst, psSrc2Reg))
		{
			return 0;
		}
	}

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeXORInst

 Purpose:	Encode an basic bitwise XOR HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			bSkipInv		- The skip-invalid state for the instruction
			psDestReg		- The register to be used by the dest operand
			psSrc1Reg		- The register to be used by the src1 operand
			psSrc2Reg		- The register to be used by the src2 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeXORInst(PHW_INST		psHWInst,
											IMG_UINT32		uRepeatCount,
											IMG_UINT32		uPred,
											IMG_BOOL		bSkipInv,
											PUSP_REG		psDestReg,
											PUSP_REG		psSrc1Reg,
											PUSP_REG		psSrc2Reg)
{
	IMG_UINT32	uSrc2ImdVal = 0;
	IMG_BOOL	bInvertVal = IMG_FALSE;
	IMG_BOOL	bIsSrc2Imd = IMG_FALSE;
	IMG_UINT32	uRotateVal = 0;

	if(psSrc2Reg->eType == USP_REGTYPE_IMM)
	{
		bIsSrc2Imd = IMG_TRUE;
	}

	if(bIsSrc2Imd)
	{
		/*
			Adjust immediate value if it is out of range.
		*/
		IMG_BOOL bValidVal = IMG_FALSE;
		IMG_UINT32 uTry;
		for(uTry=0; ((uTry<2)&&(!bValidVal)); uTry++)
		{
			uSrc2ImdVal = psSrc2Reg->uNum;
			uRotateVal = 0;
			
			/*
				Trying second time
			*/
			if(uTry == 1)
			{
				/*
					Now try with invert option	
				*/
				bInvertVal = IMG_TRUE;

				uSrc2ImdVal = ~uSrc2ImdVal;
			}

			while((uRotateVal<32) && 
				  (uSrc2ImdVal & 0xFFFF0000))
			{
				IMG_UINT32 uShiftVal;
				IMG_UINT32 uRotBit;
				uRotateVal++;
				uShiftVal = (uSrc2ImdVal>>1);
				uRotBit = ((uSrc2ImdVal & 0x1) << 31);
				uSrc2ImdVal = uShiftVal | uRotBit;
			}
			
			/*
				we have got a valid value;
			*/
			if(uRotateVal<32)
			{
				bValidVal = IMG_TRUE;
			}
		}
		
		/*
			Check if we have a valid value
		*/
		if(!bValidVal)
		{
			return 0;
		}
	}

	/*
		Encode a base bitwise OR inst (with Src2 set to immediate 0)
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_XOR << EURASIA_USE1_OP_SHIFT) | 
					   (EURASIA_USE1_BITWISE_OP2_XOR << EURASIA_USE1_BITWISE_OP2_SHIFT) | 
					   (uRotateVal << EURASIA_USE1_BITWISE_SRC2ROT_SHIFT) | 
					   (bInvertVal ? EURASIA_USE1_BITWISE_SRC2INV : 0) |
					   (uPred << EURASIA_USE1_EPRED_SHIFT) |
					   EURASIA_USE1_S2BEXT;

	psHWInst->uWord0 = (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT);

	#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
	if(uRepeatCount>0)
	{
		psHWInst->uWord1 |= ((uRepeatCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
	}
	#else
	/*
		Set repeat count
	*/
	psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
	psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
					    (~EURASIA_USE1_RMSKCNT_CLRMSK);
	#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_XOR,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_XOR, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	if(bIsSrc2Imd)
	{
		psHWInst->uWord0 |= ((uSrc2ImdVal & 0x007F) >> 0) << EURASIA_USE0_SRC2_SHIFT;
		psHWInst->uWord0 |= ((uSrc2ImdVal & 0x3F80) >> 7) << EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_SHIFT;
		psHWInst->uWord1 |= ((uSrc2ImdVal & 0xC000) >> 14) << EURASIA_USE1_BITWISE_SRC2IEXTH_SHIFT;
	}
	else
	{
		/*
			Encode the src2 register bank and number
		*/
		if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_XOR, psHWInst, psSrc2Reg))
		{
			return 0;
		}
	}

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeSHLInst

 Purpose:	Encode an basic bitwise SHL HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			bSkipInv		- The skip-invalid state for the instruction
			psDestReg		- The register to be used by the dest operand
			psSrc1Reg		- The register to be used by the src1 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSHLInst(PHW_INST		psHWInst,
											IMG_UINT32	uRepeatCount,
											IMG_BOOL		bSkipInv,
											PUSP_REG		psDestReg,
											PUSP_REG		psSrc1Reg,
											PUSP_REG		psSrc2Reg)
{
	IMG_UINT32	uSrc2ImdVal = 0;
	IMG_BOOL	bInvertVal = IMG_FALSE;
	IMG_BOOL	bIsSrc2Imd = IMG_FALSE;
	IMG_UINT32	uRotateVal = 0;

	if(psSrc2Reg->eType == USP_REGTYPE_IMM)
	{
		bIsSrc2Imd = IMG_TRUE;
	}

	if(bIsSrc2Imd)
	{
		/*
			Adjust immediate value if it is out of range.
		*/
		IMG_BOOL bValidVal = IMG_FALSE;
		IMG_UINT32 uTry;
		for(uTry=0; ((uTry<2)&&(!bValidVal)); uTry++)
		{
			uSrc2ImdVal = psSrc2Reg->uNum;
			uRotateVal = 0;
			
			/*
				Trying second time
			*/
			if(uTry == 1)
			{
				/*
					Now try with invert option	
				*/
				bInvertVal = IMG_TRUE;

				uSrc2ImdVal = ~uSrc2ImdVal;
			}

			while((uRotateVal<32) && 
				  (uSrc2ImdVal & 0xFFFF0000))
			{
				IMG_UINT32 uShiftVal;
				IMG_UINT32 uRotBit;
				uRotateVal++;
				uShiftVal = (uSrc2ImdVal>>1);
				uRotBit = ((uSrc2ImdVal & 0x1) << 31);
				uSrc2ImdVal = uShiftVal | uRotBit;
			}
			
			/*
				we have got a valid value;
			*/
			if(uRotateVal<32)
			{
				bValidVal = IMG_TRUE;
			}
		}
		
		/*
			Check if we have a valid value
		*/
		if(!bValidVal)
		{
			return 0;
		}
	}

	/*
		Encode a base SHL inst (with Src2 set to immediate 0)
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_SHLROL << EURASIA_USE1_OP_SHIFT) | 
					   (EURASIA_USE1_BITWISE_OP2_SHL << EURASIA_USE1_BITWISE_OP2_SHIFT) | 
					   (uRotateVal << EURASIA_USE1_BITWISE_SRC2ROT_SHIFT) | 
					   (bInvertVal ? EURASIA_USE1_BITWISE_SRC2INV : 0) |
					   (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) | 
					   EURASIA_USE1_S2BEXT;

	psHWInst->uWord0 = (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT);

	#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
	if(uRepeatCount>0)
	{
		psHWInst->uWord1 |= ((uRepeatCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
	}
	#else
	/*
		Set repeat count
	*/
	psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
	psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
					    (~EURASIA_USE1_RMSKCNT_CLRMSK);
	#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_SHL,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_SHL, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	if(bIsSrc2Imd)
	{
		psHWInst->uWord0 |= ((uSrc2ImdVal & 0x007F) >> 0) << EURASIA_USE0_SRC2_SHIFT;
		psHWInst->uWord0 |= ((uSrc2ImdVal & 0x3F80) >> 7) << EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_SHIFT;
		psHWInst->uWord1 |= ((uSrc2ImdVal & 0xC000) >> 14) << EURASIA_USE1_BITWISE_SRC2IEXTH_SHIFT;
	}
	else
	{
		/*
			Encode the src2 register bank and number
		*/
		if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_SHL, psHWInst, psSrc2Reg))
		{
			return 0;
		}
	}

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeSHRInst

 Purpose:	Encode an basic bitwise SHR HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			bSkipInv		- The skip-invalid state for the instruction
			psDestReg		- The register to be used by the dest operand
			psSrc1Reg		- The register to be used by the src1 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSHRInst(PHW_INST		psHWInst,
											IMG_UINT32	uRepeatCount,
											IMG_BOOL		bSkipInv,
											PUSP_REG		psDestReg,
											PUSP_REG		psSrc1Reg,
											PUSP_REG		psSrc2Reg)
{
	IMG_UINT32	uSrc2ImdVal = 0;
	IMG_BOOL	bInvertVal = IMG_FALSE;
	IMG_BOOL	bIsSrc2Imd = IMG_FALSE;
	IMG_UINT32	uRotateVal = 0;

	if(psSrc2Reg->eType == USP_REGTYPE_IMM)
	{
		bIsSrc2Imd = IMG_TRUE;
	}

	if(bIsSrc2Imd)
	{
		/*
			Adjust immediate value if it is out of range.
		*/
		IMG_BOOL bValidVal = IMG_FALSE;
		IMG_UINT32 uTry;
		for(uTry=0; ((uTry<2)&&(!bValidVal)); uTry++)
		{
			uSrc2ImdVal = psSrc2Reg->uNum;
			uRotateVal = 0;
			
			/*
				Trying second time
			*/
			if(uTry == 1)
			{
				/*
					Now try with invert option	
				*/
				bInvertVal = IMG_TRUE;

				uSrc2ImdVal = ~uSrc2ImdVal;
			}

			while((uRotateVal<32) && 
				  (uSrc2ImdVal & 0xFFFF0000))
			{
				IMG_UINT32 uShiftVal;
				IMG_UINT32 uRotBit;
				uRotateVal++;
				uShiftVal = (uSrc2ImdVal>>1);
				uRotBit = ((uSrc2ImdVal & 0x1) << 31);
				uSrc2ImdVal = uShiftVal | uRotBit;
			}
			
			/*
				we have got a valid value;
			*/
			if(uRotateVal<32)
			{
				bValidVal = IMG_TRUE;
			}
		}
		
		/*
			Check if we have a valid value
		*/
		if(!bValidVal)
		{
			return 0;
		}
	}

	/*
		Encode a base SHR inst (with Src2 set to immediate 0)
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_SHRASR << EURASIA_USE1_OP_SHIFT) | 
					   (EURASIA_USE1_BITWISE_OP2_SHR << EURASIA_USE1_BITWISE_OP2_SHIFT) | 
					   (uRotateVal << EURASIA_USE1_BITWISE_SRC2ROT_SHIFT) | 
					   (bInvertVal ? EURASIA_USE1_BITWISE_SRC2INV : 0) |
					   (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) | 
					   EURASIA_USE1_S2BEXT;

	psHWInst->uWord0 = (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT);

	#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
	if(uRepeatCount>0)
	{
		psHWInst->uWord1 |= ((uRepeatCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
	}
	#else
	/*
		Set repeat count
	*/
	psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
	psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
					    (~EURASIA_USE1_RMSKCNT_CLRMSK);
	#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_SHR,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_SHR, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	if(bIsSrc2Imd)
	{
		psHWInst->uWord0 |= ((uSrc2ImdVal & 0x007F) >> 0) << EURASIA_USE0_SRC2_SHIFT;
		psHWInst->uWord0 |= ((uSrc2ImdVal & 0x3F80) >> 7) << EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_SHIFT;
		psHWInst->uWord1 |= ((uSrc2ImdVal & 0xC000) >> 14) << EURASIA_USE1_BITWISE_SRC2IEXTH_SHIFT;
	}
	else
	{
		/*
			Encode the src2 register bank and number
		*/
		if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_SHR, psHWInst, psSrc2Reg))
		{
			return 0;
		}
	}

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeIMA16Inst

 Purpose:	Encode an IMA16 HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			psDestReg		- The register to be used by the dest operand
			psSrc1Reg		- The register to be used by the src1 operand
			psSrc2Reg		- The register to be used by the src2 operand
			psSrc3Reg		- The register to be used by the src3 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeIMA16Inst(PHW_INST		psHWInst,
										 	  IMG_UINT32	uRepeatCount,
											  PUSP_REG		psDestReg,
											  PUSP_REG		psSrc0Reg,
											  PUSP_REG		psSrc1Reg,
											  PUSP_REG		psSrc2Reg)
{
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = 0;

	PVR_UNREFERENCED_PARAMETER(uRepeatCount);

	/*
		Encode an IMA16 inst
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_IMA16 << EURASIA_USE1_OP_SHIFT);

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_IMA16,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src0 register bank and number
	*/
	if	(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_IMA16, IMG_TRUE, psHWInst, psSrc0Reg))
	{
		return 0;
	}

	/*
		Encode the src2 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_IMA16, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	/*
		Encode the src3 register bank and number
	*/
	if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_IMA16, psHWInst, psSrc2Reg))
	{
		return 0;
	}

	psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeIMAEInst

 Purpose:	Encode an IMAE HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			psDestReg		- The register to be used by the dest operand
			psSrc1Reg		- The register to be used by the src1 operand
			psSrc2Reg		- The register to be used by the src2 operand
			psSrc3Reg		- The register to be used by the src3 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeIMAEInst(PHW_INST		psHWInst,
										 	  IMG_UINT32	uRepeatCount,
											  PUSP_REG		psDestReg,
											  PUSP_REG		psSrc0Reg,
											  PUSP_REG		psSrc1Reg,
											  PUSP_REG		psSrc2Reg)
{
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = 0;

	PVR_UNREFERENCED_PARAMETER(uRepeatCount);

	/*
		Encode an IMAE inst
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_IMAE << EURASIA_USE1_OP_SHIFT);

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_IMAE,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src0 register bank and number
	*/
	if	(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_IMAE, IMG_TRUE, psHWInst, psSrc0Reg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_IMAE, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	/*
		Encode the src2 register bank and number
	*/
	if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_IMAE, psHWInst, psSrc2Reg))
	{
		return 0;
	}

	/* Set the second argument to be a 32-bit integer */
	psHWInst->uWord1 &= EURASIA_USE1_IMAE_SRC2TYPE_CLRMSK;
	psHWInst->uWord1 |= (EURASIA_USE1_IMAE_SRC2TYPE_32BIT << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT) &
						(~EURASIA_USE1_IMAE_SRC2TYPE_CLRMSK);
	psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeSTInst

 Purpose:	Encode a basic ST instruction (absolute addressing or 32-bit data)

 Inputs:	bSkipInv		- Whether the SkipInv flag should be set
			uRepeatCount	- The number of 32-bit words should be fetched
			psBaseAddrReg	- The register to use for the src1 operand
			psAddrOffReg	- The register to use for the src2 operand
			psDataReg		- The register to use for the data to store

 Outputs:	psHWInst		- The encoded HW instruction

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSTInst(PHW_INST		psHWInst,
										   IMG_BOOL		bSkipInv,
										   IMG_UINT32	uRepeatCount,
										   PUSP_REG		psBaseAddrReg,
										   PUSP_REG		psAddrOffReg,
										   PUSP_REG		psDataReg)
{
	PVR_UNREFERENCED_PARAMETER(uRepeatCount);

	/*
		Encode a base ST instruction absolute-addressing, 32-bit data-size)
	*/
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = (EURASIA_USE1_OP_ST << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
					   (EURASIA_USE1_LDST_AMODE_ABSOLUTE << EURASIA_USE1_LDST_AMODE_SHIFT) | 
					   (EURASIA_USE1_LDST_DTYPE_32BIT << EURASIA_USE1_LDST_DTYPE_SHIFT) |
					   (EURASIA_USE1_LDST_MOEEXPAND);

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	/*
		Encode the base-address into source 0
	*/
	if	(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE, 
									 USP_OPCODE_ST,
									 IMG_FALSE,
									 psHWInst, 
									 psBaseAddrReg))
	{
		return 0;
	}

	/*
		Encode the offset into source 1
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_ST, psHWInst, psAddrOffReg))
	{
		return 0;
	}

	/*
		Encode the data into source 2
	*/
	if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_ST, psHWInst, psDataReg))
	{
		return 0;
	}

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeTESTInst

 Purpose:	Encode a TEST HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			dwFlags			- The flags for the instruction
			psDest			- The register to be used by the dest operand
			psSrc0			- The register to be used by the src1 operand
			psSrc1			- The register to be used by the src1 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeTESTInst(PHW_INST		psHWInst,
											IMG_UINT32		uRepeatCount,
											IMG_UINT32		dwFlags,
											PUSP_REG		psDest,
											PUSP_REG		psSrc0,
											PUSP_REG		psSrc1,
											IMG_UINT32		dwPDST,
											IMG_UINT32		dwSTST,
											IMG_UINT32		dwZTST,
											IMG_UINT32		dwCHANCC,
											IMG_UINT32		dwALUSEL,
											IMG_UINT32		dwALUOP)
{
	/* Set up the test instruction */
	psHWInst->uWord0 = 	(dwALUSEL << EURASIA_USE0_TEST_ALUSEL_SHIFT) |
						 (dwALUOP << EURASIA_USE0_TEST_ALUOP_SHIFT);
	
	psHWInst->uWord1 = 	(EURASIA_USE1_OP_TEST << EURASIA_USE1_OP_SHIFT) |
						 (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
						 (uRepeatCount << EURASIA_USE1_RMSKCNT_SHIFT) |
						 (dwPDST << EURASIA_USE1_TEST_PDST_SHIFT) |
						 (dwCHANCC << EURASIA_USE1_TEST_CHANCC_SHIFT) |
						 (dwSTST << EURASIA_USE1_TEST_STST_SHIFT) |
						 (dwZTST << EURASIA_USE1_TEST_ZTST_SHIFT) |
						 (dwFlags & ~EURASIA_USE1_RCNTSEL) |
						 EURASIA_USE1_SKIPINV;

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_TEST,
									 psHWInst,
									 psDest))
	{
		return 0;
	}

	/*
		Encode source 1
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_TEST, psHWInst, psSrc0))
	{
		return 0;
	}

	/*
		Encode source 2
	*/
	if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_TEST, psHWInst, psSrc1))
	{
		return 0;
	}

	return 1;
}

IMG_INTERNAL IMG_UINT32 HWInstEncodeIDFInst(PHW_INST psHWInst)
{
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
						 (EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
						 (EURASIA_USE1_OTHER_OP2_IDF << EURASIA_USE1_OTHER_OP2_SHIFT) |
						 (0 << EURASIA_USE1_IDFWDF_DRCSEL_SHIFT) |
						 (0 << EURASIA_USE1_IDF_PATH_SHIFT);

	return 1;
}

#if !defined(SGX_FEATURE_USE_VEC34)
/*****************************************************************************
 Name:		HWInstEncodeMOVInst

 Purpose:	Encode an basic unconditional MOV HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			bSkipInv		- The skip-invalid state for the instruction
			psDestReg		- The register to be used by the dest operand
			psSrc1Reg		- The register to be used by the src1 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeMOVInst(PHW_INST	psHWInst,
										    IMG_UINT32	uRepeatCount,
										    IMG_BOOL	bSkipInv,
										    PUSP_REG	psDestReg,
										    PUSP_REG	psSrc1Reg)
{
	/*
		Encode a base unconditional MOV inst (with Src2 set to immediate 0)
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
					   (EURASIA_USE1_MOVC_TSTDTYPE_UNCOND << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT) |
					   EURASIA_USE1_S2BEXT;
	psHWInst->uWord0 = EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT;

	/*
		Encode the required repeat count
	*/
	if	((uRepeatCount - 1) > EURASIA_USE1_RCOUNT_MAX)
	{
		return 0;
	}

	psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
	psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
					    (~EURASIA_USE1_RMSKCNT_CLRMSK);

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_MOVC,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_MOVC, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	return 1;
}
#endif /* !defined(SGX_FEATURE_USE_VEC34) */

/*****************************************************************************
 Name:		HWInstEncodeMOVCInst

 Purpose:	Encode an conditional MOV HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			uRepeatCount	- The repeat count for the instruction
							  (1 = no repeat)
			uPred			- The (hardware) predicate number
			bSkipInv		- The skip-invalid state for the instruction
			psDestReg		- The register to be used by the dest operand
			psSrc1Reg		- The register to be used by the src1 operand
			psSrc2Reg		- The register to be used by the src2 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeMOVCInst(PHW_INST	psHWInst,
											 IMG_UINT32	uRepeatCount,
											 IMG_UINT32 uPred,
											 IMG_BOOL 	bTestDst,
											 IMG_BOOL	bSkipInv,
											 PUSP_REG	psDestReg,
											 PUSP_REG	psSrc0Reg,
											 PUSP_REG	psSrc1Reg,
											 PUSP_REG	psSrc2Reg)
{
#if defined(SGX_FEATURE_USE_VEC34)

#if 0
		/* Generate a VMOV, with a write mask to transfer 1 word only */
		psHWInst->uWord0 = 	(psDestReg->uNum << SGXVEC_USE0_VMOVC_DST_SHIFT) | /* Dest */
							(psSrc0Reg->uNum << SGXVEC_USE0_VMOVC_SRC0_SHIFT) | /* Src0 */
							(psSrc1Reg->uNum << SGXVEC_USE0_VMOVC_SRC1_SHIFT) | /* Src1 */
							(psSrc2Reg->uNum << SGXVEC_USE0_VMOVC_SRC2_SHIFT); /* Src2 */
#endif

		psHWInst->uWord1 = (SGXVEC_USE1_OP_VECMOV << EURASIA_USE1_OP_SHIFT) |
								((uPred << SGXVEC_USE1_VECNONMAD_EVPRED_SHIFT) & ~SGXVEC_USE1_VECNONMAD_EVPRED_CLRMSK) |
								EURASIA_USE1_SKIPINV | // skipinv
								(SGXVEC_USE1_VMOVC_MTYPE_CONDITIONAL << SGXVEC_USE1_VMOVC_MTYPE_SHIFT) |
								(SGXVEC_USE1_VMOVC_MDTYPE_INT32 << SGXVEC_USE1_VMOVC_MDTYPE_SHIFT); /* INT32 */

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_VMOV,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src0 register bank and number
	*/
	if	(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_VMOV, IMG_TRUE, psHWInst, psSrc0Reg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_VMOV, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	/*
		Encode the src2 register bank and number
	*/
	if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_VMOV, psHWInst, psSrc2Reg))
	{
		return 0;
	}

	PVR_UNREFERENCED_PARAMETER(uRepeatCount);
	PVR_UNREFERENCED_PARAMETER(bTestDst);
	PVR_UNREFERENCED_PARAMETER(bSkipInv);

#else /* defined(SGX_FEATURE_USE_VEC34) */
	/*
		Encode a conditional MOV inst
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
					   (uPred << EURASIA_USE1_EPRED_SHIFT) |
					   (EURASIA_USE1_MOVC_TESTCC_STDNONZERO << EURASIA_USE1_MOVC_TESTCC_SHIFT) |
					   EURASIA_USE1_S2BEXT;

	if(bTestDst)
	{
		psHWInst->uWord1 |= (EURASIA_USE1_MOVC_TSTDTYPE_INT32 << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT);
	}
	else
	{
		psHWInst->uWord1 |= (EURASIA_USE1_MOVC_TSTDTYPE_UNCOND << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT);
	}

	psHWInst->uWord0 = EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT;

	/*
		Encode the required repeat count
	*/
	if	((uRepeatCount - 1) > EURASIA_USE1_RCOUNT_MAX)
	{
		return 0;
	}

	psHWInst->uWord1 |= EURASIA_USE1_RCNTSEL;
	psHWInst->uWord1 |= ((uRepeatCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) &
					    (~EURASIA_USE1_RMSKCNT_CLRMSK);

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_MOVC,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src0 register bank and number
	*/
	if	(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_MOVC, IMG_TRUE, psHWInst, psSrc0Reg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_MOVC, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	/*
		Encode the src2 register bank and number
	*/
	if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_MOVC, psHWInst, psSrc2Reg))
	{
		return 0;
	}

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}
#endif /* defined(SGX_FEATURE_USE_VEC34) */

	return 1;
}

/*****************************************************************************
 Name:		HWInstDecodeMOVInstTestDataType

 Purpose:	Decode the test data-type used by a MOVC instruction

 Inputs:	psHWInst	- The HW instruction to initialise

 Outputs:	peTestType	- The decode MOVC test data-type

 Returns:	IMG_TRUE/IMG_FALSE on success/failure
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstDecodeMOVInstTestDataType(
											PHW_INST			psHWInst,
											PUSP_MOVC_TESTDTYPE	peTestType)
{
	IMG_UINT32			uHWTestDType;
	USP_MOVC_TESTDTYPE	eTestType;

	/*
		Extract and decode the HW test data-type
	*/
	uHWTestDType = (psHWInst->uWord1 & ~EURASIA_USE1_MOVC_TSTDTYPE_CLRMSK) >> 
				   EURASIA_USE1_MOVC_TSTDTYPE_SHIFT;

	switch (uHWTestDType)
	{
		case EURASIA_USE1_MOVC_TSTDTYPE_UNCOND:
		{
			eTestType = USP_MOVC_TESTDTYPE_NONE;
			break;
		}
		case EURASIA_USE1_MOVC_TSTDTYPE_INT8:
		{
			eTestType = USP_MOVC_TESTDTYPE_INT8;
			break;
		}
		case EURASIA_USE1_MOVC_TSTDTYPE_INT16:
		{
			eTestType = USP_MOVC_TESTDTYPE_INT16;
			break;
		}
		case EURASIA_USE1_MOVC_TSTDTYPE_INT32:
		{
			eTestType = USP_MOVC_TESTDTYPE_INT32;
			break;
		}
		case EURASIA_USE1_MOVC_TSTDTYPE_FLOAT:
		{
			eTestType = USP_MOVC_TESTDTYPE_FLOAT32;
			break;
		}
		case EURASIA_USE1_MOVC_TSTDTYPE_INT10:
		{
			eTestType = USP_MOVC_TESTDTYPE_INT10;
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}

	*peTestType = eTestType;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstDecodeRepeat

 Purpose:	Decode the repeat options for an instruction

 Inputs:	psHWInst	- The HW instruction to initialise
			eOpcode		- The decoded opcode of the HW instruction

 Outputs:	peMode		- The decoded repeat mode
			puRepeat	- The decoded repeat count or mask

 Returns:	IMG_TRUE/IMG_FALSE on success/failure.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstDecodeRepeat(PHW_INST			psHWInst,
										 USP_OPCODE			eOpcode,
										 PUSP_REPEAT_MODE	peMode,
										 IMG_PUINT32		puRepeat)
{
	USP_REPEAT_MODE	eMode;
	IMG_UINT32		uRepeat;

	switch (eOpcode)
	{
		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_MAD:
		case USP_OPCODE_ADM:
		case USP_OPCODE_MSA:
		case USP_OPCODE_FRC:
		case USP_OPCODE_RCP:
		case USP_OPCODE_RSQ:
		case USP_OPCODE_LOG:
		case USP_OPCODE_EXP:
		case USP_OPCODE_DP:
		case USP_OPCODE_DDP:
		case USP_OPCODE_DDPC:
		case USP_OPCODE_MIN:
		case USP_OPCODE_MAX:
		case USP_OPCODE_MOVC:
		case USP_OPCODE_FMAD16:
		case USP_OPCODE_PCKUNPCK:
		case USP_OPCODE_AND:
		case USP_OPCODE_OR:
		case USP_OPCODE_XOR:
		case USP_OPCODE_SHL:
		case USP_OPCODE_ROL:
		case USP_OPCODE_SHR:
		case USP_OPCODE_ASR:
		case USP_OPCODE_RLP:
		{
			#if defined(SGX545)
			if(eOpcode == USP_OPCODE_FMAD16)
			{
				uRepeat = ((psHWInst->uWord1) & ~SGX545_USE1_FARITH16_RCNT_CLRMSK) >> 
				      SGX545_USE1_FARITH16_RCNT_SHIFT;
				
				uRepeat = (uRepeat==IMG_TRUE) ? 2 : 1;
			
				/* It is always a repeat counter on SGX545 */
				eMode = USP_REPEAT_MODE_REPEAT;
			}
			else
			#endif /* defined(SGX545) */
			{
				/*
					All these opcodes support either a repeat-count or a mask
				*/
				uRepeat = (psHWInst->uWord1 & ~EURASIA_USE1_RMSKCNT_CLRMSK) >>
						  EURASIA_USE1_RMSKCNT_SHIFT;

				if	(uRepeat >= EURASIA_USE_MAXIMUM_REPEAT)
				{
					return IMG_FALSE;
				}

				/*
					Decode the mode used
				*/
				if	(psHWInst->uWord1 & EURASIA_USE1_RCNTSEL)
				{
					eMode = USP_REPEAT_MODE_REPEAT;
				}
				else
				{
					eMode = USP_REPEAT_MODE_MASK;
				}
			}
			break;
		}
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */

		#if defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_PCKUNPCK:
		{
			/* It is always a repeat count on SGX543 */
			eMode = USP_REPEAT_MODE_REPEAT;
			uRepeat = (psHWInst->uWord1 & ~SGXVEC_USE1_PCK_RCOUNT_CLRMSK) >> SGXVEC_USE1_PCK_RCOUNT_SHIFT;
			break;
		}
		#endif /* defined(SGX_FEATURE_USE_VEC34) */

		#if defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
		case USP_OPCODE_AND:
		case USP_OPCODE_OR:
		case USP_OPCODE_XOR:
		case USP_OPCODE_SHL:
		case USP_OPCODE_ROL:
		case USP_OPCODE_SHR:
		case USP_OPCODE_ASR:
		case USP_OPCODE_RLP:
		{
			/* It is always a repeat count on SGX543 */
			eMode = USP_REPEAT_MODE_REPEAT;
			uRepeat = (psHWInst->uWord1 & ~SGXVEC_USE1_BITWISE_RCOUNT_CLRMSK) >> SGXVEC_USE1_BITWISE_RCOUNT_SHIFT;
			break;
		}
		#endif /* defined(SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */

		case USP_OPCODE_TEST:
		case USP_OPCODE_TESTMASK:
		{
			/*
				Only mask-based repeat available for tests
			*/
			eMode = USP_REPEAT_MODE_MASK;
			uRepeat = (psHWInst->uWord1 & ~EURASIA_USE1_TEST_PREDMASK_CLRMSK) >>
					  EURASIA_USE1_TEST_PREDMASK_SHIFT;

			break;
		}

		case USP_OPCODE_LD:
		case USP_OPCODE_ST:
		{
			/*
				Decode the MOE-repeat/fetch count
			*/
			uRepeat = (psHWInst->uWord1 & ~EURASIA_USE1_RMSKCNT_CLRMSK) >>
					  EURASIA_USE1_RMSKCNT_SHIFT;

			if	(uRepeat > HW_INST_LD_MAX_REPEAT_COUNT)
			{
				return IMG_FALSE;	
			}

			/*
				Decode the mode used
			*/
			if	(psHWInst->uWord1 & EURASIA_USE1_LDST_MOEEXPAND)
			{
				eMode = USP_REPEAT_MODE_REPEAT;
			}
			else
			{
				eMode = USP_REPEAT_MODE_FETCH;
				uRepeat++;
			}

			break;
		}

		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_EFO:
		{
			/*
				EFO does not support mask-based repeats
			*/
			uRepeat	= (psHWInst->uWord1 & ~EURASIA_USE1_EFO_RCOUNT_CLRMSK) >>
					  EURASIA_USE1_EFO_RCOUNT_SHIFT;

			if	(uRepeat > HW_INST_EFO_MAX_REPEAT_COUNT)
			{
				return IMG_FALSE;
			}

			eMode = USP_REPEAT_MODE_REPEAT;

			break;
		}
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */

		case USP_OPCODE_SOP2:
		case USP_OPCODE_IMA8:
		case USP_OPCODE_IMA16:
		case USP_OPCODE_IMAE:
		case USP_OPCODE_ADIF:
		case USP_OPCODE_BILIN:
		case USP_OPCODE_FIRV:
		case USP_OPCODE_DOT3:
		case USP_OPCODE_DOT4:
		case USP_OPCODE_FPMA:
		{
			/*
				These colour/integer instructions do not support masked repeats
			*/
			uRepeat	= (psHWInst->uWord1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >>
					  EURASIA_USE1_INT_RCOUNT_SHIFT;

			if	(uRepeat >= EURASIA_USE1_INT_RCOUNT_MAXIMUM)
			{
				return IMG_FALSE;
			}

			eMode = USP_REPEAT_MODE_REPEAT;

			break;
		}

		case USP_OPCODE_FIRH:
		{
			/*
				No masked-repeat mode, and a reduced repeat range for FIRH
			*/
			uRepeat	= (psHWInst->uWord1 & ~EURASIA_USE1_FIRH_RCOUNT_CLRMSK) >>
					  EURASIA_USE1_FIRH_RCOUNT_SHIFT;

			if	(uRepeat > EURASIA_USE1_FIRH_RCOUNT_MAX)
			{
				return IMG_FALSE;
			}

			eMode = USP_REPEAT_MODE_REPEAT;

			break;
		}

		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_DSX:
		case USP_OPCODE_DSY:
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */
		case USP_OPCODE_SOP3:
		case USP_OPCODE_SOPWM:
		case USP_OPCODE_SMP:
		case USP_OPCODE_SMPBIAS:
		case USP_OPCODE_SMPREPLACE:
		case USP_OPCODE_SMPGRAD:
		case USP_OPCODE_BA:
		case USP_OPCODE_BR:
		case USP_OPCODE_LAPC:
		case USP_OPCODE_SETL:
		case USP_OPCODE_SAVL:
		case USP_OPCODE_NOP:
		case USP_OPCODE_PREAMBLE:
		case USP_OPCODE_SMOA:
		case USP_OPCODE_SMR:
		case USP_OPCODE_SMLSI:
		case USP_OPCODE_SMBO:
		case USP_OPCODE_IMO:
		case USP_OPCODE_SETFC:
		case USP_OPCODE_IDF:
		case USP_OPCODE_WDF:
		case USP_OPCODE_EMIT:
		case USP_OPCODE_LIMM:
		case USP_OPCODE_LOCK:
		case USP_OPCODE_RELEASE:
		case USP_OPCODE_LDR:
		case USP_OPCODE_STR:
		case USP_OPCODE_WOP:
		case USP_OPCODE_PCOEFF:
		case USP_OPCODE_PTOFF:
		case USP_OPCODE_ATST8:
		case USP_OPCODE_DEPTHF:
		#if defined(SGX_FEATURE_USE_IDXSC)
		case USP_OPCODE_IDXSCR:
		case USP_OPCODE_IDXSCW:
		#endif /* defined(SGX_FEATURE_USE_IDXSC) */
		#if defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_VMAD:
		case USP_OPCODE_VMUL:
		case USP_OPCODE_VADD:
		case USP_OPCODE_VFRC:
		case USP_OPCODE_VDSX:
		case USP_OPCODE_VDSY:
		case USP_OPCODE_VMIN:
		case USP_OPCODE_VMAX:
		case USP_OPCODE_VDP:
		case USP_OPCODE_VDUAL:
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
		{
			/*
				None of these instructions support a repeat	at all
			*/
			eMode	= USP_REPEAT_MODE_NONE;
			uRepeat	= 0;
			break;
		}

		#if defined(SGX545)
		case USP_OPCODE_DUAL:
		{
			uRepeat = ((psHWInst->uWord1) & ~SGX545_USE1_FDUAL_RCNT_CLRMSK) >> 
				      SGX545_USE1_FDUAL_RCNT_SHIFT;

			uRepeat = (uRepeat==IMG_TRUE) ? 2 : 1;
			
			/* It is always a repeat counter on SGX545 */
			eMode = USP_REPEAT_MODE_REPEAT;

			break;
		}
		#endif /* defined(SGX545) */

		#if defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_VRCP:
		case USP_OPCODE_VRSQ:
		case USP_OPCODE_VLOG:
		case USP_OPCODE_VEXP:
		{
			uRepeat = (psHWInst->uWord1 & ~SGXVEC_USE1_VECCOMPLEXOP_RCNT_CLRMSK) >> SGXVEC_USE1_VECCOMPLEXOP_RCNT_SHIFT;
			eMode = USP_REPEAT_MODE_REPEAT;
			break;
		}
		case USP_OPCODE_VMOV:
		{
			uRepeat = (psHWInst->uWord1 & ~SGXVEC_USE1_VMOVC_RCNT_CLRMSK) >> SGXVEC_USE1_VMOVC_RCNT_SHIFT;
			eMode = USP_REPEAT_MODE_REPEAT;
			break;
		}
		case USP_OPCODE_VDP3:
		case USP_OPCODE_VDP4:
		case USP_OPCODE_VMAD3:
		case USP_OPCODE_VMAD4:
		{
			uRepeat = (psHWInst->uWord1 & ~SGXVEC_USE1_SVEC_RCNT_CLRMSK) >> SGXVEC_USE1_SVEC_RCNT_SHIFT;
			eMode = USP_REPEAT_MODE_REPEAT;
			break;
		}
		#endif /* defined(SGX_FEATURE_USE_VEC34) */

		default:
		{
			return IMG_FALSE;
		}
	}

	*peMode		= eMode;
	*puRepeat	= uRepeat;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstEncodeSMLSIInst

 Purpose:	Encode an SMLSI HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			pbUseSwiz		- Whether swizzle or increment mode is required (for
							  each HW operand)
			piIncrements	- The required increments (one for each HW operand)
			puSwizzles		- The required swizzles (one for each HW operand)
			puLimits		- The required TEMP, PA and SA limits (in that order).

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSMLSIInst(PHW_INST		psHWInst,
											  IMG_PBOOL		pbUseSwiz,
											  IMG_PINT32	piIncrements,
											  IMG_PUINT32	puSwizzles,
											  IMG_PUINT32	puLimits)
{
	IMG_UINT32	i;

	/*
		Encode the base SMLSI instruction - no instruction flags supported
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_SPECIAL_OPCAT_MOECTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					   (EURASIA_USE1_MOECTRL_OP2_SMLSI << EURASIA_USE1_MOECTRL_OP2_SHIFT);
	psHWInst->uWord0 = 0;

	/*
		Encode the required swizzle or increments for each operand
	*/
	for (i = 0; i <= USP_OPERANDIDX_MAX; i++)
	{
		static const IMG_UINT32 auIncOrSwizShift[] = 
		{
			EURASIA_USE0_MOECTRL_SMLSI_DINC_SHIFT,
			EURASIA_USE0_MOECTRL_SMLSI_S0INC_SHIFT,
			EURASIA_USE0_MOECTRL_SMLSI_S1INC_SHIFT,
			EURASIA_USE0_MOECTRL_SMLSI_S2INC_SHIFT
		};
		static const IMG_UINT32 auIncOrSwizMask[] = 
		{
			(IMG_UINT32)(~EURASIA_USE0_MOECTRL_SMLSI_DINC_CLRMSK),
			(IMG_UINT32)(~EURASIA_USE0_MOECTRL_SMLSI_S0INC_CLRMSK),
			(IMG_UINT32)(~EURASIA_USE0_MOECTRL_SMLSI_S1INC_CLRMSK),
			(IMG_UINT32)(~EURASIA_USE0_MOECTRL_SMLSI_S2INC_CLRMSK)
		};

		if	(!pbUseSwiz[i])
		{
			if	((piIncrements[i] < -128) || (piIncrements[i] > 128))
			{
				return 0;
			}
			psHWInst->uWord0 |= (piIncrements[i] << auIncOrSwizShift[i]) & 
								auIncOrSwizMask[i];
		}
		else
		{
			static const IMG_UINT32 auUseSwiz[] = 
			{
				EURASIA_USE1_MOECTRL_SMLSI_DUSESWIZ,
				EURASIA_USE1_MOECTRL_SMLSI_S0USESWIZ,
				EURASIA_USE1_MOECTRL_SMLSI_S1USESWIZ,
				EURASIA_USE1_MOECTRL_SMLSI_S2USESWIZ
			};

			if	(puSwizzles[i] > 255)
			{
				return 0;
			}
			psHWInst->uWord0 |= (puSwizzles[i] << auIncOrSwizShift[i]) &
								auIncOrSwizMask[i];
			psHWInst->uWord1 |= auUseSwiz[i];
		}
	}

	/*
		Encode the required register-access limits
	*/
	for (i = 0; i < 3; i++)
	{
		static const IMG_UINT32 auLimitShift[] = 
		{
			EURASIA_USE1_MOECTRL_SMLSI_TLIMIT_SHIFT,
			EURASIA_USE1_MOECTRL_SMLSI_PLIMIT_SHIFT,
			EURASIA_USE1_MOECTRL_SMLSI_SLIMIT_SHIFT
		};
		static const IMG_UINT32 auLimitMask[] = 
		{
			~EURASIA_USE1_MOECTRL_SMLSI_TLIMIT_CLRMSK,
			~EURASIA_USE1_MOECTRL_SMLSI_PLIMIT_CLRMSK,
			~EURASIA_USE1_MOECTRL_SMLSI_SLIMIT_CLRMSK
		};
		static const IMG_UINT32 auLimitGran[] = 
		{
			EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN,
			EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN,
			EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN
		};

		IMG_UINT32	uHWLimit;

		if	(
				(puLimits[i] > (auLimitMask[i] * auLimitGran[i])) ||
				(puLimits[i] % auLimitGran[i])
			)
		{
			return 0;
		}

		uHWLimit = puLimits[i] / auLimitGran[i];

		psHWInst->uWord1 |= (uHWLimit << auLimitShift[i]) & auLimitMask[i];
	}

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeSMBOInst

 Purpose:	Encode an SMBO HW-instruction

 Inputs:	psHWInst		- The HW instruction to initialise
			puBaseOffsets	- The MOE base-offsets for each operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSMBOInst(PHW_INST		psHWInst,
											 IMG_PUINT32	puBaseOffsets)
{
	IMG_UINT32	uMaxBaseOffset;
	IMG_UINT32	i;

	/*
		Encode a base SMBO instruction
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_SPECIAL_OPCAT_MOECTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					   (EURASIA_USE1_MOECTRL_OP2_SMBO << EURASIA_USE1_MOECTRL_OP2_SHIFT);

	/*
		Encode the base-offsets for each of the operands
	*/
	uMaxBaseOffset = (~EURASIA_USE0_MOECTRL_SMR_S2RANGE_CLRMSK) >>
					 EURASIA_USE0_MOECTRL_SMR_S2RANGE_SHIFT;
	
	for (i = 0; i <= USP_OPERANDIDX_MAX; i++)
	{
		if	(puBaseOffsets[i] > uMaxBaseOffset)
		{
			return 0;
		}
	}

	psHWInst->uWord1 |= 
			(((puBaseOffsets[1] >> 8) << EURASIA_USE1_MOECTRL_SMR_S0RANGE_SHIFT) & 
			 (~EURASIA_USE1_MOECTRL_SMR_S0RANGE_CLRMSK)) |
			((puBaseOffsets[0] << EURASIA_USE1_MOECTRL_SMR_DRANGE_SHIFT) & 
			 (~EURASIA_USE1_MOECTRL_SMR_DRANGE_CLRMSK));

	psHWInst->uWord0 = 
			((puBaseOffsets[3] << EURASIA_USE0_MOECTRL_SMR_S2RANGE_SHIFT) & 
			 (~EURASIA_USE0_MOECTRL_SMR_S2RANGE_CLRMSK)) |
			((puBaseOffsets[2] << EURASIA_USE0_MOECTRL_SMR_S1RANGE_SHIFT) & 
			 (~EURASIA_USE0_MOECTRL_SMR_S1RANGE_CLRMSK)) |
			((puBaseOffsets[1] << EURASIA_USE0_MOECTRL_SMR_S0RANGE_SHIFT) & 
			 (~EURASIA_USE0_MOECTRL_SMR_S0RANGE_CLRMSK));


	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeSETFCInst

 Purpose:	Encode an SETFC HW-instruction

 Inputs:	psHWInst	- The HW instruction to initialise
			bColFmtCtl	- Whether per-operand format-control should be
						  enabled for colour instructions
			bEFOFmtCtl	- Whether per-operand format-control should be
						  enabled for floating-point instructions

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSETFCInst(PHW_INST	psHWInst,
										      IMG_BOOL	bColFmtCtl,
											  IMG_BOOL	bEFOFmtCtl)
{
	/*
		Encode a base SETFC instruction
	*/
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_SPECIAL_OPCAT_MOECTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					   (EURASIA_USE1_MOECTRL_OP2_SETFC << EURASIA_USE1_MOECTRL_OP2_SHIFT);

	/*
		Encode whether we want per-operand format control for colour and/or
		floating-point instructions
	*/
	if	(bEFOFmtCtl)
	{
		psHWInst->uWord0 |= EURASIA_USE0_MOECTRL_SETFC_EFO_SELFMTCTL;
	}

	if	(bColFmtCtl)
	{
		psHWInst->uWord0 |= EURASIA_USE0_MOECTRL_SETFC_COL_SETFMTCTL;
	}

	return 1;
}

#if defined(SGX_FEATURE_USE_VEC34)
/*****************************************************************************
 Name:		HWInstEncodeVectorPCKUNPCKInst

 Purpose:	Encode a PCKUNPCK instruction

 Inputs:	psHWInst		- The HW instruction to modify
			bSkipInv		- Whether the SkipInv flag should be set
			eDstFmt			- The destination data format required
			eSrcFmt			- The source data format required
			bScaleEnable	- Whether the data should be scaled/normalised
			uWriteMask		- Byte write-mask for the destination
			psDestReg		- The register to use for the dest operand
			uSrc1Select		- Channel selection for source 1
			uSrc2Select		- Channel selection for source 2
			psSrc1Reg		- The register to use for the src1 operand
			psSrc2Reg		- The register to use for the src2 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeVectorPCKUNPCKInst(PHW_INST			psHWInst, 
													   IMG_BOOL			bSkipInv, 
													   USP_PCKUNPCK_FMT	eDstFmt, 
													   USP_PCKUNPCK_FMT	eSrcFmt, 
													   IMG_BOOL			bScaleEnable, 
													   IMG_UINT32		uWriteMask, 
													   PUSP_REG			psDestReg, 
													   IMG_BOOL			bUseSrcSelect, 
													   IMG_PUINT32		puSrcSelect, 
													   IMG_UINT32		uSrc1Select, 
													   IMG_UINT32		uSrc2Select, 
													   PUSP_REG			psSrc1Reg, 
													   PUSP_REG			psSrc2Reg)
{
	IMG_UINT32	uSrc1Comp;
	IMG_UINT32	uSrc2Comp;
	IMG_UINT32	uHWSrcPckFmt;
	IMG_UINT32	uHWDstPckFmt;
	IMG_INT32	iSrcRegNumDiff; 

	/*
		Encode a base, unconditional, unrepeated PCKUNPCK instruction
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_PCKUNPCK << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT);

	psHWInst->uWord0 = 0;

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	/*
		Encode the source data-format
	*/
	switch (eSrcFmt)
	{
		case USP_PCKUNPCK_FMT_U8:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_U8;
			break;
		}
		case USP_PCKUNPCK_FMT_S8:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_S8;
			break;
		}
		case USP_PCKUNPCK_FMT_O8:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_O8;
			break;
		}
		case USP_PCKUNPCK_FMT_U16:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_U16;
			break;
		}
		case USP_PCKUNPCK_FMT_S16:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_S16;
			break;
		}
		case USP_PCKUNPCK_FMT_F16:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_F16;
			break;
		}
		case USP_PCKUNPCK_FMT_F32:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_F32;
			break;
		}
		case USP_PCKUNPCK_FMT_C10:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_C10;
			break;
		}
		default:
		{
			return 0;
		}
	}

	psHWInst->uWord1 |= (uHWSrcPckFmt << EURASIA_USE1_PCK_SRCF_SHIFT) &
						(~EURASIA_USE1_PCK_SRCF_CLRMSK);

	/*
		Encode the destination data-format
	*/
	switch (eDstFmt)
	{
		case USP_PCKUNPCK_FMT_U8:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_U8;
			break;
		}
		case USP_PCKUNPCK_FMT_S8:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_S8;
			break;
		}
		case USP_PCKUNPCK_FMT_O8:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_O8;
			break;
		}
		case USP_PCKUNPCK_FMT_U16:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_U16;
			break;
		}
		case USP_PCKUNPCK_FMT_S16:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_S16;
			break;
		}
		case USP_PCKUNPCK_FMT_F16:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_F16;
			break;
		}
		case USP_PCKUNPCK_FMT_F32:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_F32;
			break;
		}
		case USP_PCKUNPCK_FMT_C10:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_C10;
			break;
		}
		default:
		{
			return 0;
		}
	}

	psHWInst->uWord1 |= (uHWDstPckFmt << EURASIA_USE1_PCK_DSTF_SHIFT) &
						(~EURASIA_USE1_PCK_DSTF_CLRMSK);

	/*
		Encode the scaling option
	*/
	if	(bScaleEnable)
	{
		psHWInst->uWord0 |= EURASIA_USE0_PCK_SCALE;
	}

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_PCKUNPCK,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the dest byte mask

		NB: We must do this after the destination has been encoded as different
			dest banks can support different masks.
	*/
	if(!bUseSrcSelect)
	{
		if(eDstFmt == USP_PCKUNPCK_FMT_F32)
		{
			uWriteMask = 1;
			if((psDestReg->uNum%2) != 0 && HWInstHasVectorResultPCKUNPCKInst(psHWInst))
			{
				uWriteMask = 2;
				uSrc1Select++;
			}
		}
		else if(eDstFmt == USP_PCKUNPCK_FMT_F16)
		{
			if((psDestReg->uNum%2) != 0 && HWInstHasVectorResultPCKUNPCKInst(psHWInst))
			{
				if(uWriteMask<=3)
				{
					uWriteMask <<= 2;
					uSrc1Select += 2;
					uSrc2Select += 2;
				}
			}
		}
	}

	if	(!HWInstEncodePCKUNPCKInstWriteMask(psHWInst, uWriteMask, IMG_FALSE))
	{
		return 0;
	}

	#if defined(SGX_FEATURE_USE_VEC34)
	/*
		At most 64-bits (two F32 channels) can be written to a unified store destination.
	*/
	if (eDstFmt == USP_PCKUNPCK_FMT_F32 && psDestReg->eType != USP_REGTYPE_INTERNAL && (uWriteMask & 0xC) != 0)
	{
		return 0;
	}
	#endif /* defined(SGX_FEATURE_USE_VEC34) */

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_PCKUNPCK, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	if(eSrcFmt == USP_PCKUNPCK_FMT_F32)
	{
		/*
			Encode the src2 register bank and number
		*/
		if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_PCKUNPCK, psHWInst, psSrc2Reg))
		{
			return 0;
		}
	}


	if(bUseSrcSelect)
	{
		IMG_UINT32 uSrcChanSelect;

		for(uSrcChanSelect=0; uSrcChanSelect<USP_MAX_SAMPLE_CHANS; uSrcChanSelect++)
		{
			if(puSrcSelect[uSrcChanSelect] != (IMG_UINT32)-1)
			{
				switch(uSrcChanSelect)
				{
					case 0:
					{
						if(eSrcFmt == USP_PCKUNPCK_FMT_F32)
						{
							psHWInst->uWord0 |= (((puSrcSelect[uSrcChanSelect] >> 0) & 1) << SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT0_SHIFT);
							psHWInst->uWord0 |= (((puSrcSelect[uSrcChanSelect] >> 1) & 1) << SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT1_SHIFT);
						}
						else
						{
							psHWInst->uWord0 |= (puSrcSelect[uSrcChanSelect] << SGXVEC_USE0_PCK_NONF32SRC_C0SSEL_SHIFT);
						}
						break;
					}

					case 1:
					{
						psHWInst->uWord0 |= (puSrcSelect[uSrcChanSelect] << SGXVEC_USE0_PCK_C1SSEL_SHIFT);
						break;
					}

					case 2:
					{
						psHWInst->uWord0 |= (puSrcSelect[uSrcChanSelect] << SGXVEC_USE0_PCK_C2SSEL_SHIFT);
						break;
					}

					case 3:
					{
						psHWInst->uWord0 |= (puSrcSelect[uSrcChanSelect] << SGXVEC_USE0_PCK_C3SSEL_SHIFT);
						break;
					}

					default :
					{
						return 0;
					}
				}
			}
		}
	}
	else
	{
		uSrc1Comp = psSrc1Reg->uComp;

		if(uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_F16)
		{
			uSrc1Comp = uSrc1Comp/2;
			if((psSrc1Reg->uNum%2) && (psSrc1Reg->eType != USP_REGTYPE_SPECIAL))
			{
				if(uSrc1Comp<=1)
				{
					uSrc1Comp += 2;
				}
			}
		}
		else if(uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_U16 || 
			    uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_S16)
		{
			uSrc1Comp = uSrc1Comp/2;
			if((psSrc1Reg->uNum%2) && (psSrc1Reg->eType != USP_REGTYPE_SPECIAL))
			{
				if(uSrc1Comp<=1)
				{
					uSrc1Comp += 2;
				}
			}
		}
		else if(uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_F32)
		{
			if((psSrc1Reg->uNum%2) && (psSrc1Reg->eType != USP_REGTYPE_SPECIAL))
			{
				uSrc1Comp = 1;
			}
		}

		switch(uSrc1Select)
		{
			case 0:
			{
				if(eSrcFmt == USP_PCKUNPCK_FMT_F32)
				{
					psHWInst->uWord0 |= (((uSrc1Comp >> 0) & 1) << SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT0_SHIFT);
					psHWInst->uWord0 |= (((uSrc1Comp >> 1) & 1) << SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT1_SHIFT);
				}
				else
				{
					psHWInst->uWord0 |= (uSrc1Comp << SGXVEC_USE0_PCK_NONF32SRC_C0SSEL_SHIFT);
				}
				break;
			}

			case 1:
			{
				psHWInst->uWord0 |= (uSrc1Comp << SGXVEC_USE0_PCK_C1SSEL_SHIFT);
				break;
			}

			case 2:
			{
				psHWInst->uWord0 |= (uSrc1Comp << SGXVEC_USE0_PCK_C2SSEL_SHIFT);
				break;
			}

			case 3:
			{
				psHWInst->uWord0 |= (uSrc1Comp << SGXVEC_USE0_PCK_C3SSEL_SHIFT);
				break;
			}

			default :
			{
				return 0;
			}
		}

		iSrcRegNumDiff = psSrc2Reg->uNum - psSrc1Reg->uNum;

		if(iSrcRegNumDiff<0)
		{
			iSrcRegNumDiff *= -1;
		}

		/*
			Only decode Src2 if we are merging
		*/
		if(psSrc1Reg->eType == psSrc2Reg->eType && 
			psSrc1Reg->eFmt == psSrc2Reg->eFmt &&
			psSrc1Reg->eDynIdx == psSrc2Reg->eDynIdx && 
			((psSrc1Reg->uNum == psSrc2Reg->uNum) || 
			 (eSrcFmt == USP_PCKUNPCK_FMT_F32) || 
			 ((eSrcFmt == USP_PCKUNPCK_FMT_F16) && (iSrcRegNumDiff == 1))))
		{

			uSrc2Comp = psSrc2Reg->uComp;

			if(uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_F16)
			{
				uSrc2Comp = uSrc2Comp/2;
				if((psSrc2Reg->uNum%2) && (psSrc1Reg->eType != USP_REGTYPE_SPECIAL))
				{
					if(uSrc2Comp<=1)
					{
						uSrc2Comp += 2;
					}
				}
			}
			else if(uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_U16 || 
				    uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_S16)
			{
				uSrc2Comp = uSrc2Comp/2;
				if((psSrc2Reg->uNum%2) && (psSrc1Reg->eType != USP_REGTYPE_SPECIAL))
				{
					if(uSrc2Comp<=1)
					{
						uSrc2Comp += 2;
					}
				}
			}
			else if(uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_F32)
			{
				if((psSrc2Reg->uNum%2) && (psSrc1Reg->eType != USP_REGTYPE_SPECIAL))
				{
					uSrc2Comp = 1;
				}
				uSrc2Comp += 2;
			}
			switch(uSrc2Select)
			{
				case 0:
				{
					if(eSrcFmt == USP_PCKUNPCK_FMT_F32)
					{
						psHWInst->uWord0 |= (((uSrc2Comp >> 0) & 1) << SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT0_SHIFT);
						psHWInst->uWord0 |= (((uSrc2Comp >> 1) & 1) << SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT1_SHIFT);
					}
					else
					{
						psHWInst->uWord0 |= (uSrc2Comp << SGXVEC_USE0_PCK_NONF32SRC_C0SSEL_SHIFT);
					}
					break;
				}

				case 1:
				{
					psHWInst->uWord0 |= (uSrc2Comp << SGXVEC_USE0_PCK_C1SSEL_SHIFT);
					break;
				}

				case 2:
				{
					psHWInst->uWord0 |= (uSrc2Comp << SGXVEC_USE0_PCK_C2SSEL_SHIFT);
					break;
				}

				case 3:
				{
					psHWInst->uWord0 |= (uSrc2Comp << SGXVEC_USE0_PCK_C3SSEL_SHIFT);
					break;
				}

				default :
				{
					return 0;
				}
			}
		}
	}
	return 1;
}
#else
/*****************************************************************************
 Name:		HWInstEncodePCKUNPCKInstNonVec

 Purpose:	Encode a PCKUNPCK instruction

 Inputs:	psHWInst		- The HW instruction to modify
			bSkipInv		- Whether the SkipInv flag should be set
			eDstFmt			- The destination data format required
			eSrcFmt			- The source data format required
			bScaleEnable	- Whether the data should be scaled/normalised
			uWriteMask		- Byte write-mask for the destination
			psDestReg		- The register to use for the dest operand
			psSrc1Reg		- The register to use for the src1 operand
			psSrc2Reg		- The register to use for the src2 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
static IMG_UINT32 HWInstEncodePCKUNPCKInstNonVec(PHW_INST			psHWInst,
												 IMG_BOOL			bSkipInv,
							  					 USP_PCKUNPCK_FMT	eDstFmt,
							  					 USP_PCKUNPCK_FMT	eSrcFmt,
							  					 IMG_BOOL			bScaleEnable,
							  					 IMG_UINT32			uWriteMask,
							  					 PUSP_REG			psDestReg,
												 PUSP_REG			psSrc1Reg,
												 PUSP_REG			psSrc2Reg)
{
	IMG_UINT32	uSrc1Comp;
	IMG_UINT32	uSrc2Comp;
	IMG_UINT32	uHWSrcPckFmt;
	IMG_UINT32	uHWDstPckFmt;
	IMG_BOOL	bDestFloat = IMG_FALSE;
	IMG_BOOL	bSrcFloat = IMG_FALSE;

	/*
		Encode a base, unconditional, unrepeated PCKUNPCK instruction
	*/
	psHWInst->uWord1 = (EURASIA_USE1_OP_PCKUNPCK << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
					   EURASIA_USE1_RCNTSEL;

	psHWInst->uWord0 = 0;

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	/*
		Encode the source data-format
	*/
	switch (eSrcFmt)
	{
		case USP_PCKUNPCK_FMT_U8:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_U8;
			break;
		}
		case USP_PCKUNPCK_FMT_S8:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_S8;
			break;
		}
		case USP_PCKUNPCK_FMT_O8:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_O8;
			break;
		}
		case USP_PCKUNPCK_FMT_U16:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_U16;
			break;
		}
		case USP_PCKUNPCK_FMT_S16:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_S16;
			break;
		}
		case USP_PCKUNPCK_FMT_F16:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_F16;
			bSrcFloat = IMG_TRUE;
			break;
		}
		case USP_PCKUNPCK_FMT_F32:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_F32;
			bSrcFloat = IMG_TRUE;
			break;
		}
		case USP_PCKUNPCK_FMT_C10:
		{
			uHWSrcPckFmt = EURASIA_USE1_PCK_FMT_C10;
			break;
		}
		default:
		{
			return 0;
		}
	}

	psHWInst->uWord1 |= (uHWSrcPckFmt << EURASIA_USE1_PCK_SRCF_SHIFT) &
						(~EURASIA_USE1_PCK_SRCF_CLRMSK);

	/*
		Encode the destination data-format
	*/
	switch (eDstFmt)
	{
		case USP_PCKUNPCK_FMT_U8:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_U8;
			break;
		}
		case USP_PCKUNPCK_FMT_S8:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_S8;
			break;
		}
		case USP_PCKUNPCK_FMT_O8:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_O8;
			break;
		}
		case USP_PCKUNPCK_FMT_U16:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_U16;
			break;
		}
		case USP_PCKUNPCK_FMT_S16:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_S16;
			break;
		}
		case USP_PCKUNPCK_FMT_F16:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_F16;
			bDestFloat = IMG_TRUE;
			break;
		}
		case USP_PCKUNPCK_FMT_F32:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_F32;
			bDestFloat = IMG_TRUE;
			break;
		}
		case USP_PCKUNPCK_FMT_C10:
		{
			uHWDstPckFmt = EURASIA_USE1_PCK_FMT_C10;
			break;
		}
		default:
		{
			return 0;
		}
	}

	psHWInst->uWord1 |= (uHWDstPckFmt << EURASIA_USE1_PCK_DSTF_SHIFT) &
						(~EURASIA_USE1_PCK_DSTF_CLRMSK);

	/*
		Encode the scaling option
	*/
	if	(bScaleEnable)
	{
		psHWInst->uWord0 |= EURASIA_USE0_PCK_SCALE;
	}

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_PCKUNPCK,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the dest byte mask

		NB: We must do this after the destination has been encoded as different
			dest banks can support different masks.
	*/
	if	(!HWInstEncodePCKUNPCKInstWriteMask(psHWInst, uWriteMask, IMG_FALSE))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_PCKUNPCK, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	/*
		Encode the source 1 byte-offset
	*/
	uSrc1Comp = psSrc1Reg->uComp;
	if	(uSrc1Comp > 3)
	{
		return 0;
	}

	switch (uHWSrcPckFmt)
	{
		case EURASIA_USE1_PCK_FMT_U8:
		case EURASIA_USE1_PCK_FMT_S8:
		case EURASIA_USE1_PCK_FMT_O8:
		{
			/*
				Any byte-offset < 4 is OK
			*/
			break;
		}
		case EURASIA_USE1_PCK_FMT_U16:
		case EURASIA_USE1_PCK_FMT_S16:
		case EURASIA_USE1_PCK_FMT_F16:
		{
			/*
				Byte-offset must be 0 or 2
			*/
			if	(
					(uSrc1Comp != 0) &&
					(uSrc1Comp != 2)
				)
			{
				return 0;
			}
			break;
		}

		case EURASIA_USE1_PCK_FMT_F32:
		{
			/*
				Byte-offset must be zero
			*/
			if	(uSrc1Comp != 0)
			{
				return 0;
			}
			break;
		}

		case EURASIA_USE1_PCK_FMT_C10:
		{
			/*
				If sourcing data fom an IReg, any offset < 3 is OK.
				Otherwise, only 0-2 are available.
			*/
			if	(psSrc1Reg->eType != USP_REGTYPE_INTERNAL)
			{
				if	(uSrc1Comp > 2)
				{
					return 0;
				}
			}
			break;			
		}

		default:
		{
			return 0;
		}
	}

	psHWInst->uWord0 |= (uSrc1Comp << EURASIA_USE0_PCK_S1SCSEL_SHIFT) &
						(~EURASIA_USE0_PCK_S1SCSEL_CLRMSK);

	/*
		Encode the src2 register bank and number
	*/
	if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_PCKUNPCK, psHWInst, psSrc2Reg))
	{
		return 0;
	}

	/*
		Encode the source 2 byte offset
	*/
	uSrc2Comp = psSrc2Reg->uComp;
	if	(uSrc2Comp > 3)
	{
		return 0;
	}

	switch (uHWSrcPckFmt)
	{
		case EURASIA_USE1_PCK_FMT_U8:
		case EURASIA_USE1_PCK_FMT_S8:
		case EURASIA_USE1_PCK_FMT_O8:
		{
			/*
				Any byte-offset < 4 is OK
			*/
			break;
		}
		case EURASIA_USE1_PCK_FMT_U16:
		case EURASIA_USE1_PCK_FMT_S16:
		case EURASIA_USE1_PCK_FMT_F16:
		{
			/*
				Byte-offset must be 0 or 2
			*/
			if	(
					(uSrc2Comp != 0) && 
					(uSrc2Comp != 2)
				)
			{
				return 0;
			}
			break;
		}

		case EURASIA_USE1_PCK_FMT_F32:
		{
			/*
				Byte-offset must be zero
			*/
			if	(uSrc2Comp != 0)
			{
				return 0;
			}
			break;
		}

		case EURASIA_USE1_PCK_FMT_C10:
		{
			/*
				If sourcing data fom an IReg, any offset < 3 is OK.
				Otherwise, only 0-2 are available.
			*/
			if	(psSrc2Reg->eType != USP_REGTYPE_INTERNAL)
			{
				if	(uSrc2Comp > 2)
				{
					return 0;
				}
			}
			break;
		}

		default:
		{
			return 0;
		}
	}

	psHWInst->uWord0 |= (uSrc2Comp << EURASIA_USE0_PCK_S2SCSEL_SHIFT) &
						(~EURASIA_USE0_PCK_S2SCSEL_CLRMSK);

	return 1;
}
#endif /* defined(SGX_FEATURE_USE_VEC34) */

/*****************************************************************************
 Name:		HWInstEncodePCKUNPCKInst

 Purpose:	Encode a PCKUNPCK instruction

 Inputs:	psHWInst		- The HW instruction to modify
			bSkipInv		- Whether the SkipInv flag should be set
			eDstFmt			- The destination data format required
			eSrcFmt			- The source data format required
			bScaleEnable	- Whether the data should be scaled/normalised
			uWriteMask		- Byte write-mask for the destination
			psDestReg		- The register to use for the dest operand
			uSrc1Select		- Channel selection for source 1 only for SGX543
			uSrc2Select		- Channel selection for source 2 only for SGX543
			psSrc1Reg		- The register to use for the src1 operand
			psSrc2Reg		- The register to use for the src2 operand

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodePCKUNPCKInst(PHW_INST			psHWInst,
												 IMG_BOOL			bSkipInv,
							  					 USP_PCKUNPCK_FMT	eDstFmt,
							  					 USP_PCKUNPCK_FMT	eSrcFmt,
							  					 IMG_BOOL			bScaleEnable,
							  					 IMG_UINT32			uWriteMask,
							  					 PUSP_REG			psDestReg,
												 IMG_UINT32			uSrc1Select,
												 IMG_UINT32			uSrc2Select,
												 PUSP_REG			psSrc1Reg,
												 PUSP_REG			psSrc2Reg)
{
	#if defined(SGX_FEATURE_USE_VEC34)
	return HWInstEncodeVectorPCKUNPCKInst(psHWInst, bSkipInv, eDstFmt, eSrcFmt, 
		bScaleEnable, uWriteMask, psDestReg, IMG_FALSE, IMG_NULL, uSrc1Select, 
		uSrc2Select, psSrc1Reg, psSrc2Reg);
	#else
	PVR_UNREFERENCED_PARAMETER(uSrc1Select);
	PVR_UNREFERENCED_PARAMETER(uSrc2Select);
	return HWInstEncodePCKUNPCKInstNonVec(psHWInst, bSkipInv, eDstFmt, 
		eSrcFmt, bScaleEnable, uWriteMask, psDestReg, psSrc1Reg, psSrc2Reg);
	#endif /* defined(SGX_FEATURE_USE_VEC34) */
}

/*****************************************************************************
 Name:		HWInstSetBranchOffset

 Purpose:	Encode the offset for a relative branch

 Inputs:	psHWInst	- The HW instruction to modify
			iOffset		- The required branch offset

 Outputs:	none

 Returns:	IMG_TRUE/FALSE on success/failure.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstSetBranchOffset(PHW_INST psHWInst,
											IMG_INT32 iOffset)
{
	/*
		Validate the required branch offset
	*/
	if	(
			(iOffset < -(IMG_INT32)((((~EURASIA_USE0_BRANCH_OFFSET_CLRMSK) >> EURASIA_USE0_BRANCH_OFFSET_SHIFT) + 1) / 2)) ||
			(iOffset >= (IMG_INT32)((((~EURASIA_USE0_BRANCH_OFFSET_CLRMSK) >> EURASIA_USE0_BRANCH_OFFSET_SHIFT) + 1) / 2))
		)
	{
		return IMG_FALSE;
	}

	psHWInst->uWord0 &= EURASIA_USE0_BRANCH_OFFSET_CLRMSK;
	psHWInst->uWord0 |= (iOffset << EURASIA_USE0_BRANCH_OFFSET_SHIFT) &
						(~EURASIA_USE0_BRANCH_OFFSET_CLRMSK);

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstSetBranchAddr

 Purpose:	Encode the address for an absolute branch

 Inputs:	psHWInst	- The HW instruction to modify
			iAddr		- The required branch address

 Outputs:	none

 Returns:	IMG_TRUE/FALSE on success/failure.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstSetBranchAddr(PHW_INST psHWInst,
										  IMG_INT32 iAddr)
{
	/*
		Validate the required branch address
	*/
	if	(
			(iAddr < -(IMG_INT32)(((~EURASIA_USE0_BRANCH_OFFSET_CLRMSK) >> EURASIA_USE0_BRANCH_OFFSET_SHIFT) + 1)) ||
			(iAddr >= (IMG_INT32)(((~EURASIA_USE0_BRANCH_OFFSET_CLRMSK) >> EURASIA_USE0_BRANCH_OFFSET_SHIFT) + 1))
		)
	{
		return IMG_FALSE;
	}

	/* Set the opcode to be an absolute branch */
	psHWInst->uWord1 &= EURASIA_USE1_FLOWCTRL_OP2_CLRMSK;
	psHWInst->uWord1 |= (EURASIA_USE1_FLOWCTRL_OP2_BA << EURASIA_USE1_FLOWCTRL_OP2_SHIFT) &
						(~EURASIA_USE1_FLOWCTRL_OP2_CLRMSK);

	psHWInst->uWord0 &= EURASIA_USE0_BRANCH_OFFSET_CLRMSK;
	psHWInst->uWord0 |= (iAddr << EURASIA_USE0_BRANCH_OFFSET_SHIFT) &
						(~EURASIA_USE0_BRANCH_OFFSET_CLRMSK);

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstSupportsNoSched

 Purpose:	Determine whether a HW instruction supports the NoSched flag

 Inputs:	eOpcode	- The decoded opcode of the HW instruction to check

 Outputs:	none

 Returns:	IMG_TRUE if the instruction can have the NoSched flag set
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstSupportsNoSched(USP_OPCODE eOpcode)
#if defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING)
{
	PVR_UNREFERENCED_PARAMETER(eOpcode);
	#if defined(SGX_FEATURE_USE_EXTENDED_LOAD)
	/*
		Cores which support the ELDD/ELDQ instructions don't
		support the NOSCHED flag on memory load instructions.
	*/
	if (eOpcode == USP_OPCODE_LD)
	{
		return IMG_FALSE;
	}
	#endif /* defined(SGX_FEATURE_USE_EXTENDED_LOAD) */
	/*
		Preamble instructions represent parts of the program
		reserved by us for the driver to overwrite with its
		own code. So don't try and set NOSCHED on them.
	*/
	if (eOpcode == USP_OPCODE_PREAMBLE)
	{
		return IMG_FALSE;
	}
	/*
		If NOSCHED is set on flow control instructions then
		scheduling is disabled between the next two
		instructions at the destination of the branch. The
		NOSCHED insertion code isn't smart enough to handle this 
		for the branch taken case (it treats instructions as though 
		they are always sequentially executed). So pretend NOSCHED 
		isn't supported,
	*/
	if (eOpcode == USP_OPCODE_BR || 
		eOpcode == USP_OPCODE_BA ||
		eOpcode == USP_OPCODE_LAPC)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}
#else /* defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING) */
{
	switch (eOpcode)
	{
		case USP_OPCODE_LD:
		case USP_OPCODE_ST:
		case USP_OPCODE_IDF:
		case USP_OPCODE_WDF:
		case USP_OPCODE_SMP:
		case USP_OPCODE_SMPBIAS:
		case USP_OPCODE_SMPREPLACE:
		case USP_OPCODE_SMPGRAD:
		case USP_OPCODE_AND:
		case USP_OPCODE_OR:
		case USP_OPCODE_SHL:
		case USP_OPCODE_SHR:
		case USP_OPCODE_XOR:
		case USP_OPCODE_RLP:
		case USP_OPCODE_ASR:
		case USP_OPCODE_BA:
		case USP_OPCODE_BR:
		case USP_OPCODE_LAPC:
		case USP_OPCODE_SETL:
		case USP_OPCODE_SAVL:
		case USP_OPCODE_SMOA:
		case USP_OPCODE_SMR:
		case USP_OPCODE_SMLSI:
		case USP_OPCODE_SMBO:
		case USP_OPCODE_IMO:
		case USP_OPCODE_SETFC:
		case USP_OPCODE_TEST:
		case USP_OPCODE_TESTMASK:
		case USP_OPCODE_PREAMBLE:
		#if defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_VDUAL:
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
		{
			return IMG_FALSE;
		}

		default:
		{
			break;
		}
	}

	return IMG_TRUE;
}
#endif /* defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING) */

/*****************************************************************************
 Name:		HWInstSetNoSched

 Purpose:	Set the NoSched flag on a HW instruction

 Inputs:	eOpcode			- The decoded opcode of the instruction
			psHWInst		- The HW instruction to modify
			bNoSchedState	- The state of the NoSched flag

 Outputs:	none

 Returns:	IMG_TRUE/FALSE on success/failure.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstSetNoSched(USP_OPCODE	eOpcode,
									   PHW_INST		psHWInst,
									   IMG_BOOL		bNoSchedState)
{
	#if defined(DEBUG)
	{
		/*
			Make sure this instruction can take the flag
		*/
		if	(!HWInstSupportsNoSched(eOpcode))
		{
			return IMG_FALSE;
		}
	}
	#endif /* #if defined(DEBUG) */

	/*
		Set the NoSched flag on the HW instruction
	*/
	switch (eOpcode)
	{
		case USP_OPCODE_PCKUNPCK:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_PCK_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_PCK_NOSCHED;
			}
			break;
		}

		case USP_OPCODE_DOT3:
		case USP_OPCODE_DOT4:
		case USP_OPCODE_SOP2:
		case USP_OPCODE_SOP3:
		case USP_OPCODE_SOPWM:
		case USP_OPCODE_IMA8:
		case USP_OPCODE_IMA16:
		case USP_OPCODE_IMAE:
		case USP_OPCODE_ADIF:
		case USP_OPCODE_BILIN:
		case USP_OPCODE_FIRV:
		case USP_OPCODE_FIRH:
		case USP_OPCODE_FPMA:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_INT_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_INT_NOSCHED;
			}
			break;
		}

		case USP_OPCODE_LIMM:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_LIMM_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_LIMM_NOSCHED;
			}
			break;
		}

		#if defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING)
		case USP_OPCODE_SMOA:
		case USP_OPCODE_SMR:
		case USP_OPCODE_SMLSI:
		case USP_OPCODE_SMBO:
		case USP_OPCODE_IMO:
		case USP_OPCODE_SETFC:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_MOECTRL_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_MOECTRL_NOSCHED;
			}
			break;
		}

		case USP_OPCODE_TEST:
		case USP_OPCODE_TESTMASK:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_TEST_NOPAIR_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_TEST_NOPAIR_NOSCHED;
			}
			break;
		}

		case USP_OPCODE_AND:
		case USP_OPCODE_OR:
		case USP_OPCODE_SHL:
		case USP_OPCODE_SHR:
		case USP_OPCODE_RLP:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_BITWISE_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_BITWISE_NOSCHED;
			}
			break;
		}

		case USP_OPCODE_SMP:
		case USP_OPCODE_SMPBIAS:
		case USP_OPCODE_SMPREPLACE:
		case USP_OPCODE_SMPGRAD:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_SMP_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_SMP_NOSCHED;
			}
			break;
		}

		case USP_OPCODE_LD:
		case USP_OPCODE_ST:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_LDST_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_LDST_NOSCHED;
			}
			break;
		}

		case USP_OPCODE_BA:
		case USP_OPCODE_BR:
		case USP_OPCODE_SETL:
		case USP_OPCODE_SAVL:
		case USP_OPCODE_LAPC:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_FLOWCTRL_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_FLOWCTRL_NOSCHED;
			}
			break;
		}

		#if defined(SGX_FEATURE_USE_IDXSC)
		case USP_OPCODE_IDXSCR:
		case USP_OPCODE_IDXSCW:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= SGXVEC_USE1_OTHER2_IDXSC_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~SGXVEC_USE1_OTHER2_IDXSC_NOSCHED;
			}
			break;
		}
		#endif /* defined(SGX_FEATURE_USE_IDXSC) */

		case USP_OPCODE_IDF:
		case USP_OPCODE_WDF:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_IDFWDF_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_IDFWDF_NOSCHED;
			}
			break;
		}
		#endif /* defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING) */

		#if defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_VDUAL:
		{
			/*
				The NOSCHED flag is always implicitly set on
				vector dual-issue instructions.
			*/
			break;
		}
		#endif /* defined(SGX_FEATURE_USE_VEC34) */

		default:
		{
			if	(bNoSchedState)
			{
				psHWInst->uWord1 |= EURASIA_USE1_NOSCHED;
			}
			else
			{
				psHWInst->uWord1 &= ~EURASIA_USE1_NOSCHED;
			}
			break;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstEncodeNOPInst

 Purpose:	Encode a NOP HW instruction

 Inputs:	psHWInst		- The HW instruction to modify

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeNOPInst(PHW_INST psHWInst)
{
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					   (EURASIA_USE1_FLOWCTRL_OP2_NOP << EURASIA_USE1_FLOWCTRL_OP2_SHIFT);

	return 1;
}

/*****************************************************************************
 Name:		HWInstNeedsSyncStartBefore

 Purpose:	Determine whether a HW instruction needs the sync-start flag set
			on the preceeding instruction

 Inputs:	eOpcode	- The decoded opcode of the HW instruction to check
			psHWInst - The instruction to check.

 Outputs:	none

 Returns:	IMG_TRUE if the instruction can have the SyncStart flag set
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstNeedsSyncStartBefore(USP_OPCODE eOpcode,
												 PHW_INST	psHWInst)
{
	switch (eOpcode)
	{
		case USP_OPCODE_SMP:
		case USP_OPCODE_SMPBIAS:
		#if defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_VDSX:
		case USP_OPCODE_VDSY:
		#else /* defined(SGX_FEATURE_USE_VEC34) */
		case USP_OPCODE_DSX:
		case USP_OPCODE_DSY:
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
		{
			return IMG_TRUE;
		}

		case USP_OPCODE_TEST:
		case USP_OPCODE_TESTMASK:
		{
			IMG_UINT32	uALUSel, uALUOp;

			/*
				Check the ALU operand whose result we are testing is
				DSX or DSY.
			*/
			uALUSel	= (psHWInst->uWord0 & ~EURASIA_USE0_TEST_ALUSEL_CLRMSK) >>
					  EURASIA_USE0_TEST_ALUSEL_SHIFT;
			uALUOp	= (psHWInst->uWord0 & ~EURASIA_USE0_TEST_ALUOP_CLRMSK) >>
					  EURASIA_USE0_TEST_ALUOP_SHIFT;
			if (
					uALUSel == EURASIA_USE0_TEST_ALUSEL_F32 &&
					(
						#if defined(SGX_FEATURE_USE_VEC34)
						uALUOp == SGXVEC_USE0_TEST_ALUOP_F32_VDSX ||
						uALUOp == SGXVEC_USE0_TEST_ALUOP_F32_VDSY
						#else /* defined(SGX_FEATURE_USE_VEC34) */
						uALUOp == EURASIA_USE0_TEST_ALUOP_F32_DSX ||
						uALUOp == EURASIA_USE0_TEST_ALUOP_F32_DSY
						#endif /* defined(SGX_FEATURE_USE_VEC34) */
					)
			   )
			{
				return IMG_TRUE;
			}
			break;
		}

		default:
		{
			break;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 Name:		HWInstSupportsSyncStart

 Purpose:	Determine whether a HW instruction supports the SyncStart flag

 Inputs:	eOpcode	- The decoded opcode of the HW instruction to check

 Outputs:	none

 Returns:	IMG_TRUE if the instruction can have the SyncStart flag set
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstSupportsSyncStart(USP_OPCODE eOpcode)
{
	switch (eOpcode)
	{
		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_EFO:
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */
		case USP_OPCODE_SOP2:
		case USP_OPCODE_SOP3:
		case USP_OPCODE_SOPWM:
		case USP_OPCODE_IMA8:
		case USP_OPCODE_IMA16:
		case USP_OPCODE_IMAE:
		case USP_OPCODE_ADIF:
		case USP_OPCODE_BILIN:
		case USP_OPCODE_FIRV:
		case USP_OPCODE_FIRH:
		case USP_OPCODE_DOT3:
		case USP_OPCODE_DOT4:
		case USP_OPCODE_FPMA:
		case USP_OPCODE_IDF:
		case USP_OPCODE_WDF:
		case USP_OPCODE_BA:
		case USP_OPCODE_BR:
		case USP_OPCODE_LAPC:
		case USP_OPCODE_SETL:
		case USP_OPCODE_SAVL:
		case USP_OPCODE_SMOA:
		case USP_OPCODE_SMR:
		case USP_OPCODE_SMLSI:
		case USP_OPCODE_SMBO:
		case USP_OPCODE_IMO:
		case USP_OPCODE_SETFC:
		case USP_OPCODE_PREAMBLE:
		case USP_OPCODE_LIMM:
		#if defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_VDP3:
		case USP_OPCODE_VDP4:
		case USP_OPCODE_VMAD3:
		case USP_OPCODE_VMAD4:
		case USP_OPCODE_VDUAL:
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
		{
			return IMG_FALSE;
		}

		default:
		{
			break;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstGetSyncStart

 Purpose:	Get the state of the SyncStart flag on an instruction

 Inputs:	eOpcode				- The decoded opcode of the HW instruction to
								  check
			psHWInst			- The HW instruction to check
			pbSyncStartState	- The decoded state of the SyncStart flag

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstGetSyncStart(USP_OPCODE	eOpcode,
										 PHW_INST	psHWInst,
										 IMG_BOOL*	pbSyncStartState)
{
	#if defined(DEBUG)
	/*
		Make sure the opcode supports the sync-start flag
	*/
	if	(!HWInstSupportsSyncStart(eOpcode))
	{
		return IMG_FALSE;
	}
	#else	/* #if defined(DEBUG) */
	USP_UNREFERENCED_PARAMETER(eOpcode);
	#endif	/* #if defined(DEBUG) */

	if	(eOpcode == USP_OPCODE_NOP)
	{
		if	(psHWInst->uWord1 & EURASIA_USE1_FLOWCTRL_NOP_SYNCS)
		{
			*pbSyncStartState = IMG_TRUE;
		}
		else
		{
			*pbSyncStartState = IMG_FALSE;
		}
	}
	else
	{
		if	(psHWInst->uWord1 & EURASIA_USE1_SYNCSTART)
		{
			*pbSyncStartState = IMG_TRUE;
		}
		else
		{
			*pbSyncStartState = IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstSetSyncStart

 Purpose:	Set the state of the SyncStart flag on an instruction

			NB: This routine assumes that the SyncStart flag is valid for the
				instruction. It does not check this.

 Inputs:	eOpcode			- The decoded opcode of the HW instruction to
							  modify
			psHWInst		- The HW instruction to modify
			bSyncStartState	- The required SyncStart flag state

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstSetSyncStart(USP_OPCODE	eOpcode,
										 PHW_INST	psHWInst,
										 IMG_BOOL	bSyncStartState)
{
	#if defined(DEBUG)
	/*
		Make sure the opcode supports the sync-start flag
	*/
	if	(!HWInstSupportsSyncStart(eOpcode))
	{
		return IMG_FALSE;
	}
	#else	/* #if defined(DEBUG) */
	USP_UNREFERENCED_PARAMETER(eOpcode);
	#endif	/* #if defined(DEBUG) */

	if	(eOpcode == USP_OPCODE_NOP)
	{
		if	(bSyncStartState)
		{
			psHWInst->uWord1 |= EURASIA_USE1_FLOWCTRL_NOP_SYNCS;
		}
		else
		{
			psHWInst->uWord1 &= ~EURASIA_USE1_FLOWCTRL_NOP_SYNCS;
		}
	}
	else
	{
		if	(bSyncStartState)
		{
			psHWInst->uWord1 |= EURASIA_USE1_SYNCSTART;
		}
		else
		{
			psHWInst->uWord1 &= ~EURASIA_USE1_SYNCSTART;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstSupportsSyncEnd

 Purpose:	Determine whether a HW instruction supports the SyncEnd flag

 Inputs:	eOpcode	- The decoded opcode of the HW instruction to check

 Outputs:	none

 Returns:	IMG_TRUE if the instruction can have the SyncEnd flag set
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstSupportsSyncEnd(USP_OPCODE eOpcode)
{
	switch (eOpcode)
	{
		case USP_OPCODE_BA:
		case USP_OPCODE_BR:
		case USP_OPCODE_LAPC:
		case USP_OPCODE_SETL:
		case USP_OPCODE_SAVL:
		case USP_OPCODE_NOP:
		{
			return IMG_TRUE;
		}

		default:
		{
			break;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 Name:		HWInstGetSyncEnd

 Purpose:	Get the state of the SyncEnd flag on an instruction

 Inputs:	eOpcode				- The decoded opcode of the HW instruction to
								  check
			psHWInst			- The HW instruction to check
			pbSyncEndState	- The decoded state of the SyncEnd flag

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstGetSyncEnd(USP_OPCODE	eOpcode,
									   PHW_INST		psHWInst,
									   IMG_BOOL*	pbSyncEndState)
{
	#if defined(DEBUG)
	/*
		Make sure the opcode supports the sync-start flag
	*/
	if	(!HWInstSupportsSyncEnd(eOpcode))
	{
		return IMG_FALSE;
	}
	#else	/* #if defined(DEBUG) */
	USP_UNREFERENCED_PARAMETER(eOpcode);
	#endif	/* #if defined(DEBUG) */

	if	(psHWInst->uWord1 & EURASIA_USE1_FLOWCTRL_SYNCEND)
	{
		*pbSyncEndState = IMG_TRUE;
	}
	else
	{
		*pbSyncEndState = IMG_FALSE;
	}
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstForcesDeschedule

 Purpose:	Determine whether a HW instruction forces a deschedule

 Inputs:	eOpcode		- The decoded opcode of the HW instruction to check
			psHWInst	- The HW instruction to check

 Outputs:	none

 Returns:	Whether the instruction will force a deschedule at the end of
			it's instruction-pair.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstForcesDeschedule(USP_OPCODE	eOpcode,
											 PHW_INST	psHWInst)
{
	IMG_BOOL bForcesDeschedule;

	bForcesDeschedule = IMG_FALSE;

	switch (eOpcode)
	{
		case USP_OPCODE_BA:
		case USP_OPCODE_BR:
		case USP_OPCODE_LAPC:
		case USP_OPCODE_WDF:
		case USP_OPCODE_WOP:
		case USP_OPCODE_LOCK:
		{
			/*
				Execution is always descheduled by these instructions.
			*/
			bForcesDeschedule = IMG_TRUE;
			break;
		}

		default:
		{
			/*
				Execution is descheduled after instructions with the SyncStart
				modifier set.
			*/
			if	(HWInstSupportsSyncStart(eOpcode))
			{
				HWInstGetSyncStart(eOpcode, psHWInst, &bForcesDeschedule);
			}

			break;
		}
	}

	return bForcesDeschedule;
}

/*****************************************************************************
 name:		HWInstIsIllegalInstPair

 purpose:	Check for an illegal combination of instructions.

 inputs:	eOpcode1	- The opcode of the even instruction in a pair
			psHWInst1	- The even instruction of a pair
			eOpcode2	- The opcode of the odd instruction within a pair
			psHWInst2	- The odd instruction in the pair
			
 outputs:	none

 returns:	IMG_TRUE or IMG_FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstIsIllegalInstPair(USP_OPCODE	eOpcode1,
											  PHW_INST 		psHWInst1,
											  USP_OPCODE	eOpcode2,
											  PHW_INST		psHWInst2)
{
	if	(
			(
				(eOpcode1 == USP_OPCODE_LAPC) || 
				(eOpcode1 == USP_OPCODE_BR) || 
				(eOpcode1 == USP_OPCODE_BA)
			) &&
			(
				(eOpcode2 == USP_OPCODE_LAPC) || 
				(eOpcode2 == USP_OPCODE_BR) || 
				(eOpcode1 == USP_OPCODE_BA)
			)
		)
	{
		/*
			Can't have two flow-control insts in a pair
		*/
		return IMG_TRUE;
	}

	if	(
			(eOpcode1 == USP_OPCODE_SETL) && 
			(
				(eOpcode2 == USP_OPCODE_SAVL) ||
				(eOpcode2 == USP_OPCODE_LAPC)
			)
		)
	{
		/*
			Can't set the link-address and then use it in the same pair
		*/
		return IMG_TRUE;
	}

	if	(eOpcode1 == USP_OPCODE_SETL)
	{
		if	(
				(eOpcode2 == USP_OPCODE_BR) &&
				(psHWInst2->uWord1 & EURASIA_USE1_BRANCH_SAVELINK)
			)
		{
			/*
				Can't have both instructions writing the link-address
			*/
			return IMG_TRUE;
		}
	}

	if	(eOpcode2 == USP_OPCODE_SAVL)
	{
		if	(
				(eOpcode1 == USP_OPCODE_BR) &&
				(psHWInst1->uWord1 & EURASIA_USE1_BRANCH_SAVELINK)
			)
		{
			/*
				Can't have both instructions writing the link-address
			*/
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 name:		HWInstSupportsEnd

 purpose:	Test whether a HW instruction supports the END flag

 inputs:	eOpcode	- The opcode of the HW instruction

 outputs:	none

 returns:	IMG_TRUE or IMG_FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstSupportsEnd(USP_OPCODE eOpcode)
{
	switch (eOpcode)
	{
		case USP_OPCODE_WDF:
		case USP_OPCODE_BR:
		case USP_OPCODE_BA:
		#if !defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_EFO:
		#endif /* !defined(SGX_FEATURE_USE_VEC34) */
		case USP_OPCODE_TEST:
		case USP_OPCODE_SMLSI:
		case USP_OPCODE_SMBO:
		case USP_OPCODE_SETFC:
		#if defined(SGX_FEATURE_USE_FMAD16_SWIZZLES)
		case USP_OPCODE_FMAD16:
		#endif /* defined(SGX_FEATURE_USE_FMAD16_SWIZZLES) */
		#if defined(SGX_FEATURE_USE_VEC34)
		case USP_OPCODE_VMAD:

		case USP_OPCODE_VMUL:
		case USP_OPCODE_VADD:
		case USP_OPCODE_VFRC:
		case USP_OPCODE_VDSX:
		case USP_OPCODE_VDSY:
		case USP_OPCODE_VMIN:
		case USP_OPCODE_VMAX:
		case USP_OPCODE_VDP:

		case USP_OPCODE_VDUAL:
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
		{
			return IMG_FALSE;
		}

		default:
		{
			break;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 name:		HWInstSupportsWriteMask

 purpose:	Test whether a HW instruction supports partial writes to the
			destination register (i.e. where the entire contents of the
			register is not replaced).

 inputs:	eOpcode	- The opcode of the HW instruction

 outputs:	none

 returns:	IMG_TRUE or IMG_FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstSupportsWriteMask(USP_OPCODE eOpcode)
{
	switch (eOpcode)
	{
		case USP_OPCODE_PCKUNPCK:
		case USP_OPCODE_SOPWM:
		{
			return IMG_TRUE;
		}

		default:
		{
			break;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 name:		HWInstDecodePCKUNPCKInstWriteMask

 purpose:	Decode the write-mask for a PCKUNPCK instruction

 inputs:	psHWInst - The instruction to be modified

 outputs:	none

 returns:	The decoded write-mask
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstDecodePCKUNPCKInstWriteMask(PHW_INST psHWInst)
{
	return (psHWInst->uWord1 & ~EURASIA_USE1_PCK_DMASK_CLRMSK) >> 
		    EURASIA_USE1_PCK_DMASK_SHIFT;
}

/*****************************************************************************
 name:		HWInstDecodeSOPWMInstWriteMask

 purpose:	Decode the write-mask for a SOPWM instruction

			NB: For SOPWM, the mask-bits are assumed to represent the
				colour channels ARGB, with bit 3 corresponding to the alpha
				channel, and bit 0 for the blue channel.

 inputs:	psHWInst - The instruction to be modified

 outputs:	none

 returns:	The decoded write-mask
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstDecodeSOPWMInstWriteMask(PHW_INST psHWInst)
{
	IMG_UINT32	uHWWriteMask;

	uHWWriteMask = (psHWInst->uWord1 & ~EURASIA_USE1_SOP2WM_WRITEMASK_CLRMSK) >>
				   EURASIA_USE1_SOP2WM_WRITEMASK_SHIFT;

	return ((uHWWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_W) ? 0x8 : 0) |
		   ((uHWWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_Z) ? 0x4 : 0) |
		   ((uHWWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_Y) ? 0x2 : 0) |
		   ((uHWWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_X) ? 0x1 : 0);
}

/*****************************************************************************
 name:		HWInstDecodeWriteMask

 purpose:	Decode the write-mask for an instruction that supports partial-
			writes to registers

			NB: For SOPWM, the mask-bits are assumed to represent the
				colour channels ARGB, with bit 3 corresponding to the alpha
				channel, and bit 0 for the blue channel.

 inputs:	eOpcode		- The opcode of the HW instruction to update
			psHWInst	- The instruction to be modified

 outputs:	puWriteMask	- The decode register write-mask

 returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstDecodeWriteMask(USP_OPCODE	eOpcode,
											PHW_INST	psHWInst,
											IMG_PUINT32	puWriteMask)
{
	switch (eOpcode)
	{
		case USP_OPCODE_PCKUNPCK:
		{
			*puWriteMask = HWInstDecodePCKUNPCKInstWriteMask(psHWInst);
			break;
		}

		case USP_OPCODE_SOPWM:
		{
			*puWriteMask = HWInstDecodeSOPWMInstWriteMask(psHWInst);
			break;
		}

		default:
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 name:		HWInstEncodePCKUNPCKInstWriteMask

 purpose:	Encode the write-mask for a PCKUNPCK instruction

 inputs:	psHWInst	- The instruction to be modified
			uWriteMask	- The register write-mask to encode
			bForceMask	- Forces the specified mask to be set. No validation
						  performed.

 outputs:	none

 returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodePCKUNPCKInstWriteMask(PHW_INST	psHWInst,
														IMG_UINT32	uWriteMask,
														IMG_BOOL	bForceMask)
{
	#if !defined(SGX_FEATURE_USE_VEC34)
	if	(uWriteMask > 0xF)
	{
		return IMG_FALSE;
	}
	#endif /* !defined(SGX_FEATURE_USE_VEC34) */

	if	(!bForceMask)
	{
		IMG_UINT32	uHWDstPckFmt;
		IMG_UINT32	uHWSrcPckFmt;

		uHWDstPckFmt = (psHWInst->uWord1 & ~EURASIA_USE1_PCK_DSTF_CLRMSK) >>
						EURASIA_USE1_PCK_DSTF_SHIFT;
		uHWSrcPckFmt = (psHWInst->uWord1 & ~EURASIA_USE1_PCK_SRCF_CLRMSK) >>
						EURASIA_USE1_PCK_SRCF_SHIFT;

		switch (uHWDstPckFmt)
		{
			case EURASIA_USE1_PCK_FMT_U8:
			case EURASIA_USE1_PCK_FMT_S8:
			case EURASIA_USE1_PCK_FMT_O8:
			{
				#if !defined(SGX_FEATURE_USE_VEC34)
				/*
					Can't have 3 bit's set in mask - anything else is OK
				*/
				if	(
						(uWriteMask == 0x7) ||
						(uWriteMask == 0xB) ||
						(uWriteMask == 0xD) ||
						(uWriteMask == 0xE)
					)
				{
					return IMG_FALSE;
				}
				#endif /* !defined(SGX_FEATURE_USE_VEC34) */
				break;
			}
			case EURASIA_USE1_PCK_FMT_U16:
			case EURASIA_USE1_PCK_FMT_S16:
			case EURASIA_USE1_PCK_FMT_F16:
			{
				#if defined(SGX_FEATURE_USE_VEC34)
				if(uWriteMask > 0xF)
				{
					return IMG_FALSE;
				}
				if (uHWDstPckFmt == EURASIA_USE1_PCK_FMT_F16 && !HWInstHasVectorResultPCKUNPCKInst(psHWInst))
				{
					if ((uWriteMask & 0xC) != 0)
					{
						return IMG_FALSE;
					}
				}
				#else /* defined(SGX_FEATURE_USE_VEC34) */
				/*
					Must have first and/or last 2 bits set
				*/
				if	(
						(uWriteMask != 0x3) &&
						(uWriteMask != 0xC) &&
						(uWriteMask != 0xF)
					)
				{
					return IMG_FALSE;
				}
				#endif /* defined(SGX_FEATURE_USE_VEC34) */ 
				break;
			}

			case EURASIA_USE1_PCK_FMT_F32:
			{
				#if !defined(SGX_FEATURE_USE_VEC34)
				/*
					Must have all bits set
				*/
				if	(uWriteMask != 0xF)
				{
					return IMG_FALSE;
				}
				#else /* !defined(SGX_FEATURE_USE_VEC34) */
				if (!HWInstHasVectorResultPCKUNPCKInst(psHWInst))
				{
					if ((uWriteMask & 0xE) != 0)
					{
						return IMG_FALSE;
					}
				}
				#endif /* !defined(SGX_FEATURE_USE_VEC34) */

				break;
			}

			case EURASIA_USE1_PCK_FMT_C10:
			{
				#if defined(SGX_FEATURE_USE_VEC34)
				if (uHWSrcPckFmt == EURASIA_USE1_PCK_FMT_C10)
				{
					if	(
							(uWriteMask == 0x7) ||
							(uWriteMask == 0xB) ||
							(uWriteMask == 0xD) ||
							(uWriteMask == 0xE)
						)
					{
						return IMG_FALSE;
					}
				}
				#else
				IMG_UINT32	uHWDestBankExt;
				IMG_UINT32	uHWDestBank;

				/*
					If packing to an IReg, then same rules an 8-bit data apply
					(can't have 3 bits set). If packing to unifiex reg, then
					only 1 or 2 bits can be set from the bottom 3 only.
				*/
				uHWDestBankExt	= psHWInst->uWord1 & EURASIA_USE1_DBEXT;
				uHWDestBank		= (psHWInst->uWord1 & ~EURASIA_USE1_D1BANK_CLRMSK) >>
								   EURASIA_USE1_D1BANK_SHIFT;

				if	(
						(uHWDestBankExt == EURASIA_USE1_DBEXT) &&
						#if defined(SGX_FEATURE_USE_VEC34)
						(uHWDestBank == SGXVEC_USE1_D1EXTBANK_INDEXED_IH)
						#else
						(uHWDestBank == EURASIA_USE1_D1EXTBANK_FPINTERNAL)
						#endif /* defined(SGX_FEATURE_USE_VEC34) */
					)
				{
					if	(
							(uWriteMask == 0x7) ||
							(uWriteMask == 0xB) ||
							(uWriteMask == 0xD) ||
							(uWriteMask == 0xE)
						)
					{
						return IMG_FALSE;
					}
				}
				else
				{
					if	(uWriteMask > 0x6)
					{
						return IMG_FALSE;
					}
				}
				#endif
				break;
			}

			default:
			{
				return IMG_FALSE;
			}
		}
	}

	psHWInst->uWord1 &= EURASIA_USE1_PCK_DMASK_CLRMSK;
	psHWInst->uWord1 |=	(uWriteMask << EURASIA_USE1_PCK_DMASK_SHIFT) & 
						(~EURASIA_USE1_PCK_DMASK_CLRMSK);

	return IMG_TRUE;
}

/*****************************************************************************
 name:		HWInstEncodeSOPWMInstWriteMask
    
 purpose:	Encode the write-mask for a SOPWM instruction

			NB: For SOPWM, the mask-bits are assumed to represent the
				colour channels ARGB, with bit 3 corresponding to the alpha
				channel, and bit 0 for the blue channel.

 inputs:	psHWInst	- The instruction to be modified
			uWriteMask	- The register write-mask to encode

 outputs:	none
  
 returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeSOPWMInstWriteMask(PHW_INST	psHWInst,
													 IMG_UINT32	uWriteMask)
{
	IMG_UINT32	uHWWriteMask;

	if	(uWriteMask > 0xF)
	{
		return IMG_FALSE;
	}

	uHWWriteMask = ((uWriteMask & 0x8) ? EURASIA_USE1_SOP2WM_WRITEMASK_W : 0) |
				   ((uWriteMask & 0x4) ? EURASIA_USE1_SOP2WM_WRITEMASK_Z : 0) |
				   ((uWriteMask & 0x2) ? EURASIA_USE1_SOP2WM_WRITEMASK_Y : 0) |
				   ((uWriteMask & 0x1) ? EURASIA_USE1_SOP2WM_WRITEMASK_X : 0);

	psHWInst->uWord1 &= EURASIA_USE1_SOP2WM_WRITEMASK_CLRMSK;
	psHWInst->uWord1 |=	(uHWWriteMask << EURASIA_USE1_SOP2WM_WRITEMASK_SHIFT) & 
						(~EURASIA_USE1_SOP2WM_WRITEMASK_CLRMSK);

	return IMG_TRUE;
}

/*****************************************************************************
 name:		HWInstEncodeWriteMask

 purpose:	Encode the write-mask for an instruction that supports partial-
			writes to registers

			NB: For SOPWM, the mask-bits are assumed to represent the
				colour channels ARGB, with bit 3 corresponding to the alpha
				channel, and bit 0 for the blue channel.

 inputs:	eOpcode		- The opcode of the HW instruction to update
			psHWInst	- The instruction to be modified
			uWriteMask	- The register write-mask to encode

 outputs:	none

 returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeWriteMask(USP_OPCODE	eOpcode,
											PHW_INST	psHWInst,
											IMG_UINT32	uWriteMask)
{
	switch (eOpcode)
	{
		case USP_OPCODE_PCKUNPCK:
		{
			return HWInstEncodePCKUNPCKInstWriteMask(psHWInst,
													 uWriteMask,
													 IMG_FALSE);
		}

		case USP_OPCODE_SOPWM:
		{
			return HWInstEncodeSOPWMInstWriteMask(psHWInst, uWriteMask);
		}

		default:
		{
			break;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 Name:		HWInstForceFullWriteMask

 Purpose:	Force an instruction that performs a partial-write to completely
			initialise the destination (i.e. use a full write-mask)

 Inputs:	eOpcode			- The decoded opcode of the HW instruction to change
			psHWInst		- The HW instruction to modify

 Outputs:	pbHasFullMask	- Set to IMG_TRUE if the instruction has a full
							  mask. If IMG_FALSE, then a full-mask could
							  not be set without changing the result.

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstForceFullWriteMask(USP_OPCODE	eOpcode,
											   PHW_INST		psHWInst,
											   IMG_PBOOL	pbHasFullMask)
{
	IMG_UINT32	uWriteMask;

	/*
		Modify the instruction so that all channels of the destination
		are written
	*/
	switch (eOpcode)
	{
		case USP_OPCODE_PCKUNPCK:
		{
			IMG_UINT32	uHWDestPackFmt;
			IMG_BOOL	bSwapSources;

			/*
				Do nothing if the instruction already writes all destination
				channels
			*/
			uWriteMask = (psHWInst->uWord1 & ~EURASIA_USE1_PCK_DMASK_CLRMSK) >> 
						  EURASIA_USE1_PCK_DMASK_SHIFT;

			if	(uWriteMask == 0xF)
			{
				break;
			}

			/*
				See if we can safely change the instruction to use a full
				write-mask
			*/
			bSwapSources = IMG_FALSE;

			uHWDestPackFmt = (psHWInst->uWord1 & ~EURASIA_USE1_PCK_DSTF_CLRMSK) >> 
							 EURASIA_USE1_PCK_DSTF_SHIFT;

			switch (uHWDestPackFmt)
			{
				case EURASIA_USE1_PCK_FMT_U8:
				case EURASIA_USE1_PCK_FMT_S8:
				case EURASIA_USE1_PCK_FMT_O8:
				case EURASIA_USE1_PCK_FMT_C10:
				{
					/*
						For 8-bit writes, the instruction will alternately use
						sources 1 and 2 for each destination channel being
						written (low to high), starting with source 1. So, if
						we switch to a full write-mask, destination channel 0
						will then be derived from source 1, channel 1 from
						source 2, channel 3 from source 1 again, and finally
						channel 3 from source 2.

						Thus, To ensure that the result of the instruction
						remains unchanged, we can sometimes swap source 1 and
						2. However, due to the fixed relationship between
						sources and destination channels with a full write-
						mask, there are cases that cannot be supported.
					*/
					switch (uWriteMask)
					{
						case 0x1:
						case 0x4:
						case 0x3:
						case 0x9:
						case 0xC:
						{
							/*
								Source 1 currently used for dest channel
								0 or 2, and source 2 currently used for
								dest channel 1 or 3. Same is true with a
								full write-mask.
							*/
							break;
						}

						case 0x2:
						case 0x6:
						case 0x8:
						{
							/*
								Source 1 currently used for dest channel 1 or
								3, and source 2 currently used for dest channel
								2. Opposite will be true with a full write-
								mask, so swap the sources.
							*/
							bSwapSources = IMG_TRUE;
							break;
						}

						case 0x5:
						case 0xA:
						{
							IMG_BOOL	bHWSrc1BankExt;
							IMG_UINT32	uHWSrc1Bank;
							IMG_UINT32	uHWSrc1Num;
							IMG_UINT32	uHWSrc1Chan;
							IMG_BOOL	bHWSrc2BankExt;
							IMG_UINT32	uHWSrc2Bank;
							IMG_UINT32	uHWSrc2Num;
							IMG_UINT32	uHWSrc2Chan;

							/*
								Sources 1 and 2 are currently used for the two
								even or odd numbered destination channels. The
								same result cannot be achieved with a full write-
								mask unless the two sources are the same.
							*/
							bHWSrc1BankExt	= (IMG_BOOL)((psHWInst->uWord1 & EURASIA_USE1_S1BEXT) != 0);
							uHWSrc1Bank		= (psHWInst->uWord0 & ~EURASIA_USE0_S1BANK_CLRMSK) >>
											  EURASIA_USE0_S1BANK_SHIFT;
							uHWSrc1Num		= (psHWInst->uWord0 & ~EURASIA_USE0_SRC1_CLRMSK) >>
											  EURASIA_USE0_SRC1_SHIFT;
							uHWSrc1Chan		= (psHWInst->uWord0 & ~EURASIA_USE0_PCK_S1SCSEL_CLRMSK) >>
											  EURASIA_USE0_PCK_S1SCSEL_SHIFT;

							bHWSrc2BankExt	= (IMG_BOOL)((psHWInst->uWord1 & EURASIA_USE1_S2BEXT) != 0);
							uHWSrc2Bank		= (psHWInst->uWord0 & ~EURASIA_USE0_S2BANK_CLRMSK) >>
											  EURASIA_USE0_S2BANK_SHIFT;
							uHWSrc2Num		= (psHWInst->uWord0 & ~EURASIA_USE0_SRC2_CLRMSK) >>
											  EURASIA_USE0_SRC2_SHIFT;
							uHWSrc2Chan		= (psHWInst->uWord0 & ~EURASIA_USE0_PCK_S2SCSEL_CLRMSK) >>
											  EURASIA_USE0_PCK_S2SCSEL_SHIFT;

							if	(
									(bHWSrc1BankExt != bHWSrc2BankExt) ||
									(uHWSrc1Bank	!= uHWSrc2Bank) ||
									(uHWSrc1Num		!= uHWSrc2Num) ||
									(uHWSrc1Chan	!= uHWSrc2Chan)
								)
							{
								*pbHasFullMask = IMG_FALSE;
								return IMG_TRUE;
							}

							break;
						}

						case 0x7:
						case 0xB:
						case 0xE:
						case 0xD:
						{
							/*
								Invalid write-masks (3 bits set).
							*/
							return IMG_FALSE;
						}

						default:
						{
							break;
						}
					}

					break;
				}

				case EURASIA_USE1_PCK_FMT_U16:
				case EURASIA_USE1_PCK_FMT_S16:
				case EURASIA_USE1_PCK_FMT_F16:
				{
					/*
						For 16-bit writes, if only the upper half of the
						register is being written, the instruction will use
						source 1 only for that half. With a full write-
						mask, source 1 is used for the lower half, and
						source 2 for the upper, so we must swap them.
					*/
					if	(uWriteMask == 0xC)
					{
						bSwapSources = IMG_TRUE;
					}

					break;
				}

				case EURASIA_USE1_PCK_FMT_F32:
				{
					/*
						Write-mask can only be 0xF or 0x0 for this instruction, and
						we should never see any with 0x0 anyway.
					*/
					break;
				}

				default:
				{
					return IMG_FALSE;
				}
			}

			/*
				Swap sources 1 and 2 if needed	
			*/
			if	(bSwapSources)
			{
				IMG_BOOL	bHWSrc1BankExt;
				IMG_UINT32	uHWSrc1Bank;
				IMG_UINT32	uHWSrc1Num;
				IMG_UINT32	uHWSrc1Chan;
				IMG_BOOL	bHWSrc2BankExt;
				IMG_UINT32	uHWSrc2Bank;
				IMG_UINT32	uHWSrc2Num;
				IMG_UINT32	uHWSrc2Chan;

				/*
					Extract all fields used to specify HW sources 1 and 2
				*/
				bHWSrc1BankExt	= (IMG_BOOL)((psHWInst->uWord1 & EURASIA_USE1_S1BEXT) != 0);
				uHWSrc1Bank		= (psHWInst->uWord0 & ~EURASIA_USE0_S1BANK_CLRMSK) >>
								  EURASIA_USE0_S1BANK_SHIFT;
				uHWSrc1Num		= (psHWInst->uWord0 & ~EURASIA_USE0_SRC1_CLRMSK) >>
								  EURASIA_USE0_SRC1_SHIFT;
				uHWSrc1Chan		= (psHWInst->uWord0 & ~EURASIA_USE0_PCK_S1SCSEL_CLRMSK) >>
								  EURASIA_USE0_PCK_S1SCSEL_SHIFT;
					 
				bHWSrc2BankExt	= (IMG_BOOL)((psHWInst->uWord1 & EURASIA_USE1_S2BEXT) != 0);
				uHWSrc2Bank		= (psHWInst->uWord0 & ~EURASIA_USE0_S2BANK_CLRMSK) >>
								  EURASIA_USE0_S2BANK_SHIFT;
				uHWSrc2Num		= (psHWInst->uWord0 & ~EURASIA_USE0_SRC2_CLRMSK) >>
								  EURASIA_USE0_SRC2_SHIFT;
				uHWSrc2Chan		= (psHWInst->uWord0 & ~EURASIA_USE0_PCK_S2SCSEL_CLRMSK) >>
								  EURASIA_USE0_PCK_S2SCSEL_SHIFT;

				/*
					HW source 1 and 2 have the same encoding, so we can
					simply swap all the fields for the two sources
				*/
				psHWInst->uWord0 &= EURASIA_USE0_S1BANK_CLRMSK &
									EURASIA_USE0_SRC1_CLRMSK &
									EURASIA_USE0_PCK_S1SCSEL_CLRMSK &
									EURASIA_USE0_S2BANK_CLRMSK &
									EURASIA_USE0_SRC2_CLRMSK &
									EURASIA_USE0_PCK_S2SCSEL_CLRMSK;
									
				psHWInst->uWord1 &= (~EURASIA_USE1_S1BEXT) &
									(~EURASIA_USE1_S2BEXT);

				if	(bHWSrc2BankExt)
				{
					psHWInst->uWord1 |= EURASIA_USE1_S1BEXT;
				}
				psHWInst->uWord0 |= (uHWSrc2Bank << EURASIA_USE0_S1BANK_SHIFT) &
									(~EURASIA_USE0_S1BANK_CLRMSK);
				psHWInst->uWord0 |= (uHWSrc2Num << EURASIA_USE0_SRC1_SHIFT) &
									(~EURASIA_USE0_SRC1_CLRMSK);
				psHWInst->uWord0 |= (uHWSrc2Chan << EURASIA_USE0_PCK_S1SCSEL_SHIFT) &
									(~EURASIA_USE0_PCK_S1SCSEL_CLRMSK);

				if	(bHWSrc1BankExt)
				{
					psHWInst->uWord1 |= EURASIA_USE1_S2BEXT;
				}
				psHWInst->uWord0 |= (uHWSrc1Bank << EURASIA_USE0_S2BANK_SHIFT) &
									(~EURASIA_USE0_S2BANK_CLRMSK);
				psHWInst->uWord0 |= (uHWSrc1Num << EURASIA_USE0_SRC2_SHIFT) &
									(~EURASIA_USE0_SRC2_CLRMSK);
				psHWInst->uWord0 |= (uHWSrc1Chan << EURASIA_USE0_PCK_S2SCSEL_SHIFT) &
									(~EURASIA_USE0_PCK_S2SCSEL_CLRMSK);
			}

			/*
				Set a full write-mask
			*/
			psHWInst->uWord1 |=	~EURASIA_USE1_PCK_DMASK_CLRMSK;
			break;
		}

		case USP_OPCODE_SOPWM:
		{
			/*
				Safe to simply set a full write-mask for SOPWM
			*/
			psHWInst->uWord1 |=	~EURASIA_USE1_SOP2WM_WRITEMASK_CLRMSK;
			break;
		}

		default:
		{
			/*
				No other instructions support partial writes
			*/
			return IMG_FALSE;
		}
	}

	/*
		Instruction has a full-mask now
	*/
	*pbHasFullMask = IMG_TRUE;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstDecodeSMPInstDRCNum

 Purpose:	Decode the DRC number used by a SMP instruction

 Inputs:	psHWInst	- The HW instruction to decode

 Outputs:	none

 Returns:	The decoded DRC number
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstDecodeSMPInstDRCNum(PHW_INST psHWInst)
{
	return (psHWInst->uWord1 & (~EURASIA_USE1_SMP_DRCSEL_CLRMSK)) >>
		   EURASIA_USE1_SMP_DRCSEL_SHIFT;
}

/*****************************************************************************
 Name:		HWInstEncodeWDFInst

 Purpose:	Encode a WDF instruction

 Inputs:	psHWInst	- The HW instruction to modify
			uDRCNum		- The DRC that the WDF should wait on

 Outputs:	none

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeWDFInst(PHW_INST	psHWInst,
											IMG_UINT32	uDRCNum)
{
	/*
		Encode a base WDF instruction - no instruction flags supported by WDF
	*/
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
					   (EURASIA_USE1_OTHER_OP2_WDF << EURASIA_USE1_OTHER_OP2_SHIFT);

	/*
		Encode the DRC to use
	*/
	if	(uDRCNum > EURASIA_USE_DRC_BANK_SIZE)
	{
		return 0;
	}
	psHWInst->uWord1 |= uDRCNum << EURASIA_USE1_IDFWDF_DRCSEL_SHIFT;

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeSOPWMInst

 Purpose:	Encode a SOPWM instruction

 Inputs:	psMOEState	- The MOE state prior to the new instruction
			bSkipInv	- Whether the SkipInv flag should be set
			uHWCop		- The encoded SOPWM colour-operantion
			uHWAop		- The encoded SOPWM alpha-operation
			uHWMod1		- The encoded SOPWM source 1 modifier
			uHWSel1		- The encoded SOPWM source 1 selection
			uHWMod2		- The encoded SOPWM source 2 modifier
			uHWSel2		- The encoded SOPWM source 2 selection
			uWriteMask	- Byte write-mask for the destination
			psDestReg	- The register to use for the dest operand
			psSrc1Reg	- The register to use for the src1 operand
			psSrc2Reg	- The register to use for the src2 operand

 Outputs:	psHWInst	- The HW instruction to modify

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSOPWMInst(PUSP_MOESTATE	psMOEState,
											  PHW_INST		psHWInst,
											  IMG_BOOL		bSkipInv,
											  IMG_UINT32	uHWCop,
											  IMG_UINT32	uHWAop,
											  IMG_UINT32	uHWMod1,
											  IMG_UINT32	uHWSel1,
											  IMG_UINT32	uHWMod2,
											  IMG_UINT32	uHWSel2,
							  				  IMG_UINT32	uWriteMask,
							  				  PUSP_REG		psDestReg,
											  PUSP_REG		psSrc1Reg,
											  PUSP_REG		psSrc2Reg)
{
	USP_FMTCTL	eFmtCtl;

	/*
		Encode a base, unconditional, unrepeated SOPWM instruction
	*/
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = (EURASIA_USE1_OP_SOPWM << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_SPRED_ALWAYS << EURASIA_USE1_SPRED_SHIFT);

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	/*
		Copy the pre-encoded SOPWM options into the instruction
	*/
	psHWInst->uWord1 |= ((uHWMod1 << EURASIA_USE1_SOP2WM_MOD1_SHIFT) &
						 (~EURASIA_USE1_SOP2WM_MOD1_CLRMSK)) |
						((uHWCop << EURASIA_USE1_SOP2WM_COP_SHIFT) & 
						 (~EURASIA_USE1_SOP2WM_COP_CLRMSK)) |
						((uHWMod2 << EURASIA_USE1_SOP2WM_MOD2_SHIFT) &
						 (~EURASIA_USE1_SOP2WM_MOD2_CLRMSK)) |
						((uHWAop << EURASIA_USE1_SOP2WM_AOP_SHIFT) &
						 (~EURASIA_USE1_SOP2WM_AOP_CLRMSK)) |
						((uHWSel1 << EURASIA_USE1_SOP2WM_SEL1_SHIFT) &
						 (~EURASIA_USE1_SOP2WM_SEL1_CLRMSK)) |
						((uHWSel2 << EURASIA_USE1_SOP2WM_SEL2_SHIFT) &
						 (~EURASIA_USE1_SOP2WM_SEL2_CLRMSK));

	/*
		Encode the dest byte mask
	*/
	if	(!HWInstEncodeSOPWMInstWriteMask(psHWInst, uWriteMask))
	{
		return 0;
	}

	/*
		Get the per-operand format control available
	*/
	if	(!HWInstGetPerOperandFmtCtl(psMOEState,
									USP_OPCODE_SOPWM, 
									psHWInst,
									&eFmtCtl))
	{
		return 0;
	}

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(eFmtCtl,
									 USP_OPCODE_SOPWM,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(eFmtCtl, USP_OPCODE_SOPWM, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	/*
		Encode the src2 register bank and number
	*/
	if	(!HWInstEncodeSrc2BankAndNum(eFmtCtl, USP_OPCODE_SOPWM, psHWInst, psSrc2Reg))
	{
		return 0;
	}

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeSOP2Inst

 Purpose:	Encode a SOP2 instruction

 Inputs:	psMOEState	- The MOE state prior to the new instruction
			bSkipInv	- Whether the SkipInv flag should be set
			uHWCop		- The encoded SOPWM colour-operantion
			uHWAop		- The encoded SOPWM alpha-operation
			uHWMod1		- The encoded SOPWM source 1 modifier
			uHWSel1		- The encoded SOPWM source 1 selection
			uHWMod2		- The encoded SOPWM source 2 modifier
			uHWSel2		- The encoded SOPWM source 2 selection
			uWriteMask	- Byte write-mask for the destination
			psDestReg	- The register to use for the dest operand
			psSrc1Reg	- The register to use for the src1 operand
			psSrc2Reg	- The register to use for the src2 operand

 Outputs:	psHWInst	- The HW instruction to modify

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeSOP2Inst(PUSP_MOESTATE	psMOEState,
											 PHW_INST		psHWInst,
											 IMG_BOOL		bSkipInv,
											 IMG_UINT32		uHWCOp,
											 IMG_UINT32		uHWCMod1,
											 IMG_UINT32		uHWCSel1,
											 IMG_UINT32		uHWCMod2,
											 IMG_UINT32		uHWCSel2,
											 IMG_UINT32		uHWAOp,
											 IMG_UINT32		uHWAMod1,
											 IMG_UINT32		uHWASel1,
											 IMG_UINT32		uHWAMod2,
											 IMG_UINT32		uHWASel2,
							  				 PUSP_REG		psDestReg,
											 PUSP_REG		psSrc1Reg,
											 PUSP_REG		psSrc2Reg)
{
	USP_FMTCTL	eFmtCtl;

	/*
		Encode a base, unconditional, unrepeated SOP2 instruction, with no source
		or dest modifiers.
	*/
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = (EURASIA_USE1_OP_SOP2 << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_SPRED_ALWAYS << EURASIA_USE1_SPRED_SHIFT);

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	/*
		Copy the pre-encoded SOP2 options into the instruction
	*/
	psHWInst->uWord0 |= ((uHWCOp << EURASIA_USE0_SOP2_COP_SHIFT) & 
						 (~EURASIA_USE0_SOP2_COP_CLRMSK)) |
						((uHWAOp << EURASIA_USE0_SOP2_AOP_SHIFT) & 
						 (~EURASIA_USE0_SOP2_AOP_CLRMSK));

	psHWInst->uWord1 |= ((uHWCMod1 << EURASIA_USE1_SOP2_CMOD1_SHIFT) & 
						 (~EURASIA_USE1_SOP2_CMOD1_CLRMSK)) |
						((uHWCSel1 << EURASIA_USE1_SOP2_CSEL1_SHIFT) & 
						 (~EURASIA_USE1_SOP2_CSEL1_CLRMSK)) |
						((uHWCMod2 << EURASIA_USE1_SOP2_CMOD2_SHIFT) & 
						 (~EURASIA_USE1_SOP2_CMOD2_CLRMSK)) |
						((uHWCSel2 << EURASIA_USE1_SOP2_CSEL2_SHIFT) & 
						 (~EURASIA_USE1_SOP2_CSEL2_CLRMSK)) |
						((uHWAMod1 << EURASIA_USE1_SOP2_AMOD1_SHIFT) & 
						 (~EURASIA_USE1_SOP2_AMOD1_CLRMSK)) |
						((uHWASel1 << EURASIA_USE1_SOP2_ASEL1_SHIFT) & 
						 (~EURASIA_USE1_SOP2_ASEL1_CLRMSK)) |
						((uHWAMod2 << EURASIA_USE1_SOP2_AMOD2_SHIFT) & 
						 (~EURASIA_USE1_SOP2_AMOD2_CLRMSK)) |
						((uHWASel2 << EURASIA_USE1_SOP2_ASEL2_SHIFT) & 
						 (~EURASIA_USE1_SOP2_ASEL2_CLRMSK));

	/*
		Get the per-operand format control available
	*/
	if	(!HWInstGetPerOperandFmtCtl(psMOEState,
									USP_OPCODE_SOPWM, 
									psHWInst,
									&eFmtCtl))
	{
		return 0;
	}

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(eFmtCtl,
									 USP_OPCODE_SOPWM,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(eFmtCtl, USP_OPCODE_SOPWM, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	/*
		Encode the src2 register bank and number
	*/
	if	(!HWInstEncodeSrc2BankAndNum(eFmtCtl, USP_OPCODE_SOPWM, psHWInst, psSrc2Reg))
	{
		return 0;
	}

	return 1;
}

/*****************************************************************************
 Name:		HWInstDecodeSMPInstCoordFmt

 Purpose:	Decode the data-format for the texture-coordinates used by a
			SMP instruction

 Inputs:	psHWInst	- The HW instruction to decode

 Outputs:	none

 Returns:	The decoded register format (USP_REFFMT_UNKNOWN on error)
*****************************************************************************/
IMG_INTERNAL USP_REGFMT HWInstDecodeSMPInstCoordFmt(PHW_INST psHWInst)
#if !defined(SGX_FEATURE_EXTENDED_USE_ALU)
{
	IMG_UINT32	uHWCoordFmt;

	/*
		F16 and C10 coordinate formats only available with an extended ALU
	*/
	uHWCoordFmt = (psHWInst->uWord1 & (~EURASIA_USE1_SMP_CTYPE_CLRMSK)) >>
				  EURASIA_USE1_SMP_CTYPE_SHIFT;

	switch (uHWCoordFmt)
	{
		case EURASIA_USE1_SMP_CTYPE_F32:
		{
			return USP_REGFMT_F32;
		}
		case EURASIA_USE1_SMP_CTYPE_F16:
		{
			return USP_REGFMT_F16;
		}
		case EURASIA_USE1_SMP_CTYPE_C10:
		{
			return USP_REGFMT_C10;
		}
		default:
		{
			break;
		}
	}

	return USP_REGFMT_UNKNOWN;
}
#else
{
	PVR_UNREFERENCED_PARAMETER(psHWInst);

	return USP_REGFMT_F32;
}
#endif	/* #if !defined(SGX_FEATURE_EXTENDED_USE_ALU) */

/*****************************************************************************
 Name:		HWInstDecodeSMPInstCoordDim

 Purpose:	Decode the dimensionality of the texture-coordinates used by a
			SMP instruction

 Inputs:	psHWInst	- The HW instruction to decode

 Outputs:	none

 Returns:	The decoded coordinate dimensionality (0 on error)
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstDecodeSMPInstCoordDim(PHW_INST psHWInst)
{
	IMG_UINT32	uHWCoordDim;

	uHWCoordDim = (psHWInst->uWord1 & (~EURASIA_USE1_SMP_CDIM_CLRMSK)) >>
				  EURASIA_USE1_SMP_CDIM_SHIFT;

	switch (uHWCoordDim)
	{
		case EURASIA_USE1_SMP_CDIM_U:
		{
			return 1;
		}
		case EURASIA_USE1_SMP_CDIM_UV:
		{
			return 2;
		}
		case EURASIA_USE1_SMP_CDIM_UVS:
		{
			return 3;
		}
		default:
		{
			break;
		}
	}

	return 0;
}

/*****************************************************************************
 Name:		HWInstDecodePCKUNPCKInstSrcFormat

 Purpose:	Decode the source format for a pack/unpack HW instruction

 Inputs:	psHWInst	- The HW instruction to decode

 Outputs:	none

 Returns:	The decoded source data-format
*****************************************************************************/
IMG_INTERNAL USP_PCKUNPCK_FMT HWInstDecodePCKUNPCKInstSrcFormat(PHW_INST psHWInst)
{
	IMG_UINT32	uHWSrcPckFmt;

	uHWSrcPckFmt = (psHWInst->uWord1 & (~EURASIA_USE1_PCK_SRCF_CLRMSK)) >> 
				   EURASIA_USE1_PCK_SRCF_SHIFT;

	switch (uHWSrcPckFmt)
	{
		case EURASIA_USE1_PCK_FMT_U8:
		{
			return USP_PCKUNPCK_FMT_U8;
		}
		case EURASIA_USE1_PCK_FMT_S8:
		{
			return USP_PCKUNPCK_FMT_S8;
		}
		case EURASIA_USE1_PCK_FMT_O8:
		{
			return USP_PCKUNPCK_FMT_O8;
		}
		case EURASIA_USE1_PCK_FMT_U16:
		{
			return USP_PCKUNPCK_FMT_U16;
		}
		case EURASIA_USE1_PCK_FMT_S16:
		{
			return USP_PCKUNPCK_FMT_S16;
		}
		case EURASIA_USE1_PCK_FMT_F16:
		{
			return USP_PCKUNPCK_FMT_F16;
		}
		case EURASIA_USE1_PCK_FMT_F32:
		{
			return USP_PCKUNPCK_FMT_F32;
		}
		case EURASIA_USE1_PCK_FMT_C10:
		{
			return USP_PCKUNPCK_FMT_C10;
		}
		default:
		{
			break;
		}
	}

	return USP_PCKUNPCK_FMT_INVALID;
}

/*****************************************************************************
 Name:		HWInstDecodePCKUNPCKInstDstFormat

 Purpose:	Decode the destination format for a pack/unpack HW instruction

 Inputs:	psHWInst	- The HW instruction to decode

 Outputs:	none

 Returns:	The decoded destination data-format
*****************************************************************************/
IMG_INTERNAL USP_PCKUNPCK_FMT HWInstDecodePCKUNPCKInstDstFormat(PHW_INST psHWInst)
{
	IMG_UINT32	uHWDstPckFmt;

	uHWDstPckFmt = (psHWInst->uWord1 & (~EURASIA_USE1_PCK_DSTF_CLRMSK)) >> 
				   EURASIA_USE1_PCK_DSTF_SHIFT;

	switch (uHWDstPckFmt)
	{
		case EURASIA_USE1_PCK_FMT_U8:
		{
			return USP_PCKUNPCK_FMT_U8;
		}
		case EURASIA_USE1_PCK_FMT_S8:
		{
			return USP_PCKUNPCK_FMT_S8;
		}
		case EURASIA_USE1_PCK_FMT_O8:
		{
			return USP_PCKUNPCK_FMT_O8;
		}
		case EURASIA_USE1_PCK_FMT_U16:
		{
			return USP_PCKUNPCK_FMT_U16;
		}
		case EURASIA_USE1_PCK_FMT_S16:
		{
			return USP_PCKUNPCK_FMT_S16;
		}
		case EURASIA_USE1_PCK_FMT_F16:
		{
			return USP_PCKUNPCK_FMT_F16;
		}
		case EURASIA_USE1_PCK_FMT_F32:
		{
			return USP_PCKUNPCK_FMT_F32;
		}
		case EURASIA_USE1_PCK_FMT_C10:
		{
			return USP_PCKUNPCK_FMT_C10;
		}
		default:
		{
			break;
		}
	}

	return USP_PCKUNPCK_FMT_INVALID;
}

/*****************************************************************************
 Name:		HWInstEncodeLDInstFetchCount

 Purpose:	Encode the fetch/repeat count for a LD instruction

 Inputs:	psHWInst		- The HW instruction to modify
			uFetchCount		- The number of 32-bit words should be fetched

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeLDInstFetchCount(PHW_INST		psHWInst, 
												   IMG_UINT32	uFetchCount)
{
	#if defined(DEBUG)
	/*
		Validate the supplied repeat/fetch count
	*/
	if	((uFetchCount < 1) || (uFetchCount > HW_INST_LD_MAX_REPEAT_COUNT))
	{
		return IMG_FALSE;
	}
	#endif /* #if defined(DEBUG) */

	psHWInst->uWord1 &= EURASIA_USE1_RMSKCNT_CLRMSK;
	psHWInst->uWord1 |= ((uFetchCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) & 
						(~EURASIA_USE1_RMSKCNT_CLRMSK);

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		HWInstEncodeLDInst

 Purpose:	Encode a basic LD instruction (absolute addressing or 32-bit data)

 Inputs:	bSkipInv		- Whether the SkipInv flag should be set
			uFetchCount		- The number of 32-bit words should be fetched
			psDestReg		- The register to use for the dest operand
			psBaseAddrReg	- The register to use for the src1 operand
			psAddrOffReg	- The register to use for the src2 operand
			uDRCNum			- The DRC update when the LD has completed

 Outputs:	psHWInst		- The encoded HW instruction

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeLDInst(PHW_INST		psHWInst,
										   IMG_BOOL		bSkipInv,
										   IMG_UINT32	uFetchCount,
							  			   PUSP_REG		psDestReg,
										   PUSP_REG		psBaseAddrReg,
										   PUSP_REG		psAddrOffReg,
										   IMG_UINT32	uDRCNum)
{
	/*
		Encode a base LD instruction absolute-addressing, 32-bit data-size)
	*/
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = (EURASIA_USE1_OP_LD << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
					   (EURASIA_USE1_LDST_AMODE_ABSOLUTE << EURASIA_USE1_LDST_AMODE_SHIFT) | 
					   (EURASIA_USE1_LDST_DTYPE_32BIT << EURASIA_USE1_LDST_DTYPE_SHIFT);

	/*
		Encode the SkipInvalid flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	/*
		Encode the repeat count
	*/
	if	(!HWInstEncodeLDInstFetchCount(psHWInst, uFetchCount))
	{
		return 0;
	}

	/*
		Encode the DRC to use
	*/
	#if defined(DEBUG)
	if	(uDRCNum > EURASIA_USE_DRC_BANK_SIZE)
	{
		return 0;
	}
	#endif /* #if defined(DEBUG) */

	psHWInst->uWord1 |= uDRCNum << EURASIA_USE1_LDST_DRCSEL_SHIFT;

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_LD,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the base-address into source 0
	*/
	if	(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE, 
									 USP_OPCODE_LD,
									 IMG_TRUE,
									 psHWInst, 
									 psBaseAddrReg))
	{
		return 0;
	}

	/*
		Encode the offset into source 1
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_LD, psHWInst, psAddrOffReg))
	{
		return 0;
	}

	return 1;
}

/*****************************************************************************
 Name:		HWInstEncodeFIRHInst

 Purpose:	Encode an basic FIRH instruction (unpredicated, unrepeated)

 Inputs:	bSkipInv	- Whether the SkipInv flag should be set
			eSrcFormat	- The format of the YUV source data
			uCoeffSel	- The chosen set of filter coefficients
			iSOff		- The required centre offset
			uPOff		- The required packing offset
			psDestReg	- The register to use for the dest operand
			psSrc0Reg	- The register to use for the src1 operand
			psSrc1Reg	- The register to use for the src1 operand
			psSrc2Reg	- The register to use for the src2 operand

 Outputs:	psHWInst	- The encoded HW instruction

 Returns:	The number of HW instructions generated
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 HWInstEncodeFIRHInst(PHW_INST			psHWInst,
											 IMG_BOOL			bSkipInv,
											 USP_SRC_FORMAT		eSrcFormat,
											 IMG_UINT32			uCoeffSel,
											 IMG_INT32			iSOff,
											 IMG_UINT32			uPOff,
							  				 PUSP_REG			psDestReg,
											 PUSP_REG			psSrc0Reg,
											 PUSP_REG			psSrc1Reg,
											 PUSP_REG			psSrc2Reg)
{
	/*
		Encode a base LD instruction absolute-addressing, 32-bit data-size)
	*/
	psHWInst->uWord0 = 0;
	psHWInst->uWord1 = (EURASIA_USE1_OP_FIRH << EURASIA_USE1_OP_SHIFT) |
					   (EURASIA_USE1_SPRED_ALWAYS << EURASIA_USE1_SPRED_SHIFT) |
					   (EURASIA_USE1_FIRH_EDGEMODE_MIRRORSINGLE << EURASIA_USE1_FIRH_EDGEMODE_SHIFT);

	/*
		Encode the SkipInvalid flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	/*
		Encode the coefficient selection
	*/
	#if defined(DEBUG)
	if	(uCoeffSel > EURASIA_USE1_FIRH_COEFSEL_MAX)
	{
		return 0;	
	}
	#endif
	psHWInst->uWord1 |= uCoeffSel << EURASIA_USE1_FIRH_COEFSEL_SHIFT;

	/*
		Encode the source data format (only U8, S8 allowed)
	*/
	switch (eSrcFormat)
	{
		case USP_SRC_FORMAT_U8:
		{
			psHWInst->uWord1 |= EURASIA_USE1_FIRH_SRCFORMAT_U8 << EURASIA_USE1_FIRH_SRCFORMAT_SHIFT;
			break;
		}
		case USP_SRC_FORMAT_S8:
		{
			psHWInst->uWord1 |= EURASIA_USE1_FIRH_SRCFORMAT_S8 << EURASIA_USE1_FIRH_SRCFORMAT_SHIFT;
			break;
		}		
	}

	/*
		Encode the filter center offset
	*/
	#if defined(DEBUG)
	if ((iSOff < EURASIA_USE1_FIRH_SOFF_MIN) || (iSOff > EURASIA_USE1_FIRH_SOFF_MAX))
	{
		return 0;
	}
	#endif
	psHWInst->uWord1 |= (((iSOff >> 2) << EURASIA_USE1_FIRH_SOFFH_SHIFT) & ~EURASIA_USE1_FIRH_SOFFH_CLRMSK) | 
						(((iSOff >> 0) << EURASIA_USE1_FIRH_SOFFL_SHIFT) & ~EURASIA_USE1_FIRH_SOFFL_CLRMSK) |
						(((iSOff >> 4) << EURASIA_USE1_FIRH_SOFFS_SHIFT) & ~EURASIA_USE1_FIRH_SOFFS_CLRMSK);

	/* 
		Encode the packing register offset.
	*/
	#if	defined(DEBUG)
	if	(uPOff > EURASIA_USE1_FIRH_POFF_MAX)
	{
		return 0;
	}
	#endif
	psHWInst->uWord1 |= uPOff << EURASIA_USE1_FIRH_POFF_SHIFT;

	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_FIRH,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the three sources registers containing the UYV data

		NB: Src0 and Src1 must use the same bank, and can only be one of
			Temp, Output, PA or SA, and cannot be indexed.
	*/
	#if defined(DEBUG)
	if	(
			(psSrc0Reg->eType != psSrc1Reg->eType) ||
			(psSrc0Reg->eDynIdx != USP_DYNIDX_NONE) ||
			(psSrc1Reg->eDynIdx != USP_DYNIDX_NONE) ||
			(
				(psSrc0Reg->eType != USP_REGTYPE_TEMP) &&
				(psSrc0Reg->eType != USP_REGTYPE_PA) &&
				(psSrc0Reg->eType != USP_REGTYPE_SA) &&
				(psSrc0Reg->eType != USP_REGTYPE_OUTPUT)
			)
		)
	{
		return 0;
	}
	#endif

	if	(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE, 
									 USP_OPCODE_FIRH,
									 IMG_TRUE,
									 psHWInst, 
									 psSrc0Reg))
	{
		return 0;
	}

	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_FIRH, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_FIRH, psHWInst, psSrc2Reg))
	{
		return 0;
	}

	return 1;
}

#if defined(SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT)
/*****************************************************************************
 Name:		HWInstEncodeSMPInstUnPackingFormat

 Purpose:	Encodes the unpacking format for SMP instruction on SGX543

 Inputs:	psHWInst	- The HW instruction to encode
			eUnPckFmt	- The unpacking format

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeSMPInstUnPackingFormat(PHW_INST	psHWInst, 
														 USP_REGFMT	eUnPckFmt)
{
	switch(eUnPckFmt)
	{
		case USP_REGFMT_F32:
		{
			psHWInst->uWord1 |= 
				(SGXVEC_USE1_SMP_FCONV_F32 << SGXVEC_USE1_SMP_FCONV_SHIFT);
			break;
		}
		case USP_REGFMT_F16:
		{
			psHWInst->uWord1 |= 
				(SGXVEC_USE1_SMP_FCONV_F16 << SGXVEC_USE1_SMP_FCONV_SHIFT);
			break;
		}
		case USP_REGFMT_UNKNOWN:
		{
			psHWInst->uWord1 |= 
				(SGXVEC_USE1_SMP_FCONV_NONE  << SGXVEC_USE1_SMP_FCONV_SHIFT);
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}
#endif /* defined(SGX_FEATURE_USE_VEC34) */

/*****************************************************************************
 Name:		HWInstEncodeSMPInstCoordDim

 Purpose:	Encode the dimensionality of the texture-coordinates used by a
			SMP instruction

 Inputs:	psHWInst	- The HW instruction to encode
			uCoordDim	- Number of coordinate dimensions

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeSMPInstCoordDim(PHW_INST psHWInst, 
												  IMG_UINT32 uCoordDim)
{
	IMG_UINT32	uHWCoordDim;
	switch (uCoordDim)
	{
		case 1:
		{
			uHWCoordDim = EURASIA_USE1_SMP_CDIM_U;
			break;
		}
		
		case 2:
		{
			uHWCoordDim = EURASIA_USE1_SMP_CDIM_UV;
			break;
		}
		case 3:
		{
			uHWCoordDim = EURASIA_USE1_SMP_CDIM_UVS;
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
	psHWInst->uWord1 = (psHWInst->uWord1) | (uHWCoordDim << EURASIA_USE1_SMP_CDIM_SHIFT);	
	return IMG_TRUE;
}


#if defined(SGX_FEATURE_USE_32BIT_INT_MAD)
/*****************************************************************************
 Name:		HWInstEncodeIMA32Inst

 Purpose:	Encode a 32-bit interger MAD instruction

 Inputs:	psHWInst	- The HW instruction to encode

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HWInstEncodeIMA32Inst(PHW_INST		psHWInst,
										 	IMG_BOOL		bSkipInv,
										 	PUSP_REG		psDestReg,
											PUSP_REG		psSrc0Reg,
											PUSP_REG		psSrc1Reg,
											PUSP_REG		psSrc2Reg)
{
	/*
		Encode the dest register bank and number
	*/
	if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
									 USP_OPCODE_IMA32,
									 psHWInst,
									 psDestReg))
	{
		return 0;
	}

	/*
		Encode the src0 register bank and number
	*/
	if	(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_IMA32, IMG_TRUE, psHWInst, psSrc0Reg))
	{
		return 0;
	}

	/*
		Encode the src1 register bank and number
	*/
	if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_IMA32, psHWInst, psSrc1Reg))
	{
		return 0;
	}

	/*
		Encode the src2 register bank and number
	*/
	if	(!HWInstEncodeSrc2BankAndNum(USP_FMTCTL_NONE, USP_OPCODE_IMA32, psHWInst, psSrc2Reg))
	{
		return 0;
	}

	/*
		Encode the skip-inv flag
	*/
	if	(bSkipInv)
	{
		psHWInst->uWord1 |= EURASIA_USE1_SKIPINV;
	}

	psHWInst->uWord1 &= EURASIA_USE1_32BITINT_OP2_CLRMSK;
	psHWInst->uWord1 |= (EURASIA_USE1_OP_32BITINT << EURASIA_USE1_OP_SHIFT) |
					    (EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_EPRED_SHIFT) |
					    (EURASIA_USE1_32BITINT_OP2_IMA32 << EURASIA_USE1_32BITINT_OP2_SHIFT);

	return 1;
}
#endif /* defined(SGX_FEATURE_USE_32BIT_INT_MAD) */

/******************************************************************************
 End of file (HWINST.C)
******************************************************************************/

