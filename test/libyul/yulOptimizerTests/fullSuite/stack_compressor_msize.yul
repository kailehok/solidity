{
	let a

	function foo_singlereturn_0() -> out
	{
		mstore(lt(or(gt(1,or(or(gt(or(or(or(1,gt(or(gt(or(or(keccak256(msize(),or(gt(not(msize()),1),1)),1),not(1)),msize()),1),msize())),lt(or(1,sub(msize(),1)),1)),msize()),1),1),gt(not(msize()),1))),1),1),1)
		sstore(not(msize()),1)
	}

	function foo_singlereturn_1(in_1, in_2) -> out
	{
		extcodecopy(1,msize(),1,1)
	}

	a := foo_singlereturn_0()
	sstore(0,0)
	sstore(2,1)

	a := foo_singlereturn_1(calldataload(0),calldataload(3))
	sstore(0,0)
	sstore(3,1)
}
// ----
// step: fullSuite
//
// {
//     {
//         let _1 := gt(not(msize()), 1)
//         let _2 := msize()
//         let _3 := not(0)
//         let _4 := lt(or(1, add(msize(), _3)), 1)
//         let _5 := msize()
//         let _6 := msize()
//         pop(keccak256(msize(), or(gt(not(msize()), 1), 1)))
//         mstore(lt(or(gt(1, or(or(gt(or(or(or(gt(or(gt(_3, _6), 1), _5), _4), _2), 1), 1), _1), 1)), 1), 1), 1)
//         sstore(not(msize()), 1)
//         sstore(0, 0)
//         sstore(2, 1)
//         pop(foo_singlereturn_1(calldataload(0), calldataload(3)))
//         sstore(0, 0)
//         sstore(3, 1)
//     }
//     function foo_singlereturn_1(in, in_1) -> out
//     { extcodecopy(1, msize(), 1, 1) }
// }
