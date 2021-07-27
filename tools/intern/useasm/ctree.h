#define YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  IMG_PCHAR		pszFilename;
  IMG_UINT32	uLine;
} YYLTYPE;

typedef struct _YZEXPRESSION
{
	YYLTYPE						sLocation;
	IMG_UINT32					uOperator;
	struct _YZEXPRESSION*		psSubExpr1;
	struct _YZEXPRESSION*		psSubExpr2;
	struct _YZEXPRESSION*		psSubExpr3;
	struct _YZTYPENAME*			psSizeofTypeName;
	IMG_UINT32					uNumber;
	IMG_PCHAR					pszString;
	struct _YZEXPRESSION*		psNext;
} YZEXPRESSION;

#define YZEXPRESSION_IDENTIFIER					0
#define YZEXPRESSION_INTEGER_NUMBER				1
#define YZEXPRESSION_FLOAT_NUMBER				2
#define YZEXPRESSION_STRING_LITERAL				3
#define YZEXPRESSION_ARRAYACCESS				4
#define YZEXPRESSION_FUNCTIONCALL				5
#define YZEXPRESSION_STRUCTACCESS				6
#define YZEXPRESSION_STRUCTPTRACCESS			7
#define YZEXPRESSION_POSTINCREMENT				8
#define YZEXPRESSION_POSTDECREMENT				9
#define YZEXPRESSION_INITIALIZER				10
#define YZEXPRESSION_PREINCREMENT				11
#define YZEXPRESSION_SIZEOFEXPRESSION			12
#define YZEXPRESSION_SIZEOFTYPE					13
#define YZEXPRESSION_ADDRESSOF					14
#define YZEXPRESSION_DEREFERENCE				15
#define YZEXPRESSION_POSITIVE					16
#define YZEXPRESSION_NEGATE						17
#define YZEXPRESSION_BITWISENOT					18
#define YZEXPRESSION_LOGICALNOT					19
#define YZEXPRESSION_CAST						20
#define YZEXPRESSION_MULTIPLY					21
#define YZEXPRESSION_DIVIDE						22
#define YZEXPRESSION_MODULUS					23
#define YZEXPRESSION_ADD						24
#define YZEXPRESSION_SUB						25
#define YZEXPRESSION_SHIFTLEFT					26
#define YZEXPRESSION_SHIFTRIGHT					27
#define YZEXPRESSION_LT							28
#define YZEXPRESSION_GT							29
#define YZEXPRESSION_LTE						31
#define YZEXPRESSION_GTE						32
#define YZEXPRESSION_EQ							33
#define YZEXPRESSION_NEQ						34
#define YZEXPRESSION_BITWISEAND					35
#define YZEXPRESSION_BITWISEXOR					36
#define YZEXPRESSION_BITWISEOR					37
#define YZEXPRESSION_LOGICALAND					38
#define YZEXPRESSION_LOGICALOR					39
#define YZEXPRESSION_CONDITIONAL				40
#define YZEXPRESSION_ASSIGNMENT					41
#define YZEXPRESSION_MULTIPLYASSIGNMENT			42
#define YZEXPRESSION_DIVIDEASSIGNMENT			43
#define YZEXPRESSION_MODULUSASSIGNMENT			44
#define YZEXPRESSION_ADDASSIGNMENT				45
#define YZEXPRESSION_SUBASSIGNMENT				46
#define YZEXPRESSION_SHIFTLEFTASSIGNMENT		47
#define YZEXPRESSION_SHIFTRIGHTASSIGNMENT		48
#define YZEXPRESSION_BITWISEANDASSIGNMENT		49
#define YZEXPRESSION_BITWISEXORASSIGNMENT		50
#define YZEXPRESSION_BITWISEORASSIGNMENT		51
#define YZEXPRESSION_COMMA						52

YZEXPRESSION* CTreeMakeExpression(IMG_UINT32 uOperator, YYLTYPE sLocation);
YZEXPRESSION* CTreeAddToExpressionList(YZEXPRESSION* psList, YZEXPRESSION* psItem);

struct _YZDECLSPECIFIER;

typedef struct _YZPOINTER
{
	YYLTYPE						sLocation;
	struct _YZDECLSPECIFIER*	psQualifierList;
	struct _YZPOINTER*			psNext;
} YZPOINTER;

typedef struct _YZDECLARATOR
{
	YYLTYPE						sLocation;
	IMG_UINT32					uType;
	YZPOINTER*					psPointer;
	struct _YZDECLARATOR*		psNext;
	IMG_PCHAR					pszIdentifier;
	struct _YZDECLARATOR*		psSubDeclarator;
	YZEXPRESSION*				psArraySize;
	struct _YZDECLSPECIFIER*	psQualifierList;
} YZDECLARATOR;

#define YZDECLARATOR_IDENTIFIER				(0)
#define YZDECLARATOR_VARIABLELENGTHARRAY	(1)
#define YZDECLARATOR_FIXEDLENGTHARRAY		(2)
#define YZDECLARATOR_FUNCTION				(3)
#define YZDECLARATOR_BRACKETED				(4)
#define YZDECLARATOR_ABSTRACTPOINTER		(5)

typedef struct _YZSTRUCTDECLARATOR
{
	YYLTYPE						sLocation;
	struct _YZSTRUCTDECLARATOR*	psNext;
	YZDECLARATOR*				psDeclarator;
	IMG_BOOL					bBitField;
	YZEXPRESSION*				psBitFieldSize;
} YZSTRUCTDECLARATOR;

typedef struct _YZSTRUCTDECLARATION
{	
	YYLTYPE							sLocation;
	struct _YZDECLSPECIFIER*		psSpecifierList;
	YZSTRUCTDECLARATOR*				psDeclaratorList;
	struct _YZSTRUCTDECLARATION*	psNext;
} YZSTRUCTDECLARATION;

typedef struct _YZENUMERATOR
{
	YYLTYPE					sLocation;
	IMG_PCHAR				pszIdentifier;
	YZEXPRESSION*			psValue;
	struct _YZENUMERATOR*	psNext;
} YZENUMERATOR;

typedef struct _YZENUM
{
	YYLTYPE				sLocation;
	IMG_BOOL			bPartial;
	IMG_PCHAR			pszIdentifier;
	YZENUMERATOR*		psEnumeratorList;
} YZENUM;

typedef struct _YZSTRUCT
{
	YYLTYPE					sLocation;
	IMG_BOOL				bStruct;
	YZSTRUCTDECLARATION*	psMemberList;
	IMG_PCHAR				pszName;
	IMG_UINT32				uAlignOverride;
} YZSTRUCT;

typedef struct _YZDECLSPECIFIER
{
	YYLTYPE						sLocation;
	struct _YZDECLSPECIFIER*	psNext;
	IMG_UINT32					uType;
	YZSTRUCT*					psStruct;
	YZENUM*						psEnum;
	IMG_PCHAR					pszIdentifier;
} YZDECLSPECIFIER;

#define YZDECLSPECIFIER_VOID				(0)
#define YZDECLSPECIFIER_CHAR				(1)
#define YZDECLSPECIFIER_SHORT				(2)
#define YZDECLSPECIFIER_INT					(3)
#define YZDECLSPECIFIER_FLOAT				(4)
#define YZDECLSPECIFIER_DOUBLE				(5)
#define YZDECLSPECIFIER_SIGNED				(6)
#define YZDECLSPECIFIER_UNSIGNED			(7)
#define YZDECLSPECIFIER_STRUCT				(8)
#define YZDECLSPECIFIER_ENUM				(9)
#define YZDECLSPECIFIER_TYPEDEF_NAME		(10)
#define YZDECLSPECIFIER_CONST				(11)
#define YZDECLSPECIFIER_RESTRICT			(12)
#define YZDECLSPECIFIER_VOLATILE			(13)
#define YZDECLSPECIFIER_TYPEDEF				(14)
#define YZDECLSPECIFIER_EXTERN				(15)
#define YZDECLSPECIFIER_STATIC				(16)
#define YZDECLSPECIFIER_AUTO				(17)
#define YZDECLSPECIFIER_REGISTER			(18)
#define YZDECLSPECIFIER_LONG				(19)
#define YZDECLSPECIFIER_INLINE				(20)
#define YZDECLSPECIFIER_FORCEINLINE			(21)
#define YZDECLSPECIFIER__FASTCALL			(22)
#define YZDECLSPECIFIER_MSVC_INT8			(23)
#define YZDECLSPECIFIER_MSVC_INT16			(24)
#define YZDECLSPECIFIER_MSVC_INT32			(25)
#define YZDECLSPECIFIER_MSVC_INT64			(26)
#define YZDECLSPECIFIER_CDECL				(27)
#define YZDECLSPECIFIER_STDCALL				(28)
#define YZDECLSPECIFIER_DECLSPEC			(29)
#define YZDECLSPECIFIER_MSVC__W64			(30)
#define YZDECLSPECIFIER_GCC_BUILTIN_VA_LIST	(31)
#define YZDECLSPECIFIER_BOOL				(32)
#define YZDECLSPECIFIER_MSVC_PTR32			(33)
#define YZDECLSPECIFIER_MSVC_PTR64			(34)

typedef struct _YZINITDECLARATOR
{
	YYLTYPE							sLocation;
	struct _YZINITDECLARATOR*		psNext;
	YZDECLARATOR*					psDeclarator;
	YZEXPRESSION*					psInitializer;
} YZINITDECLARATOR;

typedef struct _YZDECLARATION
{
	YYLTYPE							sLocation;
	YZDECLSPECIFIER*				psDeclSpecifierList;
	YZINITDECLARATOR*				psInitDeclaratorList;
	struct _YZDECLARATION*			psNext;
} YZDECLARATION;

typedef struct _YZTYPENAME
{
	YYLTYPE							sLocation;
	YZDECLSPECIFIER*				psDeclSpecifierList;
	struct _YZDECLARATOR*			psDeclarator;
} YZTYPENAME, *PYZTYPENAME;

IMG_VOID CTreeAddExternDecl(YZDECLARATION* psDeclaration);
YZDECLARATION* CTreeMakeDeclaration(YYLTYPE sLocation);
YZDECLSPECIFIER* CTreeAddToDeclSpecifierList(YZDECLSPECIFIER* psList, YZDECLSPECIFIER* psItem);
YZINITDECLARATOR* CTreeAddToInitDeclaratorList(YZINITDECLARATOR* psList, YZINITDECLARATOR* psItem);
YZINITDECLARATOR* CTreeMakeInitDeclarator(YYLTYPE sLocation);
YZDECLSPECIFIER* CTreeMakeDeclSpecifier(YYLTYPE sLocation);
YZSTRUCT* CTreeMakeStruct(YYLTYPE sLocation);
YZSTRUCTDECLARATION* CTreeAddToStructDeclarationList(YZSTRUCTDECLARATION* psList, YZSTRUCTDECLARATION* psItem);
YZSTRUCTDECLARATION* CTreeMakeStructDeclaration(YYLTYPE sLocation);
YZSTRUCTDECLARATOR* CTreeAddToStructDeclaratorList(YZSTRUCTDECLARATOR* psList, YZSTRUCTDECLARATOR* psItem);
YZSTRUCTDECLARATOR* CTreeMakeStructDeclarator(YYLTYPE sLocation);
YZENUM* CTreeMakeEnum(YYLTYPE sLocation);
YZENUMERATOR* CTreeAddToEnumeratorList(YZENUMERATOR* psList, YZENUMERATOR* psItem);
YZENUMERATOR* CTreeMakeEnumerator(YYLTYPE sLocation);
YZDECLARATOR* CTreeMakeDeclarator(YYLTYPE sLocation);
YZPOINTER* CTreeMakePointer(YYLTYPE sLocation);
YZTYPENAME* CTreeMakeTypeName(YYLTYPE sLocation, YZDECLSPECIFIER* psDeclSpecifierList, YZDECLARATOR* psAbstractDeclarator);

IMG_BOOL CTreeIsTypedefName(IMG_PCHAR pszName);

IMG_VOID CTreeInitialize(IMG_VOID);

IMG_VOID DumpGlobalTypes(IMG_VOID);

IMG_VOID ResolveNameToType(IMG_PCHAR pszTypeMemberName, IMG_UINT32 uTypeMemberNameLength, IMG_PUINT32 puOffset, IMG_PUINT32 puSize, IMG_PCHAR pszErrorFilename, IMG_UINT32 uErrorLine);

void CollapseStringCodes(IMG_PCHAR pszCodes);

extern IMG_PCHAR g_pszYzSourceFile;
extern IMG_UINT32 g_uYzSourceLine;

extern IMG_BOOL g_bCCodeError;

extern IMG_UINT32 g_uStructAlignOverride;
extern IMG_UINT32 g_uDefaultStructAlignOverride;

IMG_VOID ParseHashLine(IMG_PCHAR pszLine, IMG_UINT32 uLength);