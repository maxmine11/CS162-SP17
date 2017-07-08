# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(safe-boundary) begin
safe-boundary: exit(-1)
EOF
pass;
