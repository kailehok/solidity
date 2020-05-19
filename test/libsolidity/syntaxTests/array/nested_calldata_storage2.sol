pragma experimental ABIEncoderV2;

contract C {
  uint[][] tmp_i;
  function i(uint[][] calldata s) external { tmp_i = s; }
}

// ----
// UnimplementedFeatureError: Copying nested calldata dynamic arrays from calldata to storage is not implemented in the old codegen.
