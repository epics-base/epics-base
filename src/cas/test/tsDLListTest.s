!

	.seg	"data"

/* 000000	   4 */		.align	1
!
! SUBROUTINE LI9
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       LI9:
/* 000000	   6 */		.ascii	"A\0"
/* 0x0002	   8 */		.align	1

!
! ENTRY LI10
!

                       LI10:
/* 0x0002	  10 */		.ascii	"B\0"
/* 0x0004	  12 */		.align	4

!
! ENTRY LI11
!

                       LI11:
/* 0x0004	  14 */		.ascii	"Assertion failed: file \"%s\", line %d\n\0"
/* 0x002a	  16 */		.align	4

!
! ENTRY LI12
!

                       LI12:
/* 0x002c	  18 */		.ascii	"tsDLListTest.cc\0"
/* 0x003c	  20 */		.align	4

!
! ENTRY LI13
!

                       LI13:
/* 0x003c	  22 */		.ascii	"Assertion failed: file \"%s\", line %d\n\0"
/* 0x0062	  24 */		.align	4

!
! ENTRY LI14
!

                       LI14:
/* 0x0064	  26 */		.ascii	"tsDLListTest.cc\0"
/* 0x0074	  28 */		.align	4

!
! ENTRY LI15
!

                       LI15:
/* 0x0074	  30 */		.ascii	"Assertion failed: file \"%s\", line %d\n\0"
/* 0x009a	  32 */		.align	4

!
! ENTRY LI16
!

                       LI16:
/* 0x009c	  34 */		.ascii	"tsDLListTest.cc\0"
/* 0x00ac	  36 */		.align	4

!
! ENTRY LI17
!

                       LI17:
/* 0x00ac	  38 */		.ascii	"Assertion failed: file \"%s\", line %d\n\0"
/* 0x00d2	  40 */		.align	4

!
! ENTRY LI18
!

                       LI18:
/* 0x00d4	  42 */		.ascii	"tsDLListTest.cc\0"
/* 0x00e4	  44 */		.align	4

!
! ENTRY LI19
!

                       LI19:
/* 0x00e4	  46 */		.ascii	"Assertion failed: file \"%s\", line %d\n\0"
/* 0x010a	  48 */		.align	4

!
! ENTRY LI20
!

                       LI20:
/* 0x010c	  50 */		.ascii	"tsDLListTest.cc\0"
/* 0x011c	  52 */		.align	1

!
! ENTRY LI21
!

                       LI21:
/* 0x011c	  54 */		.ascii	"C\0"
/* 0x011e	  56 */		.align	1

!
! ENTRY LI22
!

                       LI22:
/* 0x011e	  58 */		.ascii	"D\0"
/* 0x0120	  60 */		.align	4

!
! ENTRY LI23
!

                       LI23:
/* 0x0120	  62 */		.ascii	"%s\n\0"
/* 0x0124	  64 */		.align	1

!
! ENTRY LI24
!

                       LI24:
/* 0x0124	  66 */		.ascii	"JA\0"
/* 0x0127	  68 */		.align	1

!
! ENTRY LI25
!

                       LI25:
/* 0x0127	  70 */		.ascii	"JB\0"
/* 0x012a	  72 */		.align	4

!
! ENTRY LI26
!

                       LI26:
/* 0x012c	  74 */		.ascii	"%s\n\0"
/* 0x0130	  76 */		.align	4

!
! ENTRY LI27
!

                       LI27:
/* 0x0130	  78 */		.ascii	"%s\n\0"
!

	.seg	"text"

/* 000000	   0 */		.align	8
/* 000000	     */		.skip	16
! FILE tsDLListTest.cc
!    4		      !#include <tsDLList.h>
!    5		      !#include <assert.h>
!    7		      !class fred : public tsDLNode<fred> {
!    8		      !public:
!    9		      !	fred(const char * const pNameIn) : pName(pNameIn){}
!   10		      !	void show () {printf("%s\n", pName);}
!   11		      !private:
!   12		      !	const char * const pName;
!   13		      !};
!   15		      !class jane : public fred, public tsDLNode<jane> {
!   16		      !public:
!   17		      !	jane(const char * const pNameIn) : fred(pNameIn){}
!   18		      !private:
!   19		      !};
!   21		      !main ()
!   22		      !{
!
! SUBROUTINE _main
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global _main
                       _main:
/* 000000	     */		save	%sp,-136,%sp
!   23		      !	tsDLList<fred>	list;
!   24		      !	tsDLIter<fred>	iter(list);
!   25		      !	fred		*pFred;
!   26		      !	fred		*pFredII;
!   27		      !	fred		*pFredBack;
!   28		      !	tsDLList<jane>	janeList;
!   29		      !	tsDLIter<jane>	janeIter(janeList);
!   30		      !	jane		*pJane;
!   32		      !	pFred = new fred("A");
/* 0x0004	  32 */		sethi	%hi(LI9),%o0
/* 0x0008	  22 */		call	__cinit_,0	! Result = %g0
/* 0x000c	   0 */		add	%o0,%lo(LI9),%l1
/* 0x0010	  23 */		add	%fp,-12,%o0
/* 0x0014	  24 */		add	%fp,-20,%o1
/* 0x0018	  23 */		st	%g0,[%o0] ! volatile
/* 0x001c	     */		st	%g0,[%o0+4] ! volatile
/* 0x0020	     */		st	%g0,[%o0+8] ! volatile
/* 0x0024	  24 */		add	%fp,-12,%o0
/* 0x0028	     */		st	%o0,[%o1] ! volatile
/* 0x002c	  28 */		add	%fp,-32,%o0
/* 0x0030	  24 */		st	%g0,[%o1+4] ! volatile
/* 0x0034	  29 */		add	%fp,-40,%o1
/* 0x0038	  28 */		st	%g0,[%o0] ! volatile
/* 0x003c	     */		st	%g0,[%o0+4] ! volatile
/* 0x0040	     */		st	%g0,[%o0+8] ! volatile
/* 0x0044	  29 */		add	%fp,-32,%o0
/* 0x0048	     */		st	%o0,[%o1] ! volatile
/* 0x004c	  32 */		or	%g0,12,%o0
/* 0x0050	     */		call	___0OnwUi,1	! Result = %o0
/* 0x0054	  29 */		st	%g0,[%o1+4] ! volatile
/* 0x0058	  32 */		orcc	%g0,%o0,%l3
/* 0x005c	     */		be	L77000004
/* 0x0060	     */		or	%g0,%l3,%o0
                       L77000002:
/* 0x0064	     */		st	%g0,[%o0] ! volatile
/* 0x0068	     */		add	%l1,0,%o1
/* 0x006c	     */		st	%g0,[%o0+4] ! volatile
/* 0x0070	     */		st	%o1,[%l3+8] ! volatile
                       L77000004:
!   33		      !	pFredII = new fred("B");
/* 0x0074	  33 */		call	___0OnwUi,1	! Result = %o0
/* 0x0078	     */		or	%g0,12,%o0
/* 0x007c	     */		orcc	%g0,%o0,%l2
/* 0x0080	     */		be	L77000007
/* 0x0084	     */		or	%g0,%l2,%o0
                       L77000005:
/* 0x0088	     */		st	%g0,[%o0] ! volatile
/* 0x008c	     */		add	%l1,2,%o1
/* 0x0090	     */		st	%g0,[%o0+4] ! volatile
/* 0x0094	     */		st	%o1,[%l2+8] ! volatile
                       L77000007:
!   35		      !	list.add(*pFred);
/* 0x0098	  35 */		add	%fp,-12,%o0
/* 0x009c	     */		st	%g0,[%l3] ! volatile
/* 0x00a0	     */		ld	[%o0+4],%o1 ! volatile
/* 0x00a4	     */		st	%o1,[%l3+4] ! volatile
/* 0x00a8	     */		ld	[%o0+8],%o2 ! volatile
/* 0x00ac	     */		cmp	%o2,0
/* 0x00b0	     */		bne	L77000008
/* 0x00b4	     */		or	%g0,%l3,%o3
                       L77000009:
/* 0x00b8	     */		ba	L77000010
/* 0x00bc	     */		st	%o3,[%o0] ! volatile
!   36		      !	list.add(*pFredII);
!   37		      !	pFredBack = list.first();
!   38		      !	assert(pFredBack == pFred);
!   39		      !	pFredBack = list.last();
!   40		      !	assert(pFredBack == pFredII);
!   41		      !	list.remove(*pFred);
!   42		      !	list.add(*pFred);
!   43		      !	pFredBack = list.get();
!   44		      !	assert (pFredBack == pFredII);
!   45		      !	pFredBack = list.get();
!   46		      !	assert (pFredBack == pFred);
!   47		      !	assert (list.count() == 0u);
!   48		      !	list.add(*pFred);
!   49		      !	list.add(*pFredII);
!   50		      !	list.add(* new fred("C"));
!   51		      !	list.add(* new fred("D"));
!   53		      !	while (pFredBack = iter()) {
!   54		      !		pFredBack->show();
!   55		      !	}
!   57		      !	pJane = new jane("JA");
!   58		      !	janeList.add(*pJane);	
!   59		      !	pJane = new jane("JB");
!   60		      !	janeList.add(*pJane);	
!   62		      !	while (pJane = janeIter()) {
!   63		      !		pJane->show();
!   64		      !	}
!   66		      !	iter = list;
!   67		      !	while (pFredBack = iter()) {
!   68		      !		pFredBack->show();
                       L77000099:
/* 0x00c0	     */		ret
/* 0x00c4	     */		restore	%g0,0,%o0
                       L77000008:
/* 0x00c8	     */		ld	[%o0+4],%o1 ! volatile
/* 0x00cc	     */		st	%o3,[%o1] ! volatile
                       L77000010:
/* 0x00d0	     */		st	%o3,[%o0+4] ! volatile
/* 0x00d4	  36 */		add	%fp,-12,%o3
/* 0x00d8	  35 */		ld	[%o0+8],%o1 ! volatile
/* 0x00dc	     */		add	%o1,1,%o1
/* 0x00e0	     */		st	%o1,[%o0+8] ! volatile
/* 0x00e4	  36 */		st	%g0,[%l2] ! volatile
/* 0x00e8	     */		ld	[%o3+4],%o1 ! volatile
/* 0x00ec	     */		st	%o1,[%l2+4] ! volatile
/* 0x00f0	     */		ld	[%o3+8],%o2 ! volatile
/* 0x00f4	     */		cmp	%o2,0
/* 0x00f8	     */		bne	L77000011
/* 0x00fc	     */		or	%g0,%l2,%o4
                       L77000012:
/* 0x0100	     */		ba	L77000013
/* 0x0104	     */		st	%o4,[%o3] ! volatile
                       L77000011:
/* 0x0108	     */		ld	[%o3+4],%o1 ! volatile
/* 0x010c	     */		st	%o4,[%o1] ! volatile
                       L77000013:
/* 0x0110	     */		st	%o4,[%o3+4] ! volatile
/* 0x0114	  38 */		sethi	%hi(__iob),%o0
/* 0x0118	  36 */		ld	[%o3+8],%o1 ! volatile
/* 0x011c	   0 */		add	%o0,%lo(__iob),%l4
/* 0x0120	  36 */		add	%o1,1,%o1
/* 0x0124	     */		st	%o1,[%o3+8] ! volatile
/* 0x0128	  37 */		add	%fp,-12,%o0
/* 0x012c	     */		ld	[%o0],%o0 ! volatile
/* 0x0130	  38 */		cmp	%o0,%l3
/* 0x0134	     */		bne	L77000014
/* 0x0138	     */		or	%g0,40,%l5
                       L77000015:
/* 0x013c	  39 */		add	%fp,-12,%o0
                       L900000116:
/* 0x0140	  39 */		ld	[%o0+4],%o0 ! volatile
/* 0x0144	  40 */		cmp	%o0,%l2
/* 0x0148	     */		bne,a	L900000115
/* 0x014c	     */		add	%l4,0,%l0
                       L77000017:
/* 0x0150	  41 */		add	%fp,-12,%o0
                       L900000117:
/* 0x0154	  41 */		ld	[%o0+4],%o1 ! volatile
/* 0x0158	     */		cmp	%o1,%l3
/* 0x015c	     */		bne	L77000019
/* 0x0160	     */		or	%g0,%l3,%o3
                       L77000018:
/* 0x0164	     */		ld	[%o3+4],%o1 ! volatile
/* 0x0168	     */		ba	L77000020
/* 0x016c	     */		st	%o1,[%o0+4] ! volatile
                       L77000014:
/* 0x0170	     */		add	%l4,0,%l0
/* 0x0174	     */		add	%l5,%l0,%o0
/* 0x0178	     */		add	%l1,4,%o1
/* 0x017c	     */		add	%l1,44,%o2
/* 0x0180	     */		call	_fprintf,4	! Result = %g0
/* 0x0184	     */		or	%g0,38,%o3
/* 0x0188	     */		call	_exit,1	! Result = %g0
/* 0x018c	     */		or	%g0,1,%o0
/* 0x0190	     */		ba	L900000116
/* 0x0194	  39 */		add	%fp,-12,%o0
                       L900000115:
/* 0x0198	     */		add	%l5,%l0,%o0
/* 0x019c	     */		add	%l1,60,%o1
/* 0x01a0	     */		add	%l1,100,%o2
/* 0x01a4	     */		call	_fprintf,4	! Result = %g0
/* 0x01a8	     */		or	%g0,40,%o3
/* 0x01ac	     */		call	_exit,1	! Result = %g0
/* 0x01b0	     */		or	%g0,1,%o0
/* 0x01b4	     */		ba	L900000117
/* 0x01b8	  41 */		add	%fp,-12,%o0
                       L77000019:
/* 0x01bc	     */		ld	[%o3+4],%o1 ! volatile
/* 0x01c0	     */		ld	[%o3],%o2 ! volatile
/* 0x01c4	     */		st	%o1,[%o2+4] ! volatile
                       L77000020:
/* 0x01c8	     */		ld	[%o0],%o1 ! volatile
/* 0x01cc	     */		cmp	%o1,%o3
/* 0x01d0	     */		bne,a	L900000118
/* 0x01d4	     */		ld	[%o3],%o1 ! volatile
                       L77000021:
/* 0x01d8	     */		ld	[%o3],%o1 ! volatile
/* 0x01dc	     */		ba	L77000023
/* 0x01e0	     */		st	%o1,[%o0] ! volatile
                       L900000118:
/* 0x01e4	     */		ld	[%o3+4],%o2 ! volatile
/* 0x01e8	     */		st	%o1,[%o2] ! volatile
                       L77000023:
/* 0x01ec	     */		ld	[%o0+8],%o1 ! volatile
/* 0x01f0	     */		add	%o1,-1,%o1
/* 0x01f4	     */		st	%o1,[%o0+8] ! volatile
/* 0x01f8	  42 */		add	%fp,-12,%o0
/* 0x01fc	     */		st	%g0,[%l3] ! volatile
/* 0x0200	     */		ld	[%o0+4],%o1 ! volatile
/* 0x0204	     */		st	%o1,[%l3+4] ! volatile
/* 0x0208	     */		ld	[%o0+8],%o2 ! volatile
/* 0x020c	     */		cmp	%o2,0
/* 0x0210	     */		bne	L77000024
/* 0x0214	     */		or	%g0,%l3,%o3
                       L77000025:
/* 0x0218	     */		ba	L77000026
/* 0x021c	     */		st	%o3,[%o0] ! volatile
                       L77000024:
/* 0x0220	     */		ld	[%o0+4],%o1 ! volatile
/* 0x0224	     */		st	%o3,[%o1] ! volatile
                       L77000026:
/* 0x0228	     */		st	%o3,[%o0+4] ! volatile
/* 0x022c	     */		ld	[%o0+8],%o1 ! volatile
/* 0x0230	     */		add	%o1,1,%o1
/* 0x0234	     */		st	%o1,[%o0+8] ! volatile
/* 0x0238	  43 */		add	%fp,-12,%o0
/* 0x023c	     */		ld	[%o0],%o3 ! volatile
/* 0x0240	     */		cmp	%o3,0
/* 0x0244	     */		be,a	L900000119
/* 0x0248	  44 */		cmp	%o3,%l2
                       L77000027:
/* 0x024c	     */		ld	[%o0+4],%o1 ! volatile
/* 0x0250	     */		cmp	%o1,%o3
/* 0x0254	     */		bne,a	L900000120
/* 0x0258	     */		ld	[%o3+4],%o1 ! volatile
                       L77000028:
/* 0x025c	     */		ld	[%o3+4],%o1 ! volatile
/* 0x0260	     */		ba	L77000030
/* 0x0264	     */		st	%o1,[%o0+4] ! volatile
                       L900000120:
/* 0x0268	     */		ld	[%o3],%o2 ! volatile
/* 0x026c	     */		st	%o1,[%o2+4] ! volatile
                       L77000030:
/* 0x0270	     */		ld	[%o0],%o1 ! volatile
/* 0x0274	     */		cmp	%o1,%o3
/* 0x0278	     */		bne,a	L900000121
/* 0x027c	     */		ld	[%o3],%o1 ! volatile
                       L77000031:
/* 0x0280	     */		ld	[%o3],%o1 ! volatile
/* 0x0284	     */		ba	L77000033
/* 0x0288	     */		st	%o1,[%o0] ! volatile
                       L900000121:
/* 0x028c	     */		ld	[%o3+4],%o2 ! volatile
/* 0x0290	     */		st	%o1,[%o2] ! volatile
                       L77000033:
/* 0x0294	     */		ld	[%o0+8],%o1 ! volatile
/* 0x0298	  44 */		cmp	%o3,%l2
/* 0x029c	  43 */		add	%o1,-1,%o1
/* 0x02a0	     */		st	%o1,[%o0+8] ! volatile
                       L900000119:
/* 0x02a4	  44 */		bne,a	L900000122
/* 0x02a8	     */		add	%l4,0,%l0
                       L77000037:
/* 0x02ac	  45 */		add	%fp,-12,%o0
                       L900000125:
/* 0x02b0	  45 */		ld	[%o0],%o3 ! volatile
/* 0x02b4	     */		cmp	%o3,0
/* 0x02b8	     */		be,a	L900000123
/* 0x02bc	  46 */		cmp	%o3,%l3
                       L77000038:
/* 0x02c0	     */		ld	[%o0+4],%o1 ! volatile
/* 0x02c4	     */		cmp	%o1,%o3
/* 0x02c8	     */		bne,a	L900000124
/* 0x02cc	     */		ld	[%o3+4],%o1 ! volatile
                       L77000039:
/* 0x02d0	     */		ld	[%o3+4],%o1 ! volatile
/* 0x02d4	     */		ba	L77000041
/* 0x02d8	     */		st	%o1,[%o0+4] ! volatile
                       L900000122:
/* 0x02dc	     */		add	%l5,%l0,%o0
/* 0x02e0	     */		add	%l1,116,%o1
/* 0x02e4	     */		add	%l1,156,%o2
/* 0x02e8	     */		call	_fprintf,4	! Result = %g0
/* 0x02ec	     */		or	%g0,44,%o3
/* 0x02f0	     */		call	_exit,1	! Result = %g0
/* 0x02f4	     */		or	%g0,1,%o0
/* 0x02f8	     */		ba	L900000125
/* 0x02fc	  45 */		add	%fp,-12,%o0
                       L900000124:
/* 0x0300	     */		ld	[%o3],%o2 ! volatile
/* 0x0304	     */		st	%o1,[%o2+4] ! volatile
                       L77000041:
/* 0x0308	     */		ld	[%o0],%o1 ! volatile
/* 0x030c	     */		cmp	%o1,%o3
/* 0x0310	     */		bne,a	L900000126
/* 0x0314	     */		ld	[%o3],%o1 ! volatile
                       L77000042:
/* 0x0318	     */		ld	[%o3],%o1 ! volatile
/* 0x031c	     */		ba	L77000044
/* 0x0320	     */		st	%o1,[%o0] ! volatile
                       L900000126:
/* 0x0324	     */		ld	[%o3+4],%o2 ! volatile
/* 0x0328	     */		st	%o1,[%o2] ! volatile
                       L77000044:
/* 0x032c	     */		ld	[%o0+8],%o1 ! volatile
/* 0x0330	  46 */		cmp	%o3,%l3
/* 0x0334	  45 */		add	%o1,-1,%o1
/* 0x0338	     */		st	%o1,[%o0+8] ! volatile
                       L900000123:
/* 0x033c	  46 */		bne,a	L900000127
/* 0x0340	     */		add	%l4,0,%l0
                       L77000048:
/* 0x0344	  47 */		or	%g0,8,%o1
                       L900000129:
/* 0x0348	  47 */		add	%fp,-12,%o2
/* 0x034c	     */		ld	[%o1+%o2],%o1 ! volatile
/* 0x0350	     */		cmp	%o1,0
/* 0x0354	     */		bne,a	L900000128
/* 0x0358	     */		add	%l4,0,%l0
                       L77000050:
/* 0x035c	  48 */		add	%fp,-12,%o0
                       L900000130:
/* 0x0360	  48 */		st	%g0,[%l3] ! volatile
/* 0x0364	     */		ld	[%o0+4],%o1 ! volatile
/* 0x0368	     */		st	%o1,[%l3+4] ! volatile
/* 0x036c	     */		ld	[%o0+8],%o2 ! volatile
/* 0x0370	     */		cmp	%o2,0
/* 0x0374	     */		bne	L77000051
/* 0x0378	     */		or	%g0,%l3,%o3
                       L77000052:
/* 0x037c	     */		ba	L77000053
/* 0x0380	     */		st	%o3,[%o0] ! volatile
                       L900000127:
/* 0x0384	     */		add	%l5,%l0,%o0
/* 0x0388	     */		add	%l1,172,%o1
/* 0x038c	     */		add	%l1,212,%o2
/* 0x0390	     */		call	_fprintf,4	! Result = %g0
/* 0x0394	     */		or	%g0,46,%o3
/* 0x0398	     */		call	_exit,1	! Result = %g0
/* 0x039c	     */		or	%g0,1,%o0
/* 0x03a0	     */		ba	L900000129
/* 0x03a4	  47 */		or	%g0,8,%o1
                       L900000128:
/* 0x03a8	     */		add	%l5,%l0,%o0
/* 0x03ac	     */		add	%l1,228,%o1
/* 0x03b0	     */		add	%l1,268,%o2
/* 0x03b4	     */		call	_fprintf,4	! Result = %g0
/* 0x03b8	     */		or	%g0,47,%o3
/* 0x03bc	     */		call	_exit,1	! Result = %g0
/* 0x03c0	     */		or	%g0,1,%o0
/* 0x03c4	     */		ba	L900000130
/* 0x03c8	  48 */		add	%fp,-12,%o0
                       L77000051:
/* 0x03cc	     */		ld	[%o0+4],%o1 ! volatile
/* 0x03d0	     */		st	%o3,[%o1] ! volatile
                       L77000053:
/* 0x03d4	     */		st	%o3,[%o0+4] ! volatile
/* 0x03d8	     */		ld	[%o0+8],%o1 ! volatile
/* 0x03dc	     */		add	%o1,1,%o1
/* 0x03e0	     */		st	%o1,[%o0+8] ! volatile
/* 0x03e4	  49 */		add	%fp,-12,%o0
/* 0x03e8	     */		st	%g0,[%l2] ! volatile
/* 0x03ec	     */		ld	[%o0+4],%o1 ! volatile
/* 0x03f0	     */		st	%o1,[%l2+4] ! volatile
/* 0x03f4	     */		ld	[%o0+8],%o2 ! volatile
/* 0x03f8	     */		cmp	%o2,0
/* 0x03fc	     */		bne	L77000054
/* 0x0400	     */		or	%g0,%l2,%o3
                       L77000055:
/* 0x0404	     */		ba	L77000056
/* 0x0408	     */		st	%o3,[%o0] ! volatile
                       L77000054:
/* 0x040c	     */		ld	[%o0+4],%o1 ! volatile
/* 0x0410	     */		st	%o3,[%o1] ! volatile
                       L77000056:
/* 0x0414	     */		st	%o3,[%o0+4] ! volatile
/* 0x0418	  50 */		add	%fp,-12,%l2
/* 0x041c	  49 */		ld	[%o0+8],%l0 ! volatile
/* 0x0420	     */		add	%l0,1,%l0
/* 0x0424	     */		st	%l0,[%o0+8] ! volatile
/* 0x0428	  50 */		call	___0OnwUi,1	! Result = %o0
/* 0x042c	     */		or	%g0,12,%o0
/* 0x0430	     */		orcc	%g0,%o0,%o3
/* 0x0434	     */		be,a	L900000131
/* 0x0438	     */		st	%g0,[%o3] ! volatile
                       L77000057:
/* 0x043c	     */		or	%g0,%o3,%o0
/* 0x0440	     */		add	%l1,284,%o1
/* 0x0444	     */		st	%g0,[%o0] ! volatile
/* 0x0448	     */		st	%g0,[%o0+4] ! volatile
/* 0x044c	     */		st	%o1,[%o3+8] ! volatile
/* 0x0450	     */		st	%g0,[%o3] ! volatile
                       L900000131:
/* 0x0454	     */		ld	[%l2+4],%o1 ! volatile
/* 0x0458	     */		st	%o1,[%o3+4] ! volatile
/* 0x045c	     */		ld	[%l2+8],%o2 ! volatile
/* 0x0460	     */		cmp	%o2,0
/* 0x0464	     */		bne,a	L900000132
/* 0x0468	     */		ld	[%l2+4],%o1 ! volatile
                       L77000061:
/* 0x046c	     */		ba	L77000062
/* 0x0470	     */		st	%o3,[%l2] ! volatile
                       L900000132:
/* 0x0474	     */		st	%o3,[%o1] ! volatile
                       L77000062:
/* 0x0478	     */		st	%o3,[%l2+4] ! volatile
/* 0x047c	     */		ld	[%l2+8],%l0 ! volatile
/* 0x0480	     */		add	%l0,1,%l0
/* 0x0484	     */		st	%l0,[%l2+8] ! volatile
/* 0x0488	  51 */		call	___0OnwUi,1	! Result = %o0
/* 0x048c	     */		or	%g0,12,%o0
/* 0x0490	     */		orcc	%g0,%o0,%o3
/* 0x0494	     */		add	%fp,-12,%l0
/* 0x0498	     */		be,a	L900000133
/* 0x049c	     */		st	%g0,[%o3] ! volatile
                       L77000063:
/* 0x04a0	     */		or	%g0,%o3,%o0
/* 0x04a4	     */		add	%l1,286,%o1
/* 0x04a8	     */		st	%g0,[%o0] ! volatile
/* 0x04ac	     */		st	%g0,[%o0+4] ! volatile
/* 0x04b0	     */		st	%o1,[%o3+8] ! volatile
/* 0x04b4	     */		st	%g0,[%o3] ! volatile
                       L900000133:
/* 0x04b8	     */		ld	[%l0+4],%o1 ! volatile
/* 0x04bc	     */		st	%o1,[%o3+4] ! volatile
/* 0x04c0	     */		ld	[%l0+8],%o2 ! volatile
/* 0x04c4	     */		cmp	%o2,0
/* 0x04c8	     */		bne,a	L900000134
/* 0x04cc	     */		ld	[%l0+4],%o1 ! volatile
                       L77000067:
/* 0x04d0	     */		ba	L77000068
/* 0x04d4	     */		st	%o3,[%l0] ! volatile
                       L900000134:
/* 0x04d8	     */		st	%o3,[%o1] ! volatile
                       L77000068:
/* 0x04dc	     */		st	%o3,[%l0+4] ! volatile
/* 0x04e0	     */		ld	[%l0+8],%o1 ! volatile
/* 0x04e4	     */		add	%o1,1,%o1
/* 0x04e8	     */		st	%o1,[%l0+8] ! volatile
/* 0x04ec	  53 */		add	%fp,-20,%l0
/* 0x04f0	     */		ba	L900000135
/* 0x04f4	     */		ld	[%l0+4],%o1 ! volatile
                       L900000136:
/* 0x04f8	     */		ld	[%o0],%o1 ! volatile
/* 0x04fc	     */		st	%o1,[%l0+4] ! volatile
                       L77000072:
/* 0x0500	     */		ld	[%l0+4],%o1 ! volatile
/* 0x0504	     */		cmp	%o1,0
/* 0x0508	     */		be	L77000074
/* 0x050c	  54 */		add	%l1,288,%o0
                       L77000073:
/* 0x0510	  54 */		call	_printf,2	! Result = %g0
/* 0x0514	     */		ld	[%o1+8],%o1 ! volatile
/* 0x0518	  53 */		ld	[%l0+4],%o1 ! volatile
                       L900000135:
/* 0x051c	     */		cmp	%o1,0
/* 0x0520	     */		be,a	L900000136
/* 0x0524	     */		ld	[%l0],%o0 ! volatile
                       L77000070:
/* 0x0528	     */		ld	[%l0+4],%o1 ! volatile
/* 0x052c	     */		ld	[%o1],%o1 ! volatile
/* 0x0530	     */		ba	L77000072
/* 0x0534	     */		st	%o1,[%l0+4] ! volatile
                       L77000074:
/* 0x0538	  57 */		call	___0OnwUi,1	! Result = %o0
/* 0x053c	     */		or	%g0,20,%o0
/* 0x0540	     */		orcc	%g0,%o0,%o4
/* 0x0544	     */		be	L77000077
/* 0x0548	     */		or	%g0,%o4,%o2
                       L77000075:
/* 0x054c	     */		or	%g0,%o2,%o0
/* 0x0550	     */		add	%l1,292,%o1
/* 0x0554	     */		st	%g0,[%o0] ! volatile
/* 0x0558	     */		st	%g0,[%o0+4] ! volatile
/* 0x055c	     */		add	%o4,12,%o0
/* 0x0560	     */		st	%o1,[%o2+8] ! volatile
/* 0x0564	     */		st	%g0,[%o0] ! volatile
/* 0x0568	     */		st	%g0,[%o0+4] ! volatile
                       L77000077:
/* 0x056c	  58 */		add	%o4,12,%o0
/* 0x0570	     */		add	%fp,-32,%o3
/* 0x0574	     */		st	%g0,[%o0] ! volatile
/* 0x0578	     */		ld	[%o3+4],%o1 ! volatile
/* 0x057c	     */		st	%o1,[%o0+4] ! volatile
/* 0x0580	     */		ld	[%o3+8],%o2 ! volatile
/* 0x0584	     */		cmp	%o2,0
/* 0x0588	     */		bne,a	L900000137
/* 0x058c	     */		ld	[%o3+4],%o1 ! volatile
                       L77000079:
/* 0x0590	     */		ba	L77000080
/* 0x0594	     */		st	%o4,[%o3] ! volatile
                       L900000137:
/* 0x0598	     */		add	%o1,12,%o1
/* 0x059c	     */		st	%o4,[%o1] ! volatile
                       L77000080:
/* 0x05a0	     */		st	%o4,[%o3+4] ! volatile
/* 0x05a4	  59 */		or	%g0,20,%o0
/* 0x05a8	  58 */		ld	[%o3+8],%l0 ! volatile
/* 0x05ac	     */		add	%l0,1,%l0
/* 0x05b0	  59 */		call	___0OnwUi,1	! Result = %o0
/* 0x05b4	  58 */		st	%l0,[%o3+8] ! volatile
/* 0x05b8	  59 */		orcc	%g0,%o0,%o4
/* 0x05bc	     */		be	L77000083
/* 0x05c0	     */		or	%g0,%o4,%o2
                       L77000081:
/* 0x05c4	     */		or	%g0,%o2,%o0
/* 0x05c8	     */		add	%l1,295,%o1
/* 0x05cc	     */		st	%g0,[%o0] ! volatile
/* 0x05d0	     */		st	%g0,[%o0+4] ! volatile
/* 0x05d4	     */		add	%o4,12,%o0
/* 0x05d8	     */		st	%o1,[%o2+8] ! volatile
/* 0x05dc	     */		st	%g0,[%o0] ! volatile
/* 0x05e0	     */		st	%g0,[%o0+4] ! volatile
                       L77000083:
/* 0x05e4	  60 */		add	%o4,12,%o0
/* 0x05e8	     */		add	%fp,-32,%o3
/* 0x05ec	     */		st	%g0,[%o0] ! volatile
/* 0x05f0	     */		ld	[%o3+4],%o1 ! volatile
/* 0x05f4	     */		st	%o1,[%o0+4] ! volatile
/* 0x05f8	     */		ld	[%o3+8],%o2 ! volatile
/* 0x05fc	     */		cmp	%o2,0
/* 0x0600	     */		bne,a	L900000138
/* 0x0604	     */		ld	[%o3+4],%o1 ! volatile
                       L77000085:
/* 0x0608	     */		ba	L77000086
/* 0x060c	     */		st	%o4,[%o3] ! volatile
                       L900000138:
/* 0x0610	     */		add	%o1,12,%o1
/* 0x0614	     */		st	%o4,[%o1] ! volatile
                       L77000086:
/* 0x0618	     */		st	%o4,[%o3+4] ! volatile
/* 0x061c	  62 */		add	%fp,-40,%l0
/* 0x0620	  60 */		ld	[%o3+8],%o1 ! volatile
/* 0x0624	     */		add	%o1,1,%o1
/* 0x0628	     */		st	%o1,[%o3+8] ! volatile
/* 0x062c	  62 */		ba	L900000139
/* 0x0630	     */		ld	[%l0+4],%o1 ! volatile
                       L900000140:
/* 0x0634	     */		add	%o1,12,%o1
/* 0x0638	     */		ld	[%o1],%o1 ! volatile
/* 0x063c	     */		st	%o1,[%l0+4] ! volatile
                       L77000090:
/* 0x0640	     */		ld	[%l0+4],%o1 ! volatile
/* 0x0644	     */		cmp	%o1,0
/* 0x0648	     */		be	L77000092
/* 0x064c	  63 */		add	%l1,300,%o0
                       L77000091:
/* 0x0650	  63 */		call	_printf,2	! Result = %g0
/* 0x0654	     */		ld	[%o1+8],%o1 ! volatile
/* 0x0658	  62 */		ld	[%l0+4],%o1 ! volatile
                       L900000139:
/* 0x065c	     */		cmp	%o1,0
/* 0x0660	     */		bne,a	L900000140
/* 0x0664	     */		ld	[%l0+4],%o1 ! volatile
                       L77000089:
/* 0x0668	     */		ld	[%l0],%o0 ! volatile
/* 0x066c	     */		ld	[%o0],%o1 ! volatile
/* 0x0670	     */		ba	L77000090
/* 0x0674	     */		st	%o1,[%l0+4] ! volatile
                       L77000092:
/* 0x0678	  66 */		add	%fp,-20,%o1
/* 0x067c	     */		add	%fp,-12,%o0
/* 0x0680	     */		st	%g0,[%o1+4] ! volatile
/* 0x0684	  67 */		add	%fp,-20,%l0
/* 0x0688	  66 */		ld	[%o1],%o3 ! volatile
/* 0x068c	     */		ld	[%o0],%o1 ! volatile
/* 0x0690	     */		st	%o1,[%o3] ! volatile
/* 0x0694	     */		ld	[%o0+4],%o2 ! volatile
/* 0x0698	     */		st	%o2,[%o3+4] ! volatile
/* 0x069c	     */		ld	[%o0+8],%o1 ! volatile
/* 0x06a0	     */		st	%o1,[%o3+8] ! volatile
/* 0x06a4	  67 */		ba	L900000141
/* 0x06a8	     */		ld	[%l0+4],%o1 ! volatile
                       L900000142:
/* 0x06ac	     */		ld	[%o0],%o1 ! volatile
/* 0x06b0	     */		st	%o1,[%l0+4] ! volatile
                       L77000096:
/* 0x06b4	     */		ld	[%l0+4],%o1 ! volatile
/* 0x06b8	     */		cmp	%o1,0
/* 0x06bc	     */		be	L77000099
/* 0x06c0	  68 */		add	%l1,304,%o0
                       L77000097:
/* 0x06c4	  68 */		call	_printf,2	! Result = %g0
/* 0x06c8	     */		ld	[%o1+8],%o1 ! volatile
/* 0x06cc	  67 */		ld	[%l0+4],%o1 ! volatile
                       L900000141:
/* 0x06d0	     */		cmp	%o1,0
/* 0x06d4	     */		be,a	L900000142
/* 0x06d8	     */		ld	[%l0],%o0 ! volatile
                       L77000094:
/* 0x06dc	     */		ld	[%l0+4],%o1 ! volatile
/* 0x06e0	     */		ld	[%o1],%o1 ! volatile
/* 0x06e4	     */		ba	L77000096
/* 0x06e8	     */		st	%o1,[%l0+4] ! volatile
/* 0x06ec	   0 */		unimp	65536

! Begin Disassembling Stabs

	.stabs	"ptf;ptx;ptk;Xa;O;s;V=3.0;R=<<C++4.0.1 (ccfe 1.0) 7/13/94>>",60,0,0,0	! (/tmp/ccfe.20405.1.s:1)
	.stabs	"/tmp_mnt/home/lear1/hill/epics/base/src/cas/; /usr/lang//SC3.0.1/bin/CC -O -S -I.  tsDLListTest.cc",52,0,0,0	! (/tmp/ccfe.20405.1.s:2)
! End Disassembling Stabs

! Begin Disassembling Ident
! End Disassembling Ident
