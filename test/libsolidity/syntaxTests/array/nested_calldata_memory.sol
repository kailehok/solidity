pragma experimental ABIEncoderV2;

contract Test {
    struct shouldBug {
        bytes[2] deadly;
    }
    function killer(bytes[2] calldata weapon) pure external {
      shouldBug(weapon);
    }
}

// ----
// UnimplementedFeatureError: Copying nested calldata dynamic arrays from calldata to memory is not implemented in the old codegen.
